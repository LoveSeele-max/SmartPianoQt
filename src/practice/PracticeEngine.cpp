#include "practice/PracticeEngine.h"

#include <algorithm>

void PracticeEngine::setSong(const QVector<NoteEvent> &notes)
{
    rebuildSteps(notes);
    reset(true);
}

void PracticeEngine::reset(bool resetStats)
{
    m_matchedNotes.clear();
    m_stepIndex = 0;
    if (resetStats) {
        m_correctCount = 0;
        m_wrongCount = 0;
        m_missedCount = 0;
    }
}

bool PracticeEngine::seek(qint64 tick)
{
    if (m_steps.isEmpty()) return false;

    auto it = std::lower_bound(m_steps.begin(), m_steps.end(), tick,
        [](const PracticeStep &step, qint64 targetTick) {
            return step.tick < targetTick;
        });
    m_stepIndex = int(std::distance(m_steps.begin(), it));

    m_matchedNotes.clear();
    return true;
}

qint64 PracticeEngine::expectedTick() const
{
    const PracticeStep *step = currentStep();
    return step ? step->tick : -1;
}

QVector<NoteEvent> PracticeEngine::expectedNotes() const
{
    const PracticeStep *step = currentStep();
    return step ? step->notes : QVector<NoteEvent>{};
}

bool PracticeEngine::hasExpectedNotes() const
{
    return currentStep() != nullptr;
}

PracticeNoteResult PracticeEngine::noteOn(int midi, int velocity)
{
    PracticeNoteResult result;
    result.actualMidi = midi;
    result.actualVelocity = velocity;

    const PracticeStep *step = currentStep();
    if (!step) return result;

    result.expectedTick = step->tick;
    result.expectedMidi = step->expectedMidi.contains(midi) ? midi : step->notes.first().midi;

    if (!step->expectedMidi.contains(midi)) {
        ++m_wrongCount;
        result.type = PracticeJudgeType::WrongNote;
        result.statsChanged = true;
        return result;
    }

    if (m_matchedNotes.contains(midi)) {
        result.type = PracticeJudgeType::RepeatedNote;
    } else {
        result.type = PracticeJudgeType::Correct;
        m_matchedNotes.insert(midi);
        ++m_correctCount;
        result.countedCorrect = true;
        result.statsChanged = true;
    }

    if (m_matchedNotes.size() >= step->expectedMidi.size()) {
        result.stepComplete = true;
        result.completedTick = step->tick;
        m_matchedNotes.clear();
        ++m_stepIndex;

        if (m_stepIndex >= m_steps.size()) {
            result.songComplete = true;
        } else {
            result.nextTick = m_steps.at(m_stepIndex).tick;
        }
    }

    return result;
}

PracticeNoteResult PracticeEngine::noteOnRhythm(int midi, int velocity, qint64 actualTick, qint64 toleranceTick)
{
    PracticeNoteResult result;
    result.actualMidi = midi;
    result.actualVelocity = velocity;
    result.actualTick = actualTick;

    const PracticeStep *step = currentStep();
    if (!step) return result;

    result.expectedTick = step->tick;
    result.expectedMidi = step->expectedMidi.contains(midi) ? midi : step->notes.first().midi;
    result.timingOffsetTick = actualTick - step->tick;

    if (result.timingOffsetTick < -toleranceTick) {
        ++m_wrongCount;
        result.type = PracticeJudgeType::Early;
        result.statsChanged = true;
        return result;
    }

    if (result.timingOffsetTick > toleranceTick) {
        m_missedCount += step->expectedMidi.size();
        result.type = PracticeJudgeType::Late;
        result.stepComplete = true;
        result.statsChanged = true;
        result.completedTick = step->tick;
        m_matchedNotes.clear();
        ++m_stepIndex;
        if (m_stepIndex >= m_steps.size()) {
            result.songComplete = true;
        } else {
            result.nextTick = m_steps.at(m_stepIndex).tick;
        }
        return result;
    }

    PracticeNoteResult judged = noteOn(midi, velocity);
    judged.actualTick = actualTick;
    judged.timingOffsetTick = actualTick - judged.expectedTick;
    return judged;
}

QVector<PracticeNoteResult> PracticeEngine::markMissedUntil(qint64 actualTick, qint64 toleranceTick)
{
    QVector<PracticeNoteResult> results;
    while (const PracticeStep *step = currentStep()) {
        if (actualTick <= step->tick + toleranceTick) break;

        for (const NoteEvent &note : step->notes) {
            PracticeNoteResult result;
            result.type = PracticeJudgeType::Missed;
            result.statsChanged = true;
            result.expectedMidi = note.midi;
            result.expectedTick = step->tick;
            result.actualTick = actualTick;
            result.timingOffsetTick = actualTick - step->tick;
            result.completedTick = step->tick;
            results.push_back(result);
        }

        m_missedCount += step->notes.size();
        m_matchedNotes.clear();
        ++m_stepIndex;
    }

    if (!results.isEmpty()) {
        PracticeNoteResult &last = results.last();
        last.stepComplete = true;
        if (m_stepIndex >= m_steps.size()) {
            last.songComplete = true;
        } else {
            last.nextTick = m_steps.at(m_stepIndex).tick;
        }
    }

    return results;
}

void PracticeEngine::rebuildSteps(const QVector<NoteEvent> &notes)
{
    QVector<NoteEvent> sortedNotes = notes;
    std::sort(sortedNotes.begin(), sortedNotes.end(), [](const NoteEvent &a, const NoteEvent &b) {
        if (a.startTick != b.startTick) return a.startTick < b.startTick;
        return a.midi < b.midi;
    });

    m_steps.clear();
    for (const auto &note : sortedNotes) {
        if (m_steps.isEmpty() || m_steps.last().tick != note.startTick) {
            PracticeStep step;
            step.tick = note.startTick;
            m_steps.push_back(step);
        }
        PracticeStep &step = m_steps.last();
        step.notes.push_back(note);
        step.expectedMidi.insert(note.midi);
    }
}

const PracticeStep *PracticeEngine::currentStep() const
{
    if (m_stepIndex < 0 || m_stepIndex >= m_steps.size()) return nullptr;
    return &m_steps.at(m_stepIndex);
}
