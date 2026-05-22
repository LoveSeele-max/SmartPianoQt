#include "core/PianoController.h"

#include "core/NoteUtils.h"
#include "parser/MidiFileParser.h"

#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtMath>
#include <QUrl>
#include <QVariantMap>
#include <algorithm>

PianoController::PianoController(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PianoController::onFrame);
    m_localMidiLibraryPath = resolveLocalMidiLibraryPath();
    refreshLocalMidiLibrary();
    loadDemoSong();
}

double PianoController::currentBeat() const
{
    return tickToBeat(m_currentTick);
}

double PianoController::totalBeats() const
{
    return tickToBeat(m_totalTicks);
}

QVariantList PianoController::notes() const
{
    QVariantList list;
    list.reserve(m_notes.size());
    for (const auto &note : m_notes) {
        list.push_back(noteToVariant(note));
    }
    return list;
}

QVariantList PianoController::activeNotes() const
{
    QList<int> values = m_activeNotes.values();
    std::sort(values.begin(), values.end());

    QVariantList list;
    list.reserve(values.size());
    for (int midi : values) list.push_back(midi);
    return list;
}

QVariantList PianoController::expectedNotes() const
{
    QVariantList list;
    if (m_mode != QStringLiteral("practice") ||
        m_waitTickIndex < 0 ||
        m_waitTickIndex >= m_practiceTicks.size()) {
        return list;
    }

    const qint64 tick = m_practiceTicks.at(m_waitTickIndex);
    for (const auto &note : m_notes) {
        if (note.startTick == tick) list.push_back(noteToVariant(note));
    }
    return list;
}

qint64 PianoController::expectedTickValue() const
{
    if (m_mode != QStringLiteral("practice") ||
        m_waitTickIndex < 0 ||
        m_waitTickIndex >= m_practiceTicks.size()) {
        return -1;
    }
    return m_practiceTicks.at(m_waitTickIndex);
}

void PianoController::setBpm(int bpm)
{
    const int clamped = qBound(20, bpm, 260);
    if (m_bpm == clamped) return;
    m_bpm = clamped;
    emit bpmChanged();
}

void PianoController::setMode(const QString &mode)
{
    const QString normalized = mode == QStringLiteral("practice") ? QStringLiteral("practice")
                                                                  : QStringLiteral("auto");
    if (m_mode == normalized) return;

    m_mode = normalized;
    resetPracticeState(false);
    if (m_mode == QStringLiteral("practice")) {
        preparePracticeAtCurrentPosition();
        setStatusMessage(QStringLiteral("练习模式：按下当前高亮音符后继续"));
    } else {
        setStatusMessage(QStringLiteral("自动播放模式就绪"));
    }

    refreshActiveNotes();
    emit modeChanged();
    emit practiceChanged();
    emit notesChanged();
}

void PianoController::setVolume(int volume)
{
    const int clamped = qBound(0, volume, 127);
    if (m_synth.volume() == clamped) return;
    m_synth.setVolume(clamped);
    emit volumeChanged();
}

void PianoController::playPause()
{
    if (m_playing) {
        setPlaying(false);
        setStatusMessage(QStringLiteral("已暂停"));
        return;
    }

    if (m_notes.isEmpty()) {
        setStatusMessage(QStringLiteral("请先加载曲谱"));
        return;
    }

    if (m_currentTick >= m_totalTicks) {
        seekBeat(0);
    }

    if (m_mode == QStringLiteral("practice")) {
        preparePracticeAtCurrentPosition();
    }

    m_frameClock.restart();
    setPlaying(true);
    setStatusMessage(m_mode == QStringLiteral("practice")
                         ? QStringLiteral("练习中：等待正确音符")
                         : QStringLiteral("播放中"));
}

void PianoController::stop()
{
    setPlaying(false);
    m_pressedNotes.clear();
    m_synth.stopAll();
    m_currentTick = 0;
    resetPracticeState(true);
    refreshActiveNotes();
    setStatusMessage(QStringLiteral("已回到曲首"));
    emit positionChanged();
    emit notesChanged();
    emit practiceChanged();
}

