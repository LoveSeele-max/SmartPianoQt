# Practice Database Design

This is the first SQLite design pass for storing local sheet metadata and practice history.

## Goals

- Keep the MIDI library scan fast while preserving stable sheet ids.
- Store each practice run as a session.
- Store per-note judge events so reports can find hard notes, hard bars, and repeated mistakes.
- Leave room for future adaptive practice features such as auto slow-down and focused loop ranges.

## Tables

```sql
CREATE TABLE sheets (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    file_path TEXT NOT NULL,
    source_format TEXT NOT NULL,
    file_hash TEXT NOT NULL UNIQUE,
    bpm INTEGER NOT NULL DEFAULT 120,
    ppq INTEGER NOT NULL DEFAULT 480,
    note_count INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE practice_sessions (
    id INTEGER PRIMARY KEY,
    sheet_id INTEGER NOT NULL,
    started_at TEXT NOT NULL,
    ended_at TEXT,
    duration_seconds INTEGER NOT NULL DEFAULT 0,
    mode TEXT NOT NULL,
    playback_speed INTEGER NOT NULL DEFAULT 100,
    start_tick INTEGER NOT NULL DEFAULT 0,
    end_tick INTEGER,
    correct_count INTEGER NOT NULL DEFAULT 0,
    wrong_count INTEGER NOT NULL DEFAULT 0,
    missed_count INTEGER NOT NULL DEFAULT 0,
    score INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(sheet_id) REFERENCES sheets(id)
);

CREATE TABLE practice_events (
    id INTEGER PRIMARY KEY,
    session_id INTEGER NOT NULL,
    expected_midi INTEGER,
    actual_midi INTEGER,
    expected_tick INTEGER NOT NULL,
    actual_tick INTEGER NOT NULL,
    result TEXT NOT NULL,
    velocity INTEGER NOT NULL DEFAULT 0,
    offset_ms INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL,
    FOREIGN KEY(session_id) REFERENCES practice_sessions(id)
);

CREATE INDEX idx_practice_sessions_sheet_started
    ON practice_sessions(sheet_id, started_at);

CREATE INDEX idx_practice_events_session_tick
    ON practice_events(session_id, expected_tick);

CREATE INDEX idx_practice_events_result
    ON practice_events(result);
```

## First Service Boundary

```cpp
class PracticeRecordStore {
public:
    bool open(const QString &path);
    qint64 upsertSheet(const Song &song, const QString &filePath, const QString &format);
    qint64 beginSession(qint64 sheetId, const PracticeSessionStart &start);
    void appendEvent(qint64 sessionId, const PracticeEventRecord &event);
    void finishSession(qint64 sessionId, const PracticeSessionSummary &summary);
};
```

The first implementation can live behind `src/storage/PracticeRecordStore.*` and be used only at session boundaries. Real-time note rendering and judge logic should not wait on SQLite writes.
