#include "playback/PlaybackClock.h"

#include <algorithm>
#include <iterator>

namespace {

constexpr int DefaultBpm = 120;
constexpr int DefaultMicrosPerQuarter = 500000;

int clampBpm(int bpm)
{
    return std::clamp(bpm, 20, 260);
}

int microsFromBpm(int bpm)
{
    return int(60000000.0 / double(clampBpm(bpm)));
}

int safeMicros(int microsecondsPerQuarter)
{
    return microsecondsPerQuarter > 0 ? microsecondsPerQuarter : DefaultMicrosPerQuarter;
}

int tempoIndexForTick(const QVector<TempoEvent> &tempos, double tick)
{
    const auto firstAfterTick = std::upper_bound(
        tempos.begin(), tempos.end(), tick,
        [](double tickValue, const TempoEvent &tempo) {
            return tickValue < double(tempo.tick);
        });
    return std::max(0, int(std::distance(tempos.begin(), firstAfterTick)) - 1);
}

}

QVector<TempoEvent> PlaybackClock::tempoMapFromBpm(int bpm)
{
    return { { 0, microsFromBpm(bpm) } };
}

QVector<TempoEvent> PlaybackClock::normalizedTempoMap(QVector<TempoEvent> tempos, int fallbackBpm)
{
    QVector<TempoEvent> normalized;
    normalized.reserve(tempos.size() + 1);
    for (TempoEvent tempo : tempos) {
        if (tempo.microsecondsPerQuarter <= 0) continue;
        if (tempo.tick < 0) tempo.tick = 0;
        normalized.push_back(tempo);
    }

    std::stable_sort(normalized.begin(), normalized.end(), [](const TempoEvent &a, const TempoEvent &b) {
        return a.tick < b.tick;
    });

    QVector<TempoEvent> coalesced;
    coalesced.reserve(normalized.size());
    for (const TempoEvent &tempo : normalized) {
        if (!coalesced.isEmpty() && coalesced.last().tick == tempo.tick) {
            coalesced.last() = tempo;
        } else {
            coalesced.push_back(tempo);
        }
    }
    normalized = coalesced;

    if (normalized.isEmpty()) {
        normalized = tempoMapFromBpm(fallbackBpm);
    } else if (normalized.first().tick > 0) {
        normalized.prepend({ 0, microsFromBpm(fallbackBpm) });
    }

    return normalized;
}

double PlaybackClock::advance(double currentTick, qint64 elapsedMs, const QVector<TempoEvent> &tempos, int ppq)
{
    if (elapsedMs <= 0 || ppq <= 0) return currentTick;

    QVector<TempoEvent> fallback;
    const QVector<TempoEvent> *tempoMap = &tempos;
    if (tempoMap->isEmpty()) {
        fallback = tempoMapFromBpm(DefaultBpm);
        tempoMap = &fallback;
    }

    double tick = std::max(0.0, currentTick);
    double remainingMs = double(elapsedMs);
    int tempoIndex = tempoIndexForTick(*tempoMap, tick);

    while (remainingMs > 0.0) {
        const int microsPerQuarter = safeMicros(tempoMap->at(tempoIndex).microsecondsPerQuarter);
        const double ticksPerMs = 1000.0 * double(ppq) / double(microsPerQuarter);

        if (tempoIndex + 1 >= tempoMap->size()) {
            tick += remainingMs * ticksPerMs;
            break;
        }

        const double nextTempoTick = double(tempoMap->at(tempoIndex + 1).tick);
        if (nextTempoTick <= tick) {
            ++tempoIndex;
            continue;
        }

        const double ticksToNextTempo = nextTempoTick - tick;
        const double msToNextTempo = ticksToNextTempo / ticksPerMs;
        if (remainingMs < msToNextTempo) {
            tick += remainingMs * ticksPerMs;
            break;
        }

        tick = nextTempoTick;
        remainingMs -= msToNextTempo;
        ++tempoIndex;
    }

    return tick;
}
