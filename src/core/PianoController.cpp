#include "core/PianoController.h"

#include "core/NoteUtils.h"
#include "library/MidiLibraryService.h"
#include "parser/JsonSheetParser.h"
#include "parser/MidiFileParser.h"
#include "playback/PlaybackClock.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QtMath>
#include <QUrl>
#include <QVariantMap>
#include <algorithm>

namespace {

qint64 beatsToTicks(double beats, int ppq)
{
    return qRound64(beats * double(ppq));
}

}

PianoController::PianoController(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PianoController::onFrame);
    m_localMidiLibraryPath = MidiLibraryService::resolveLibraryPath();
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
    if (m_mode != QStringLiteral("practice")) {
        return list;
    }

    const qint64 tick = m_practice.expectedTick();
    if (tick < 0) return list;

    for (const auto &note : m_notes) {
        if (note.startTick == tick) list.push_back(noteToVariant(note));
    }
    return list;
}

qint64 PianoController::expectedTickValue() const
{
    return m_mode == QStringLiteral("practice") ? m_practice.expectedTick() : -1;
}

void PianoController::setPlaybackSpeed(int speed)
{
    const int clamped = qBound(50, speed, 150);
    if (m_playbackSpeed == clamped) return;

    m_playbackSpeed = clamped;
    m_playbackRate = double(m_playbackSpeed) / 100.0;
    m_playbackMsRemainder = 0.0;
    emit playbackSpeedChanged();
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

    m_preciseTick = double(m_currentTick);
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
    m_preciseTick = 0.0;
    m_playbackMsRemainder = 0.0;
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
    m_preciseTick = double(m_currentTick);
    m_playbackMsRemainder = 0.0;

    resetPracticeState(false, false);
    if (m_mode == QStringLiteral("practice")) preparePracticeAtCurrentPosition();
    for (auto &note : m_notes) {
        note.played = note.startTick < m_currentTick;
    }

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
    Song song;
    song.title = QStringLiteral("小星星 Demo");
    song.bpm = 100;
    song.ppq = DefaultPpq;
    song.tempos = PlaybackClock::tempoMapFromBpm(song.bpm);

    int id = 1;

    auto add = [&](int midi, double startBeat, double durationBeat, int finger) {
        NoteEvent note;
        note.id = id++;
        note.midi = midi;
        note.velocity = 112;
        note.startTick = beatsToTicks(startBeat, DefaultPpq);
        note.durationTick = beatsToTicks(durationBeat, DefaultPpq);
        note.fingering = finger;
        note.noteName = NoteUtils::midiToName(midi);
        song.notes.push_back(note);
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

    setSong(std::move(song));
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
    m_localMidiFiles = MidiLibraryService::scanLibrary(m_localMidiLibraryPath);
    emit localMidiLibraryChanged();
    setStatusMessage(QStringLiteral("本地 MIDI 库已刷新：%1 首").arg(m_localMidiFiles.size()));
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
    MidiLibraryService::openLibrary(m_localMidiLibraryPath);
}

void PianoController::onFrame()
{
    if (!m_playing) return;
    const qint64 elapsedMs = m_frameClock.restart();
    const qint64 previousTick = m_currentTick;

    if (m_mode == QStringLiteral("auto")) {
        const double scaledElapsedMs = double(elapsedMs) * m_playbackRate + m_playbackMsRemainder;
        const qint64 playbackElapsedMs = qMax<qint64>(0, qint64(scaledElapsedMs));
        m_playbackMsRemainder = scaledElapsedMs - double(playbackElapsedMs);

        m_preciseTick = PlaybackClock::advance(m_preciseTick, playbackElapsedMs, m_tempos, m_ppq);
        m_preciseTick = qBound(0.0, m_preciseTick, double(m_totalTicks));
        m_currentTick = qBound<qint64>(0, qRound64(m_preciseTick), m_totalTicks);
        if (m_currentTick >= m_totalTicks) {
            m_currentTick = m_totalTicks;
            m_preciseTick = double(m_totalTicks);
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

void PianoController::setSong(Song song)
{
    setPlaying(false);
    m_pressedNotes.clear();
    m_synth.stopAll();

    std::sort(song.notes.begin(), song.notes.end(), [](const NoteEvent &a, const NoteEvent &b) {
        if (a.startTick != b.startTick) return a.startTick < b.startTick;
        return a.midi < b.midi;
    });

    m_songTitle = song.title;
    m_bpm = qBound(20, song.bpm, 260);
    m_ppq = song.ppq > 0 ? song.ppq : DefaultPpq;
    m_tempos = PlaybackClock::normalizedTempoMap(std::move(song.tempos), m_bpm);
    m_notes = std::move(song.notes);
    m_currentTick = 0;
    m_preciseTick = 0.0;
    m_playbackMsRemainder = 0.0;
    m_totalTicks = m_ppq * 8;
    m_maxNoteDurationTick = m_ppq * 2;
    for (const auto &note : m_notes) {
        m_totalTicks = qMax(m_totalTicks, note.startTick + note.durationTick);
        m_maxNoteDurationTick = qMax(m_maxNoteDurationTick, note.durationTick);
    }

    m_practice.setSong(m_notes);
    resetPracticeState(true);
    refreshActiveNotes();

    emit songChanged();
    emit bpmChanged();
    emit notesChanged();
    emit positionChanged();
    emit practiceChanged();
}

void PianoController::loadJsonSheet(const QString &path)
{
    ParsedJsonSheet parsed = JsonSheetParser::parseFile(path);
    if (!parsed.ok) {
        setStatusMessage(parsed.error);
        return;
    }

    const QString title = parsed.song.title;
    setSong(std::move(parsed.song));
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

    if (parsed.song.title.isEmpty()) {
        parsed.song.title = info.completeBaseName();
    }
    const QString title = parsed.song.title;
    setSong(std::move(parsed.song));
    setStatusMessage(QStringLiteral("已加载 MIDI：%1").arg(title));
}

void PianoController::preparePracticeAtCurrentPosition()
{
    if (!m_practice.seek(m_currentTick)) return;

    m_currentTick = m_practice.expectedTick();
    m_preciseTick = double(m_currentTick);
    emit positionChanged();
    emit practiceChanged();
}

void PianoController::evaluatePracticeNote(int midi)
{
    const PracticeNoteResult result = m_practice.noteOn(midi);
    if (result.type == PracticeJudgeType::Ignored) return;

    if (result.type == PracticeJudgeType::WrongNote) {
        setStatusMessage(QStringLiteral("错音：%1").arg(NoteUtils::midiToName(midi)));
        emit statsChanged();
        return;
    }

    if (result.statsChanged) {
        emit statsChanged();
    }

    if (result.stepComplete) {
        for (auto &note : m_notes) {
            if (note.startTick == result.completedTick) note.played = true;
        }

        if (result.songComplete) {
            m_currentTick = m_totalTicks;
            m_preciseTick = double(m_currentTick);
            setPlaying(false);
            setStatusMessage(QStringLiteral("练习完成"));
        } else {
            m_currentTick = result.nextTick;
            m_preciseTick = double(m_currentTick);
            setStatusMessage(QStringLiteral("正确，继续"));
        }
        emit positionChanged();
    } else {
        setStatusMessage(QStringLiteral("很好，还差当前和弦里的其他音"));
    }

    emit notesChanged();
    emit practiceChanged();
}

void PianoController::resetPracticeState(bool resetStats, bool resetPlayed)
{
    m_practice.reset(resetStats);
    if (resetPlayed) {
        for (auto &note : m_notes) note.played = false;
    }
    if (resetStats) {
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
        m_playbackMsRemainder = 0.0;
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
