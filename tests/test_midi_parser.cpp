#include "test_support.h"

#include "parser/MidiFileParser.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace SmartPianoTest {

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

void testMidiNoteOnVelocityZeroActsAsNoteOff()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 60, 100 });
    appendEvent(track, 240, { 0x90, 60, 0 });
    appendEnd(track, 0);

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(parsed.ok, "MIDI parser should accept note-on velocity zero files");
    expect(parsed.song.notes.size() == 1, "velocity-zero note-on should close the active note");
    expect(parsed.song.notes.at(0).midi == 60, "velocity-zero note-off should preserve pitch");
    expect(parsed.song.notes.at(0).durationTick == 240, "velocity-zero note-off should preserve duration");
}

void testMidiRunningStatus()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 60, 100 });
    track += vlq(120);
    track += raw({ 64, 100 });
    track += vlq(120);
    track += raw({ 60, 0 });
    track += vlq(120);
    track += raw({ 64, 0 });
    appendEnd(track, 0);

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(parsed.ok, "MIDI parser should accept running-status note events");
    expect(parsed.song.notes.size() == 2, "running-status test should produce two notes");
    expect(parsed.song.notes.at(0).midi == 60, "running-status first note should preserve pitch");
    expect(parsed.song.notes.at(0).durationTick == 240, "running-status first note should close from velocity zero");
    expect(parsed.song.notes.at(1).midi == 64, "running-status second note should preserve pitch");
    expect(parsed.song.notes.at(1).startTick == 120, "running-status second note should preserve start tick");
    expect(parsed.song.notes.at(1).durationTick == 240, "running-status second note should close from velocity zero");
}

void testMidiMultiTrackNotesAndEmptyTracks()
{
    QByteArray emptyTrack;
    appendEnd(emptyTrack, 0);

    QByteArray noteTrack;
    appendEvent(noteTrack, 0, { 0x90, 67, 100 });
    appendEvent(noteTrack, 240, { 0x80, 67, 0 });
    appendEnd(noteTrack, 0);

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(QVector<QByteArray>{ emptyTrack, noteTrack }));
    expect(parsed.ok, "MIDI parser should accept empty tracks beside note tracks");
    expect(parsed.song.notes.size() == 1, "multi-track file should keep notes from non-empty tracks");
    expect(parsed.song.notes.at(0).midi == 67, "multi-track note should preserve pitch");
    expect(parsed.song.notes.at(0).track == 1, "multi-track note should preserve source track index");
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

void testMidiRejectsMissingRunningStatus()
{
    QByteArray track;
    track += vlq(0);
    track += raw({ 60, 100 });

    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(!parsed.ok, "MIDI parser should reject data bytes without running status");
    expect(!parsed.error.isEmpty(), "missing running status should report an error");
}

void testMidiRejectsTruncatedTrackChunk()
{
    QByteArray header;
    header += be16(0);
    header += be16(1);
    header += be16(480);

    QByteArray file = chunk("MThd", header);
    file += QByteArray("MTr", 3);

    const ParsedMidi parsed = MidiFileParser::parse(file);
    expect(!parsed.ok, "MIDI parser should reject truncated track chunks");
    expect(!parsed.error.isEmpty(), "truncated track chunks should report an error");
}

void testMidiRejectsFormat2()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 60, 100 });
    appendEvent(track, 120, { 0x80, 60, 0 });
    appendEnd(track, 0);

    const ParsedMidi parsed = MidiFileParser::parse(
        midiFileWithFormat(QVector<QByteArray>{ track }, 2));
    expect(!parsed.ok, "MIDI parser should reject format 2 files");
    expect(parsed.error.contains(QStringLiteral("Format 2")), "format 2 rejection should explain the unsupported format");
}

void testMidiRejectsSmpteDivision()
{
    QByteArray track;
    appendEvent(track, 0, { 0x90, 60, 100 });
    appendEvent(track, 120, { 0x80, 60, 0 });
    appendEnd(track, 0);

    const ParsedMidi parsed = MidiFileParser::parse(midiFileWithFormat(
        QVector<QByteArray>{ track }, 0, 0xE728));
    expect(!parsed.ok, "MIDI parser should reject SMPTE division files");
    expect(parsed.error.contains(QStringLiteral("SMPTE")), "SMPTE rejection should explain the unsupported division");
}

void testMidiRejectsInvalidSysexLength()
{
    const QByteArray track = raw({ 0x00, 0xF0, 0x05, 0x01 });
    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(!parsed.ok, "MIDI parser should reject SysEx events that overrun the track");
    expect(parsed.error.contains(QStringLiteral("SysEx")), "invalid SysEx length should report a SysEx error");
}

void testMidiRejectsTruncatedChannelEvent()
{
    const QByteArray track = raw({ 0x00, 0x90, 60 });
    const ParsedMidi parsed = MidiFileParser::parse(midiFile(track));
    expect(!parsed.ok, "MIDI parser should reject truncated channel events");
    expect(parsed.error.contains(QStringLiteral("通道事件")), "truncated channel event should report a channel event error");
}

}
