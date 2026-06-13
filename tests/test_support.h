#pragma once

#include "core/Song.h"

#include <QByteArray>
#include <QVector>

#include <initializer_list>

namespace SmartPianoTest {

extern int failures;

void expect(bool condition, const char *message);
QByteArray raw(std::initializer_list<int> values);
QByteArray be16(int value);
QByteArray be32(int value);
QByteArray vlq(int value);
QByteArray chunk(const char *id, const QByteArray &payload);
void appendEvent(QByteArray &track, int delta, std::initializer_list<int> event);
void appendTempo(QByteArray &track, int delta, int microsPerQuarter);
void appendEnd(QByteArray &track, int delta);
QByteArray midiFileWithFormat(const QVector<QByteArray> &tracks, int format, int ppq = 480);
QByteArray midiFile(const QVector<QByteArray> &tracks, int ppq = 480);
QByteArray midiFile(const QByteArray &track, int ppq = 480);
NoteEvent makeNote(int midi, qint64 startTick);
Song makePlaybackSong();

void testAudioSettingsVelocityAndLatency();
void testNoteUtils();
void testJsonParser();
void testJsonParserSkipsOutOfRangeMidi();
void testJsonParserUsesFixedPpqAfterNonDefaultMidi();
void testHandPracticeFiltersTargetHand();
void testHandPracticeDisplayStateRules();
void testHandPracticeDisplayReferenceModes();
void testHandPracticeExpectedStateRebuildsForHandChanges();
void testHandPracticeCompletedStepTargetsOnlySelectedHand();
void testHandPracticePlayedBeforeTickRespectsTargetHand();
void testHandPracticeIgnoresReferenceInputBeforeJudgement();
void testHandPracticeNoteIdentityFallsBackWithoutIds();
void testMidiSustainAndTempo();
void testMidiUnclosedNoteFallback();
void testMidiDefaultTempo();
void testMidiNoteOnVelocityZeroActsAsNoteOff();
void testMidiRunningStatus();
void testMidiMultiTrackNotesAndEmptyTracks();
void testMidiTempoSortingAcrossTracks();
void testMidiLateTempoGetsDefaultAtZero();
void testMidiRejectsMalformedVlq();
void testMidiRejectsMissingRunningStatus();
void testMidiRejectsTruncatedTrackChunk();
void testMidiRejectsFormat2();
void testMidiRejectsSmpteDivision();
void testMidiRejectsInvalidSysexLength();
void testMidiRejectsTruncatedChannelEvent();
void testPlaybackClockSingleTempo();
void testPlaybackClockCrossesTempoChange();
void testPlaybackClockNormalizesLateTempo();
void testPlaybackClockCoalescesSameTickTempos();
void testPlaybackClockDurationMsBetweenTicks();
void testPlaybackEngineAdvanceAndSpeed();
void testPlaybackEngineSeekStopAndClamp();
void testPlaybackEngineEndReached();
void testPracticeEngineSingleNoteAndWrongNote();
void testPracticeEngineChordRequiresAllNotes();
void testPracticeEngineSeekAndReset();
void testPracticeEngineRhythmJudging();
void testPracticeEngineRhythmBoundaryWindows();
void testMidiInputMessageFiltering();
void testPracticeRecordStore();

}
