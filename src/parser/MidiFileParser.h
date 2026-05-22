#pragma once

#include "core/Song.h"

#include <QByteArray>
#include <QString>

struct ParsedMidi {
    bool ok = false;
    QString error;
    Song song;
};

class MidiFileParser {
public:
    static ParsedMidi parse(const QByteArray &bytes);
};
