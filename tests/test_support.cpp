#include "test_support.h"

#include "core/NoteUtils.h"
#include "playback/PlaybackClock.h"

#include <QString>
#include <iostream>

namespace SmartPianoTest {
int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

QByteArray raw(std::initializer_list<int> values)
{
    QByteArray data;
    data.reserve(int(values.size()));
    for (int value : values) {
        data.append(char(value & 0xFF));
    }
    return data;
}

QByteArray be16(int value)
{
    return raw({ (value >> 8) & 0xFF, value & 0xFF });
}

QByteArray be32(int value)
{
    return raw({
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF
    });
}

QByteArray vlq(int value)
{
    QByteArray encoded;
    int buffer = value & 0x7F;
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= ((value & 0x7F) | 0x80);
    }

    while (true) {
        encoded.append(char(buffer & 0xFF));
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
    return encoded;
}

QByteArray chunk(const char *id, const QByteArray &payload)
{
    QByteArray data(id, 4);
    data += be32(payload.size());
    data += payload;
    return data;
}

void appendEvent(QByteArray &track, int delta, std::initializer_list<int> event)
{
    track += vlq(delta);
    track += raw(event);
}

void appendTempo(QByteArray &track, int delta, int microsPerQuarter)
{
    appendEvent(track, delta, {
        0xFF, 0x51, 0x03,
        (microsPerQuarter >> 16) & 0xFF,
        (microsPerQuarter >> 8) & 0xFF,
        microsPerQuarter & 0xFF
    });
}

void appendEnd(QByteArray &track, int delta)
{
    appendEvent(track, delta, { 0xFF, 0x2F, 0x00 });
}

QByteArray midiFileWithFormat(const QVector<QByteArray> &tracks, int format, int ppq)
{
    QByteArray header;
    header += be16(format);
    header += be16(tracks.size());
    header += be16(ppq);

    QByteArray file = chunk("MThd", header);
    for (const QByteArray &track : tracks) {
        file += chunk("MTrk", track);
    }
    return file;
}

QByteArray midiFile(const QVector<QByteArray> &tracks, int ppq)
{
    return midiFileWithFormat(tracks, tracks.size() > 1 ? 1 : 0, ppq);
}

QByteArray midiFile(const QByteArray &track, int ppq)
{
    return midiFile(QVector<QByteArray>{ track }, ppq);
}

NoteEvent makeNote(int midi, qint64 startTick)
{
    NoteEvent note;
    note.id = midi + int(startTick);
    note.midi = midi;
    note.velocity = 100;
    note.startTick = startTick;
    note.durationTick = 240;
    note.noteName = NoteUtils::midiToName(midi);
    return note;
}

Song makePlaybackSong()
{
    Song song;
    song.title = QStringLiteral("Playback Test");
    song.bpm = 120;
    song.ppq = 480;
    song.tempos = PlaybackClock::tempoMapFromBpm(song.bpm);
    song.notes = { makeNote(60, 0), makeNote(62, 480), makeNote(64, 960) };
    song.notes.last().durationTick = 480;
    return song;
}

}