void PianoController::seekBeat(double beat)
{
    m_currentTick = beatToTick(beat);
    clampPosition();

    for (auto &note : m_notes) {
        note.played = note.startTick < m_currentTick;
    }
    resetPracticeState(false);
    if (m_mode == QStringLiteral("practice")) preparePracticeAtCurrentPosition();

    refreshActiveNotes();
    emit positionChanged();
    emit notesChanged();
    emit practiceChanged();
}

void PianoController::noteOn(int midi)
{
    if (midi < 0 || midi > 127) return;
    const bool inserted = !m_pressedNotes.contains(midi);
    m_pressedNotes.insert(midi);

    if (m_mode == QStringLiteral("practice") && m_playing && inserted) {
        evaluatePracticeNote(midi);
    }

    refreshActiveNotes();
}

void PianoController::noteOff(int midi)
{
    m_pressedNotes.remove(midi);
    refreshActiveNotes();
}

void PianoController::loadDemoSong()
{
    QVector<NoteEvent> demo;
    int id = 1;

    auto add = [&](int midi, double startBeat, double durationBeat, int finger) {
        NoteEvent note;
        note.id = id++;
        note.midi = midi;
        note.velocity = 112;
        note.startTick = beatToTick(startBeat);
        note.durationTick = beatToTick(durationBeat);
        note.fingering = finger;
        note.noteName = NoteUtils::midiToName(midi);
        demo.push_back(note);
    };

    add(60, 0.0, 1.0, 1);
    add(60, 1.0, 1.0, 1);
    add(67, 2.0, 1.0, 5);
    add(67, 3.0, 1.0, 5);
    add(69, 4.0, 1.0, 5);
    add(69, 5.0, 1.0, 5);
    add(67, 6.0, 2.0, 4);
    add(65, 8.0, 1.0, 4);
    add(65, 9.0, 1.0, 4);
    add(64, 10.0, 1.0, 3);
    add(64, 11.0, 1.0, 3);
    add(62, 12.0, 1.0, 2);
    add(62, 13.0, 1.0, 2);
    add(60, 14.0, 2.0, 1);

    setSong(QStringLiteral("小星星 Demo"), 100, DefaultPpq, demo);
    setStatusMessage(QStringLiteral("已加载内置示例曲"));
}

void PianoController::loadSheet(const QUrl &url)
{
    const QString path = url.toLocalFile();
    if (path.isEmpty()) {
        setStatusMessage(QStringLiteral("文件路径无效"));
        return;
    }

    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();

    try {
        if (suffix == QStringLiteral("json")) {
            loadJsonSheet(path);
            return;
        }

        if (suffix == QStringLiteral("mid") || suffix == QStringLiteral("midi")) {
            loadMidiFile(path);
            return;
        }

        setStatusMessage(QStringLiteral("初版先支持 JSON / MIDI，MusicXML 和 MXL 下一步接入"));
    } catch (...) {
        setStatusMessage(QStringLiteral("导入失败，请检查文件格式"));
    }
}

void PianoController::refreshLocalMidiLibrary()
{
    QDir dir(m_localMidiLibraryPath);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    const QFileInfoList files = dir.entryInfoList(
        { QStringLiteral("*.mid"), QStringLiteral("*.midi") },
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    QVariantList entries;
    entries.reserve(files.size());
    for (const QFileInfo &file : files) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), file.completeBaseName());
        item.insert(QStringLiteral("fileName"), file.fileName());
        item.insert(QStringLiteral("path"), file.absoluteFilePath());
        item.insert(QStringLiteral("sizeKb"), qMax<qint64>(1, file.size() / 1024));
        entries.push_back(item);
    }

    m_localMidiFiles = entries;
    emit localMidiLibraryChanged();
    setStatusMessage(QStringLiteral("本地 MIDI 库已刷新：%1 首").arg(entries.size()));
}

void PianoController::loadLocalMidi(int index)
{
    if (index < 0 || index >= m_localMidiFiles.size()) {
        setStatusMessage(QStringLiteral("请选择本地 MIDI 曲谱"));
        return;
    }

    const QVariantMap item = m_localMidiFiles.at(index).toMap();
    loadMidiFile(item.value(QStringLiteral("path")).toString());
}

void PianoController::openLocalMidiLibrary()
{
    QDir dir(m_localMidiLibraryPath);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
}

