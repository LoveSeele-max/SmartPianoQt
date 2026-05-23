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
    int controller = 0;
    int value = 0;
};

struct ActiveNote {
    qint64 tick = 0;
    int note = 0;
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
    return false;
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

QString noteKey(int track, int channel, int note)
{
    return QStringLiteral("%1:%2:%3").arg(track).arg(channel).arg(note);
}

QString laneKey(int track, int channel)
{
    return QStringLiteral("%1:%2").arg(track).arg(channel);
}

int eventOrder(const RawMidiEvent &event)
{
    if (event.type == 0xB0) return 0;
    if (event.type == 0x80 || (event.type == 0x90 && event.velocity == 0)) return 1;
    return 2;
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

    const int format = qFromBigEndian<quint16>(
        reinterpret_cast<const uchar *>(payload.constData()));
    if (format > 1) {
        result.error = QStringLiteral("暂不支持 MIDI Format 2 文件");
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
    result.song.ppq = division > 0 ? division : 480;

    QVector<RawMidiEvent> rawEvents;
    QHash<int, qint64> trackEndTicks;
    qint64 lastTick = 0;
    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
        if (!readChunk(bytes, offset, id, payload)) break;
        if (id != "MTrk") continue;

        int pos = 0;
        qint64 tick = 0;
        int runningStatus = 0;

        while (pos < payload.size()) {
            qint64 delta = 0;
            if (!readVarLen(payload, pos, delta)) {
                result.error = QStringLiteral("MIDI 事件长度编码无效");
                return result;
            }
            tick += delta;
            lastTick = qMax(lastTick, tick);

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
                if (!readVarLen(payload, pos, length)) {
                    result.error = QStringLiteral("MIDI 事件长度编码无效");
                    return result;
                }
                if (length < 0 || pos + length > payload.size()) break;

                if (metaType == 0x51 && length == 3) {
                    const int microsPerQuarter =
                        (quint8(payload.at(pos)) << 16) |
                        (quint8(payload.at(pos + 1)) << 8) |
                        quint8(payload.at(pos + 2));
                    if (microsPerQuarter > 0) {
                        result.song.tempos.push_back({ tick, microsPerQuarter });
                    }
                } else if ((metaType == 0x03 || metaType == 0x04) && result.song.title.isEmpty()) {
                    result.song.title = QString::fromUtf8(readSizedText(payload, pos, int(length))).trimmed();
                }

                pos += int(length);
                continue;
            }

            if (status == 0xF0 || status == 0xF7) {
                qint64 length = 0;
                if (!readVarLen(payload, pos, length)) {
                    result.error = QStringLiteral("MIDI 事件长度编码无效");
                    return result;
                }
                if (length < 0 || pos + length > payload.size()) {
                    result.error = QStringLiteral("MIDI SysEx 事件长度无效");
                    return result;
                }
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
                rawEvents.push_back({ tick, high, note, velocity, trackIndex, channel, 0, 0 });
            } else if (high == 0xB0) {
                const int controller = quint8(payload.at(pos++));
                const int value = quint8(payload.at(pos++));
                if (controller == 64) {
                    rawEvents.push_back({ tick, high, 0, 0, trackIndex, channel, controller, value });
                }
            } else {
                pos += needed;
            }
        }

        trackEndTicks.insert(trackIndex, tick);
    }

    std::sort(rawEvents.begin(), rawEvents.end(), [](const RawMidiEvent &a, const RawMidiEvent &b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return eventOrder(a) < eventOrder(b);
    });

    QHash<QString, QVector<ActiveNote>> active;
    QHash<QString, bool> sustainDown;
    QHash<QString, QVector<RawMidiEvent>> delayedNoteOffs;
    int idCounter = 1;

    auto appendNote = [&](const ActiveNote &start, qint64 endTick) {
        const qint64 minDuration = qMax<qint64>(1, result.song.ppq / 16);
        NoteEvent note;
        note.id = idCounter++;
        note.midi = start.note;
        note.velocity = start.velocity;
        note.startTick = start.tick;
        note.durationTick = qMax(minDuration, endTick - start.tick);
        note.track = start.track;
        note.channel = start.channel;
        note.fingering = 0;
        note.noteName = NoteUtils::midiToName(start.note);
        result.song.notes.push_back(note);
    };

    auto finishNote = [&](const RawMidiEvent &event, qint64 endTick) {
        const QString key = noteKey(event.track, event.channel, event.note);
        auto &stack = active[key];
        if (stack.isEmpty()) return;
        appendNote(stack.takeFirst(), endTick);
    };

    auto releaseDelayedNotes = [&](const QString &lane, qint64 endTick) {
        const QVector<RawMidiEvent> delayed = delayedNoteOffs.take(lane);
        for (const auto &noteOff : delayed) {
            finishNote(noteOff, endTick);
        }
    };

    for (const auto &event : rawEvents) {
        if (event.type == 0xB0 && event.controller == 64) {
            const QString lane = laneKey(event.track, event.channel);
            const bool wasDown = sustainDown.value(lane, false);
            const bool isDown = event.value >= 64;
            sustainDown.insert(lane, isDown);
            if (wasDown && !isDown) {
                releaseDelayedNotes(lane, event.tick);
            }
            continue;
        }

        const QString key = noteKey(event.track, event.channel, event.note);
        if (event.type == 0x90 && event.velocity > 0) {
            active[key].push_back({ event.tick, event.note, event.velocity, event.track, event.channel });
            continue;
        }

        const QString lane = laneKey(event.track, event.channel);
        if (sustainDown.value(lane, false)) {
            delayedNoteOffs[lane].push_back(event);
            continue;
        }

        finishNote(event, event.tick);
    }

    for (auto it = delayedNoteOffs.cbegin(); it != delayedNoteOffs.cend(); ++it) {
        for (const auto &noteOff : it.value()) {
            const qint64 endTick = qMax(noteOff.tick, trackEndTicks.value(noteOff.track, lastTick));
            finishNote(noteOff, endTick);
        }
    }

    for (auto it = active.cbegin(); it != active.cend(); ++it) {
        for (const auto &start : it.value()) {
            const qint64 endTick = qMax(trackEndTicks.value(start.track, lastTick),
                                       start.tick + qint64(result.song.ppq));
            appendNote(start, endTick);
        }
    }

    std::stable_sort(result.song.tempos.begin(), result.song.tempos.end(), [](const TempoEvent &a, const TempoEvent &b) {
        return a.tick < b.tick;
    });
    if (result.song.tempos.isEmpty()) {
        result.song.tempos.push_back({ 0, 500000 });
    } else if (result.song.tempos.first().tick > 0) {
        result.song.tempos.prepend({ 0, 500000 });
    }
    result.song.bpm = qRound(60000000.0 / double(result.song.tempos.first().microsecondsPerQuarter));

    std::sort(result.song.notes.begin(), result.song.notes.end(), [](const NoteEvent &a, const NoteEvent &b) {
        if (a.startTick != b.startTick) return a.startTick < b.startTick;
        return a.midi < b.midi;
    });

    if (result.song.notes.isEmpty()) {
        result.error = QStringLiteral("未在 MIDI 文件中找到有效音符");
        return result;
    }

    result.ok = true;
    return result;
}
