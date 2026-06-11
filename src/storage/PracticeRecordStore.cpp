#include "storage/PracticeRecordStore.h"

#include "core/NoteUtils.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QtMath>
#include <algorithm>

namespace {

constexpr int CurrentSchemaVersion = 3;

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

class ScopedTransaction {
public:
    explicit ScopedTransaction(QSqlDatabase &db)
        : m_db(db)
        , m_active(db.transaction())
    {
    }

    ~ScopedTransaction()
    {
        if (m_active) {
            m_db.rollback();
        }
    }

    bool isActive() const { return m_active; }

    bool commit()
    {
        if (!m_active) return false;
        if (!m_db.commit()) return false;
        m_active = false;
        return true;
    }

private:
    QSqlDatabase &m_db;
    bool m_active = false;
};

QString placeholders(int count)
{
    QStringList items;
    items.reserve(count);
    for (int i = 0; i < count; ++i) {
        items.push_back(QStringLiteral("?"));
    }
    return items.join(QStringLiteral(","));
}

void appendSessionFilters(QString &sql,
                          QVariantList &bindings,
                          const QString &alias,
                          qint64 sheetId,
                          bool completedOnly,
                          const QString &mode)
{
    if (sheetId > 0) {
        sql += QStringLiteral("AND %1.sheet_id = ? ").arg(alias);
        bindings.push_back(sheetId);
    }
    if (completedOnly) {
        sql += QStringLiteral("AND %1.completed = 1 ").arg(alias);
    }
    if (!mode.isEmpty()) {
        sql += QStringLiteral("AND %1.mode = ? ").arg(alias);
        bindings.push_back(mode);
    }
}

void bindAll(QSqlQuery &query, const QVariantList &bindings)
{
    for (const QVariant &value : bindings) {
        query.addBindValue(value);
    }
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

    ScopedTransaction transaction(m_db);
    if (!transaction.isActive()) {
        setLastError(m_db.lastError().text());
        return -1;
    }

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
        if (!transaction.commit()) {
            setLastError(m_db.lastError().text());
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
    const qint64 id = insert.lastInsertId().toLongLong();
    if (!transaction.commit()) {
        setLastError(m_db.lastError().text());
        return -1;
    }

    return id;
}

qint64 PracticeRecordStore::beginSession(qint64 sheetId, const PracticeSessionStart &start)
{
    if (sheetId <= 0 || !ensureOpen()) return -1;

    ScopedTransaction transaction(m_db);
    if (!transaction.isActive()) {
        setLastError(m_db.lastError().text());
        return -1;
    }

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
    const qint64 id = insert.lastInsertId().toLongLong();
    if (!transaction.commit()) {
        setLastError(m_db.lastError().text());
        return -1;
    }
    return id;
}

bool PracticeRecordStore::appendEvent(qint64 sessionId, const PracticeEventRecord &event)
{
    if (sessionId <= 0 || event.result == PracticeJudgeType::Ignored || !ensureOpen()) return false;

    ScopedTransaction transaction(m_db);
    if (!transaction.isActive()) {
        setLastError(m_db.lastError().text());
        return false;
    }

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
    if (!transaction.commit()) {
        setLastError(m_db.lastError().text());
        return false;
    }
    return true;
}

bool PracticeRecordStore::finishSession(qint64 sessionId, const PracticeSessionSummary &summary)
{
    if (sessionId <= 0 || !ensureOpen()) return false;

    ScopedTransaction transaction(m_db);
    if (!transaction.isActive()) {
        setLastError(m_db.lastError().text());
        return false;
    }

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
    const int activeDurationSeconds = summary.activeDurationSeconds >= 0
        ? summary.activeDurationSeconds
        : durationSeconds;

    QSqlQuery update(m_db);
    update.prepare(QStringLiteral(
        "UPDATE practice_sessions "
        "SET ended_at = ?, duration_seconds = ?, active_duration_seconds = ?, completed = ?, end_tick = ?, "
        "correct_count = ?, wrong_count = ?, missed_count = ?, score = ? "
        "WHERE id = ?"));
    update.addBindValue(ended.toString(Qt::ISODateWithMs));
    update.addBindValue(qMax(0, durationSeconds));
    update.addBindValue(qMax(0, activeDurationSeconds));
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
    if (!transaction.commit()) {
        setLastError(m_db.lastError().text());
        return false;
    }
    return true;
}

QVector<PracticeSessionRecord> PracticeRecordStore::recentSessions(int limit,
                                                                    qint64 sheetId,
                                                                    bool completedOnly,
                                                                    const QString &mode)
{
    QVector<PracticeSessionRecord> sessions;
    if (!ensureOpen()) return sessions;

    const int clampedLimit = qBound(1, limit, 50);
    QString sql = QStringLiteral(
        "SELECT ps.id, ps.sheet_id, s.title, ps.started_at, ps.ended_at, ps.mode, ps.completed, "
        "ps.duration_seconds, ps.active_duration_seconds, ps.playback_speed, ps.start_tick, COALESCE(ps.end_tick, 0), "
        "ps.correct_count, ps.wrong_count, ps.missed_count, ps.score "
        "FROM practice_sessions ps "
        "JOIN sheets s ON s.id = ps.sheet_id "
        "WHERE 1 = 1 ");
    QVariantList bindings;
    appendSessionFilters(sql, bindings, QStringLiteral("ps"), sheetId, completedOnly, mode);
    sql += QStringLiteral("ORDER BY ps.started_at DESC, ps.id DESC LIMIT ?");

    QSqlQuery query(m_db);
    query.prepare(sql);
    bindAll(query, bindings);
    query.addBindValue(clampedLimit);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return sessions;
    }

    while (query.next()) {
        PracticeSessionRecord session;
        session.id = query.value(0).toLongLong();
        session.sheetId = query.value(1).toLongLong();
        session.sheetTitle = query.value(2).toString();
        session.startedAt = query.value(3).toString();
        session.endedAt = query.value(4).toString();
        session.mode = query.value(5).toString();
        session.completed = query.value(6).toInt() != 0;
        session.durationSeconds = query.value(7).toInt();
        session.activeDurationSeconds = query.value(8).toInt();
        session.playbackSpeed = query.value(9).toInt();
        session.startTick = query.value(10).toLongLong();
        session.endTick = query.value(11).toLongLong();
        session.correctCount = query.value(12).toInt();
        session.wrongCount = query.value(13).toInt();
        session.missedCount = query.value(14).toInt();
        session.score = query.value(15).toInt();
        sessions.push_back(session);
    }
    return sessions;
}

QVector<PracticeMistakeStat> PracticeRecordStore::mistakeStatsForSheet(qint64 sheetId,
                                                                        int limit,
                                                                        bool completedOnly,
                                                                        const QString &mode)
{
    QVector<PracticeMistakeStat> stats;
    if (sheetId <= 0 || !ensureOpen()) return stats;

    QString sql = QStringLiteral(
        "SELECT COALESCE(pe.expected_midi, pe.actual_midi) AS midi, "
        "SUM(CASE WHEN pe.result = 'wrong_note' THEN 1 ELSE 0 END) AS wrong_count, "
        "SUM(CASE WHEN pe.result = 'missed' THEN 1 ELSE 0 END) AS missed_count, "
        "SUM(CASE WHEN pe.result = 'early' THEN 1 ELSE 0 END) AS early_count, "
        "SUM(CASE WHEN pe.result = 'late' THEN 1 ELSE 0 END) AS late_count, "
        "COUNT(*) AS total_count "
        "FROM practice_events pe "
        "JOIN practice_sessions ps ON ps.id = pe.session_id "
        "WHERE pe.result IN ('wrong_note', 'missed', 'early', 'late') "
        "AND COALESCE(pe.expected_midi, pe.actual_midi) IS NOT NULL ");
    QVariantList bindings;
    appendSessionFilters(sql, bindings, QStringLiteral("ps"), sheetId, completedOnly, mode);
    sql += QStringLiteral(
        "GROUP BY midi "
        "ORDER BY total_count DESC, midi ASC "
        "LIMIT ?");

    QSqlQuery query(m_db);
    query.prepare(sql);
    bindAll(query, bindings);
    query.addBindValue(qBound(1, limit, 50));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return stats;
    }

