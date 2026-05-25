#include "storage/PracticeRecordStore.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QVariant>
#include <QtMath>

namespace {

QString utcNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QVariant nullableInt(int value)
{
    return value >= 0 ? QVariant(value) : QVariant();
}

int scoreFor(const PracticeSessionSummary &summary)
{
    const int total = summary.correctCount + summary.wrongCount + summary.missedCount;
    if (total <= 0) return 0;
    return qBound(0, qRound(double(summary.correctCount) * 100.0 / double(total)), 100);
}

}

PracticeRecordStore::PracticeRecordStore()
    : m_connectionName(QStringLiteral("practice-records-%1").arg(reinterpret_cast<quintptr>(this)))
{
}

PracticeRecordStore::~PracticeRecordStore()
{
    close();
}

bool PracticeRecordStore::open(const QString &path)
{
    if (isOpen()) return true;
    if (m_db.isValid()) close();

    m_databasePath = path.isEmpty() ? defaultDatabasePath() : path;
    const QFileInfo info(m_databasePath);
    QDir dir = info.absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        setLastError(QStringLiteral("无法创建练习记录目录：%1").arg(dir.absolutePath()));
        return false;
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_databasePath);
    if (!m_db.open()) {
        setLastError(m_db.lastError().text());
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    return initializeSchema();
}

void PracticeRecordStore::close()
{
    if (!m_db.isValid()) return;

    const QString connection = m_db.connectionName();
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection);
}

bool PracticeRecordStore::isOpen() const
{
    return m_db.isValid() && m_db.isOpen();
}

qint64 PracticeRecordStore::upsertSheet(const Song &song, const QString &filePath, const QString &format)
{
    if (!ensureOpen()) return -1;

    const QString hash = sheetHash(song);
    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("SELECT id FROM sheets WHERE file_hash = ?"));
    select.addBindValue(hash);
    if (!select.exec()) {
        setLastError(select.lastError().text());
        return -1;
    }

    const QString now = utcNow();
    if (select.next()) {
        const qint64 id = select.value(0).toLongLong();
        QSqlQuery update(m_db);
        update.prepare(QStringLiteral(
            "UPDATE sheets "
            "SET title = ?, file_path = ?, source_format = ?, bpm = ?, ppq = ?, note_count = ?, updated_at = ? "
            "WHERE id = ?"));
        update.addBindValue(song.title);
        update.addBindValue(filePath);
        update.addBindValue(format);
        update.addBindValue(song.bpm);
        update.addBindValue(song.ppq);
        update.addBindValue(song.notes.size());
        update.addBindValue(now);
        update.addBindValue(id);
        if (!update.exec()) {
            setLastError(update.lastError().text());
            return -1;
        }
        return id;
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO sheets "
        "(title, file_path, source_format, file_hash, bpm, ppq, note_count, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    insert.addBindValue(song.title);
    insert.addBindValue(filePath);
    insert.addBindValue(format);
    insert.addBindValue(hash);
    insert.addBindValue(song.bpm);
    insert.addBindValue(song.ppq);
    insert.addBindValue(song.notes.size());
    insert.addBindValue(now);
    insert.addBindValue(now);
    if (!insert.exec()) {
        setLastError(insert.lastError().text());
        return -1;
    }

    return insert.lastInsertId().toLongLong();
}

qint64 PracticeRecordStore::beginSession(qint64 sheetId, const PracticeSessionStart &start)
{
    if (sheetId <= 0 || !ensureOpen()) return -1;

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO practice_sessions "
        "(sheet_id, started_at, mode, playback_speed, start_tick) "
        "VALUES (?, ?, ?, ?, ?)"));
    insert.addBindValue(sheetId);
    insert.addBindValue(utcNow());
    insert.addBindValue(start.mode);
    insert.addBindValue(start.playbackSpeed);
    insert.addBindValue(start.startTick);
    if (!insert.exec()) {
        setLastError(insert.lastError().text());
        return -1;
    }
    return insert.lastInsertId().toLongLong();
}

bool PracticeRecordStore::appendEvent(qint64 sessionId, const PracticeEventRecord &event)
{
    if (sessionId <= 0 || event.result == PracticeJudgeType::Ignored || !ensureOpen()) return false;

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO practice_events "
        "(session_id, expected_midi, actual_midi, expected_tick, actual_tick, result, velocity, offset_ms, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    insert.addBindValue(sessionId);
    insert.addBindValue(nullableInt(event.expectedMidi));
    insert.addBindValue(nullableInt(event.actualMidi));
    insert.addBindValue(event.expectedTick);
    insert.addBindValue(event.actualTick);
    insert.addBindValue(judgeTypeToString(event.result));
    insert.addBindValue(event.velocity);
    insert.addBindValue(event.offsetMs);
    insert.addBindValue(utcNow());
    if (!insert.exec()) {
        setLastError(insert.lastError().text());
        return false;
    }
    return true;
}

