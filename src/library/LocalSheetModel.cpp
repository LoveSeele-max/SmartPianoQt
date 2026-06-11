#include "library/LocalSheetModel.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QVariantMap>
#include <QtMath>

#include <utility>

LocalSheetModel::LocalSheetModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LocalSheetModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant LocalSheetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const LocalSheetEntry &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return entry.name;
    case FileNameRole:
        return entry.fileName;
    case PathRole:
        return entry.path;
    case SourceFormatRole:
        return entry.sourceFormat;
    case SizeKbRole:
        return entry.sizeKb;
    case SheetIdRole:
        return entry.sheetId;
    case KnownSheetRole:
        return entry.sheetId > 0;
    case BpmRole:
        return entry.bpm;
    case NoteCountRole:
        return entry.noteCount;
    case UpdatedAtRole:
        return entry.updatedAt;
    case CategoryIdsRole: {
        QVariantList ids;
        ids.reserve(entry.categoryIds.size());
        for (qint64 id : entry.categoryIds) ids.push_back(id);
        return ids;
    }
    case CategoryNamesRole:
        return entry.categoryNames;
    default:
        return {};
    }
}

QHash<int, QByteArray> LocalSheetModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { FileNameRole, "fileName" },
        { PathRole, "path" },
        { SourceFormatRole, "sourceFormat" },
        { SizeKbRole, "sizeKb" },
        { SheetIdRole, "sheetId" },
        { KnownSheetRole, "knownSheet" },
        { BpmRole, "bpm" },
        { NoteCountRole, "noteCount" },
        { UpdatedAtRole, "updatedAt" },
        { CategoryIdsRole, "categoryIds" },
        { CategoryNamesRole, "categoryNames" }
    };
}

void LocalSheetModel::setRecordStore(PracticeRecordStore *store)
{
    m_store = store;
}

void LocalSheetModel::setLibraryPath(const QString &path)
{
    if (m_libraryPath == path) return;
    m_libraryPath = path;
}

void LocalSheetModel::refresh()
{
    QDir dir(m_libraryPath);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    const QFileInfoList files = dir.entryInfoList(
        { QStringLiteral("*.mid"), QStringLiteral("*.midi") },
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    QStringList paths;
    paths.reserve(files.size());
    for (const QFileInfo &file : files) {
        paths.push_back(file.absoluteFilePath());
    }
    const QHash<QString, StoredSheetInfo> knownSheets = m_store
        ? m_store->sheetsForPaths(paths)
        : QHash<QString, StoredSheetInfo>{};
    QVector<qint64> knownSheetIds;
    knownSheetIds.reserve(knownSheets.size());
    for (auto it = knownSheets.cbegin(); it != knownSheets.cend(); ++it) {
        if (it->id > 0) knownSheetIds.push_back(it->id);
    }

    const QHash<qint64, QVector<qint64>> categoriesBySheet = m_store
        ? m_store->categoriesForSheets(knownSheetIds)
        : QHash<qint64, QVector<qint64>>{};
    QHash<qint64, QString> categoryNamesById;
    if (m_store) {
        const QVector<SheetCategoryInfo> categories = m_store->sheetCategories();
        for (const SheetCategoryInfo &category : categories) {
            categoryNamesById.insert(category.id, category.name);
        }
    }
    const QSet<qint64> filteredSheetIds = m_categoryFilterId > 0 && m_store
        ? m_store->sheetIdsForCategory(m_categoryFilterId)
        : QSet<qint64>{};

    QVector<LocalSheetEntry> next;
    next.reserve(files.size());
    for (const QFileInfo &file : files) {
        LocalSheetEntry entry;
        entry.name = file.completeBaseName();
        entry.fileName = file.fileName();
        entry.path = file.absoluteFilePath();
        entry.sourceFormat = file.suffix().toLower();
        entry.sizeKb = int(qMax<qint64>(1, file.size() / 1024));

        const auto it = knownSheets.constFind(entry.path);
        if (it != knownSheets.constEnd()) {
            entry.sheetId = it->id;
            entry.name = it->title.isEmpty() ? entry.name : it->title;
            entry.sourceFormat = it->sourceFormat.isEmpty() ? entry.sourceFormat : it->sourceFormat;
            entry.bpm = it->bpm;
            entry.noteCount = it->noteCount;
            entry.updatedAt = it->updatedAt;
            entry.categoryIds = categoriesBySheet.value(entry.sheetId);
            for (qint64 categoryId : entry.categoryIds) {
                const QString categoryName = categoryNamesById.value(categoryId);
                if (!categoryName.isEmpty()) {
                    entry.categoryNames.push_back(categoryName);
                }
            }
        }
        if (m_categoryFilterId > 0 &&
            (entry.sheetId <= 0 || !filteredSheetIds.contains(entry.sheetId))) {
            continue;
        }
        next.push_back(entry);
    }

    beginResetModel();
    m_entries = std::move(next);
    endResetModel();
}

QString LocalSheetModel::filePathAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) return {};
    return m_entries.at(row).path;
}

qint64 LocalSheetModel::sheetIdAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) return -1;
    return m_entries.at(row).sheetId;
}

QVector<qint64> LocalSheetModel::categoryIdsAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) return {};
    return m_entries.at(row).categoryIds;
}

bool LocalSheetModel::setCategoryFilterId(qint64 categoryId)
{
    const qint64 normalized = qMax<qint64>(0, categoryId);
    if (m_categoryFilterId == normalized) return false;
    m_categoryFilterId = normalized;
    refresh();
    return true;
}

QVariantList LocalSheetModel::toVariantList() const
{
    QVariantList items;
    items.reserve(m_entries.size());
    for (const LocalSheetEntry &entry : m_entries) {
        items.push_back(entryToMap(entry));
    }
    return items;
}

QVariantMap LocalSheetModel::entryToMap(const LocalSheetEntry &entry) const
{
    QVariantMap item;
    item.insert(QStringLiteral("name"), entry.name);
    item.insert(QStringLiteral("fileName"), entry.fileName);
    item.insert(QStringLiteral("path"), entry.path);
    item.insert(QStringLiteral("sourceFormat"), entry.sourceFormat);
    item.insert(QStringLiteral("sizeKb"), entry.sizeKb);
    item.insert(QStringLiteral("sheetId"), entry.sheetId);
    item.insert(QStringLiteral("knownSheet"), entry.sheetId > 0);
    item.insert(QStringLiteral("bpm"), entry.bpm);
    item.insert(QStringLiteral("noteCount"), entry.noteCount);
    item.insert(QStringLiteral("updatedAt"), entry.updatedAt);
    QVariantList categoryIds;
    categoryIds.reserve(entry.categoryIds.size());
    for (qint64 categoryId : entry.categoryIds) {
        categoryIds.push_back(categoryId);
    }
    item.insert(QStringLiteral("categoryIds"), categoryIds);
    item.insert(QStringLiteral("categoryNames"), entry.categoryNames);
    return item;
}