    while (query.next()) {
        PracticeMistakeStat stat;
        stat.midi = query.value(0).toInt();
        stat.noteName = NoteUtils::midiToName(stat.midi);
        stat.wrongCount = query.value(1).toInt();
        stat.missedCount = query.value(2).toInt();
        stat.earlyCount = query.value(3).toInt();
        stat.lateCount = query.value(4).toInt();
        stat.totalCount = query.value(5).toInt();
        stats.push_back(stat);
    }
    return stats;
}

QVector<PracticeMistakeStat> PracticeRecordStore::mistakeStatsForResult(qint64 sheetId,
                                                                        const QString &result,
                                                                        int limit,
                                                                        bool completedOnly,
                                                                        const QString &mode)
{
    QVector<PracticeMistakeStat> stats;
    if (sheetId <= 0 || result.isEmpty() || !ensureOpen()) return stats;

    QString sql = QStringLiteral(
        "SELECT COALESCE(pe.expected_midi, pe.actual_midi) AS midi, "
        "SUM(CASE WHEN pe.result = 'wrong_note' THEN 1 ELSE 0 END) AS wrong_count, "
        "SUM(CASE WHEN pe.result = 'missed' THEN 1 ELSE 0 END) AS missed_count, "
        "SUM(CASE WHEN pe.result = 'early' THEN 1 ELSE 0 END) AS early_count, "
        "SUM(CASE WHEN pe.result = 'late' THEN 1 ELSE 0 END) AS late_count, "
        "COUNT(*) AS total_count "
        "FROM practice_events pe "
        "JOIN practice_sessions ps ON ps.id = pe.session_id "
        "WHERE pe.result = ? "
        "AND COALESCE(pe.expected_midi, pe.actual_midi) IS NOT NULL ");
    QVariantList bindings;
    bindings.push_back(result);
    appendSessionFilters(sql, bindings, QStringLiteral("ps"), sheetId, completedOnly, mode);
    sql += QStringLiteral(
        "GROUP BY midi "
        "ORDER BY total_count DESC, midi ASC "
        "LIMIT ?");

    QSqlQuery query(m_db);
    query.prepare(sql);
    bindAll(query, bindings);
    query.addBindValue(qBound(1, limit, 50));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return stats;
    }

    while (query.next()) {
        PracticeMistakeStat stat;
        stat.midi = query.value(0).toInt();
        stat.noteName = NoteUtils::midiToName(stat.midi);
        stat.wrongCount = query.value(1).toInt();
        stat.missedCount = query.value(2).toInt();
        stat.earlyCount = query.value(3).toInt();
        stat.lateCount = query.value(4).toInt();
        stat.totalCount = query.value(5).toInt();
        stats.push_back(stat);
    }
    return stats;
}

