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
    static constexpr int Ppq = 480;

    static ParsedJsonSheet parse(const QByteArray &bytes, const QString &fallbackTitle);
    static ParsedJsonSheet parseFile(const QString &path);
};
