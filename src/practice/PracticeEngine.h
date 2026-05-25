#pragma once

#include "core/Song.h"

#include <QSet>
#include <QVector>

enum class PracticeJudgeType {
    Ignored,
    Correct,
    RepeatedNote,
    WrongNote,
    Early,
    Late,
    Missed
};

struct PracticeNoteResult {
    PracticeJudgeType type = PracticeJudgeType::Ignored;
    bool countedCorrect = false;
    bool stepComplete = false;
    bool songComplete = false;
    bool statsChanged = false;
    int expectedMidi = -1;
    int actualMidi = -1;
    int actualVelocity = 0;
    qint64 expectedTick = -1;
    qint64 actualTick = -1;
    qint64 timingOffsetTick = 0;
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
    QVector<NoteEvent> expectedNotes() const;
    bool hasExpectedNotes() const;
    PracticeNoteResult noteOn(int midi, int velocity);
    PracticeNoteResult noteOnRhythm(int midi, int velocity, qint64 actualTick, qint64 toleranceTick);
    QVector<PracticeNoteResult> markMissedUntil(qint64 actualTick, qint64 toleranceTick);

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