PracticeReportSummary PracticeRecordStore::reportForSheet(qint64 sheetId,
                                                           int sessionLimit,
                                                           int mistakeLimit,
                                                           bool completedOnly,
                                                           const QString &mode)
{
    PracticeReportSummary report;
    if (sheetId <= 0 || !ensureOpen()) return report;

    report.recentSessions = recentSessions(sessionLimit, sheetId, completedOnly, mode);
    report.scoreTrend = recentSessions(5, sheetId, completedOnly, mode);
    std::reverse(report.scoreTrend.begin(), report.scoreTrend.end());
    report.mistakeStats = mistakeStatsForSheet(sheetId, mistakeLimit, completedOnly, mode);
    report.topWrongNotes = mistakeStatsForResult(sheetId,
                                                 QStringLiteral("wrong_note"),
                                                 5,
                                                 completedOnly,
                                                 mode);
    report.topMissedNotes = mistakeStatsForResult(sheetId,
                                                  QStringLiteral("missed"),
                                                  5,
                                                  completedOnly,
                                                  mode);

    QString sql = QStringLiteral(
        "SELECT COUNT(*), COALESCE(ROUND(AVG(score)), 0), "
        "COALESCE(SUM(correct_count), 0), COALESCE(SUM(wrong_count), 0), COALESCE(SUM(missed_count), 0) "
        "FROM practice_sessions ps "
        "WHERE 1 = 1 ");
    QVariantList bindings;
    appendSessionFilters(sql, bindings, QStringLiteral("ps"), sheetId, completedOnly, mode);

    QSqlQuery query(m_db);
    query.prepare(sql);
    bindAll(query, bindings);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return report;
    }

    if (query.next()) {
        report.sessionCount = query.value(0).toInt();
        report.averageScore = query.value(1).toInt();
        report.totalCorrect = query.value(2).toInt();
        report.totalWrong = query.value(3).toInt();
        report.totalMissed = query.value(4).toInt();
    }
    return report;
}

QHash<QString, StoredSheetInfo> PracticeRecordStore::sheetsForPaths(const QStringList &paths)
{
    QHash<QString, StoredSheetInfo> records;
    if (paths.isEmpty() || !ensureOpen()) return records;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, title, file_path, source_format, bpm, ppq, note_count, updated_at "
        "FROM sheets "
        "WHERE file_path IN (%1) "
        "ORDER BY updated_at DESC, id DESC").arg(placeholders(paths.size())));
    for (const QString &path : paths) {
        query.addBindValue(path);
    }

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return records;
    }

    while (query.next()) {
        StoredSheetInfo info;
        info.id = query.value(0).toLongLong();
        info.title = query.value(1).toString();
        info.filePath = query.value(2).toString();
        info.sourceFormat = query.value(3).toString();
        info.bpm = query.value(4).toInt();
        info.ppq = query.value(5).toInt();
        info.noteCount = query.value(6).toInt();
        info.updatedAt = query.value(7).toString();
        if (!records.contains(info.filePath)) {
            records.insert(info.filePath, info);
        }
    }
    return records;
}

