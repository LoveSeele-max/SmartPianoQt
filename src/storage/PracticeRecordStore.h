#pragma once

#include "core/Song.h"
#include "practice/PracticeEngine.h"

#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
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
    int activeDurationSeconds = -1;
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
    int activeDurationSeconds = 0;
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
    QVector<PracticeSessionRecord> scoreTrend;
    QVector<PracticeMistakeStat> mistakeStats;
    QVector<PracticeMistakeStat> topWrongNotes;
    QVector<PracticeMistakeStat> topMissedNotes;
    int sessionCount = 0;
    int averageScore = 0;
    int totalCorrect = 0;
    int totalWrong = 0;
    int totalMissed = 0;
};

struct StoredSheetInfo {
    qint64 id = -1;
    QString title;
    QString filePath;
    QString sourceFormat;
    int bpm = 0;
    int ppq = 0;
    int noteCount = 0;
    QString updatedAt;
};

struct SheetCategoryInfo {
    qint64 id = -1;
    QString name;
    QString builtInKey;
    int sheetCount = 0;

    bool builtIn() const { return !builtInKey.isEmpty(); }
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
    QVector<PracticeSessionRecord> recentSessions(int limit = 5,
                                                  qint64 sheetId = -1,
                                                  bool completedOnly = false,
                                                  const QString &mode = QString());
    QVector<PracticeMistakeStat> mistakeStatsForSheet(qint64 sheetId,
                                                      int limit = 8,
                                                      bool completedOnly = false,
                                                      const QString &mode = QString());
    PracticeReportSummary reportForSheet(qint64 sheetId,
                                         int sessionLimit = 5,
                                         int mistakeLimit = 8,
                                         bool completedOnly = false,
                                         const QString &mode = QString());
    QHash<QString, StoredSheetInfo> sheetsForPaths(const QStringList &paths);
    QVector<SheetCategoryInfo> sheetCategories();
    qint64 createSheetCategory(const QString &name);
    bool addSheetToCategory(qint64 sheetId, qint64 categoryId);
    bool removeSheetFromCategory(qint64 sheetId, qint64 categoryId);
    bool setSheetCategoryMembership(qint64 sheetId, qint64 categoryId, bool enabled);
    QHash<qint64, QVector<qint64>> categoriesForSheets(const QVector<qint64> &sheetIds);
    QSet<qint64> sheetIdsForCategory(qint64 categoryId);

    static QString judgeTypeToString(PracticeJudgeType type);

private:
    bool ensureOpen();
    bool initializeSchema();
    int readUserVersion();
    bool setUserVersion(int version);
    bool ensureColumn(const QString &table, const QString &column, const QString &definition);
    bool ensureDefaultSheetCategories();
    QString defaultDatabasePath() const;
    QString sheetHash(const Song &song) const;
    QString normalizedCategoryName(const QString &name) const;
    QVector<PracticeMistakeStat> mistakeStatsForResult(qint64 sheetId,
                                                       const QString &result,
                                                       int limit,
                                                       bool completedOnly,
                                                       const QString &mode);
    void setLastError(const QString &message);

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QSqlDatabase m_db;
};
