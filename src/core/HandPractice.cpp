#include "core/HandPractice.h"

#include <QtGlobal>

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

bool midiIsLeftHand(int midi, int splitMidi)
{
    return midi < normalizeSplitMidi(splitMidi);
}

bool midiMatchesSide(int midi, Side side, int splitMidi)
{
    const bool left = midiIsLeftHand(midi, splitMidi);
    return side == Side::Left ? left : !left;
}

bool midiMatchesFilter(int midi, const Filter &filter)
{
    return !filter.enabled || midiMatchesSide(midi, filter.side, filter.splitMidi);
}

bool noteIsLeftHand(const NoteEvent &note, int splitMidi)
{
    return midiIsLeftHand(note.midi, splitMidi);
}

bool noteMatchesSide(const NoteEvent &note, Side side, int splitMidi)
{
    return midiMatchesSide(note.midi, side, splitMidi);
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

} // namespace HandPractice