QVector<SheetCategoryInfo> PracticeRecordStore::sheetCategories()
{
    QVector<SheetCategoryInfo> categories;
    if (!ensureOpen()) return categories;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT c.id, c.name, COALESCE(c.built_in_key, ''), COUNT(scm.sheet_id) "
        "FROM sheet_categories c "
        "LEFT JOIN sheet_category_members scm ON scm.category_id = c.id "
        "GROUP BY c.id, c.name, c.built_in_key "
        "ORDER BY "
        "CASE c.built_in_key "
        "WHEN 'favorite' THEN 0 "
        "WHEN 'practice' THEN 1 "
        "ELSE 2 END, "
        "LOWER(c.name) ASC"));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return categories;
    }

    while (query.next()) {
        SheetCategoryInfo category;
        category.id = query.value(0).toLongLong();
        category.name = query.value(1).toString();
        category.builtInKey = query.value(2).toString();
        category.sheetCount = query.value(3).toInt();
        categories.push_back(category);
    }
    return categories;
}

qint64 PracticeRecordStore::createSheetCategory(const QString &name)
{
    if (!ensureOpen()) return -1;

    const QString normalized = normalizedCategoryName(name);
    if (normalized.isEmpty()) {
        setLastError(QStringLiteral("分类名称不能为空"));
        return -1;
    }
    if (normalized == QStringLiteral("全部")) {
        setLastError(QStringLiteral("全部是内置筛选，不能作为自定义分类"));
        return -1;
    }

    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("SELECT id FROM sheet_categories WHERE name = ? COLLATE NOCASE"));
    select.addBindValue(normalized);
    if (!select.exec()) {
        setLastError(select.lastError().text());
        return -1;
    }
    if (select.next()) {
        return select.value(0).toLongLong();
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO sheet_categories (name, built_in_key, created_at, updated_at) "
        "VALUES (?, NULL, ?, ?)"));
    const QString now = utcNow();
    insert.addBindValue(normalized);
    insert.addBindValue(now);
    insert.addBindValue(now);
    if (!insert.exec()) {
        setLastError(insert.lastError().text());
        return -1;
    }
    return insert.lastInsertId().toLongLong();
}

bool PracticeRecordStore::addSheetToCategory(qint64 sheetId, qint64 categoryId)
{
    if (sheetId <= 0 || categoryId <= 0 || !ensureOpen()) return false;

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO sheet_category_members (category_id, sheet_id, created_at) "
        "VALUES (?, ?, ?)"));
    insert.addBindValue(categoryId);
    insert.addBindValue(sheetId);
    insert.addBindValue(utcNow());
    if (!insert.exec()) {
        setLastError(insert.lastError().text());
        return false;
    }
    return true;
}

bool PracticeRecordStore::removeSheetFromCategory(qint64 sheetId, qint64 categoryId)
{
    if (sheetId <= 0 || categoryId <= 0 || !ensureOpen()) return false;

    QSqlQuery remove(m_db);
    remove.prepare(QStringLiteral(
        "DELETE FROM sheet_category_members WHERE category_id = ? AND sheet_id = ?"));
    remove.addBindValue(categoryId);
    remove.addBindValue(sheetId);
    if (!remove.exec()) {
        setLastError(remove.lastError().text());
        return false;
    }
    return true;
}

bool PracticeRecordStore::setSheetCategoryMembership(qint64 sheetId, qint64 categoryId, bool enabled)
{
    return enabled ? addSheetToCategory(sheetId, categoryId)
                   : removeSheetFromCategory(sheetId, categoryId);
}

QHash<qint64, QVector<qint64>> PracticeRecordStore::categoriesForSheets(const QVector<qint64> &sheetIds)
{
    QHash<qint64, QVector<qint64>> categories;
    if (sheetIds.isEmpty() || !ensureOpen()) return categories;

    QVector<qint64> uniqueIds;
    uniqueIds.reserve(sheetIds.size());
    QSet<qint64> seen;
    for (qint64 sheetId : sheetIds) {
        if (sheetId <= 0 || seen.contains(sheetId)) continue;
        seen.insert(sheetId);
        uniqueIds.push_back(sheetId);
        categories.insert(sheetId, {});
    }
    if (uniqueIds.isEmpty()) return categories;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT sheet_id, category_id "
        "FROM sheet_category_members "
        "WHERE sheet_id IN (%1) "
        "ORDER BY created_at ASC, category_id ASC").arg(placeholders(uniqueIds.size())));
    for (qint64 sheetId : uniqueIds) {
        query.addBindValue(sheetId);
    }

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return categories;
    }

    while (query.next()) {
        categories[query.value(0).toLongLong()].push_back(query.value(1).toLongLong());
    }
    return categories;
}

