#include "test_support.h"

#include <QCoreApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    using namespace SmartPianoTest;

    testAudioSettingsVelocityAndLatency();
    testNoteUtils();
    testJsonParser();
    testJsonParserSkipsOutOfRangeMidi();
    testJsonParserUsesFixedPpqAfterNonDefaultMidi();
    testHandPracticeFiltersTargetHand();
    testHandPracticeDisplayStateRules();
    testHandPracticeDisplayReferenceModes();
    testHandPracticeExpectedStateRebuildsForHandChanges();
    testHandPracticeCompletedStepTargetsOnlySelectedHand();
    testHandPracticePlayedBeforeTickRespectsTargetHand();
    testHandPracticeIgnoresReferenceInputBeforeJudgement();
    testHandPracticeNoteIdentityFallsBackWithoutIds();
    testMidiSustainAndTempo();
    testMidiUnclosedNoteFallback();
    testMidiDefaultTempo();
    testMidiNoteOnVelocityZeroActsAsNoteOff();
    testMidiRunningStatus();
    testMidiMultiTrackNotesAndEmptyTracks();
    testMidiTempoSortingAcrossTracks();
    testMidiLateTempoGetsDefaultAtZero();
    testMidiRejectsMalformedVlq();
    testMidiRejectsMissingRunningStatus();
    testMidiRejectsTruncatedTrackChunk();
    testMidiRejectsFormat2();
    testMidiRejectsSmpteDivision();
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
    testPracticeEngineRhythmBoundaryWindows();
    testMidiInputMessageFiltering();
    testPracticeRecordStore();

    if (failures == 0) {
        std::cout << "All core tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
