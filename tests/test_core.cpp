#include "core/NoteUtils.h"
#include "midi/MidiInputService.h"
#include "parser/JsonSheetParser.h"
#include "parser/MidiFileParser.h"
#include "playback/PlaybackClock.h"
#include "playback/PlaybackEngine.h"
#include "practice/PracticeEngine.h"
#include "storage/PracticeRecordStore.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
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

void testPlaybackClockDurationMsBetweenTicks()
{
    const QVector<TempoEvent> tempos = PlaybackClock::normalizedTempoMap(
        { { 0, 500000 }, { 480, 1000000 } }, 120);

    const double forwardMs = PlaybackClock::durationMsBetweenTicks(0, 960, tempos, 480);
    expect(qRound64(forwardMs) == 1500, "PlaybackClock should convert tick ranges through tempo changes");

    const double backwardMs = PlaybackClock::durationMsBetweenTicks(960, 480, tempos, 480);
    expect(qRound64(backwardMs) == -1000, "PlaybackClock should preserve sign for early timing offsets");
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
    expect(practice.expectedNotes().size() == 1, "PracticeEngine should expose notes for the current step");
    expect(practice.expectedNotes().at(0).midi == 60, "PracticeEngine expected notes should preserve pitch");

    const PracticeNoteResult wrong = practice.noteOn(61, 96);
    expect(wrong.type == PracticeJudgeType::WrongNote, "PracticeEngine should reject wrong notes");
    expect(wrong.actualVelocity == 96, "PracticeEngine should preserve input velocity in judge results");
    expect(practice.wrongCount() == 1, "PracticeEngine should count wrong notes");
    expect(practice.expectedTick() == 0, "wrong notes should not advance practice");

    const PracticeNoteResult correct = practice.noteOn(60, 96);
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

    const PracticeNoteResult first = practice.noteOn(64, 96);
    expect(first.type == PracticeJudgeType::Correct, "PracticeEngine should accept a chord member");
    expect(first.countedCorrect, "first chord member should count once");
    expect(!first.stepComplete, "partial chord should not complete the step");
    expect(practice.expectedTick() == 0, "partial chord should keep the same expected tick");

    const PracticeNoteResult repeat = practice.noteOn(64, 96);
    expect(repeat.type == PracticeJudgeType::RepeatedNote, "repeated chord notes should be reported explicitly");
    expect(!repeat.countedCorrect, "repeating an already matched chord note should not count again");
    expect(practice.correctCount() == 1, "repeated chord notes should not inflate correct count");

    const PracticeNoteResult second = practice.noteOn(67, 96);
    expect(second.stepComplete, "all chord notes should complete the step");
    expect(second.nextTick == 480, "completed chord should advance to the next note tick");
    expect(practice.correctCount() == 2, "each unique expected chord note should count once");
}

