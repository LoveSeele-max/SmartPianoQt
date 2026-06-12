#include "test_support.h"

#include "practice/PracticeEngine.h"

#include <QVector>

namespace SmartPianoTest {

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
    RhythmTimingWindows windows;
    windows.perfectTick = 50;
    windows.goodTick = 100;
    windows.hitTick = 180;

    PracticeEngine practice;
    practice.setSong({ makeNote(60, 480), makeNote(62, 960) });

    const PracticeNoteResult tooEarly = practice.noteOnRhythm(60, 90, 250, windows);
    expect(tooEarly.type == PracticeJudgeType::Early, "rhythm judging should reject notes before the hit window as early");
    expect(!tooEarly.stepComplete, "too-early input should not consume the expected note");
    expect(practice.expectedTick() == 480, "too-early input should keep the same rhythm step available");

    practice.reset(true);
    const PracticeNoteResult perfect = practice.noteOnRhythm(60, 90, 520, windows);
    expect(perfect.type == PracticeJudgeType::Perfect, "rhythm judging should grade tight timing as perfect");
    expect(perfect.countedCorrect, "perfect rhythm notes should count as correct");
    expect(perfect.stepComplete, "single perfect rhythm note should complete its step");
    expect(perfect.nextTick == 960, "perfect rhythm completion should advance to the next step");

    const QVector<PracticeNoteResult> missed = practice.markMissedUntil(1141, windows);
    expect(missed.size() == 1, "rhythm mode should produce missed events for overdue notes");
    expect(missed.at(0).type == PracticeJudgeType::Missed, "overdue notes should be marked missed");
    expect(missed.at(0).expectedMidi == 62, "missed event should preserve expected pitch");
    expect(practice.missedCount() == 1, "missed rhythm notes should update missed count");

    PracticeEngine goodPractice;
    goodPractice.setSong({ makeNote(60, 480) });
    const PracticeNoteResult good = goodPractice.noteOnRhythm(60, 90, 560, windows);
    expect(good.type == PracticeJudgeType::Good, "rhythm judging should grade medium timing as good");
    expect(good.countedCorrect, "good rhythm notes should count as correct");

    PracticeEngine earlyPractice;
    earlyPractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const PracticeNoteResult early = earlyPractice.noteOnRhythm(60, 90, 340, windows);
    expect(early.type == PracticeJudgeType::Early, "rhythm judging should grade in-window negative offsets as early");
    expect(early.stepComplete, "in-window early input should consume the step");
    expect(early.nextTick == 960, "in-window early input should advance to the next step");
    expect(earlyPractice.wrongCount() == 1, "early timing should count as a timing mistake");

    PracticeEngine latePractice;
    latePractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const PracticeNoteResult late = latePractice.noteOnRhythm(60, 90, 620, windows);
    expect(late.type == PracticeJudgeType::Late, "rhythm judging should grade in-window positive offsets as late");
    expect(late.stepComplete, "in-window late input should consume the step");
    expect(late.nextTick == 960, "in-window late input should advance to the next step");
    expect(latePractice.wrongCount() == 1, "late timing should count as a timing mistake");

    PracticeEngine chordPractice;
    chordPractice.setSong({ makeNote(64, 480), makeNote(67, 480), makeNote(69, 960) });
    const QVector<PracticeNoteResult> missedChord = chordPractice.noteOnRhythmDetailed(64, 90, 700, windows);
    expect(missedChord.size() == 2, "over-window chord input should produce per-note missed results");
    expect(missedChord.at(0).type == PracticeJudgeType::Missed, "over-window chord members should be marked missed");
    expect(missedChord.at(0).expectedMidi == 64, "missed chord should preserve the first expected pitch");
    expect(missedChord.at(1).type == PracticeJudgeType::Missed, "all unplayed chord members should be recorded as missed");
    expect(missedChord.at(1).expectedMidi == 67, "missed chord member should preserve its expected pitch");
    expect(missedChord.last().stepComplete, "missed chord detail should complete the overdue step");
    expect(chordPractice.missedCount() == 2, "missed chord detail should count each missed note individually");
}

