#include "parser/JsonSheetParser.h"

#include "core/NoteUtils.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

namespace {

constexpr int JsonPpq = 480;

qint64 beatsToTicks(double beats, int ppq)
{
    return qRound64(beats * double(ppq));
}

QVector<TempoEvent> tempoMapFromBpm(int bpm)
{
    const int clamped = qBound(20, bpm, 260);
    return { { 0, qRound(60000000.0 / double(clamped)) } };
}

}

ParsedJsonSheet JsonSheetParser::parse(const QByteArray &bytes, const QString &fallbackTitle)
{
    ParsedJsonSheet result;
    result.song.ppq = JsonPpq;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.error = QStringLiteral("JSON 曲谱格式无效");
        return result;
    }

    const QJsonObject root = doc.object();
    const QJsonArray data = root.value(QStringLiteral("data")).toArray();
    if (data.isEmpty()) {
        result.error = QStringLiteral("JSON 曲谱没有 data 音符数组");
        return result;
    }

    result.song.bpm = root.value(QStringLiteral("bpm")).toInt(100);
    result.song.title = root.value(QStringLiteral("name")).toString(fallbackTitle);
    result.song.tempos = tempoMapFromBpm(result.song.bpm);
    result.song.notes.reserve(data.size());

    double cursorBeat = 0.0;
    int id = 1;
    for (const auto &value : data) {
        const QJsonObject item = value.toObject();
        int midi = item.value(QStringLiteral("midi")).toInt(-1);
        if (midi < 0) {
            midi = NoteUtils::noteNameToMidi(item.value(QStringLiteral("note")).toString());
        }
        if (midi < 0 || midi > 127) continue;

        const double startBeat = item.contains(QStringLiteral("startTimeBeat"))
                                     ? item.value(QStringLiteral("startTimeBeat")).toDouble()
                                     : cursorBeat;
        const double durationBeat = item.contains(QStringLiteral("durationBeat"))
                                        ? item.value(QStringLiteral("durationBeat")).toDouble(1.0)
                                        : item.value(QStringLiteral("duration")).toDouble(1.0);

        NoteEvent note;
        note.id = id++;
        note.midi = midi;
        note.velocity = item.value(QStringLiteral("velocity")).toInt(84);
        note.startTick = beatsToTicks(startBeat, JsonPpq);
        note.durationTick = qMax<qint64>(JsonPpq / 8, beatsToTicks(durationBeat, JsonPpq));
        note.fingering = item.value(QStringLiteral("fingering")).toInt(0);
        note.noteName = NoteUtils::midiToName(midi);
        result.song.notes.push_back(note);

        if (!item.contains(QStringLiteral("startTimeBeat"))) {
            cursorBeat += durationBeat;
        }
    }

    if (result.song.notes.isEmpty()) {
        result.error = QStringLiteral("JSON 曲谱中没有可用音符");
        return result;
    }

    result.ok = true;
    return result;
}

ParsedJsonSheet JsonSheetParser::parseFile(const QString &path)
{
    ParsedJsonSheet result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QStringLiteral("无法读取 JSON 文件");
        return result;
    }

    const QString fallbackTitle = QFileInfo(path).completeBaseName();
    return parse(file.readAll(), fallbackTitle);
}
