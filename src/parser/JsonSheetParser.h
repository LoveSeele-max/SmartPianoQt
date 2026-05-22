#pragma once

#include "parser/MidiFileParser.h"

#include <QByteArray>
#include <QString>
#include <QVector>

struct ParsedJsonSheet {
    bool ok = false;
    QString title;
    QString error;
    int bpm = 100;
    int ppq = 480;
    QVector<NoteEvent> notes;
};

class JsonSheetParser {
public:
    static ParsedJsonSheet parse(const QByteArray &bytes, const QString &fallbackTitle);
    static ParsedJsonSheet parseFile(const QString &path);
};
