#include "test_support.h"

#include "playback/PlaybackClock.h"
#include "playback/PlaybackEngine.h"

#include <QVector>
#include <QtMath>

namespace SmartPianoTest {

void testPlaybackClockSingleTempo()
{
    const QVector<TempoEvent> tempos = PlaybackClock::tempoMapFromBpm(120);
    const double advanced = PlaybackClock::advance(0.0, 500, tempos, 480);
    expect(qRound64(advanced) == 480, "PlaybackClock should advance one beat in 500 ms at 120 BPM");
}

void testPlaybackClockCrossesTempoChange()
{
    const QVector<TempoEvent> tempos = PlaybackClock::normalizedTempoMap(
        { { 0, 500000 }, { 480, 1000000 } }, 120);
    const double advanced = PlaybackClock::advance(0.0, 1000, tempos, 480);
    expect(qRound64(advanced) == 720, "PlaybackClock should consume remaining time at the slower tempo");
}

void testPlaybackClockNormalizesLateTempo()
{
    const QVector<TempoEvent> tempos = PlaybackClock::normalizedTempoMap({ { 480, 400000 } }, 120);
    expect(tempos.size() == 2, "PlaybackClock should prepend a fallback tempo before a late first tempo");
    expect(tempos.at(0).tick == 0, "PlaybackClock fallback tempo should start at tick 0");
    expect(tempos.at(0).microsecondsPerQuarter == 500000, "PlaybackClock fallback tempo should use fallback BPM");
}

void testPlaybackClockCoalescesSameTickTempos()
{
    const QVector<TempoEvent> tempos = PlaybackClock::normalizedTempoMap(
        { { 0, 500000 }, { 0, 1000000 }, { 480, 400000 }, { 480, 750000 } }, 120);
    expect(tempos.size() == 2, "PlaybackClock should coalesce duplicate tempo ticks");
    expect(tempos.at(0).microsecondsPerQuarter == 1000000, "last tempo at tick zero should win");
    expect(tempos.at(1).microsecondsPerQuarter == 750000, "last tempo at a duplicate later tick should win");

    const double advanced = PlaybackClock::advance(0.0, 1000, tempos, 480);
    expect(qRound64(advanced) == 480, "PlaybackClock should use the coalesced starting tempo");
}

void testPlaybackClockDurationMsBetweenTicks()
{
    const QVector<TempoEvent> tempos = PlaybackClock::normalizedTempoMap(
        { { 0, 500000 }, { 480, 1000000 } }, 120);

    const double forwardMs = PlaybackClock::durationMsBetweenTicks(0, 960, tempos, 480);
    expect(qRound64(forwardMs) == 1500, "PlaybackClock should convert tick ranges through tempo changes");

    const double backwardMs = PlaybackClock::durationMsBetweenTicks(960, 480, tempos, 480);
    expect(qRound64(backwardMs) == -1000, "PlaybackClock should preserve sign for early timing offsets");
}

void testPlaybackEngineAdvanceAndSpeed()
{
    PlaybackEngine playback;
    playback.setSong(makePlaybackSong());

    expect(playback.bpm() == 120, "PlaybackEngine should expose the source BPM");
    expect(playback.ppq() == 480, "PlaybackEngine should expose the source PPQ");
    expect(playback.currentTick() == 0, "PlaybackEngine should start at tick zero");

    PlaybackAdvanceResult normal = playback.advance(500);
    expect(normal.previousTick == 0, "PlaybackEngine should report the previous tick");
    expect(normal.currentTick == 480, "PlaybackEngine should advance at source tempo by default");
    expect(playback.currentTick() == 480, "PlaybackEngine current tick should follow advance results");

    expect(playback.setPlaybackSpeed(50), "PlaybackEngine should accept a slower playback speed");
    PlaybackAdvanceResult slow = playback.advance(500);
    expect(slow.currentTick == 720, "PlaybackEngine should scale elapsed time by playback speed");
}

void testPlaybackEngineSeekStopAndClamp()
{
    PlaybackEngine playback;
    playback.setSong(makePlaybackSong());

    playback.seekTick(999999);
    expect(playback.currentTick() == playback.totalTicks(), "PlaybackEngine seek should clamp to song end");
    playback.stop();
    expect(playback.currentTick() == 0, "PlaybackEngine stop should return to the beginning");

    playback.seekTick(-50);
    expect(playback.currentTick() == 0, "PlaybackEngine seek should clamp negative positions");
}

void testPlaybackEngineEndReached()
{
    PlaybackEngine playback;
    playback.setSong(makePlaybackSong());
    playback.seekTick(playback.totalTicks() - 120);

    const PlaybackAdvanceResult result = playback.advance(1000);
    expect(result.reachedEnd, "PlaybackEngine should report reaching the end");
    expect(result.currentTick == playback.totalTicks(), "PlaybackEngine should clamp current tick at the end");
}

}
