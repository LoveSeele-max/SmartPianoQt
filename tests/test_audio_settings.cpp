#include "test_support.h"

#include "audio/AudioSettings.h"

namespace SmartPianoTest {

void testAudioSettingsVelocityAndLatency()
{
    const int middle = 64;
    expect(AudioSettings::mapVelocity(middle, AudioSettings::VelocityCurve::Linear) == middle,
           "linear velocity curve should preserve MIDI velocity");
    expect(AudioSettings::mapVelocity(middle, AudioSettings::VelocityCurve::Soft) < middle,
           "soft velocity curve should make mid velocities gentler");
    expect(AudioSettings::mapVelocity(middle, AudioSettings::VelocityCurve::Bright) > middle,
           "bright velocity curve should make mid velocities more present");
    expect(AudioSettings::mapVelocity(16, AudioSettings::VelocityCurve::Compressed) > 16,
           "compressed velocity curve should lift very quiet input");
    expect(AudioSettings::mapVelocity(127, AudioSettings::VelocityCurve::Compressed) < 127,
           "compressed velocity curve should tame peak input");
    expect(AudioSettings::mapVelocity(-10, AudioSettings::VelocityCurve::Linear) == 1,
           "velocity curve should clamp low input");
    expect(AudioSettings::mapVelocity(200, AudioSettings::VelocityCurve::Linear) == 127,
           "velocity curve should clamp high input");

    expect(AudioSettings::velocityCurveFromName(QStringLiteral("SOFT")) == AudioSettings::VelocityCurve::Soft,
           "velocity curve names should be case-insensitive");
    expect(AudioSettings::velocityCurveName(AudioSettings::velocityCurveFromName(QStringLiteral("unknown"))) == QStringLiteral("linear"),
           "unknown velocity curves should fall back to linear");

    expect(AudioSettings::normalizeLatencyMode(QStringLiteral("low")) == QStringLiteral("low"),
           "low latency mode should normalize");
    expect(AudioSettings::normalizeLatencyMode(QStringLiteral("compatible")) == QStringLiteral("compatible"),
           "compatible latency mode should normalize");
    expect(AudioSettings::normalizeLatencyMode(QStringLiteral("unknown")) == QStringLiteral("stable"),
           "unknown latency mode should fall back to stable");
    expect(AudioSettings::bufferSizeForLatencyMode(QStringLiteral("low")) <
               AudioSettings::bufferSizeForLatencyMode(QStringLiteral("stable")),
           "low latency mode should use a smaller buffer than stable mode");
    expect(AudioSettings::bufferSizeForLatencyMode(QStringLiteral("compatible")) >
               AudioSettings::bufferSizeForLatencyMode(QStringLiteral("stable")),
           "compatible latency mode should use a larger buffer than stable mode");
}

}
