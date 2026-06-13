#include "audio/AudioSettings.h"

#include <QtMath>

#include <cmath>

namespace AudioSettings {

VelocityCurve velocityCurveFromName(const QString &name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("soft")) return VelocityCurve::Soft;
    if (normalized == QStringLiteral("bright")) return VelocityCurve::Bright;
    if (normalized == QStringLiteral("compressed")) return VelocityCurve::Compressed;
    return VelocityCurve::Linear;
}

QString velocityCurveName(VelocityCurve curve)
{
    switch (curve) {
    case VelocityCurve::Soft:
        return QStringLiteral("soft");
    case VelocityCurve::Bright:
        return QStringLiteral("bright");
    case VelocityCurve::Compressed:
        return QStringLiteral("compressed");
    case VelocityCurve::Linear:
    default:
        return QStringLiteral("linear");
    }
}

QStringList velocityCurveNames()
{
    return {
        QStringLiteral("linear"),
        QStringLiteral("soft"),
        QStringLiteral("bright"),
        QStringLiteral("compressed")
    };
}

int mapVelocity(int velocity, VelocityCurve curve)
{
    const int clamped = qBound(1, velocity, 127);
    const double x = double(clamped) / 127.0;
    double y = x;

    switch (curve) {
    case VelocityCurve::Soft:
        y = std::pow(x, 1.25);
        break;
    case VelocityCurve::Bright:
        y = std::pow(x, 0.72);
        break;
    case VelocityCurve::Compressed:
        y = 0.16 + x * 0.74;
        break;
    case VelocityCurve::Linear:
    default:
        y = x;
        break;
    }

    return qBound(1, qRound(y * 127.0), 127);
}

QString normalizeLatencyMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("low")) return QStringLiteral("low");
    if (normalized == QStringLiteral("compatible")) return QStringLiteral("compatible");
    return QStringLiteral("stable");
}

QStringList latencyModeNames()
{
    return {
        QStringLiteral("low"),
        QStringLiteral("stable"),
        QStringLiteral("compatible")
    };
}

int bufferSizeForLatencyMode(const QString &mode)
{
    const QString normalized = normalizeLatencyMode(mode);
    if (normalized == QStringLiteral("low")) return 2048;
    if (normalized == QStringLiteral("compatible")) return 8192;
    return 4096;
}

}
