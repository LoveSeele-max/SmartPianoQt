#pragma once

#include "core/Song.h"

#include <QString>
#include <QVector>

namespace HandPractice {

enum class Side {
    Left,
    Right
};

enum class ClassificationPolicy {
    SplitMidi
};

enum class ReferenceDisplayMode {
    TargetOnly,
    Dim,
    All
};

enum class NoteVisualState {
    Normal,
    Reference,
    Preparatory,
    Completed,
    Active,
    Expected
};

struct Filter {
    bool enabled = false;
    Side side = Side::Right;
    int splitMidi = 60;
    ClassificationPolicy policy = ClassificationPolicy::SplitMidi;
};

struct NoteDisplayContext {
    Filter filter;
    bool practiceMode = false;
    qint64 currentTick = 0;
    QVector<NoteEvent> expectedNotes;
};

struct NoteDisplayState {
    bool target = true;
    bool reference = false;
    bool expected = false;
    bool active = false;
    bool completed = false;
    bool preparatory = false;
    NoteVisualState visual = NoteVisualState::Normal;
};

QString normalizeSideName(const QString &side);
Side sideFromName(const QString &side);
int normalizeSplitMidi(int midi);
ReferenceDisplayMode referenceDisplayModeFromName(const QString &mode);

bool midiIsLeftHand(int midi, int splitMidi);
Side classifyMidi(int midi, const Filter &filter);
Side classifyNote(const NoteEvent &note, const Filter &filter);
bool midiMatchesSide(int midi, Side side, int splitMidi);
bool midiMatchesFilter(int midi, const Filter &filter);

bool noteIsLeftHand(const NoteEvent &note, int splitMidi);
bool noteMatchesSide(const NoteEvent &note, Side side, int splitMidi);
bool noteMatchesFilter(const NoteEvent &note, const Filter &filter);
QVector<NoteEvent> filterNotes(const QVector<NoteEvent> &notes, const Filter &filter);

bool sameNoteIdentity(const NoteEvent &a, const NoteEvent &b);
bool noteMatchesCompletedStep(const NoteEvent &note, qint64 completedTick, const Filter &filter);
bool noteShouldBePlayedBeforeTick(const NoteEvent &note, qint64 tick, const Filter &filter);

// Display-state rules:
// - target and reference are mutually exclusive.
// - expected, active, and completed are target-hand states; reference notes stay background-only.
// - visual priority is expected > active > completed > preparatory > reference > normal.
// - reference visibility is controlled by ReferenceDisplayMode: hidden, dimmed, or shown as-is.
NoteDisplayState displayStateForNote(const NoteEvent &note, const NoteDisplayContext &context);
bool displayStateVisible(const NoteDisplayState &state, ReferenceDisplayMode mode);
bool displayStateDimmed(const NoteDisplayState &state, ReferenceDisplayMode mode);

} // namespace HandPractice
