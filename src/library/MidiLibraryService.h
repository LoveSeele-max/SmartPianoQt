#pragma once

#include <QString>
#include <QVariantList>

class MidiLibraryService {
public:
    static QString resolveLibraryPath();
    static QVariantList scanLibrary(const QString &path);
    static void openLibrary(const QString &path);
};
