#pragma once

#include "core/Song.h"
#include "practice/PracticeEngine.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

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

struct PracticeSessionRecord {
    qint64 id = -1;
    qint64 sheetId = -1;
    QString sheetTitle;
    QString startedAt;
    QString endedAt;
    QString mode;
    bool completed = false;
    int durationSeconds = 0;
    int playbackSpeed = 100;
    qint64 startTick = 0;
    qint64 endTick = 0;
    int correctCount = 0;
    int wrongCount = 0;
    int missedCount = 0;
    int score = 0;
};

struct PracticeMistakeStat {
    int midi = -1;
    QString noteName;
    int wrongCount = 0;
    int missedCount = 0;
    int earlyCount = 0;
    int lateCount = 0;
    int totalCount = 0;
};

struct PracticeReportSummary {
    QVector<PracticeSessionRecord> recentSessions;
    QVector<PracticeMistakeStat> mistakeStats;
    int sessionCount = 0;
    int averageScore = 0;
    int totalCorrect = 0;
    int totalWrong = 0;
    int totalMissed = 0;
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
    QVector<PracticeSessionRecord> recentSessions(int limit = 5, qint64 sheetId = -1);
    QVector<PracticeMistakeStat> mistakeStatsForSheet(qint64 sheetId, int limit = 8);
    PracticeReportSummary reportForSheet(qint64 sheetId, int sessionLimit = 5, int mistakeLimit = 8);

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