void PianoController::onFrame()
{
    if (!m_playing) return;
    const qint64 elapsedMs = m_frameClock.restart();
    const qint64 previousTick = m_currentTick;

    if (m_mode == QStringLiteral("auto")) {
        m_currentTick += msToTicks(elapsedMs);
        if (m_currentTick >= m_totalTicks) {
            m_currentTick = m_totalTicks;
            retriggerAutoNoteStarts(previousTick, m_currentTick);
            setPlaying(false);
            setStatusMessage(QStringLiteral("播放完成"));
            emit positionChanged();
        } else {
            retriggerAutoNoteStarts(previousTick, m_currentTick);
            m_positionNotifyAccumulatorMs += elapsedMs;
            if (m_positionNotifyAccumulatorMs >= 80) {
                m_positionNotifyAccumulatorMs = 0;
                emit positionChanged();
            }
        }
        emit frameChanged();
    }

    refreshActiveNotes();
}

QVariantMap PianoController::noteToVariant(const NoteEvent &note) const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), note.id);
    map.insert(QStringLiteral("midi"), note.midi);
    map.insert(QStringLiteral("note"), note.noteName);
    map.insert(QStringLiteral("velocity"), note.velocity);
    map.insert(QStringLiteral("startBeat"), tickToBeat(note.startTick));
    map.insert(QStringLiteral("durationBeat"), tickToBeat(note.durationTick));
    map.insert(QStringLiteral("fingering"), note.fingering);
    map.insert(QStringLiteral("track"), note.track);
    map.insert(QStringLiteral("channel"), note.channel);
    map.insert(QStringLiteral("played"), note.played);
    return map;
}

void PianoController::setSong(const QString &title, int bpm, int ppq, QVector<NoteEvent> notes)
{
    setPlaying(false);
    m_pressedNotes.clear();
    m_synth.stopAll();

    std::sort(notes.begin(), notes.end(), [](const NoteEvent &a, const NoteEvent &b) {
        if (a.startTick != b.startTick) return a.startTick < b.startTick;
        return a.midi < b.midi;
    });

    m_songTitle = title;
    m_bpm = qBound(20, bpm, 260);
    m_ppq = ppq > 0 ? ppq : DefaultPpq;
    m_notes = std::move(notes);
    m_currentTick = 0;
    m_totalTicks = DefaultPpq * 8;
    m_maxNoteDurationTick = DefaultPpq * 2;
    for (const auto &note : m_notes) {
        m_totalTicks = qMax(m_totalTicks, note.startTick + note.durationTick);
        m_maxNoteDurationTick = qMax(m_maxNoteDurationTick, note.durationTick);
    }

    resetPracticeState(true);
    rebuildPracticeTicks();
    refreshActiveNotes();

    emit songChanged();
    emit bpmChanged();
    emit notesChanged();
    emit positionChanged();
    emit practiceChanged();
}

void PianoController::loadJsonSheet(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatusMessage(QStringLiteral("无法读取 JSON 文件"));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setStatusMessage(QStringLiteral("JSON 曲谱格式无效"));
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray data = root.value(QStringLiteral("data")).toArray();
    if (data.isEmpty()) {
        setStatusMessage(QStringLiteral("JSON 曲谱没有 data 音符数组"));
        return;
    }

    const int bpm = root.value(QStringLiteral("bpm")).toInt(100);
    const QString title = root.value(QStringLiteral("name")).toString(QFileInfo(path).completeBaseName());
    QVector<NoteEvent> parsed;
    parsed.reserve(data.size());

    double cursorBeat = 0.0;
    int id = 1;
    for (const auto &value : data) {
        const QJsonObject item = value.toObject();
        int midi = item.value(QStringLiteral("midi")).toInt(-1);
        if (midi < 0) {
            midi = NoteUtils::noteNameToMidi(item.value(QStringLiteral("note")).toString());
        }
        if (midi < 0) continue;

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
        note.startTick = beatToTick(startBeat);
        note.durationTick = qMax<qint64>(DefaultPpq / 8, beatToTick(durationBeat));
        note.fingering = item.value(QStringLiteral("fingering")).toInt(0);
        note.noteName = NoteUtils::midiToName(midi);
        parsed.push_back(note);

        if (!item.contains(QStringLiteral("startTimeBeat"))) {
            cursorBeat += durationBeat;
        }
    }

    if (parsed.isEmpty()) {
        setStatusMessage(QStringLiteral("JSON 曲谱中没有可用音符"));
        return;
    }

    setSong(title, bpm, DefaultPpq, parsed);
    setStatusMessage(QStringLiteral("已导入 JSON：%1").arg(title));
}

