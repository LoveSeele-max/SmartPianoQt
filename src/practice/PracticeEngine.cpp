#include "practice/PracticeEngine.h"

#include <algorithm>

void PracticeEngine::setSong(const QVector<NoteEvent> &notes)
{
    m_notes = notes;
    std::sort(m_notes.begin(), m_notes.end(), [](const NoteEvent &a, const NoteEvent &b) {
        if (a.startTick != b.startTick) return a.startTick < b.startTick;
        return a.midi < b.midi;
    });
    rebuildPracticeTicks();
    reset(true);
}

void PracticeEngine::reset(bool resetStats)
{
    m_matchedNotes.clear();
    m_waitTickIndex = 0;
    if (resetStats) {
        m_correctCount = 0;
        m_wrongCount = 0;
        m_missedCount = 0;
    }
}

bool PracticeEngine::seek(qint64 tick)
{
    if (m_practiceTicks.isEmpty()) return false;

    auto it = std::lower_bound(m_practiceTicks.begin(), m_practiceTicks.end(), tick);
    if (it == m_practiceTicks.end()) {
        m_waitTickIndex = m_practiceTicks.size();
    } else {
        m_waitTickIndex = int(std::distance(m_practiceTicks.begin(), it));
    }

    m_matchedNotes.clear();
    return true;
}

qint64 PracticeEngine::expectedTick() const
{
    if (!hasExpectedNotes()) return -1;
    return m_practiceTicks.at(m_waitTickIndex);
}

bool PracticeEngine::hasExpectedNotes() const
{
    return m_waitTickIndex >= 0 && m_waitTickIndex < m_practiceTicks.size();
}

PracticeNoteResult PracticeEngine::noteOn(int midi)
{
    PracticeNoteResult result;
    result.actualMidi = midi;
    if (!hasExpectedNotes()) return result;

    const qint64 tick = expectedTick();
    const QSet<int> expected = expectedMidiSet(tick);
    if (!expected.contains(midi)) {
        ++m_wrongCount;
        result.type = PracticeJudgeType::WrongNote;
        result.statsChanged = true;
        return result;
    }

    result.type = PracticeJudgeType::Correct;
    if (!m_matchedNotes.contains(midi)) {
        m_matchedNotes.insert(midi);
        ++m_correctCount;
        result.countedCorrect = true;
        result.statsChanged = true;
    }

    if (m_matchedNotes.size() >= expected.size()) {
        result.stepComplete = true;
        result.completedTick = tick;
        m_matchedNotes.clear();
        ++m_waitTickIndex;

        if (m_waitTickIndex >= m_practiceTicks.size()) {
            result.songComplete = true;
        } else {
            result.nextTick = m_practiceTicks.at(m_waitTickIndex);
        }
    }

    return result;
}

void PracticeEngine::rebuildPracticeTicks()
{
    m_practiceTicks.clear();
    qint64 last = -1;
    for (const auto &note : m_notes) {
        if (note.startTick != last) {
            m_practiceTicks.push_back(note.startTick);
            last = note.startTick;
        }
    }
}

QSet<int> PracticeEngine::expectedMidiSet(qint64 tick) const
{
    QSet<int> expected;
    for (const auto &note : m_notes) {
        if (note.startTick == tick) {
            expected.insert(note.midi);
        } else if (note.startTick > tick) {
            break;
        }
    }
    return expected;
}
