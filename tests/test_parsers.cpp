#include "core/NoteUtils.h"
#include "parser/JsonSheetParser.h"
#include "parser/MidiFileParser.h"

#include <QByteArray>
#include <QString>
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

QByteArray midiFile(const QByteArray &track, int ppq = 480)
{
    QByteArray header;
    header += be16(0);
    header += be16(1);
    header += be16(ppq);
    return chunk("MThd", header) + chunk("MTrk", track);
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
    expect(parsed.title == QStringLiteral("Json Test"), "JSON title should come from name");
    expect(parsed.bpm == 96, "JSON bpm should be parsed");
    expect(parsed.ppq == 480, "JSON parser should always use 480 PPQ");
    expect(parsed.notes.size() == 3, "JSON parser should produce three notes");
    expect(parsed.notes.at(0).startTick == 0, "first JSON note should start at tick 0");
    expect(parsed.notes.at(1).startTick == 480, "sequential JSON note should follow cursor beat");
    expect(parsed.notes.at(1).durationTick == 240, "half-beat JSON note should be 240 ticks");
    expect(parsed.notes.at(2).startTick == 1920, "explicit JSON start beat should be converted with fixed PPQ");
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
    expect(parsed.bpm == 120, "MIDI bpm should use the first tempo event");
    expect(parsed.tempos.size() == 2, "MIDI parser should retain tempo map events");
    expect(parsed.tempos.at(1).tick == 360, "second tempo event should keep its source tick");
    expect(parsed.notes.size() == 1, "sustain test should produce one note");
    expect(parsed.notes.at(0).durationTick == 480, "sustain pedal should extend note until pedal release");
}

void testMidiUnclosedNoteFallback()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 64, 100 });
    appendEnd(track, 120);

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(parsed.ok, "MIDI parser should keep files with missing note-off events usable");
    expect(parsed.notes.size() == 1, "unclosed note test should produce one fallback note");
    expect(parsed.notes.at(0).midi == 64, "fallback note should preserve pitch");
    expect(parsed.notes.at(0).durationTick == 480, "fallback note should get a musically useful duration");
}

}

int main()
{
    testNoteUtils();
    testJsonParser();
    testMidiSustainAndTempo();
    testMidiUnclosedNoteFallback();

    if (failures == 0) {
        std::cout << "All parser tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