void PianoController::loadMidiFile(const QString &path)
{
    const QFileInfo info(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatusMessage(QStringLiteral("无法读取 MIDI 文件"));
        return;
    }

    ParsedMidi parsed = MidiFileParser::parse(file.readAll());
    if (!parsed.ok) {
        setStatusMessage(parsed.error);
        return;
    }

    const QString title = parsed.title.isEmpty() ? info.completeBaseName() : parsed.title;
    setSong(title, parsed.bpm, parsed.ppq, parsed.notes);
    setStatusMessage(QStringLiteral("已加载 MIDI：%1").arg(title));
}

QString PianoController::resolveLocalMidiLibraryPath() const
{
    auto findFrom = [](QDir dir) -> QString {
        for (int i = 0; i < 6; ++i) {
            const QString candidate = dir.absoluteFilePath(QStringLiteral("midi_library"));
            if (QDir(candidate).exists()) return QDir(candidate).absolutePath();
            if (!dir.cdUp()) break;
        }
        return {};
    };

    QString path = findFrom(QDir::current());
    if (!path.isEmpty()) return path;

    path = findFrom(QDir(QCoreApplication::applicationDirPath()));
    if (!path.isEmpty()) return path;

    QDir dir(QDir::current());
    const QString created = dir.absoluteFilePath(QStringLiteral("midi_library"));
    dir.mkpath(QStringLiteral("midi_library"));
    return QDir(created).absolutePath();
}

void PianoController::rebuildPracticeTicks()
{
    m_practiceTicks.clear();
    qint64 last = -1;
    for (const auto &note : m_notes) {
        if (note.startTick != last) {
            m_practiceTicks.push_back(note.startTick);
            last = note.startTick;
        }
    }
    m_waitTickIndex = 0;
}

void PianoController::preparePracticeAtCurrentPosition()
{
    if (m_practiceTicks.isEmpty()) return;

    auto it = std::lower_bound(m_practiceTicks.begin(), m_practiceTicks.end(), m_currentTick);
    if (it == m_practiceTicks.end()) {
        m_waitTickIndex = m_practiceTicks.size() - 1;
    } else {
        m_waitTickIndex = int(std::distance(m_practiceTicks.begin(), it));
    }

    m_currentTick = m_practiceTicks.at(m_waitTickIndex);
    m_matchedPracticeNotes.clear();
    emit positionChanged();
    emit practiceChanged();
}

void PianoController::evaluatePracticeNote(int midi)
{
    if (m_waitTickIndex < 0 || m_waitTickIndex >= m_practiceTicks.size()) return;

    const qint64 tick = m_practiceTicks.at(m_waitTickIndex);
    QSet<int> expected;
    for (const auto &note : m_notes) {
        if (note.startTick == tick) expected.insert(note.midi);
    }

    if (!expected.contains(midi)) {
        ++m_wrongCount;
        setStatusMessage(QStringLiteral("错音：%1").arg(NoteUtils::midiToName(midi)));
        emit statsChanged();
        return;
    }

    if (!m_matchedPracticeNotes.contains(midi)) {
        m_matchedPracticeNotes.insert(midi);
        ++m_correctCount;
        emit statsChanged();
    }

    if (m_matchedPracticeNotes.size() >= expected.size()) {
        for (auto &note : m_notes) {
            if (note.startTick == tick) note.played = true;
        }
        advancePracticeTick();
    } else {
        setStatusMessage(QStringLiteral("很好，还差当前和弦里的其他音"));
    }

    emit notesChanged();
    emit practiceChanged();
}

void PianoController::advancePracticeTick()
{
    m_matchedPracticeNotes.clear();
    ++m_waitTickIndex;

    if (m_waitTickIndex >= m_practiceTicks.size()) {
        m_currentTick = m_totalTicks;
        setPlaying(false);
        setStatusMessage(QStringLiteral("练习完成"));
        emit positionChanged();
        return;
    }

    m_currentTick = m_practiceTicks.at(m_waitTickIndex);
    setStatusMessage(QStringLiteral("正确，继续"));
    emit positionChanged();
}

