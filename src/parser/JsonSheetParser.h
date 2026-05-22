#pragma once

#include "core/Song.h"

#include <QByteArray>
#include <QString>

struct ParsedJsonSheet {
    bool ok = false;
    QString error;
    Song song;
};

class JsonSheetParser {
public:
    static ParsedJsonSheet parse(const QByteArray &bytes, const QString &fallbackTitle);
    static ParsedJsonSheet parseFile(const QString &path);
};
