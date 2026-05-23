#pragma once

#include "core/Song.h"

#include <QSet>
#include <QVector>

enum class PracticeJudgeType {
    Ignored,
    Correct,
    WrongNote
};

struct PracticeNoteResult {
    PracticeJudgeType type = PracticeJudgeType::Ignored;
    bool countedCorrect = false;
    bool stepComplete = false;
    bool songComplete = false;
    bool statsChanged = false;
    int actualMidi = -1;
    qint64 completedTick = -1;
    qint64 nextTick = -1;
};

class PracticeEngine {
public:
    void setSong(const QVector<NoteEvent> &notes);
    void reset(bool resetStats);
    bool seek(qint64 tick);

    qint64 expectedTick() const;
    bool hasExpectedNotes() const;
    PracticeNoteResult noteOn(int midi);

    int correctCount() const { return m_correctCount; }
    int wrongCount() const { return m_wrongCount; }
    int missedCount() const { return m_missedCount; }

private:
    void rebuildPracticeTicks();
    QSet<int> expectedMidiSet(qint64 tick) const;

    QVector<NoteEvent> m_notes;
    QVector<qint64> m_practiceTicks;
    int m_waitTickIndex = 0;
    QSet<int> m_matchedNotes;
    int m_correctCount = 0;
    int m_wrongCount = 0;
    int m_missedCount = 0;
};
