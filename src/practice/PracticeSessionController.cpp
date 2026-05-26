#include "practice/PracticeSessionController.h"

#include <QtMath>

PracticeSessionController::PracticeSessionController(PracticeRecordStore *store)
    : m_store(store)
{
}

void PracticeSessionController::setRecordStore(PracticeRecordStore *store)
{
    m_store = store;
}

void PracticeSessionController::setSheetId(qint64 sheetId)
{
    if (m_sheetId == sheetId) return;
    if (m_active) return;
    m_sheetId = sheetId;
}

bool PracticeSessionController::begin(const QString &mode, int playbackSpeed, qint64 startTick)
{
    if (m_active) {
        resume();
        return true;
    }
    if (!m_store || m_sheetId <= 0) return false;

    PracticeSessionStart start;
    start.mode = mode;
    start.playbackSpeed = playbackSpeed;
    start.startTick = startTick;
    m_sessionId = m_store->beginSession(m_sheetId, start);
    m_active = m_sessionId > 0;
    if (m_active) {
        m_activeElapsedMs = 0;
        m_activeClockRunning = true;
        m_activeTimer.restart();
    }
    return m_active;
}

void PracticeSessionController::pause()
{
    if (!m_active || !m_activeClockRunning) return;
    m_activeElapsedMs += m_activeTimer.elapsed();
    m_activeClockRunning = false;
}

void PracticeSessionController::resume()
{
    if (!m_active || m_activeClockRunning) return;
    m_activeTimer.restart();
    m_activeClockRunning = true;
}

bool PracticeSessionController::append(const PracticeNoteResult &result, qint64 fallbackActualTick, int offsetMs)
{
    if (!m_active || !m_store) return false;

    PracticeEventRecord event;
    event.result = result.type;
    event.expectedMidi = result.expectedMidi;
    event.actualMidi = result.actualMidi;
    event.velocity = result.actualVelocity;
    event.expectedTick = result.expectedTick;
    event.actualTick = result.actualTick >= 0 ? result.actualTick : fallbackActualTick;
    event.offsetMs = offsetMs;
    return m_store->appendEvent(m_sessionId, event);
}

bool PracticeSessionController::finish(bool completed,
                                       qint64 endTick,
                                       int correctCount,
                                       int wrongCount,
                                       int missedCount)
{
    if (!m_active || !m_store) return false;

    pause();

    PracticeSessionSummary summary;
    summary.completed = completed;
    summary.endTick = endTick;
    summary.activeDurationSeconds = qMax(0, int(m_activeElapsedMs / 1000));
    summary.correctCount = correctCount;
    summary.wrongCount = wrongCount;
    summary.missedCount = missedCount;

    const bool ok = m_store->finishSession(m_sessionId, summary);
    m_sessionId = -1;
    m_active = false;
    m_activeClockRunning = false;
    m_activeElapsedMs = 0;
    return ok;
}
