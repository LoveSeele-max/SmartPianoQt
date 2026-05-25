#pragma once

#include "core/Song.h"

#include <QVector>

struct PlaybackAdvanceResult {
    qint64 previousTick = 0;
    qint64 currentTick = 0;
    bool reachedEnd = false;
    bool positionChanged = false;
};

class PlaybackEngine {
public:
    static constexpr int DefaultPpq = 480;

    void setSong(const Song &song);
    bool setPlaybackSpeed(int speed);
    void stop();
    void seekTick(qint64 tick);
    PlaybackAdvanceResult advance(qint64 elapsedMs);

    int bpm() const { return m_bpm; }
    int ppq() const { return m_ppq; }
    int playbackSpeed() const { return m_playbackSpeed; }
    const QVector<TempoEvent> &tempos() const { return m_tempos; }
    qint64 currentTick() const { return m_currentTick; }
    qint64 totalTicks() const { return m_totalTicks; }
    qint64 maxNoteDurationTick() const { return m_maxNoteDurationTick; }

    qint64 beatToTick(double beat) const;
    double tickToBeat(qint64 tick) const;
    double currentBeat() const { return tickToBeat(m_currentTick); }
    double totalBeats() const { return tickToBeat(m_totalTicks); }

private:
    void resetPosition();

    int m_bpm = 100;
    int m_ppq = DefaultPpq;
    QVector<TempoEvent> m_tempos;

    int m_playbackSpeed = 100;
    double m_playbackRate = 1.0;
    double m_playbackMsRemainder = 0.0;

    qint64 m_currentTick = 0;
    double m_preciseTick = 0.0;
    qint64 m_totalTicks = DefaultPpq * 8;
    qint64 m_maxNoteDurationTick = DefaultPpq * 2;
};