QSet<qint64> PracticeRecordStore::sheetIdsForCategory(qint64 categoryId)
{
    QSet<qint64> sheetIds;
    if (categoryId <= 0 || !ensureOpen()) return sheetIds;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT sheet_id FROM sheet_category_members WHERE category_id = ?"));
    query.addBindValue(categoryId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return sheetIds;
    }

    while (query.next()) {
        sheetIds.insert(query.value(0).toLongLong());
    }
    return sheetIds;
}

QString PracticeRecordStore::judgeTypeToString(PracticeJudgeType type)
{
    switch (type) {
    case PracticeJudgeType::Perfect:
        return QStringLiteral("perfect");
    case PracticeJudgeType::Good:
        return QStringLiteral("good");
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
    ScopedTransaction transaction(m_db);
    if (!transaction.isActive()) {
        setLastError(m_db.lastError().text());
        return false;
    }

    const int version = readUserVersion();
    if (version < 0) return false;

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
            "active_duration_seconds INTEGER NOT NULL DEFAULT 0,"
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
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sheet_categories ("
            "id INTEGER PRIMARY KEY,"
            "name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            "built_in_key TEXT UNIQUE,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sheet_category_members ("
            "category_id INTEGER NOT NULL,"
            "sheet_id INTEGER NOT NULL,"
            "created_at TEXT NOT NULL,"
            "PRIMARY KEY(category_id, sheet_id),"
            "FOREIGN KEY(category_id) REFERENCES sheet_categories(id) ON DELETE CASCADE,"
            "FOREIGN KEY(sheet_id) REFERENCES sheets(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_practice_sessions_sheet_started ON practice_sessions(sheet_id, started_at)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_practice_events_session_tick ON practice_events(session_id, expected_tick)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_practice_events_result ON practice_events(result)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sheet_category_members_sheet ON sheet_category_members(sheet_id)")
    };

    for (const QString &statement : statements) {
        QSqlQuery query(m_db);
        if (!query.exec(statement)) {
            setLastError(query.lastError().text());
            return false;
        }
    }

    if (version < 1 &&
        !ensureColumn(QStringLiteral("practice_sessions"),
                      QStringLiteral("completed"),
                      QStringLiteral("completed INTEGER NOT NULL DEFAULT 0"))) {
        return false;
    }
    if (version < 2 &&
        !ensureColumn(QStringLiteral("practice_sessions"),
                      QStringLiteral("active_duration_seconds"),
                      QStringLiteral("active_duration_seconds INTEGER NOT NULL DEFAULT 0"))) {
        return false;
    }
    if (!ensureDefaultSheetCategories()) {
        return false;
    }
    if (version < CurrentSchemaVersion && !setUserVersion(CurrentSchemaVersion)) {
        return false;
    }
    if (!transaction.commit()) {
        setLastError(m_db.lastError().text());
        return false;
    }
    return true;
}

int PracticeRecordStore::readUserVersion()
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA user_version"))) {
        setLastError(query.lastError().text());
        return -1;
    }
    return query.next() ? query.value(0).toInt() : 0;
}

bool PracticeRecordStore::setUserVersion(int version)
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version))) {
        setLastError(query.lastError().text());
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

bool PracticeRecordStore::ensureDefaultSheetCategories()
{
    const struct DefaultCategory {
        const char *name;
        const char *key;
    } defaults[] = {
        { "\xe5\x96\x9c\xe6\xac\xa2", "favorite" },
        { "\xe7\xbb\x83\xe4\xb9\xa0", "practice" }
    };

    const QString now = utcNow();
    for (const DefaultCategory &category : defaults) {
        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral(
            "INSERT INTO sheet_categories (name, built_in_key, created_at, updated_at) "
            "VALUES (?, ?, ?, ?) "
            "ON CONFLICT(built_in_key) DO UPDATE SET "
            "name = excluded.name, updated_at = excluded.updated_at"));
        insert.addBindValue(QString::fromUtf8(category.name));
        insert.addBindValue(QString::fromLatin1(category.key));
        insert.addBindValue(now);
        insert.addBindValue(now);
        if (!insert.exec()) {
            setLastError(insert.lastError().text());
            return false;
        }
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

QString PracticeRecordStore::normalizedCategoryName(const QString &name) const
{
    return name.simplified().left(32);
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
