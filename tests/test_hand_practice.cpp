#include "test_support.h"

#include "core/HandPractice.h"

namespace SmartPianoTest {

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
