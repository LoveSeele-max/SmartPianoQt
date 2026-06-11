#pragma once

#include "storage/PracticeRecordStore.h"

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct LocalSheetEntry {
    QString name;
    QString fileName;
    QString path;
    QString sourceFormat;
    int sizeKb = 0;
    qint64 sheetId = -1;
    int bpm = 0;
    int noteCount = 0;
    QString updatedAt;
    QVector<qint64> categoryIds;
    QStringList categoryNames;
};

class LocalSheetModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        FileNameRole,
        PathRole,
        SourceFormatRole,
        SizeKbRole,
        SheetIdRole,
        KnownSheetRole,
        BpmRole,
        NoteCountRole,
        UpdatedAtRole,
        CategoryIdsRole,
        CategoryNamesRole
    };

    explicit LocalSheetModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRecordStore(PracticeRecordStore *store);
    void setLibraryPath(const QString &path);
    QString libraryPath() const { return m_libraryPath; }
    void refresh();
    QString filePathAt(int row) const;
    qint64 sheetIdAt(int row) const;
    QVector<qint64> categoryIdsAt(int row) const;
    bool setCategoryFilterId(qint64 categoryId);
    qint64 categoryFilterId() const { return m_categoryFilterId; }
    QVariantList toVariantList() const;

private:
    QVariantMap entryToMap(const LocalSheetEntry &entry) const;

    PracticeRecordStore *m_store = nullptr;
    QString m_libraryPath;
    qint64 m_categoryFilterId = 0;
    QVector<LocalSheetEntry> m_entries;
};
