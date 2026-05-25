#pragma once

#include "core/Song.h"
#include "practice/PracticeEngine.h"

#include <QSqlDatabase>
#include <QString>

struct PracticeSessionStart {
    QString mode;
    int playbackSpeed = 100;
    qint64 startTick = 0;
};

struct PracticeEventRecord {
    PracticeJudgeType result = PracticeJudgeType::Ignored;
    int expectedMidi = -1;
    int actualMidi = -1;
    int velocity = 0;
    qint64 expectedTick = -1;
    qint64 actualTick = -1;
    int offsetMs = 0;
};

struct PracticeSessionSummary {
    bool completed = false;
    qint64 endTick = 0;
    int correctCount = 0;
    int wrongCount = 0;
    int missedCount = 0;
};

class PracticeRecordStore {
public:
    PracticeRecordStore();
    ~PracticeRecordStore();

    bool open(const QString &path = QString());
    void close();
    bool isOpen() const;

    QString databasePath() const { return m_databasePath; }
    QString lastError() const { return m_lastError; }

    qint64 upsertSheet(const Song &song, const QString &filePath, const QString &format);
    qint64 beginSession(qint64 sheetId, const PracticeSessionStart &start);
    bool appendEvent(qint64 sessionId, const PracticeEventRecord &event);
    bool finishSession(qint64 sessionId, const PracticeSessionSummary &summary);

    static QString judgeTypeToString(PracticeJudgeType type);

private:
    bool ensureOpen();
    bool initializeSchema();
    bool ensureColumn(const QString &table, const QString &column, const QString &definition);
    QString defaultDatabasePath() const;
    QString sheetHash(const Song &song) const;
    void setLastError(const QString &message);

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QSqlDatabase m_db;
};