void testPracticeEngineRhythmBoundaryWindows()
{
    RhythmTimingWindows windows;
    windows.perfectTick = 50;
    windows.goodTick = 100;
    windows.hitTick = 180;

    PracticeEngine perfectEarlyPractice;
    perfectEarlyPractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const PracticeNoteResult perfectEarly = perfectEarlyPractice.noteOnRhythm(60, 90, 430, windows);
    expect(perfectEarly.type == PracticeJudgeType::Perfect, "rhythm boundary should include the early perfect tick");
    expect(perfectEarly.countedCorrect, "early perfect boundary should count as correct");
    expect(perfectEarly.stepComplete, "early perfect boundary should complete the step");

    PracticeEngine perfectLatePractice;
    perfectLatePractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const PracticeNoteResult perfectLate = perfectLatePractice.noteOnRhythm(60, 90, 530, windows);
    expect(perfectLate.type == PracticeJudgeType::Perfect, "rhythm boundary should include the late perfect tick");
    expect(perfectLate.countedCorrect, "late perfect boundary should count as correct");

    PracticeEngine goodPractice;
    goodPractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const PracticeNoteResult good = goodPractice.noteOnRhythm(60, 90, 580, windows);
    expect(good.type == PracticeJudgeType::Good, "rhythm boundary should include the good tick");
    expect(good.countedCorrect, "good boundary should count as correct");
    expect(goodPractice.correctCount() == 1, "good boundary should update the correct count");

    PracticeEngine earlyHitPractice;
    earlyHitPractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const PracticeNoteResult earlyHit = earlyHitPractice.noteOnRhythm(60, 90, 300, windows);
    expect(earlyHit.type == PracticeJudgeType::Early, "rhythm boundary should include the early hit tick");
    expect(earlyHit.stepComplete, "early hit boundary should consume the step");
    expect(earlyHit.nextTick == 960, "early hit boundary should advance to the next step");
    expect(earlyHitPractice.wrongCount() == 1, "early hit boundary should count as a timing mistake");

    PracticeEngine lateHitPractice;
    lateHitPractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const PracticeNoteResult lateHit = lateHitPractice.noteOnRhythm(60, 90, 660, windows);
    expect(lateHit.type == PracticeJudgeType::Late, "rhythm boundary should include the late hit tick");
    expect(lateHit.stepComplete, "late hit boundary should consume the step");
    expect(lateHit.nextTick == 960, "late hit boundary should advance to the next step");
    expect(lateHitPractice.wrongCount() == 1, "late hit boundary should count as a timing mistake");

    PracticeEngine tooEarlyPractice;
    tooEarlyPractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const PracticeNoteResult tooEarly = tooEarlyPractice.noteOnRhythm(60, 90, 299, windows);
    expect(tooEarly.type == PracticeJudgeType::Early, "one tick before the hit window should be too early");
    expect(!tooEarly.stepComplete, "too-early boundary overflow should not consume the step");
    expect(tooEarlyPractice.expectedTick() == 480, "too-early boundary overflow should keep the same step active");

    PracticeEngine tooLatePractice;
    tooLatePractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const QVector<PracticeNoteResult> tooLate = tooLatePractice.noteOnRhythmDetailed(60, 90, 661, windows);
    expect(tooLate.size() == 1, "one tick after the hit window should mark the note missed");
    expect(tooLate.at(0).type == PracticeJudgeType::Missed, "late boundary overflow should produce a missed note");
    expect(tooLate.at(0).stepComplete, "late boundary overflow should complete the missed step");
    expect(tooLate.at(0).nextTick == 960, "late boundary overflow should advance to the next step");
    expect(tooLatePractice.missedCount() == 1, "late boundary overflow should update the missed count");

    PracticeEngine missedBoundaryPractice;
    missedBoundaryPractice.setSong({ makeNote(60, 480), makeNote(62, 960) });
    const QVector<PracticeNoteResult> notYetMissed = missedBoundaryPractice.markMissedUntil(660, windows);
    expect(notYetMissed.isEmpty(), "markMissedUntil should not miss notes exactly at the hit boundary");
    const QVector<PracticeNoteResult> justMissed = missedBoundaryPractice.markMissedUntil(661, windows);
    expect(justMissed.size() == 1, "markMissedUntil should miss notes one tick after the hit boundary");
    expect(justMissed.at(0).type == PracticeJudgeType::Missed, "markMissedUntil boundary overflow should mark missed");
    expect(justMissed.at(0).nextTick == 960, "markMissedUntil boundary overflow should advance to the next step");
}

}
