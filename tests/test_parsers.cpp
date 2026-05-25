#include "core/NoteUtils.h"
#include "parser/JsonSheetParser.h"
#include "parser/MidiFileParser.h"
#include "playback/PlaybackClock.h"
#include "playback/PlaybackEngine.h"
#include "practice/PracticeEngine.h"

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtMath>
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

QByteArray midiFileWithFormat(const QVector<QByteArray> &tracks, int format, int ppq = 480)
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

QByteArray midiFile(const QVector<QByteArray> &tracks, int ppq = 480)
{
    return midiFileWithFormat(tracks, tracks.size() > 1 ? 1 : 0, ppq);
}

QByteArray midiFile(const QByteArray &track, int ppq = 480)
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

void testPlaybackClockSingleTempo()
{
    const QVector<TempoEvent> tempos = PlaybackClock::tempoMapFromBpm(120);
    const double advanced = PlaybackClock::advance(0.0, 500, tempos, 480);
    expect(qRound64(advanced) == 480, "PlaybackClock should advance one beat in 500 ms at 120 BPM");
}

void testPlaybackClockCrossesTempoChange()
{
    const QVector<TempoEvent> tempos = PlaybackClock::normalizedTempoMap(
        { { 0, 500000 }, { 480, 1000000 } }, 120);
    const double advanced = PlaybackClock::advance(0.0, 1000, tempos, 480);
    expect(qRound64(advanced) == 720, "PlaybackClock should consume remaining time at the slower tempo");
}

void testPlaybackClockNormalizesLateTempo()
{
    const QVector<TempoEvent> tempos = PlaybackClock::normalizedTempoMap({ { 480, 400000 } }, 120);
    expect(tempos.size() == 2, "PlaybackClock should prepend a fallback tempo before a late first tempo");
    expect(tempos.at(0).tick == 0, "PlaybackClock fallback tempo should start at tick 0");
    expect(tempos.at(0).microsecondsPerQuarter == 500000, "PlaybackClock fallback tempo should use fallback BPM");
}

