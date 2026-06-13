#include "core/HandPractice.h"

#include <QtGlobal>
#include <algorithm>

namespace HandPractice {

QString normalizeSideName(const QString &side)
{
    return side == QStringLiteral("left") ? QStringLiteral("left") : QStringLiteral("right");
}

Side sideFromName(const QString &side)
{
    return normalizeSideName(side) == QStringLiteral("left") ? Side::Left : Side::Right;
}

int normalizeSplitMidi(int midi)
{
    return qBound(0, midi, 127);
}

ReferenceDisplayMode referenceDisplayModeFromName(const QString &mode)
{
    if (mode == QStringLiteral("dim")) {
        return ReferenceDisplayMode::Dim;
    }
    if (mode == QStringLiteral("all")) {
        return ReferenceDisplayMode::All;
    }
    return ReferenceDisplayMode::TargetOnly;
}

bool midiIsLeftHand(int midi, int splitMidi)
{
    return midi < normalizeSplitMidi(splitMidi);
}

Side classifyMidi(int midi, const Filter &filter)
{
    switch (filter.policy) {
    case ClassificationPolicy::SplitMidi:
        return midiIsLeftHand(midi, filter.splitMidi) ? Side::Left : Side::Right;
    }
    return Side::Right;
}

Side classifyNote(const NoteEvent &note, const Filter &filter)
{
    return classifyMidi(note.midi, filter);
}

bool midiMatchesSide(int midi, Side side, int splitMidi)
{
    Filter filter;
    filter.splitMidi = splitMidi;
    return classifyMidi(midi, filter) == side;
}

bool midiMatchesFilter(int midi, const Filter &filter)
{
    return !filter.enabled || classifyMidi(midi, filter) == filter.side;
}

bool noteIsLeftHand(const NoteEvent &note, int splitMidi)
{
    Filter filter;
    filter.splitMidi = splitMidi;
    return classifyNote(note, filter) == Side::Left;
}

bool noteMatchesSide(const NoteEvent &note, Side side, int splitMidi)
{
    Filter filter;
    filter.splitMidi = splitMidi;
    return classifyNote(note, filter) == side;
}

bool noteMatchesFilter(const NoteEvent &note, const Filter &filter)
{
    return midiMatchesFilter(note.midi, filter);
}

QVector<NoteEvent> filterNotes(const QVector<NoteEvent> &notes, const Filter &filter)
{
    if (!filter.enabled) {
        return notes;
    }

    QVector<NoteEvent> filtered;
    filtered.reserve(notes.size());
    for (const NoteEvent &note : notes) {
        if (noteMatchesFilter(note, filter)) {
            filtered.push_back(note);
        }
    }
    return filtered;
}

bool sameNoteIdentity(const NoteEvent &a, const NoteEvent &b)
{
    if (a.id > 0 && b.id > 0) {
        return a.id == b.id;
    }

    return a.midi == b.midi &&
        a.startTick == b.startTick &&
        a.track == b.track &&
        a.channel == b.channel;
}

bool noteMatchesCompletedStep(const NoteEvent &note, qint64 completedTick, const Filter &filter)
{
    return note.startTick == completedTick && noteMatchesFilter(note, filter);
}

bool noteShouldBePlayedBeforeTick(const NoteEvent &note, qint64 tick, const Filter &filter)
{
    return note.startTick < tick && noteMatchesFilter(note, filter);
}

NoteDisplayState displayStateForNote(const NoteEvent &note, const NoteDisplayContext &context)
{
    NoteDisplayState state;
    state.target = noteMatchesFilter(note, context.filter);
    state.reference = context.filter.enabled && !state.target;

    if (state.target) {
        const bool withinTick = context.currentTick >= note.startTick &&
            context.currentTick <= note.startTick + note.durationTick;
        state.active = withinTick;
        state.completed = note.played;

        if (context.practiceMode) {
            state.expected = std::any_of(context.expectedNotes.begin(),
                                         context.expectedNotes.end(),
                                         [&](const NoteEvent &expectedNote) {
                                             return sameNoteIdentity(note, expectedNote);
                                         });
        }
    }

    state.preparatory = state.target &&
        !state.expected &&
        !state.active &&
        !state.completed &&
        note.startTick > context.currentTick;

    if (state.expected) {
        state.visual = NoteVisualState::Expected;
    } else if (state.active) {
        state.visual = NoteVisualState::Active;
    } else if (state.completed) {
        state.visual = NoteVisualState::Completed;
    } else if (state.preparatory) {
        state.visual = NoteVisualState::Preparatory;
    } else if (state.reference) {
        state.visual = NoteVisualState::Reference;
    } else {
        state.visual = NoteVisualState::Normal;
    }

    return state;
}

bool displayStateVisible(const NoteDisplayState &state, ReferenceDisplayMode mode)
{
    return !state.reference || mode != ReferenceDisplayMode::TargetOnly;
}

bool displayStateDimmed(const NoteDisplayState &state, ReferenceDisplayMode mode)
{
    return state.reference && mode == ReferenceDisplayMode::Dim;
}

} // namespace HandPractice