void PianoController::resetPracticeState(bool resetStats)
{
    m_matchedPracticeNotes.clear();
    m_waitTickIndex = 0;
    for (auto &note : m_notes) note.played = false;
    if (resetStats) {
        m_correctCount = 0;
        m_wrongCount = 0;
        m_missedCount = 0;
        emit statsChanged();
    }
}

void PianoController::refreshActiveNotes()
{
    QSet<int> autoNotes;
    if (m_mode == QStringLiteral("auto") && m_playing) {
        const qint64 searchStartTick = qMax<qint64>(0, m_currentTick - m_maxNoteDurationTick - 1);
        const auto first = std::lower_bound(m_notes.begin(), m_notes.end(), searchStartTick,
            [](const NoteEvent &note, qint64 tick) {
                return note.startTick < tick;
            });

        for (auto it = first; it != m_notes.end(); ++it) {
            const NoteEvent &note = *it;
            if (note.startTick > m_currentTick) break;
            if (m_currentTick >= note.startTick &&
                m_currentTick <= note.startTick + note.durationTick) {
                autoNotes.insert(note.midi);
            }
        }
    }
    m_autoNotes = autoNotes;

    QSet<int> combined = m_pressedNotes;
    for (int midi : m_autoNotes) combined.insert(midi);

    if (combined == m_activeNotes) return;
    const QSet<int> previous = m_activeNotes;

    for (int midi : previous) {
        if (!combined.contains(midi)) {
            m_synth.noteOff(midi);
        }
    }
    for (int midi : combined) {
        if (!previous.contains(midi)) {
            m_synth.noteOn(midi, velocityForMidi(midi));
        }
    }

    m_activeNotes = combined;
    emit activeNotesChanged();
}

void PianoController::setPlaying(bool playing)
{
    if (m_playing == playing) return;
    m_playing = playing;
    if (m_playing) {
        m_timer.start();
        m_frameClock.restart();
        m_positionNotifyAccumulatorMs = 0;
    } else {
        m_timer.stop();
    }
    refreshActiveNotes();
    emit playbackStateChanged();
}

void PianoController::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void PianoController::clampPosition()
{
    m_currentTick = qBound<qint64>(0, m_currentTick, m_totalTicks);
}

void PianoController::retriggerAutoNoteStarts(qint64 previousTick, qint64 currentTick)
{
    if (currentTick <= previousTick || m_autoNotes.isEmpty()) return;

    const auto first = std::upper_bound(m_notes.begin(), m_notes.end(), previousTick,
        [](qint64 tick, const NoteEvent &note) {
            return tick < note.startTick;
        });

    for (auto it = first; it != m_notes.end(); ++it) {
        const NoteEvent &note = *it;
        if (note.startTick > currentTick) break;
        if (m_autoNotes.contains(note.midi)) {
            m_synth.noteOn(note.midi, qBound(1, note.velocity, 127));
        }
    }
}

qint64 PianoController::beatToTick(double beat) const
{
    return qRound64(beat * m_ppq);
}

double PianoController::tickToBeat(qint64 tick) const
{
    return m_ppq > 0 ? double(tick) / double(m_ppq) : 0.0;
}

qint64 PianoController::msToTicks(qint64 elapsedMs) const
{
    const double beats = (double(elapsedMs) / 1000.0) * (double(m_bpm) / 60.0);
    return qMax<qint64>(1, qRound64(beats * m_ppq));
}

int PianoController::velocityForMidi(int midi) const
{
    if (m_autoNotes.contains(midi)) {
        const qint64 searchStartTick = qMax<qint64>(0, m_currentTick - m_maxNoteDurationTick - 1);
        const auto first = std::lower_bound(m_notes.begin(), m_notes.end(), searchStartTick,
            [](const NoteEvent &note, qint64 tick) {
                return note.startTick < tick;
            });

        for (auto it = first; it != m_notes.end(); ++it) {
            const NoteEvent &note = *it;
            if (note.startTick > m_currentTick) break;
            if (note.midi == midi &&
                m_currentTick >= note.startTick &&
                m_currentTick <= note.startTick + note.durationTick) {
                return qBound(1, note.velocity, 127);
            }
        }
    }
    return 112;
}