void testPlaybackClockCoalescesSameTickTempos()
{
    const QVector<TempoEvent> tempos = PlaybackClock::normalizedTempoMap(
        { { 0, 500000 }, { 0, 1000000 }, { 480, 400000 }, { 480, 750000 } }, 120);
    expect(tempos.size() == 2, "PlaybackClock should coalesce duplicate tempo ticks");
    expect(tempos.at(0).microsecondsPerQuarter == 1000000, "last tempo at tick zero should win");
    expect(tempos.at(1).microsecondsPerQuarter == 750000, "last tempo at a duplicate later tick should win");

    const double advanced = PlaybackClock::advance(0.0, 1000, tempos, 480);
    expect(qRound64(advanced) == 480, "PlaybackClock should use the coalesced starting tempo");
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

void testPlaybackEngineAdvanceAndSpeed()
{
    PlaybackEngine playback;
    playback.setSong(makePlaybackSong());

    expect(playback.bpm() == 120, "PlaybackEngine should expose the source BPM");
    expect(playback.ppq() == 480, "PlaybackEngine should expose the source PPQ");
    expect(playback.currentTick() == 0, "PlaybackEngine should start at tick zero");

    PlaybackAdvanceResult normal = playback.advance(500);
    expect(normal.previousTick == 0, "PlaybackEngine should report the previous tick");
    expect(normal.currentTick == 480, "PlaybackEngine should advance at source tempo by default");
    expect(playback.currentTick() == 480, "PlaybackEngine current tick should follow advance results");

    expect(playback.setPlaybackSpeed(50), "PlaybackEngine should accept a slower playback speed");
    PlaybackAdvanceResult slow = playback.advance(500);
    expect(slow.currentTick == 720, "PlaybackEngine should scale elapsed time by playback speed");
}

void testPlaybackEngineSeekStopAndClamp()
{
    PlaybackEngine playback;
    playback.setSong(makePlaybackSong());

    playback.seekTick(999999);
    expect(playback.currentTick() == playback.totalTicks(), "PlaybackEngine seek should clamp to song end");
    playback.stop();
    expect(playback.currentTick() == 0, "PlaybackEngine stop should return to the beginning");

    playback.seekTick(-50);
    expect(playback.currentTick() == 0, "PlaybackEngine seek should clamp negative positions");
}

void testPlaybackEngineEndReached()
{
    PlaybackEngine playback;
    playback.setSong(makePlaybackSong());
    playback.seekTick(playback.totalTicks() - 120);

    const PlaybackAdvanceResult result = playback.advance(1000);
    expect(result.reachedEnd, "PlaybackEngine should report reaching the end");
    expect(result.currentTick == playback.totalTicks(), "PlaybackEngine should clamp current tick at the end");
}

void testPracticeEngineSingleNoteAndWrongNote()
{
    PracticeEngine practice;
    practice.setSong({ makeNote(60, 0), makeNote(62, 480) });

    expect(practice.expectedTick() == 0, "PracticeEngine should start at the first note tick");

    const PracticeNoteResult wrong = practice.noteOn(61);
    expect(wrong.type == PracticeJudgeType::WrongNote, "PracticeEngine should reject wrong notes");
    expect(practice.wrongCount() == 1, "PracticeEngine should count wrong notes");
    expect(practice.expectedTick() == 0, "wrong notes should not advance practice");

    const PracticeNoteResult correct = practice.noteOn(60);
    expect(correct.type == PracticeJudgeType::Correct, "PracticeEngine should accept expected notes");
    expect(correct.countedCorrect, "first correct note should increment correct count");
    expect(correct.stepComplete, "single-note step should complete immediately");
    expect(correct.nextTick == 480, "single-note completion should advance to the next step");
    expect(practice.correctCount() == 1, "PracticeEngine should count correct notes");
}

void testPracticeEngineChordRequiresAllNotes()
{
    PracticeEngine practice;
    practice.setSong({ makeNote(64, 0), makeNote(67, 0), makeNote(69, 480) });

    const PracticeNoteResult first = practice.noteOn(64);
    expect(first.type == PracticeJudgeType::Correct, "PracticeEngine should accept a chord member");
    expect(first.countedCorrect, "first chord member should count once");
    expect(!first.stepComplete, "partial chord should not complete the step");
    expect(practice.expectedTick() == 0, "partial chord should keep the same expected tick");

    const PracticeNoteResult repeat = practice.noteOn(64);
    expect(!repeat.countedCorrect, "repeating an already matched chord note should not count again");
    expect(practice.correctCount() == 1, "repeated chord notes should not inflate correct count");

    const PracticeNoteResult second = practice.noteOn(67);
    expect(second.stepComplete, "all chord notes should complete the step");
    expect(second.nextTick == 480, "completed chord should advance to the next note tick");
    expect(practice.correctCount() == 2, "each unique expected chord note should count once");
}

void testPracticeEngineSeekAndReset()
{
    PracticeEngine practice;
    practice.setSong({ makeNote(60, 0), makeNote(62, 480), makeNote(64, 960) });
    practice.noteOn(60);
    practice.noteOn(61);

    expect(practice.correctCount() == 1, "precondition: practice should have one correct note");
    expect(practice.wrongCount() == 1, "precondition: practice should have one wrong note");

    expect(practice.seek(500), "PracticeEngine should seek inside a non-empty song");
    expect(practice.expectedTick() == 960, "seek should pick the first practice tick at or after the target");

    expect(practice.seek(99999), "PracticeEngine should accept seeking past the last note");
    expect(!practice.hasExpectedNotes(), "seek past the last note should leave no expected notes");
    expect(practice.expectedTick() == -1, "seek past the last note should report no expected tick");

    practice.reset(false);
    expect(practice.expectedTick() == 0, "reset without stats should return to the first step");
    expect(practice.correctCount() == 1, "reset without stats should preserve correct count");
    expect(practice.wrongCount() == 1, "reset without stats should preserve wrong count");

    practice.reset(true);
    expect(practice.correctCount() == 0, "reset with stats should clear correct count");
    expect(practice.wrongCount() == 0, "reset with stats should clear wrong count");
}

}

int main()
{
    testNoteUtils();
    testJsonParser();
    testJsonParserSkipsOutOfRangeMidi();
    testMidiSustainAndTempo();
    testMidiUnclosedNoteFallback();
    testMidiDefaultTempo();
    testMidiTempoSortingAcrossTracks();
    testMidiLateTempoGetsDefaultAtZero();
    testMidiRejectsMalformedVlq();
    testMidiRejectsFormat2();
    testMidiRejectsInvalidSysexLength();
    testMidiRejectsTruncatedChannelEvent();
    testPlaybackClockSingleTempo();
    testPlaybackClockCrossesTempoChange();
    testPlaybackClockNormalizesLateTempo();
    testPlaybackClockCoalescesSameTickTempos();
    testPlaybackEngineAdvanceAndSpeed();
    testPlaybackEngineSeekStopAndClamp();
    testPlaybackEngineEndReached();
    testPracticeEngineSingleNoteAndWrongNote();
    testPracticeEngineChordRequiresAllNotes();
    testPracticeEngineSeekAndReset();

    if (failures == 0) {
        std::cout << "All core tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
