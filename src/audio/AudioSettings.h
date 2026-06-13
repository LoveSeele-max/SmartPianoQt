#pragma once

#include <QString>
#include <QStringList>

namespace AudioSettings {

enum class VelocityCurve {
    Linear,
    Soft,
    Bright,
    Compressed
};

VelocityCurve velocityCurveFromName(const QString &name);
QString velocityCurveName(VelocityCurve curve);
QStringList velocityCurveNames();
int mapVelocity(int velocity, VelocityCurve curve);

QString normalizeLatencyMode(const QString &mode);
QStringList latencyModeNames();
int bufferSizeForLatencyMode(const QString &mode);

}
