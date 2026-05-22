#include "library/MidiLibraryService.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

namespace {

QString findLibraryFrom(QDir dir)
{
    for (int i = 0; i < 6; ++i) {
        const QString candidate = dir.absoluteFilePath(QStringLiteral("midi_library"));
        if (QDir(candidate).exists()) return QDir(candidate).absolutePath();
        if (!dir.cdUp()) break;
    }
    return {};
}

void ensureDirectory(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

}

QString MidiLibraryService::resolveLibraryPath()
{
    QString path = findLibraryFrom(QDir::current());
    if (!path.isEmpty()) return path;

    path = findLibraryFrom(QDir(QCoreApplication::applicationDirPath()));
    if (!path.isEmpty()) return path;

    QDir dir(QDir::current());
    const QString created = dir.absoluteFilePath(QStringLiteral("midi_library"));
    dir.mkpath(QStringLiteral("midi_library"));
    return QDir(created).absolutePath();
}

QVariantList MidiLibraryService::scanLibrary(const QString &path)
{
    ensureDirectory(path);

    QDir dir(path);
    const QFileInfoList files = dir.entryInfoList(
        { QStringLiteral("*.mid"), QStringLiteral("*.midi") },
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    QVariantList entries;
    entries.reserve(files.size());
    for (const QFileInfo &file : files) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), file.completeBaseName());
        item.insert(QStringLiteral("fileName"), file.fileName());
        item.insert(QStringLiteral("path"), file.absoluteFilePath());
        item.insert(QStringLiteral("sizeKb"), qMax<qint64>(1, file.size() / 1024));
        entries.push_back(item);
    }

    return entries;
}

void MidiLibraryService::openLibrary(const QString &path)
{
    ensureDirectory(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(path).absolutePath()));
}
