#include "core/NoteUtils.h"
#include "parser/JsonSheetParser.h"
#include "parser/MidiFileParser.h"

#include <QByteArray>
#include <QString>
#include <QVector>
#include <iostream>

namespace {

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

QByteArray midiFile(const QVector<QByteArray> &tracks, int ppq = 480)
{
    QByteArray header;
    header += be16(tracks.size() > 1 ? 1 : 0);
    header += be16(tracks.size());
    header += be16(ppq);

    QByteArray file = chunk("MThd", header);
    for (const QByteArray &track : tracks) {
        file += chunk("MTrk", track);
    }
    return file;
}

QByteArray midiFile(const QByteArray &track, int ppq = 480)
{
    return midiFile(QVector<QByteArray>{ track }, ppq);
}

void testNoteUtils()
{
    expect(NoteUtils::midiToName(60) == QStringLiteral("C4"), "MIDI 60 should be C4");
    expect(NoteUtils::noteNameToMidi(QStringLiteral("Bb3")) == 58, "Bb3 should parse to MIDI 58");
    expect(NoteUtils::isBlackKey(61), "C#4 should be a black key");
    expect(!NoteUtils::isBlackKey(64), "E4 should be a white key");
}

void testJsonParser()
{
    const QByteArray json = R"({
        "name": "Json Test",
        "bpm": 96,
        "data": [
            { "note": "C4", "durationBeat": 1.0 },
            { "midi": 62, "durationBeat": 0.5 },
            { "note": "E4", "startTimeBeat": 4.0, "durationBeat": 2.0, "velocity": 100 }
        ]
    })";

    const ParsedJsonSheet parsed = JsonSheetParser::parse(json, QStringLiteral("fallback"));
    expect(parsed.ok, "JSON parser should accept a valid sheet");
    expect(parsed.song.title == QStringLiteral("Json Test"), "JSON title should come from name");
    expect(parsed.song.bpm == 96, "JSON bpm should be parsed");
    expect(parsed.song.ppq == 480, "JSON parser should always use 480 PPQ");
    expect(parsed.song.tempos.size() == 1, "JSON parser should provide a default tempo map");
    expect(parsed.song.notes.size() == 3, "JSON parser should produce three notes");
    expect(parsed.song.notes.at(0).startTick == 0, "first JSON note should start at tick 0");
    expect(parsed.song.notes.at(1).startTick == 480, "sequential JSON note should follow cursor beat");
    expect(parsed.song.notes.at(1).durationTick == 240, "half-beat JSON note should be 240 ticks");
    expect(parsed.song.notes.at(2).startTick == 1920, "explicit JSON start beat should be converted with fixed PPQ");
}

void testMidiSustainAndTempo()
{
    QByteArray track;
    appendTempo(track, 0, 500000);
    appendEvent(track, 0, { 0x90, 60, 100 });
    appendEvent(track, 120, { 0xB0, 64, 127 });
    appendEvent(track, 120, { 0x80, 60, 0 });
    appendTempo(track, 120, 600000);
    appendEvent(track, 120, { 0xB0, 64, 0 });
    appendEnd(track, 0);

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(parsed.ok, "MIDI parser should accept sustain test file");
    expect(parsed.song.bpm == 120, "MIDI bpm should use the first tempo event");
    expect(parsed.song.tempos.size() == 2, "MIDI parser should retain tempo map events");
    expect(parsed.song.tempos.at(1).tick == 360, "second tempo event should keep its source tick");
    expect(parsed.song.notes.size() == 1, "sustain test should produce one note");
    expect(parsed.song.notes.at(0).durationTick == 480, "sustain pedal should extend note until pedal release");
}

void testMidiUnclosedNoteFallback()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 64, 100 });
    appendEnd(track, 120);

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(parsed.ok, "MIDI parser should keep files with missing note-off events usable");
    expect(parsed.song.notes.size() == 1, "unclosed note test should produce one fallback note");
    expect(parsed.song.notes.at(0).midi == 64, "fallback note should preserve pitch");
    expect(parsed.song.notes.at(0).durationTick == 480, "fallback note should get a musically useful duration");
}

void testMidiDefaultTempo()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 67, 100 });
    appendEvent(track, 240, { 0x80, 67, 0 });
    appendEnd(track, 0);

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(parsed.ok, "MIDI parser should accept files without explicit tempo");
    expect(parsed.song.tempos.size() == 1, "MIDI parser should add a default tempo");
    expect(parsed.song.tempos.at(0).tick == 0, "default tempo should start at tick 0");
    expect(parsed.song.tempos.at(0).microsecondsPerQuarter == 500000, "default tempo should be 120 BPM");
    expect(parsed.song.bpm == 120, "default MIDI bpm should be 120");
}

void testMidiTempoSortingAcrossTracks()
{
    QByteArray lateTempoTrack;
    appendTempo(lateTempoTrack, 480, 400000);
    appendEnd(lateTempoTrack, 0);

    QByteArray earlyTempoAndNotesTrack;
    appendTempo(earlyTempoAndNotesTrack, 0, 600000);
    appendEvent(earlyTempoAndNotesTrack, 0, { 0x90, 60, 100 });
    appendEvent(earlyTempoAndNotesTrack, 120, { 0x80, 60, 0 });
    appendEnd(earlyTempoAndNotesTrack, 0);

    const ParsedMidi parsed = MidiFileParser::parse(
        midiFile(QVector<QByteArray>{ lateTempoTrack, earlyTempoAndNotesTrack }));
    expect(parsed.ok, "MIDI parser should accept multi-track tempo files");
    expect(parsed.song.tempos.size() == 2, "MIDI parser should keep tempos from multiple tracks");
    expect(parsed.song.tempos.at(0).tick == 0, "tempo map should be sorted by tick");
    expect(parsed.song.tempos.at(1).tick == 480, "later tempo should remain after earlier tempo");
    expect(parsed.song.bpm == 100, "song bpm should be derived from earliest sorted tempo");
}

void testMidiLateTempoGetsDefaultAtZero()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 72, 100 });
    appendEvent(track, 240, { 0x80, 72, 0 });
    appendTempo(track, 240, 400000);
    appendEnd(track, 0);

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(parsed.ok, "MIDI parser should accept tempo changes after the first tick");
    expect(parsed.song.tempos.size() == 2, "late tempo should be preceded by a default tempo");
    expect(parsed.song.tempos.at(0).tick == 0, "default tempo should define playback from tick 0");
    expect(parsed.song.tempos.at(0).microsecondsPerQuarter == 500000, "default leading tempo should be 120 BPM");
    expect(parsed.song.tempos.at(1).tick == 480, "late tempo should keep its original tick");
}

void testMidiRejectsMalformedVlq()
{
    const QByteArray malformedTrack = raw({ 0x81, 0x81, 0x81, 0x81, 0x90, 60, 100 });
    const ParsedMidi parsed = MidiFileParser::parse(midiFile(malformedTrack));
    expect(!parsed.ok, "MIDI parser should reject malformed four-byte VLQ values");
    expect(parsed.error.contains(QStringLiteral("长度编码")), "malformed VLQ should report a length encoding error");
}

}

int main()
{
    testNoteUtils();
    testJsonParser();
    testMidiSustainAndTempo();
    testMidiUnclosedNoteFallback();
    testMidiDefaultTempo();
    testMidiTempoSortingAcrossTracks();
    testMidiLateTempoGetsDefaultAtZero();
    testMidiRejectsMalformedVlq();

    if (failures == 0) {
        std::cout << "All parser tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
