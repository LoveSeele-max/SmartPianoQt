#pragma once

#include <QString>

namespace NoteUtils {

QString midiToName(int midi);
int noteNameToMidi(const QString &name);
bool isBlackKey(int midi);

}
