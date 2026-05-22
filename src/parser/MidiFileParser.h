#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct NoteEvent {
    int id = 0;
    int midi = 60;
    int velocity = 80;
    qint64 startTick = 0;
    qint64 durationTick = 0;
    int track = 0;
    int channel = 0;
    int fingering = 0;
    QString noteName;
    bool played = false;
};

struct TempoEvent {
    qint64 tick = 0;
    int microsecondsPerQuarter = 500000;
};

struct ParsedMidi {
    bool ok = false;
    QString title;
    QString error;
    int bpm = 120;
    int ppq = 480;
    QVector<TempoEvent> tempos;
    QVector<NoteEvent> notes;
};

class MidiFileParser {
public:
    static ParsedMidi parse(const QByteArray &bytes);
};
