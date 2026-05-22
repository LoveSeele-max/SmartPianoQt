#include "core/NoteUtils.h"

#include <QRegularExpression>

namespace {

constexpr const char *SharpNames[] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

int pitchClassFromName(const QString &step, const QString &accidental)
{
    int base = -1;
    if (step == "C") base = 0;
    if (step == "D") base = 2;
    if (step == "E") base = 4;
    if (step == "F") base = 5;
    if (step == "G") base = 7;
    if (step == "A") base = 9;
    if (step == "B") base = 11;
    if (base < 0) return -1;

    if (accidental == "#") base += 1;
    if (accidental == "b" || accidental == "B") base -= 1;

    return (base + 12) % 12;
}

}

namespace NoteUtils {

QString midiToName(int midi)
{
    if (midi < 0 || midi > 127) return {};
    const int pitchClass = midi % 12;
    const int octave = midi / 12 - 1;
    return QString("%1%2").arg(SharpNames[pitchClass]).arg(octave);
}

int noteNameToMidi(const QString &name)
{
    static const QRegularExpression re(R"(^\s*([A-Ga-g])([#bB]?)(-?\d+)\s*$)");
    const auto match = re.match(name);
    if (!match.hasMatch()) return -1;

    const QString step = match.captured(1).toUpper();
    const QString accidental = match.captured(2);
    const int pitchClass = pitchClassFromName(step, accidental);
    if (pitchClass < 0) return -1;

    bool ok = false;
    const int octave = match.captured(3).toInt(&ok);
    if (!ok) return -1;

    const int midi = (octave + 1) * 12 + pitchClass;
    return (midi >= 0 && midi <= 127) ? midi : -1;
}

bool isBlackKey(int midi)
{
    const int pitchClass = ((midi % 12) + 12) % 12;
    return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 ||
           pitchClass == 8 || pitchClass == 10;
}

}