void testPracticeEngineSeekAndReset()
{
    PracticeEngine practice;
    practice.setSong({ makeNote(60, 0), makeNote(62, 480), makeNote(64, 960) });
    practice.noteOn(60, 96);
    practice.noteOn(61, 96);

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

void testPracticeEngineRhythmJudging()
{
    PracticeEngine practice;
    practice.setSong({ makeNote(60, 480), makeNote(62, 960) });

    const PracticeNoteResult early = practice.noteOnRhythm(60, 90, 360, 60);
    expect(early.type == PracticeJudgeType::Early, "rhythm judging should detect early input");
    expect(early.timingOffsetTick == -120, "early result should preserve timing offset");
    expect(practice.wrongCount() == 1, "early input should count as a wrong attempt");

    const PracticeNoteResult correct = practice.noteOnRhythm(60, 90, 500, 60);
    expect(correct.type == PracticeJudgeType::Correct, "rhythm judging should accept notes inside tolerance");
    expect(correct.stepComplete, "single rhythm note should complete its step");
    expect(correct.nextTick == 960, "rhythm completion should advance to the next step");

    const QVector<PracticeNoteResult> missed = practice.markMissedUntil(1100, 60);
    expect(missed.size() == 1, "rhythm mode should produce missed events for overdue notes");
    expect(missed.at(0).type == PracticeJudgeType::Missed, "overdue notes should be marked missed");
    expect(missed.at(0).expectedMidi == 62, "missed event should preserve expected pitch");
    expect(practice.missedCount() == 1, "missed rhythm notes should update missed count");

    PracticeEngine chordPractice;
    chordPractice.setSong({ makeNote(64, 480), makeNote(67, 480), makeNote(69, 960) });
    const QVector<PracticeNoteResult> lateChord = chordPractice.noteOnRhythmDetailed(64, 90, 620, 60);
    expect(lateChord.size() == 2, "late rhythm chord input should produce per-note results");
    expect(lateChord.at(0).type == PracticeJudgeType::Late, "late chord hit should be reported as late");
    expect(lateChord.at(0).expectedMidi == 64, "late chord hit should preserve the played expected pitch");
    expect(lateChord.at(1).type == PracticeJudgeType::Missed, "unplayed chord members should be recorded as missed");
    expect(lateChord.at(1).expectedMidi == 67, "missed chord member should preserve its expected pitch");
    expect(lateChord.last().stepComplete, "late chord detail should complete the overdue step");
    expect(chordPractice.missedCount() == 2, "late chord detail should count late and missed notes individually");

    PracticeEngine legacyLatePractice;
    legacyLatePractice.setSong({ makeNote(64, 480), makeNote(67, 480), makeNote(69, 960) });
    const PracticeNoteResult legacyLate = legacyLatePractice.noteOnRhythm(64, 90, 620, 60);
    expect(legacyLate.type == PracticeJudgeType::Late, "legacy rhythm API should keep late as the primary result");
    expect(legacyLate.stepComplete, "legacy rhythm API should preserve step completion for late chords");
    expect(legacyLate.nextTick == 960, "legacy rhythm API should preserve the next tick for late chords");
}

void testMidiInputMessageFiltering()
{
    const MidiInputMessage activeSensing = MidiInputService::decodeShortMessage(0xFE, 0, 0);
    expect(activeSensing.type == MidiInputMessageType::Ignored, "MIDI input should ignore active sensing messages");

    const MidiInputMessage timingClock = MidiInputService::decodeShortMessage(0xF8, 0, 0);
    expect(timingClock.type == MidiInputMessageType::Ignored, "MIDI input should ignore timing clock messages");

    const MidiInputMessage noteOn = MidiInputService::decodeShortMessage(0x90, 64, 77);
    expect(noteOn.type == MidiInputMessageType::NoteOn, "MIDI input should decode note-on messages");
    expect(noteOn.midi == 64, "MIDI input should preserve note-on pitch");
    expect(noteOn.velocity == 77, "MIDI input should preserve note-on velocity");

    const MidiInputMessage zeroVelocity = MidiInputService::decodeShortMessage(0x90, 64, 0);
    expect(zeroVelocity.type == MidiInputMessageType::NoteOff, "MIDI input should treat note-on velocity zero as note-off");
}

void testPracticeRecordStore()
{
    QTemporaryDir dir;
    expect(dir.isValid(), "temporary directory should be available for practice record store test");
    const QString dbPath = dir.filePath(QStringLiteral("practice.sqlite"));

    PracticeRecordStore store;
    expect(store.open(dbPath), "PracticeRecordStore should open a SQLite database");

    Song song = makePlaybackSong();
    const qint64 sheetId = store.upsertSheet(song, QStringLiteral("test.mid"), QStringLiteral("midi"));
    expect(sheetId > 0, "PracticeRecordStore should upsert sheet metadata");
    const qint64 sameSheetId = store.upsertSheet(song, QStringLiteral("test.mid"), QStringLiteral("midi"));
    expect(sameSheetId == sheetId, "PracticeRecordStore should reuse sheet ids for unchanged content");

    PracticeSessionStart start;
    start.mode = QStringLiteral("rhythm");
    start.playbackSpeed = 95;
    start.startTick = 480;
    const qint64 sessionId = store.beginSession(sheetId, start);
    expect(sessionId > 0, "PracticeRecordStore should begin a practice session");

    PracticeEventRecord event;
    event.result = PracticeJudgeType::Correct;
    event.expectedMidi = 60;
    event.actualMidi = 60;
    event.velocity = 96;
    event.expectedTick = 480;
    event.actualTick = 500;
    event.offsetMs = 21;
    expect(store.appendEvent(sessionId, event), "PracticeRecordStore should append judge events");

    PracticeEventRecord wrongEvent;
    wrongEvent.result = PracticeJudgeType::WrongNote;
    wrongEvent.expectedMidi = 62;
    wrongEvent.actualMidi = 61;
    wrongEvent.velocity = 80;
    wrongEvent.expectedTick = 960;
    wrongEvent.actualTick = 940;
    wrongEvent.offsetMs = -20;
    expect(store.appendEvent(sessionId, wrongEvent), "PracticeRecordStore should append mistake events");

    PracticeSessionSummary summary;
    summary.completed = true;
    summary.endTick = 1440;
    summary.activeDurationSeconds = 7;
    summary.correctCount = 1;
    summary.wrongCount = 1;
    expect(store.finishSession(sessionId, summary), "PracticeRecordStore should finish a practice session");

    PracticeSessionStart secondStart;
    secondStart.mode = QStringLiteral("practice");
    secondStart.playbackSpeed = 100;
    secondStart.startTick = 0;
    const qint64 secondSessionId = store.beginSession(sheetId, secondStart);
    expect(secondSessionId > 0, "PracticeRecordStore should begin a second practice session");

    PracticeSessionSummary secondSummary;
    secondSummary.completed = false;
    secondSummary.endTick = 480;
    secondSummary.activeDurationSeconds = 3;
    secondSummary.correctCount = 0;
    secondSummary.wrongCount = 1;
    expect(store.finishSession(secondSessionId, secondSummary), "PracticeRecordStore should finish a filtered practice session");

    const QVector<PracticeSessionRecord> recent = store.recentSessions(3, sheetId);
    expect(recent.size() == 2, "PracticeRecordStore should query recent sessions for a sheet");
    expect(recent.at(0).sheetId == sheetId, "recent session should preserve sheet id");
    expect(recent.at(0).activeDurationSeconds == 3, "recent session should expose active duration");

    const QVector<PracticeSessionRecord> completedRhythm = store.recentSessions(
        3, sheetId, true, QStringLiteral("rhythm"));
    expect(completedRhythm.size() == 1, "PracticeRecordStore should filter recent sessions by completion and mode");
    expect(completedRhythm.at(0).score == 50, "filtered recent session should expose calculated score");

    const QVector<PracticeMistakeStat> mistakes = store.mistakeStatsForSheet(sheetId, 3);
    expect(mistakes.size() == 1, "PracticeRecordStore should query mistake stats for a sheet");
    expect(mistakes.at(0).midi == 62, "mistake stats should group by expected pitch");
    expect(mistakes.at(0).wrongCount == 1, "mistake stats should count wrong notes");

    const PracticeReportSummary report = store.reportForSheet(sheetId, 3, 3);
    expect(report.sessionCount == 2, "PracticeRecordStore report should count sessions");
    expect(report.averageScore == 25, "PracticeRecordStore report should average scores");
    expect(report.totalWrong == 2, "PracticeRecordStore report should aggregate wrong notes");

    const PracticeReportSummary completedReport = store.reportForSheet(
        sheetId, 3, 3, true, QStringLiteral("rhythm"));
    expect(completedReport.sessionCount == 1, "PracticeRecordStore report should filter completed rhythm sessions");
    expect(completedReport.averageScore == 50, "filtered report should average only matching sessions");

    const QHash<QString, StoredSheetInfo> sheetsByPath =
        store.sheetsForPaths(QStringList{ QStringLiteral("test.mid") });
    expect(sheetsByPath.contains(QStringLiteral("test.mid")), "PracticeRecordStore should query sheets by local path");
    expect(sheetsByPath.value(QStringLiteral("test.mid")).id == sheetId, "sheet path query should preserve sheet id");
    store.close();

    const QString connectionName = QStringLiteral("practice-store-readback");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(dbPath);
    expect(db.open(), "readback database connection should open");

    QSqlQuery sessions(db);
    expect(sessions.exec(QStringLiteral(
               "SELECT mode, correct_count, score, completed, active_duration_seconds "
               "FROM practice_sessions WHERE id = 1")),
           "readback session query should run");
    expect(sessions.next(), "readback should find the recorded session");
    expect(sessions.value(0).toString() == QStringLiteral("rhythm"), "recorded session should keep its mode");
    expect(sessions.value(1).toInt() == 1, "recorded session should keep final correct count");
    expect(sessions.value(2).toInt() == 50, "recorded session should calculate score");
    expect(sessions.value(3).toInt() == 1, "recorded session should keep completion state");
    expect(sessions.value(4).toInt() == 7, "recorded session should keep active duration");

    QSqlQuery events(db);
    expect(events.exec(QStringLiteral("SELECT result, actual_midi FROM practice_events WHERE session_id = 1")),
           "readback event query should run");
    expect(events.next(), "readback should find the recorded event");
    expect(events.value(0).toString() == QStringLiteral("correct"), "recorded event should store judge type text");
    expect(events.value(1).toInt() == 60, "recorded event should keep actual pitch");

    QSqlQuery version(db);
    expect(version.exec(QStringLiteral("PRAGMA user_version")), "schema version query should run");
    expect(version.next(), "schema version query should return a row");
    expect(version.value(0).toInt() >= 2, "schema should store the migration user_version");

    sessions.finish();
    events.finish();
    version.finish();

    QSqlQuery setFutureVersion(db);
    expect(setFutureVersion.exec(QStringLiteral("PRAGMA user_version = 9")),
           "schema version should be adjustable for future-version migration tests");
    setFutureVersion.finish();

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);

    PracticeRecordStore reopened;
    expect(reopened.open(dbPath), "PracticeRecordStore should reopen a future-version database");
    reopened.close();

    const QString futureConnectionName = QStringLiteral("practice-store-future-version");
    QSqlDatabase futureDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), futureConnectionName);
    futureDb.setDatabaseName(dbPath);
    expect(futureDb.open(), "future-version readback database connection should open");

    QSqlQuery futureVersion(futureDb);
    expect(futureVersion.exec(QStringLiteral("PRAGMA user_version")), "future schema version query should run");
    expect(futureVersion.next(), "future schema version query should return a row");
    expect(futureVersion.value(0).toInt() == 9, "schema migration should not downgrade newer user_version values");

    futureVersion.finish();
    futureDb.close();
    futureDb = QSqlDatabase();
    QSqlDatabase::removeDatabase(futureConnectionName);
}

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testNoteUtils();
    testJsonParser();
    testJsonParserSkipsOutOfRangeMidi();
    testJsonParserUsesFixedPpqAfterNonDefaultMidi();
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
    testPlaybackClockDurationMsBetweenTicks();
    testPlaybackEngineAdvanceAndSpeed();
    testPlaybackEngineSeekStopAndClamp();
    testPlaybackEngineEndReached();
    testPracticeEngineSingleNoteAndWrongNote();
    testPracticeEngineChordRequiresAllNotes();
    testPracticeEngineSeekAndReset();
    testPracticeEngineRhythmJudging();
    testMidiInputMessageFiltering();
    testPracticeRecordStore();

    if (failures == 0) {
        std::cout << "All core tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
