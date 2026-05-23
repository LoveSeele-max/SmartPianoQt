#include "playback/PlaybackEngine.h"

#include "playback/PlaybackClock.h"

#include <QtMath>
#include <algorithm>

void PlaybackEngine::setSong(const Song &song)
{
    m_bpm = qBound(20, song.bpm, 260);
    m_ppq = song.ppq > 0 ? song.ppq : DefaultPpq;
    m_tempos = PlaybackClock::normalizedTempoMap(song.tempos, m_bpm);

    m_totalTicks = m_ppq * 8;
    m_maxNoteDurationTick = m_ppq * 2;
    for (const auto &note : song.notes) {
        m_totalTicks = qMax(m_totalTicks, note.startTick + note.durationTick);
        m_maxNoteDurationTick = qMax(m_maxNoteDurationTick, note.durationTick);
    }

    resetPosition();
}

bool PlaybackEngine::setPlaybackSpeed(int speed)
{
    const int clamped = qBound(50, speed, 150);
    if (m_playbackSpeed == clamped) return false;

    m_playbackSpeed = clamped;
    m_playbackRate = double(m_playbackSpeed) / 100.0;
    m_playbackMsRemainder = 0.0;
    return true;
}

void PlaybackEngine::stop()
{
    seekTick(0);
}

void PlaybackEngine::seekTick(qint64 tick)
{
    m_currentTick = qBound<qint64>(0, tick, m_totalTicks);
    m_preciseTick = double(m_currentTick);
    m_playbackMsRemainder = 0.0;
}

PlaybackAdvanceResult PlaybackEngine::advance(qint64 elapsedMs)
{
    PlaybackAdvanceResult result;
    result.previousTick = m_currentTick;
    if (elapsedMs <= 0) {
        result.currentTick = m_currentTick;
        return result;
    }

    const double scaledElapsedMs = double(elapsedMs) * m_playbackRate + m_playbackMsRemainder;
    const qint64 playbackElapsedMs = qMax<qint64>(0, qint64(scaledElapsedMs));
    m_playbackMsRemainder = scaledElapsedMs - double(playbackElapsedMs);

    m_preciseTick = PlaybackClock::advance(m_preciseTick, playbackElapsedMs, m_tempos, m_ppq);
    m_preciseTick = qBound(0.0, m_preciseTick, double(m_totalTicks));
    m_currentTick = qBound<qint64>(0, qRound64(m_preciseTick), m_totalTicks);

    if (m_currentTick >= m_totalTicks) {
        m_currentTick = m_totalTicks;
        m_preciseTick = double(m_totalTicks);
        result.reachedEnd = true;
    }

    result.currentTick = m_currentTick;
    result.positionChanged = result.currentTick != result.previousTick;
    return result;
}

qint64 PlaybackEngine::beatToTick(double beat) const
{
    return qRound64(beat * double(m_ppq));
}

double PlaybackEngine::tickToBeat(qint64 tick) const
{
    return m_ppq > 0 ? double(tick) / double(m_ppq) : 0.0;
}

void PlaybackEngine::resetPosition()
{
    m_currentTick = 0;
    m_preciseTick = 0.0;
    m_playbackMsRemainder = 0.0;
}