bool PracticeRecordStore::finishSession(qint64 sessionId, const PracticeSessionSummary &summary)
{
    if (sessionId <= 0 || !ensureOpen()) return false;

    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("SELECT started_at FROM practice_sessions WHERE id = ?"));
    select.addBindValue(sessionId);
    if (!select.exec() || !select.next()) {
        setLastError(select.lastError().text().isEmpty()
                         ? QStringLiteral("练习 session 不存在：%1").arg(sessionId)
                         : select.lastError().text());
        return false;
    }

    const QDateTime started = QDateTime::fromString(select.value(0).toString(), Qt::ISODateWithMs);
    const QDateTime ended = QDateTime::currentDateTimeUtc();
    const int durationSeconds = started.isValid() ? int(started.secsTo(ended)) : 0;

    QSqlQuery update(m_db);
    update.prepare(QStringLiteral(
        "UPDATE practice_sessions "
        "SET ended_at = ?, duration_seconds = ?, completed = ?, end_tick = ?, "
        "correct_count = ?, wrong_count = ?, missed_count = ?, score = ? "
        "WHERE id = ?"));
    update.addBindValue(ended.toString(Qt::ISODateWithMs));
    update.addBindValue(qMax(0, durationSeconds));
    update.addBindValue(summary.completed ? 1 : 0);
    update.addBindValue(summary.endTick);
    update.addBindValue(summary.correctCount);
    update.addBindValue(summary.wrongCount);
    update.addBindValue(summary.missedCount);
    update.addBindValue(scoreFor(summary));
    update.addBindValue(sessionId);
    if (!update.exec()) {
        setLastError(update.lastError().text());
        return false;
    }
    return true;
}

QString PracticeRecordStore::judgeTypeToString(PracticeJudgeType type)
{
    switch (type) {
    case PracticeJudgeType::Correct:
        return QStringLiteral("correct");
    case PracticeJudgeType::RepeatedNote:
        return QStringLiteral("repeated_note");
    case PracticeJudgeType::WrongNote:
        return QStringLiteral("wrong_note");
    case PracticeJudgeType::Early:
        return QStringLiteral("early");
    case PracticeJudgeType::Late:
        return QStringLiteral("late");
    case PracticeJudgeType::Missed:
        return QStringLiteral("missed");
    case PracticeJudgeType::Ignored:
        break;
    }
    return QStringLiteral("ignored");
}

bool PracticeRecordStore::ensureOpen()
{
    return isOpen() || open();
}

bool PracticeRecordStore::initializeSchema()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sheets ("
            "id INTEGER PRIMARY KEY,"
            "title TEXT NOT NULL,"
            "file_path TEXT NOT NULL,"
            "source_format TEXT NOT NULL,"
            "file_hash TEXT NOT NULL UNIQUE,"
            "bpm INTEGER NOT NULL DEFAULT 120,"
            "ppq INTEGER NOT NULL DEFAULT 480,"
            "note_count INTEGER NOT NULL DEFAULT 0,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS practice_sessions ("
            "id INTEGER PRIMARY KEY,"
            "sheet_id INTEGER NOT NULL,"
            "started_at TEXT NOT NULL,"
            "ended_at TEXT,"
            "duration_seconds INTEGER NOT NULL DEFAULT 0,"
            "mode TEXT NOT NULL,"
            "playback_speed INTEGER NOT NULL DEFAULT 100,"
            "start_tick INTEGER NOT NULL DEFAULT 0,"
            "completed INTEGER NOT NULL DEFAULT 0,"
            "end_tick INTEGER,"
            "correct_count INTEGER NOT NULL DEFAULT 0,"
            "wrong_count INTEGER NOT NULL DEFAULT 0,"
            "missed_count INTEGER NOT NULL DEFAULT 0,"
            "score INTEGER NOT NULL DEFAULT 0,"
            "FOREIGN KEY(sheet_id) REFERENCES sheets(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS practice_events ("
            "id INTEGER PRIMARY KEY,"
            "session_id INTEGER NOT NULL,"
            "expected_midi INTEGER,"
            "actual_midi INTEGER,"
            "expected_tick INTEGER NOT NULL,"
            "actual_tick INTEGER NOT NULL,"
            "result TEXT NOT NULL,"
            "velocity INTEGER NOT NULL DEFAULT 0,"
            "offset_ms INTEGER NOT NULL DEFAULT 0,"
            "created_at TEXT NOT NULL,"
            "FOREIGN KEY(session_id) REFERENCES practice_sessions(id)"
            ")"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_practice_sessions_sheet_started ON practice_sessions(sheet_id, started_at)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_practice_events_session_tick ON practice_events(session_id, expected_tick)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_practice_events_result ON practice_events(result)")
    };

    for (const QString &statement : statements) {
        QSqlQuery query(m_db);
        if (!query.exec(statement)) {
            setLastError(query.lastError().text());
            return false;
        }
    }
    if (!ensureColumn(QStringLiteral("practice_sessions"),
                      QStringLiteral("completed"),
                      QStringLiteral("completed INTEGER NOT NULL DEFAULT 0"))) {
        return false;
    }
    return true;
}

bool PracticeRecordStore::ensureColumn(const QString &table, const QString &column, const QString &definition)
{
    QSqlQuery info(m_db);
    if (!info.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        setLastError(info.lastError().text());
        return false;
    }
    while (info.next()) {
        if (info.value(1).toString() == column) {
            return true;
        }
    }

    QSqlQuery alter(m_db);
    if (!alter.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2").arg(table, definition))) {
        setLastError(alter.lastError().text());
        return false;
    }
    return true;
}

QString PracticeRecordStore::defaultDatabasePath() const
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty()) {
        basePath = QDir::current().filePath(QStringLiteral("data"));
    }
    return QDir(basePath).filePath(QStringLiteral("practice_records.sqlite3"));
}

QString PracticeRecordStore::sheetHash(const Song &song) const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << song.title << song.bpm << song.ppq << song.notes.size();
    for (const NoteEvent &note : song.notes) {
        stream << note.id
               << note.midi
               << note.velocity
               << note.startTick
               << note.durationTick
               << note.track
               << note.channel
               << note.fingering;
    }
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

void PracticeRecordStore::setLastError(const QString &message)
{
    m_lastError = message;
}
