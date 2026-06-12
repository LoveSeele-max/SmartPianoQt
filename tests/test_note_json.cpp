#include "test_support.h"

#include "core/NoteUtils.h"
#include "parser/JsonSheetParser.h"
#include "parser/MidiFileParser.h"

#include <QByteArray>
#include <QString>

namespace SmartPianoTest {

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
    expect(parsed.song.ppq == JsonSheetParser::Ppq, "JSON parser should always use the fixed JSON PPQ");
    expect(parsed.song.tempos.size() == 1, "JSON parser should provide a default tempo map");
    expect(parsed.song.notes.size() == 3, "JSON parser should produce three notes");
    expect(parsed.song.notes.at(0).startTick == 0, "first JSON note should start at tick 0");
    expect(parsed.song.notes.at(1).startTick == JsonSheetParser::Ppq, "sequential JSON note should follow cursor beat");
    expect(parsed.song.notes.at(1).durationTick == JsonSheetParser::Ppq / 2, "half-beat JSON note should be half the fixed PPQ");
    expect(parsed.song.notes.at(2).startTick == JsonSheetParser::Ppq * 4, "explicit JSON start beat should be converted with fixed PPQ");
}

void testJsonParserSkipsOutOfRangeMidi()
{
    const QByteArray json = R"({
        "name": "Invalid Midi Test",
        "data": [
            { "midi": 200, "durationBeat": 1.0 },
            { "midi": 60, "durationBeat": 1.0 }
        ]
    })";

    const ParsedJsonSheet parsed = JsonSheetParser::parse(json, QStringLiteral("fallback"));
    expect(parsed.ok, "JSON parser should keep valid notes when skipping invalid MIDI values");
    expect(parsed.song.notes.size() == 1, "JSON parser should skip MIDI notes above 127");
    expect(parsed.song.notes.at(0).midi == 60, "JSON parser should preserve the valid MIDI note");
}

void testJsonParserUsesFixedPpqAfterNonDefaultMidi()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 60, 100 });
    appendEvent(track, 960, { 0x80, 60, 0 });
    appendEnd(track, 0);

    const ParsedMidi midi = MidiFileParser::parse(midiFile(track, 960));
    expect(midi.ok, "MIDI parser precondition should accept a 960 PPQ file");
    expect(midi.song.ppq == 960, "MIDI parser precondition should expose non-default PPQ");

    const QByteArray json = R"({
        "name": "Json PPQ Regression",
        "data": [
            { "midi": 60, "startTimeBeat": 1.0, "durationBeat": 0.5 }
        ]
    })";

    const ParsedJsonSheet parsed = JsonSheetParser::parse(json, QStringLiteral("fallback"));
    expect(parsed.ok, "JSON parser should accept the regression sheet");
    expect(parsed.song.ppq == JsonSheetParser::Ppq, "JSON parser should not inherit PPQ from prior MIDI loads");
    expect(parsed.song.notes.size() == 1, "JSON regression sheet should produce one note");
    expect(parsed.song.notes.at(0).startTick == JsonSheetParser::Ppq, "JSON start beat should use fixed PPQ");
    expect(parsed.song.notes.at(0).durationTick == JsonSheetParser::Ppq / 2, "JSON duration beat should use fixed PPQ");
}

}
