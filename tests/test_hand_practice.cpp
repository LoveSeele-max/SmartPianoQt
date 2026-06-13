#include "test_support.h"

#include "core/HandPractice.h"
#include "practice/PracticeEngine.h"

#include <utility>

namespace SmartPianoTest {

namespace {

HandPractice::NoteDisplayContext displayContext(HandPractice::Filter filter,
                                                QVector<NoteEvent> expectedNotes,
                                                qint64 currentTick = 0)
{
    HandPractice::NoteDisplayContext context;
    context.filter = filter;
    context.practiceMode = true;
    context.currentTick = currentTick;
    context.expectedNotes = std::move(expectedNotes);
    return context;
}

} // namespace

void testHandPracticeFiltersTargetHand()
{
    const QVector<NoteEvent> notes = {
        makeNote(48, 0),
        makeNote(60, 0),
        makeNote(55, 480),
        makeNote(72, 480),
    };

    HandPractice::Filter leftFilter;
    leftFilter.enabled = true;
    leftFilter.side = HandPractice::Side::Left;
    leftFilter.splitMidi = 60;

    const QVector<NoteEvent> left = HandPractice::filterNotes(notes, leftFilter);
    expect(left.size() == 2, "left-hand practice should keep only notes below the split");
    expect(left.at(0).midi == 48, "left-hand practice should keep the first low note");
    expect(left.at(1).midi == 55, "left-hand practice should keep later low notes");
    expect(!HandPractice::midiMatchesFilter(60, leftFilter), "left-hand practice should not target split-note pitches");

    HandPractice::Filter rightFilter;
    rightFilter.enabled = true;
    rightFilter.side = HandPractice::Side::Right;
    rightFilter.splitMidi = 60;

    const QVector<NoteEvent> right = HandPractice::filterNotes(notes, rightFilter);
    expect(right.size() == 2, "right-hand practice should keep only notes at or above the split");
    expect(right.at(0).midi == 60, "right-hand practice should include the split-note pitch");
    expect(right.at(1).midi == 72, "right-hand practice should keep later high notes");
    expect(!HandPractice::midiMatchesFilter(48, rightFilter), "right-hand practice should ignore non-target low input");

    rightFilter.enabled = false;
    const QVector<NoteEvent> all = HandPractice::filterNotes(notes, rightFilter);
    expect(all.size() == notes.size(), "disabled hand practice should keep both hands");
}

void testHandPracticeDisplayStateRules()
{
    NoteEvent left = makeNote(48, 480);
    NoteEvent right = makeNote(60, 480);
    right.played = true;

    HandPractice::Filter rightFilter;
    rightFilter.enabled = true;
    rightFilter.side = HandPractice::Side::Right;
    rightFilter.splitMidi = 60;

    const HandPractice::NoteDisplayContext rightContext =
        displayContext(rightFilter, QVector<NoteEvent>{ right }, 490);
    const HandPractice::NoteDisplayState targetState =
        HandPractice::displayStateForNote(right, rightContext);
    expect(targetState.target, "display state target notes should be marked as target");
    expect(!targetState.reference, "target and reference display states should be mutually exclusive");
    expect(targetState.expected, "expected target notes should be marked expected");
    expect(targetState.active, "target notes under the playhead should be active");
    expect(targetState.completed, "played target notes should remain completed");
    expect(targetState.visual == HandPractice::NoteVisualState::Expected,
           "expected should win visual priority over active and completed");

    const HandPractice::NoteDisplayState referenceState =
        HandPractice::displayStateForNote(left, rightContext);
    expect(!referenceState.target, "non-target notes should not be target");
    expect(referenceState.reference, "non-target hand notes should be reference");
    expect(!referenceState.expected, "reference notes should not become expected");
    expect(!referenceState.active, "reference notes should not become active in practice mode");
    expect(!referenceState.completed, "reference notes should not become completed");
    expect(!referenceState.preparatory, "reference notes should not become preparatory");
    expect(referenceState.visual == HandPractice::NoteVisualState::Reference,
           "reference notes should keep the reference visual state");

    HandPractice::Filter allFilter = rightFilter;
    allFilter.enabled = false;
    const HandPractice::NoteDisplayContext allContext =
        displayContext(allFilter, QVector<NoteEvent>{ left, right }, 490);
    expect(HandPractice::displayStateForNote(left, allContext).expected,
           "disabled hand practice should allow left-hand notes to be expected");
    expect(HandPractice::displayStateForNote(right, allContext).expected,
           "disabled hand practice should allow right-hand notes to be expected");
}

void testHandPracticeDisplayReferenceModes()
{
    HandPractice::NoteDisplayState reference;
    reference.target = false;
    reference.reference = true;

    const HandPractice::ReferenceDisplayMode targetOnly =
        HandPractice::referenceDisplayModeFromName(QStringLiteral("target"));
    expect(!HandPractice::displayStateVisible(reference, targetOnly),
           "target display mode should hide reference notes");
    expect(!HandPractice::displayStateDimmed(reference, targetOnly),
           "hidden reference notes should not be treated as dimmed");

    const HandPractice::ReferenceDisplayMode dim =
        HandPractice::referenceDisplayModeFromName(QStringLiteral("dim"));
    expect(HandPractice::displayStateVisible(reference, dim),
           "dim display mode should keep reference notes visible");
    expect(HandPractice::displayStateDimmed(reference, dim),
           "dim display mode should dim reference notes");

    const HandPractice::ReferenceDisplayMode all =
        HandPractice::referenceDisplayModeFromName(QStringLiteral("all"));
    expect(HandPractice::displayStateVisible(reference, all),
           "all display mode should keep reference notes visible");
    expect(!HandPractice::displayStateDimmed(reference, all),
           "all display mode should not dim reference notes");
}

void testHandPracticeExpectedStateRebuildsForHandChanges()
{
    const NoteEvent low = makeNote(55, 480);
    const NoteEvent middle = makeNote(60, 480);

    HandPractice::Filter leftFilter;
    leftFilter.enabled = true;
    leftFilter.side = HandPractice::Side::Left;
    leftFilter.splitMidi = 60;
    const QVector<NoteEvent> leftExpected =
        HandPractice::filterNotes(QVector<NoteEvent>{ low, middle }, leftFilter);
    const HandPractice::NoteDisplayContext leftContext = displayContext(leftFilter, leftExpected);
    expect(HandPractice::displayStateForNote(low, leftContext).expected,
           "left-hand practice should expect low notes");
    expect(!HandPractice::displayStateForNote(middle, leftContext).expected,
           "left-hand practice should not expect split-boundary notes");

    HandPractice::Filter rightFilter = leftFilter;
    rightFilter.side = HandPractice::Side::Right;
    const QVector<NoteEvent> rightExpected =
        HandPractice::filterNotes(QVector<NoteEvent>{ low, middle }, rightFilter);
    const HandPractice::NoteDisplayContext rightContext = displayContext(rightFilter, rightExpected);
    expect(!HandPractice::displayStateForNote(low, rightContext).expected,
           "right-hand practice should not expect low notes after switching hands");
    expect(HandPractice::displayStateForNote(middle, rightContext).expected,
           "right-hand practice should expect split-boundary notes after switching hands");

    leftFilter.splitMidi = 72;
    const QVector<NoteEvent> raisedSplitExpected =
        HandPractice::filterNotes(QVector<NoteEvent>{ low, middle }, leftFilter);
    const HandPractice::NoteDisplayContext raisedSplitContext =
        displayContext(leftFilter, raisedSplitExpected);
    expect(HandPractice::displayStateForNote(middle, raisedSplitContext).expected,
           "raising splitMidi should rebuild left-hand expected notes for the new boundary");
}

void testHandPracticeCompletedStepTargetsOnlySelectedHand()
{
    const NoteEvent left = makeNote(48, 0);
    const NoteEvent right = makeNote(60, 0);
    const NoteEvent laterRight = makeNote(72, 480);

    HandPractice::Filter rightFilter;
    rightFilter.enabled = true;
    rightFilter.side = HandPractice::Side::Right;
    rightFilter.splitMidi = 60;

    expect(!HandPractice::noteMatchesCompletedStep(left, 0, rightFilter),
           "right-hand completed steps should not mark same-tick left-hand notes");
    expect(HandPractice::noteMatchesCompletedStep(right, 0, rightFilter),
           "right-hand completed steps should mark same-tick target notes");
    expect(!HandPractice::noteMatchesCompletedStep(laterRight, 0, rightFilter),
           "completed steps should not mark notes from a different tick");

    HandPractice::Filter leftFilter = rightFilter;
    leftFilter.side = HandPractice::Side::Left;
    expect(HandPractice::noteMatchesCompletedStep(left, 0, leftFilter),
           "left-hand completed steps should mark same-tick target notes");
    expect(!HandPractice::noteMatchesCompletedStep(right, 0, leftFilter),
           "left-hand completed steps should not mark same-tick right-hand notes");

    rightFilter.enabled = false;
    expect(HandPractice::noteMatchesCompletedStep(left, 0, rightFilter),
           "full practice completed steps should include left-hand notes");
    expect(HandPractice::noteMatchesCompletedStep(right, 0, rightFilter),
           "full practice completed steps should include right-hand notes");
}

void testHandPracticePlayedBeforeTickRespectsTargetHand()
{
    const NoteEvent leftPast = makeNote(48, 0);
    const NoteEvent rightPast = makeNote(60, 0);
    const NoteEvent rightFuture = makeNote(72, 960);

    HandPractice::Filter rightFilter;
    rightFilter.enabled = true;
    rightFilter.side = HandPractice::Side::Right;
    rightFilter.splitMidi = 60;

    expect(!HandPractice::noteShouldBePlayedBeforeTick(leftPast, 480, rightFilter),
           "seek should clear same-past non-target hand completed state");
    expect(HandPractice::noteShouldBePlayedBeforeTick(rightPast, 480, rightFilter),
           "seek should preserve past completed state for target hand notes");
    expect(!HandPractice::noteShouldBePlayedBeforeTick(rightFuture, 480, rightFilter),
           "seek should not mark future target hand notes completed");
}

void testHandPracticeIgnoresReferenceInputBeforeJudgement()
{
    const QVector<NoteEvent> notes = {
        makeNote(48, 0),
        makeNote(60, 0),
    };

    HandPractice::Filter rightFilter;
    rightFilter.enabled = true;
    rightFilter.side = HandPractice::Side::Right;
    rightFilter.splitMidi = 60;

    PracticeEngine practice;
    practice.setSong(HandPractice::filterNotes(notes, rightFilter));

    if (HandPractice::midiMatchesFilter(48, rightFilter)) {
        practice.noteOn(48, 90);
    }
    expect(practice.wrongCount() == 0,
           "reference-hand input should be ignored before it can increment wrongCount");

    const PracticeNoteResult target = practice.noteOn(60, 90);
    expect(target.type == PracticeJudgeType::Correct,
           "target-hand input should still reach PracticeEngine normally");
}

void testHandPracticeNoteIdentityFallsBackWithoutIds()
{
    NoteEvent first = makeNote(60, 480);
    NoteEvent same = first;
    first.id = 0;
    same.id = 0;

    expect(HandPractice::sameNoteIdentity(first, same),
           "note identity should fall back to pitch, tick, track, and channel when ids are absent");

    same.channel = 2;
    expect(!HandPractice::sameNoteIdentity(first, same),
           "note identity fallback should distinguish channels");
}

}
