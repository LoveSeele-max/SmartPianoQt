#include "parser/MidiFileParser.h"

#include "core/NoteUtils.h"

#include <QHash>
#include <QtEndian>
#include <algorithm>

namespace {

struct RawMidiEvent {
    qint64 tick = 0;
    int type = 0;
    int note = 0;
    int velocity = 0;
    int track = 0;
    int channel = 0;
};

struct ActiveNote {
    qint64 tick = 0;
    int velocity = 80;
    int track = 0;
    int channel = 0;
};

bool readChunk(const QByteArray &bytes, int &offset, QByteArray &id, QByteArray &payload)
{
    if (offset + 8 > bytes.size()) return false;
    id = bytes.mid(offset, 4);
    offset += 4;

    const auto *lenPtr = reinterpret_cast<const uchar *>(bytes.constData() + offset);
    const quint32 length = qFromBigEndian<quint32>(lenPtr);
    offset += 4;

    if (length > quint32(bytes.size() - offset)) return false;
    payload = bytes.mid(offset, int(length));
    offset += int(length);
    return true;
}

bool readVarLen(const QByteArray &data, int &offset, qint64 &value)
{
    value = 0;
    for (int i = 0; i < 4; ++i) {
        if (offset >= data.size()) return false;
        const auto byte = quint8(data.at(offset++));
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) return true;
    }
    return true;
}

int bytesNeededForStatus(int status)
{
    const int high = status & 0xF0;
    if (high == 0xC0 || high == 0xD0) return 1;
    if (high >= 0x80 && high <= 0xE0) return 2;
    return 0;
}

QByteArray readSizedText(const QByteArray &trackData, int offset, int length)
{
    if (length <= 0 || offset < 0 || offset + length > trackData.size()) return {};
    return trackData.mid(offset, length);
}

}

ParsedMidi MidiFileParser::parse(const QByteArray &bytes)
{
    ParsedMidi result;

    int offset = 0;
    QByteArray id;
    QByteArray payload;
    if (!readChunk(bytes, offset, id, payload) || id != "MThd") {
        result.error = QStringLiteral("不是有效的 MIDI 文件");
        return result;
    }

    if (payload.size() < 6) {
        result.error = QStringLiteral("MIDI 文件头不完整");
        return result;
    }

    const int trackCount = qFromBigEndian<quint16>(
        reinterpret_cast<const uchar *>(payload.constData() + 2));
    const int division = qFromBigEndian<quint16>(
        reinterpret_cast<const uchar *>(payload.constData() + 4));

    if (division & 0x8000) {
        result.error = QStringLiteral("暂不支持 SMPTE 时间格式的 MIDI 文件");
        return result;
    }
    result.ppq = division > 0 ? division : 480;

    QVector<RawMidiEvent> rawEvents;
    bool tempoSeen = false;

    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
        if (!readChunk(bytes, offset, id, payload)) break;
        if (id != "MTrk") continue;

        int pos = 0;
        qint64 tick = 0;
        int runningStatus = 0;

        while (pos < payload.size()) {
            qint64 delta = 0;
            if (!readVarLen(payload, pos, delta)) break;
            tick += delta;

            if (pos >= payload.size()) break;
            int status = quint8(payload.at(pos++));
            if (status < 0x80) {
                --pos;
                status = runningStatus;
            } else if (status < 0xF0) {
                runningStatus = status;
            }

            if (status == 0xFF) {
                if (pos + 2 > payload.size()) break;
                const int metaType = quint8(payload.at(pos++));
                qint64 length = 0;
                if (!readVarLen(payload, pos, length)) break;
                if (length < 0 || pos + length > payload.size()) break;

                if (metaType == 0x51 && length == 3 && !tempoSeen) {
                    const int microsPerQuarter =
                        (quint8(payload.at(pos)) << 16) |
                        (quint8(payload.at(pos + 1)) << 8) |
                        quint8(payload.at(pos + 2));
                    if (microsPerQuarter > 0) {
                        result.bpm = qRound(60000000.0 / microsPerQuarter);
                        tempoSeen = true;
                    }
                } else if ((metaType == 0x03 || metaType == 0x04) && result.title.isEmpty()) {
                    result.title = QString::fromUtf8(readSizedText(payload, pos, int(length))).trimmed();
                }

                pos += int(length);
                continue;
            }

            if (status == 0xF0 || status == 0xF7) {
                qint64 length = 0;
                if (!readVarLen(payload, pos, length)) break;
                pos += int(length);
                continue;
            }

            const int needed = bytesNeededForStatus(status);
            if (needed == 0 || pos + needed > payload.size()) break;

            const int high = status & 0xF0;
            const int channel = status & 0x0F;
            if (high == 0x90 || high == 0x80) {
                const int note = quint8(payload.at(pos++));
                const int velocity = quint8(payload.at(pos++));
                rawEvents.push_back({ tick, high, note, velocity, trackIndex, channel });
            } else {
                pos += needed;
            }
        }
    }

    std::sort(rawEvents.begin(), rawEvents.end(), [](const RawMidiEvent &a, const RawMidiEvent &b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.type < b.type;
    });

    QHash<QString, QVector<ActiveNote>> active;
    int idCounter = 1;

    for (const auto &event : rawEvents) {
        const QString key = QStringLiteral("%1:%2").arg(event.channel).arg(event.note);
        if (event.type == 0x90 && event.velocity > 0) {
            active[key].push_back({ event.tick, event.velocity, event.track, event.channel });
            continue;
        }

        auto &stack = active[key];
        if (stack.isEmpty()) continue;

        const ActiveNote start = stack.takeFirst();
        NoteEvent note;
        note.id = idCounter++;
        note.midi = event.note;
        note.velocity = start.velocity;
        note.startTick = start.tick;
        note.durationTick = qMax<qint64>(result.ppq / 4, event.tick - start.tick);
        note.track = start.track;
        note.channel = start.channel;
        note.fingering = 0;
        note.noteName = NoteUtils::midiToName(event.note);
        result.notes.push_back(note);
    }

    std::sort(result.notes.begin(), result.notes.end(), [](const NoteEvent &a, const NoteEvent &b) {
        if (a.startTick != b.startTick) return a.startTick < b.startTick;
        return a.midi < b.midi;
    });

    if (result.notes.isEmpty()) {
        result.error = QStringLiteral("未在 MIDI 文件中找到有效音符");
        return result;
    }

    result.ok = true;
    return result;
}
