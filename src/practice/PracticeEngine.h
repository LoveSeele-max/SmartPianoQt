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
    int actualVelocity = 0;
    qint64 completedTick = -1;
    qint64 nextTick = -1;
};

struct PracticeStep {
    qint64 tick = 0;
    QVector<NoteEvent> notes;
    QSet<int> expectedMidi;
};

class PracticeEngine {
public:
    void setSong(const QVector<NoteEvent> &notes);
    void reset(bool resetStats);
    bool seek(qint64 tick);

    qint64 expectedTick() const;
    bool hasExpectedNotes() const;
    PracticeNoteResult noteOn(int midi, int velocity);

    int correctCount() const { return m_correctCount; }
    int wrongCount() const { return m_wrongCount; }
    int missedCount() const { return m_missedCount; }

private:
    void rebuildSteps(const QVector<NoteEvent> &notes);
    const PracticeStep *currentStep() const;

    QVector<PracticeStep> m_steps;
    int m_stepIndex = 0;
    QSet<int> m_matchedNotes;
    int m_correctCount = 0;
    int m_wrongCount = 0;
    int m_missedCount = 0;
};
