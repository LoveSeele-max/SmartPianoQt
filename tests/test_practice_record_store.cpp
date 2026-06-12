#include "test_support.h"

#include "storage/PracticeRecordStore.h"

#include <QHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

namespace SmartPianoTest {

void testPracticeRecordStore()
{
    QTemporaryDir dir;
    expect(dir.isValid(), "temporary directory should be available for practice record store test");
    const QString dbPath = dir.filePath(QStringLiteral("practice.sqlite"));

    PracticeRecordStore store;
    expect(store.open(dbPath), "PracticeRecordStore should open a SQLite database");
    expect(PracticeRecordStore::judgeTypeToString(PracticeJudgeType::Perfect) == QStringLiteral("perfect"),
           "PracticeRecordStore should persist perfect timing labels");
    expect(PracticeRecordStore::judgeTypeToString(PracticeJudgeType::Good) == QStringLiteral("good"),
           "PracticeRecordStore should persist good timing labels");

    Song song = makePlaybackSong();
    const qint64 sheetId = store.upsertSheet(song, QStringLiteral("test.mid"), QStringLiteral("midi"));
    expect(sheetId > 0, "PracticeRecordStore should upsert sheet metadata");
    const qint64 sameSheetId = store.upsertSheet(song, QStringLiteral("test.mid"), QStringLiteral("midi"));
    expect(sameSheetId == sheetId, "PracticeRecordStore should reuse sheet ids for unchanged content");

    const QVector<SheetCategoryInfo> defaultCategories = store.sheetCategories();
    auto findCategoryId = [](const QVector<SheetCategoryInfo> &categories, const QString &name) {
        for (const SheetCategoryInfo &category : categories) {
            if (category.name == name) return category.id;
        }
        return qint64(-1);
    };
    const qint64 favoriteCategoryId = findCategoryId(defaultCategories, QStringLiteral("喜欢"));
    const qint64 practiceCategoryId = findCategoryId(defaultCategories, QStringLiteral("练习"));
    expect(favoriteCategoryId > 0, "PracticeRecordStore should create the default favorite category");
    expect(practiceCategoryId > 0, "PracticeRecordStore should create the default practice category");

    const qint64 slowPracticeCategoryId = store.createSheetCategory(QStringLiteral("慢练"));
    expect(slowPracticeCategoryId > 0, "PracticeRecordStore should create custom sheet categories");
    const qint64 duplicateCategoryId = store.createSheetCategory(QStringLiteral("慢练"));
    expect(duplicateCategoryId == slowPracticeCategoryId, "PracticeRecordStore should reuse matching custom category names");

    expect(store.addSheetToCategory(sheetId, favoriteCategoryId),
           "PracticeRecordStore should add a sheet to the favorite category");
    expect(store.addSheetToCategory(sheetId, slowPracticeCategoryId),
           "PracticeRecordStore should add a sheet to a custom category");
    QHash<qint64, QVector<qint64>> categoriesBySheet = store.categoriesForSheets({ sheetId });
    expect(categoriesBySheet.value(sheetId).contains(favoriteCategoryId),
           "PracticeRecordStore should query favorite membership by sheet");
    expect(categoriesBySheet.value(sheetId).contains(slowPracticeCategoryId),
           "PracticeRecordStore should query custom membership by sheet");
    expect(store.sheetIdsForCategory(favoriteCategoryId).contains(sheetId),
           "PracticeRecordStore should query sheet ids by category");
    expect(store.removeSheetFromCategory(sheetId, slowPracticeCategoryId),
           "PracticeRecordStore should remove a sheet from a custom category");
    categoriesBySheet = store.categoriesForSheets({ sheetId });
    expect(!categoriesBySheet.value(sheetId).contains(slowPracticeCategoryId),
           "PracticeRecordStore should clear removed category membership");

    PracticeSessionStart start;
    start.mode = QStringLiteral("rhythm");
    start.playbackSpeed = 95;
    start.startTick = 480;
    const qint64 sessionId = store.beginSession(sheetId, start);
    expect(sessionId > 0, "PracticeRecordStore should begin a practice session");

    PracticeEventRecord event;
    event.result = PracticeJudgeType::Correct;
    event.expectedMidi = 60;
    event.actualMidi = 60;
    event.velocity = 96;
    event.expectedTick = 480;
    event.actualTick = 500;
    event.offsetMs = 21;
    expect(store.appendEvent(sessionId, event), "PracticeRecordStore should append judge events");

    PracticeEventRecord wrongEvent;
    wrongEvent.result = PracticeJudgeType::WrongNote;
    wrongEvent.expectedMidi = 62;
    wrongEvent.actualMidi = 61;
    wrongEvent.velocity = 80;
    wrongEvent.expectedTick = 960;
    wrongEvent.actualTick = 940;
    wrongEvent.offsetMs = -20;
    expect(store.appendEvent(sessionId, wrongEvent), "PracticeRecordStore should append mistake events");

    PracticeSessionSummary summary;
    summary.completed = true;
    summary.endTick = 1440;
    summary.activeDurationSeconds = 7;
    summary.correctCount = 1;
    summary.wrongCount = 1;
    expect(store.finishSession(sessionId, summary), "PracticeRecordStore should finish a practice session");

    PracticeSessionStart secondStart;
    secondStart.mode = QStringLiteral("practice");
    secondStart.playbackSpeed = 100;
    secondStart.startTick = 0;
    const qint64 secondSessionId = store.beginSession(sheetId, secondStart);
    expect(secondSessionId > 0, "PracticeRecordStore should begin a second practice session");

    PracticeEventRecord secondWrongEvent;
    secondWrongEvent.result = PracticeJudgeType::WrongNote;
    secondWrongEvent.expectedMidi = 62;
    secondWrongEvent.actualMidi = 61;
    secondWrongEvent.velocity = 76;
    secondWrongEvent.expectedTick = 960;
    secondWrongEvent.actualTick = 970;
    secondWrongEvent.offsetMs = 10;
    expect(store.appendEvent(secondSessionId, secondWrongEvent), "PracticeRecordStore should append repeated mistake events");

    PracticeEventRecord missedEvent;
    missedEvent.result = PracticeJudgeType::Missed;
    missedEvent.expectedMidi = 64;
    missedEvent.velocity = 0;
    missedEvent.expectedTick = 1440;
    missedEvent.actualTick = 1540;
    missedEvent.offsetMs = 104;
    expect(store.appendEvent(secondSessionId, missedEvent), "PracticeRecordStore should append missed-note events");
    missedEvent.expectedTick = 1920;
    missedEvent.actualTick = 2020;
    expect(store.appendEvent(secondSessionId, missedEvent), "PracticeRecordStore should append repeated missed-note events");

    PracticeSessionSummary secondSummary;
    secondSummary.completed = false;
    secondSummary.endTick = 480;
    secondSummary.activeDurationSeconds = 3;
    secondSummary.correctCount = 0;
    secondSummary.wrongCount = 1;
    secondSummary.missedCount = 2;
    expect(store.finishSession(secondSessionId, secondSummary), "PracticeRecordStore should finish a filtered practice session");

    const QVector<PracticeSessionRecord> recent = store.recentSessions(3, sheetId);
    expect(recent.size() == 2, "PracticeRecordStore should query recent sessions for a sheet");
    expect(recent.at(0).sheetId == sheetId, "recent session should preserve sheet id");
    expect(recent.at(0).activeDurationSeconds == 3, "recent session should expose active duration");

    const QVector<PracticeSessionRecord> completedRhythm = store.recentSessions(
        3, sheetId, true, QStringLiteral("rhythm"));
    expect(completedRhythm.size() == 1, "PracticeRecordStore should filter recent sessions by completion and mode");
    expect(completedRhythm.at(0).score == 50, "filtered recent session should expose calculated score");

    const QVector<PracticeMistakeStat> mistakes = store.mistakeStatsForSheet(sheetId, 3);
    expect(mistakes.size() == 2, "PracticeRecordStore should query mistake stats for a sheet");
    expect(mistakes.at(0).midi == 62, "mistake stats should group by expected pitch");
    expect(mistakes.at(0).wrongCount == 2, "mistake stats should count wrong notes");
    expect(mistakes.at(1).midi == 64, "mistake stats should include missed-note pitch groups");
    expect(mistakes.at(1).missedCount == 2, "mistake stats should count missed notes");

    auto addTrendSession = [&](int correct, int wrong, int missed) {
        PracticeSessionStart trendStart;
        trendStart.mode = QStringLiteral("practice");
        trendStart.playbackSpeed = 100;
        trendStart.startTick = 0;
        const qint64 trendSessionId = store.beginSession(sheetId, trendStart);
        expect(trendSessionId > 0, "PracticeRecordStore should begin trend practice sessions");

        PracticeSessionSummary trendSummary;
        trendSummary.completed = false;
        trendSummary.endTick = 480;
        trendSummary.activeDurationSeconds = 4;
        trendSummary.correctCount = correct;
        trendSummary.wrongCount = wrong;
        trendSummary.missedCount = missed;
        expect(store.finishSession(trendSessionId, trendSummary), "PracticeRecordStore should finish trend practice sessions");
    };
    addTrendSession(2, 0, 0);
    addTrendSession(1, 1, 0);
    addTrendSession(3, 0, 0);
    addTrendSession(0, 1, 0);

    const PracticeReportSummary report = store.reportForSheet(sheetId, 3, 3);
    expect(report.sessionCount == 6, "PracticeRecordStore report should count sessions");
    expect(report.averageScore == 50, "PracticeRecordStore report should average scores");
    expect(report.totalWrong == 4, "PracticeRecordStore report should aggregate wrong notes");
    expect(report.totalMissed == 2, "PracticeRecordStore report should aggregate missed notes");
    expect(report.scoreTrend.size() == 5, "PracticeRecordStore report should expose the latest five-score trend");
    expect(report.scoreTrend.at(0).score == 0, "score trend should be chronological after trimming old sessions");
    expect(report.scoreTrend.at(1).score == 100, "score trend should preserve the second latest trend score");
    expect(report.scoreTrend.last().score == 0, "score trend should end with the newest session");
    expect(report.topWrongNotes.size() == 1, "PracticeRecordStore report should expose top wrong notes");
    expect(report.topWrongNotes.at(0).midi == 62, "top wrong notes should rank by wrong-note count");
    expect(report.topWrongNotes.at(0).wrongCount == 2, "top wrong notes should expose wrong-note totals");
    expect(report.topMissedNotes.size() == 1, "PracticeRecordStore report should expose top missed notes");
    expect(report.topMissedNotes.at(0).midi == 64, "top missed notes should rank by missed-note count");
    expect(report.topMissedNotes.at(0).missedCount == 2, "top missed notes should expose missed-note totals");

    const PracticeReportSummary completedReport = store.reportForSheet(
        sheetId, 3, 3, true, QStringLiteral("rhythm"));
    expect(completedReport.sessionCount == 1, "PracticeRecordStore report should filter completed rhythm sessions");
    expect(completedReport.averageScore == 50, "filtered report should average only matching sessions");

    const QHash<QString, StoredSheetInfo> sheetsByPath =
        store.sheetsForPaths(QStringList{ QStringLiteral("test.mid") });
    expect(sheetsByPath.contains(QStringLiteral("test.mid")), "PracticeRecordStore should query sheets by local path");
    expect(sheetsByPath.value(QStringLiteral("test.mid")).id == sheetId, "sheet path query should preserve sheet id");
    store.close();

    const QString connectionName = QStringLiteral("practice-store-readback");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(dbPath);
    expect(db.open(), "readback database connection should open");

    QSqlQuery sessions(db);
    expect(sessions.exec(QStringLiteral(
               "SELECT mode, correct_count, score, completed, active_duration_seconds "
               "FROM practice_sessions WHERE id = 1")),
           "readback session query should run");
    expect(sessions.next(), "readback should find the recorded session");
    expect(sessions.value(0).toString() == QStringLiteral("rhythm"), "recorded session should keep its mode");
    expect(sessions.value(1).toInt() == 1, "recorded session should keep final correct count");
    expect(sessions.value(2).toInt() == 50, "recorded session should calculate score");
    expect(sessions.value(3).toInt() == 1, "recorded session should keep completion state");
    expect(sessions.value(4).toInt() == 7, "recorded session should keep active duration");

    QSqlQuery events(db);
    expect(events.exec(QStringLiteral("SELECT result, actual_midi FROM practice_events WHERE session_id = 1")),
           "readback event query should run");
    expect(events.next(), "readback should find the recorded event");
    expect(events.value(0).toString() == QStringLiteral("correct"), "recorded event should store judge type text");
    expect(events.value(1).toInt() == 60, "recorded event should keep actual pitch");

    QSqlQuery version(db);
    expect(version.exec(QStringLiteral("PRAGMA user_version")), "schema version query should run");
    expect(version.next(), "schema version query should return a row");
    expect(version.value(0).toInt() >= 3, "schema should store the migration user_version");

    sessions.finish();
    events.finish();
    version.finish();

    QSqlQuery setFutureVersion(db);
    expect(setFutureVersion.exec(QStringLiteral("PRAGMA user_version = 9")),
           "schema version should be adjustable for future-version migration tests");
    setFutureVersion.finish();

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);

    PracticeRecordStore reopened;
    expect(reopened.open(dbPath), "PracticeRecordStore should reopen a future-version database");
    reopened.close();

    const QString futureConnectionName = QStringLiteral("practice-store-future-version");
    QSqlDatabase futureDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), futureConnectionName);
    futureDb.setDatabaseName(dbPath);
    expect(futureDb.open(), "future-version readback database connection should open");

    QSqlQuery futureVersion(futureDb);
    expect(futureVersion.exec(QStringLiteral("PRAGMA user_version")), "future schema version query should run");
    expect(futureVersion.next(), "future schema version query should return a row");
    expect(futureVersion.value(0).toInt() == 9, "schema migration should not downgrade newer user_version values");

    futureVersion.finish();
    futureDb.close();
    futureDb = QSqlDatabase();
    QSqlDatabase::removeDatabase(futureConnectionName);
}

}
