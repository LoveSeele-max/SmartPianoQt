#pragma once

#include "core/Song.h"

class PlaybackClock {
public:
    static QVector<TempoEvent> tempoMapFromBpm(int bpm);
    static QVector<TempoEvent> normalizedTempoMap(QVector<TempoEvent> tempos, int fallbackBpm);
    static double advance(double currentTick, qint64 elapsedMs, const QVector<TempoEvent> &tempos, int ppq);
};
