#pragma once

#include "core/Song.h"

#include <QString>
#include <QVector>

namespace HandPractice {

enum class Side {
    Left,
    Right
};

struct Filter {
    bool enabled = false;
    Side side = Side::Right;
    int splitMidi = 60;
};

QString normalizeSideName(const QString &side);
Side sideFromName(const QString &side);
int normalizeSplitMidi(int midi);

bool midiIsLeftHand(int midi, int splitMidi);
bool midiMatchesSide(int midi, Side side, int splitMidi);
bool midiMatchesFilter(int midi, const Filter &filter);

bool noteIsLeftHand(const NoteEvent &note, int splitMidi);
bool noteMatchesSide(const NoteEvent &note, Side side, int splitMidi);
bool noteMatchesFilter(const NoteEvent &note, const Filter &filter);
QVector<NoteEvent> filterNotes(const QVector<NoteEvent> &notes, const Filter &filter);

bool sameNoteIdentity(const NoteEvent &a, const NoteEvent &b);
bool noteMatchesCompletedStep(const NoteEvent &note, qint64 completedTick, const Filter &filter);

} // namespace HandPractice
