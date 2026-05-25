#include "core/PianoController.h"

#include "core/NoteUtils.h"
#include "library/MidiLibraryService.h"
#include "parser/JsonSheetParser.h"
#include "parser/MidiFileParser.h"
#include "playback/PlaybackClock.h"

#include <QDateTime>
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

bool isPracticeModeName(const QString &mode)
{
    return mode == QStringLiteral("practice") || mode == QStringLiteral("rhythm");
}

}

PianoController::PianoController(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PianoController::onFrame);
    m_localMidiLibraryPath = MidiLibraryService::resolveLibraryPath();
    m_recordStore.open();
    refreshLocalMidiLibrary();
    loadDemoSong();
}

PianoController::~PianoController()
{
    finishPracticeSession(false);
}

double PianoController::currentBeat() const
{
    return m_playbackEngine.currentBeat();
}

double PianoController::totalBeats() const
{
    return m_playbackEngine.totalBeats();
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
    if (!isPracticeMode()) {
        return list;
    }

    const QVector<NoteEvent> expected = m_practice.expectedNotes();
    list.reserve(expected.size());
    for (const auto &note : expected) {
        list.push_back(noteToVariant(note));
    }
    return list;
}

qint64 PianoController::expectedTickValue() const
{
    return isPracticeMode() ? m_practice.expectedTick() : -1;
}

void PianoController::setPlaybackSpeed(int speed)
{
    if (!m_playbackEngine.setPlaybackSpeed(speed)) return;
    emit playbackSpeedChanged();
}

void PianoController::setMode(const QString &mode)
{
    const QString normalized = mode == QStringLiteral("practice") ? QStringLiteral("practice")
                                  : mode == QStringLiteral("rhythm") ? QStringLiteral("rhythm")
                                                                     : QStringLiteral("auto");
    if (m_mode == normalized) return;

    finishPracticeSession(false);
    m_mode = normalized;
    resetPracticeState(false);
    if (isPracticeMode()) {
        preparePracticeAtCurrentPosition();
        setStatusMessage(isRhythmPracticeMode()
                             ? QStringLiteral("节奏练习：跟随落点按下音符")
                             : QStringLiteral("练习模式：按下当前高亮音符后继续"));
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

    if (m_playbackEngine.currentTick() >= m_playbackEngine.totalTicks()) {
        seekBeat(0);
    }

    if (isPracticeMode()) {
        preparePracticeAtCurrentPosition();
        beginPracticeSession();
    }

    m_frameClock.restart();
    setPlaying(true);
    setStatusMessage(isPracticeMode()
                         ? (isRhythmPracticeMode()
                                ? QStringLiteral("节奏练习中：跟随落点演奏")
                                : QStringLiteral("练习中：等待正确音符"))
                         : QStringLiteral("播放中"));
}

void PianoController::stop()
{
    finishPracticeSession(false);
    setPlaying(false);
    m_pressedNotes.clear();
    m_synth.stopAll();
    m_playbackEngine.stop();
    resetPracticeState(true);
    refreshActiveNotes();
    setStatusMessage(QStringLiteral("已回到曲首"));
    emit positionChanged();
    emit notesChanged();
    emit practiceChanged();
}

void PianoController::seekBeat(double beat)
{
    const bool restartSession = m_playing && isPracticeMode();
    finishPracticeSession(false);
    m_playbackEngine.seekTick(beatToTick(beat));

    resetPracticeState(false, false);
    if (isPracticeMode()) preparePracticeAtCurrentPosition();
    if (restartSession) beginPracticeSession();
    for (auto &note : m_notes) {
        note.played = note.startTick < m_playbackEngine.currentTick();
    }

    refreshActiveNotes();
    emit positionChanged();
    emit notesChanged();
    emit practiceChanged();
}

void PianoController::noteOn(int midi, int velocity)
{
    if (midi < 0 || midi > 127) return;
    const int clampedVelocity = qBound(1, velocity, 127);
    const bool inserted = !m_pressedNotes.contains(midi);
    m_pressedNotes.insert(midi, clampedVelocity);

    if (isPracticeMode() && m_playing && inserted) {
        evaluatePracticeNote(midi, clampedVelocity);
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

    setSong(std::move(song), QString(), QStringLiteral("demo"));
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

    if (m_mode == QStringLiteral("auto") || isRhythmPracticeMode()) {
        const PlaybackAdvanceResult result = m_playbackEngine.advance(elapsedMs);
        if (isRhythmPracticeMode()) {
            handleRhythmMisses(result.currentTick);
        }

        if (result.reachedEnd) {
            if (m_mode == QStringLiteral("auto")) {
                retriggerAutoNoteStarts(result.previousTick, result.currentTick);
            } else {
                handleRhythmMisses(result.currentTick + rhythmToleranceTick() + 1);
                finishPracticeSession(true);
            }
            setPlaying(false);
            setStatusMessage(isRhythmPracticeMode()
                                 ? QStringLiteral("节奏练习完成")
                                 : QStringLiteral("播放完成"));
            emit positionChanged();
        } else {
            if (m_mode == QStringLiteral("auto")) {
                retriggerAutoNoteStarts(result.previousTick, result.currentTick);
            }
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

void PianoController::setSong(Song song, const QString &sourcePath, const QString &sourceFormat)
{
    finishPracticeSession(false);
    setPlaying(false);
    m_pressedNotes.clear();
    m_synth.stopAll();

    std::sort(song.notes.begin(), song.notes.end(), [](const NoteEvent &a, const NoteEvent &b) {
        if (a.startTick != b.startTick) return a.startTick < b.startTick;
        return a.midi < b.midi;
    });

    m_songTitle = song.title;
    m_playbackEngine.setSong(song);
    m_currentSheetId = m_recordStore.upsertSheet(song, sourcePath, sourceFormat);
    m_notes = std::move(song.notes);

    m_practice.setSong(m_notes);
    resetPracticeState(true);
    refreshActiveNotes();

    emit songChanged();
    emit bpmChanged();
    emit notesChanged();
    emit positionChanged();
    emit practiceChanged();
    refreshPracticeReport();
}

void PianoController::loadJsonSheet(const QString &path)
{
    ParsedJsonSheet parsed = JsonSheetParser::parseFile(path);
    if (!parsed.ok) {
        setStatusMessage(parsed.error);
        return;
    }

    const QString title = parsed.song.title;
    setSong(std::move(parsed.song), path, QStringLiteral("json"));
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
    setSong(std::move(parsed.song), path, QStringLiteral("midi"));
    setStatusMessage(QStringLiteral("已加载 MIDI：%1").arg(title));
}

void PianoController::preparePracticeAtCurrentPosition()
{
    if (!m_practice.seek(m_playbackEngine.currentTick())) return;

    const qint64 expectedTick = m_practice.expectedTick();
    if (expectedTick < 0) {
        emit practiceChanged();
        return;
    }

    m_playbackEngine.seekTick(expectedTick);
    emit positionChanged();
    emit practiceChanged();
}

void PianoController::evaluatePracticeNote(int midi, int velocity)
{
    const PracticeNoteResult result = isRhythmPracticeMode()
        ? m_practice.noteOnRhythm(midi, velocity, m_playbackEngine.currentTick(), rhythmToleranceTick())
        : m_practice.noteOn(midi, velocity);
    if (result.type == PracticeJudgeType::Ignored) return;

    appendPracticeEvent(result);

    if (result.type == PracticeJudgeType::WrongNote) {
        setStatusMessage(QStringLiteral("错音：%1").arg(NoteUtils::midiToName(midi)));
        emit statsChanged();
        return;
    }

    if (result.type == PracticeJudgeType::Early) {
        setStatusMessage(QStringLiteral("太早：%1").arg(NoteUtils::midiToName(midi)));
        if (result.statsChanged) emit statsChanged();
        return;
    }

    if (result.type == PracticeJudgeType::Late) {
        setStatusMessage(QStringLiteral("稍晚：%1").arg(NoteUtils::midiToName(midi)));
    } else if (result.type == PracticeJudgeType::RepeatedNote) {
        setStatusMessage(QStringLiteral("重复音：%1").arg(NoteUtils::midiToName(midi)));
    }

    if (result.statsChanged) {
        emit statsChanged();
    }

    if (result.stepComplete) {
        for (auto &note : m_notes) {
            if (note.startTick == result.completedTick) note.played = true;
        }

        if (result.songComplete) {
            m_playbackEngine.seekTick(m_playbackEngine.totalTicks());
            setPlaying(false);
            finishPracticeSession(true);
            setStatusMessage(QStringLiteral("练习完成"));
        } else if (!isRhythmPracticeMode()) {
            m_playbackEngine.seekTick(result.nextTick);
            setStatusMessage(QStringLiteral("正确，继续"));
        } else if (result.type == PracticeJudgeType::Correct) {
            setStatusMessage(QStringLiteral("节奏正确"));
        }
        emit positionChanged();
    } else if (result.type == PracticeJudgeType::Correct) {
        setStatusMessage(QStringLiteral("很好，还差当前和弦里的其他音"));
    }

    emit notesChanged();
    emit practiceChanged();
}

void PianoController::handleRhythmMisses(qint64 currentTick)
{
    const QVector<PracticeNoteResult> missed = m_practice.markMissedUntil(currentTick, rhythmToleranceTick());
    if (missed.isEmpty()) return;

    for (const PracticeNoteResult &result : missed) {
        appendPracticeEvent(result);
        for (auto &note : m_notes) {
            if (note.startTick == result.completedTick && note.midi == result.expectedMidi) {
                note.played = true;
            }
        }
    }

    emit statsChanged();
    emit notesChanged();
    emit practiceChanged();
    setStatusMessage(QStringLiteral("漏弹：%1").arg(NoteUtils::midiToName(missed.last().expectedMidi)));
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
        const qint64 currentTick = m_playbackEngine.currentTick();
        const qint64 searchStartTick = qMax<qint64>(0, currentTick - m_playbackEngine.maxNoteDurationTick() - 1);
        const auto first = std::lower_bound(m_notes.begin(), m_notes.end(), searchStartTick,
            [](const NoteEvent &note, qint64 tick) {
                return note.startTick < tick;
            });

        for (auto it = first; it != m_notes.end(); ++it) {
            const NoteEvent &note = *it;
            if (note.startTick > currentTick) break;
            if (currentTick >= note.startTick &&
                currentTick <= note.startTick + note.durationTick) {
                autoNotes.insert(note.midi);
            }
        }
    }
    m_autoNotes = autoNotes;

    QSet<int> combined;
    for (auto it = m_pressedNotes.cbegin(); it != m_pressedNotes.cend(); ++it) {
        combined.insert(it.key());
    }
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

bool PianoController::isPracticeMode() const
{
    return isPracticeModeName(m_mode);
}

bool PianoController::isRhythmPracticeMode() const
{
    return m_mode == QStringLiteral("rhythm");
}

qint64 PianoController::rhythmToleranceTick() const
{
    return qMax<qint64>(m_playbackEngine.ppq() / 6, 1);
}

void PianoController::beginPracticeSession()
{
    if (m_practiceSessionActive || m_currentSheetId <= 0) return;

    PracticeSessionStart start;
    start.mode = m_mode;
    start.playbackSpeed = m_playbackEngine.playbackSpeed();
    start.startTick = m_playbackEngine.currentTick();
    m_practiceSessionId = m_recordStore.beginSession(m_currentSheetId, start);
    m_practiceSessionActive = m_practiceSessionId > 0;
}

void PianoController::appendPracticeEvent(const PracticeNoteResult &result)
{
    if (!m_practiceSessionActive) return;

    PracticeEventRecord event;
    event.result = result.type;
    event.expectedMidi = result.expectedMidi;
    event.actualMidi = result.actualMidi;
    event.velocity = result.actualVelocity;
    event.expectedTick = result.expectedTick;
    event.actualTick = result.actualTick >= 0 ? result.actualTick : m_playbackEngine.currentTick();
    event.offsetMs = tickOffsetToMs(event.expectedTick, event.actualTick);
    m_recordStore.appendEvent(m_practiceSessionId, event);
}

void PianoController::finishPracticeSession(bool completed)
{
    if (!m_practiceSessionActive) return;

    PracticeSessionSummary summary;
    summary.completed = completed;
    summary.endTick = m_playbackEngine.currentTick();
    summary.correctCount = m_practice.correctCount();
    summary.wrongCount = m_practice.wrongCount();
    summary.missedCount = m_practice.missedCount();
    m_recordStore.finishSession(m_practiceSessionId, summary);

    m_practiceSessionId = -1;
    m_practiceSessionActive = false;
    refreshPracticeReport();
}

void PianoController::refreshPracticeReport()
{
    const QVariantMap next = practiceReportToVariant(m_recordStore.reportForSheet(m_currentSheetId));
    if (m_practiceReport == next) return;
    m_practiceReport = next;
    emit practiceReportChanged();
}

QVariantMap PianoController::practiceReportToVariant(const PracticeReportSummary &report) const
{
    QVariantMap map;
    map.insert(QStringLiteral("hasData"), report.sessionCount > 0);
    map.insert(QStringLiteral("sessionCount"), report.sessionCount);
    map.insert(QStringLiteral("averageScore"), report.averageScore);
    map.insert(QStringLiteral("totalCorrect"), report.totalCorrect);
    map.insert(QStringLiteral("totalWrong"), report.totalWrong);
    map.insert(QStringLiteral("totalMissed"), report.totalMissed);

    QVariantMap latest;
    if (!report.recentSessions.isEmpty()) {
        const PracticeSessionRecord &session = report.recentSessions.first();
        latest.insert(QStringLiteral("id"), session.id);
        latest.insert(QStringLiteral("mode"), session.mode);
        latest.insert(QStringLiteral("score"), session.score);
        latest.insert(QStringLiteral("completed"), session.completed);
        latest.insert(QStringLiteral("correct"), session.correctCount);
        latest.insert(QStringLiteral("wrong"), session.wrongCount);
        latest.insert(QStringLiteral("missed"), session.missedCount);
        latest.insert(QStringLiteral("durationSeconds"), session.durationSeconds);
        const QDateTime started = QDateTime::fromString(session.startedAt, Qt::ISODateWithMs);
        latest.insert(QStringLiteral("startedAt"),
                      started.isValid() ? started.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"))
                                        : session.startedAt);
    }
    map.insert(QStringLiteral("latest"), latest);

    QVariantList mistakes;
    mistakes.reserve(report.mistakeStats.size());
    for (const PracticeMistakeStat &stat : report.mistakeStats) {
        QVariantMap item;
        item.insert(QStringLiteral("midi"), stat.midi);
        item.insert(QStringLiteral("note"), stat.noteName);
        item.insert(QStringLiteral("wrong"), stat.wrongCount);
        item.insert(QStringLiteral("missed"), stat.missedCount);
        item.insert(QStringLiteral("early"), stat.earlyCount);
        item.insert(QStringLiteral("late"), stat.lateCount);
        item.insert(QStringLiteral("total"), stat.totalCount);
        mistakes.push_back(item);
    }
    map.insert(QStringLiteral("mistakes"), mistakes);
    return map;
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
    return m_playbackEngine.beatToTick(beat);
}

double PianoController::tickToBeat(qint64 tick) const
{
    return m_playbackEngine.tickToBeat(tick);
}

int PianoController::tickOffsetToMs(qint64 expectedTick, qint64 actualTick) const
{
    if (expectedTick < 0 || actualTick < 0 || m_playbackEngine.ppq() <= 0) return 0;
    const double sourceMs = PlaybackClock::durationMsBetweenTicks(
        expectedTick, actualTick, m_playbackEngine.tempos(), m_playbackEngine.ppq());
    const double playbackRate = qMax(0.01, double(m_playbackEngine.playbackSpeed()) / 100.0);
    return qRound(sourceMs / playbackRate);
}

int PianoController::velocityForMidi(int midi) const
{
    if (m_pressedNotes.contains(midi)) {
        return qBound(1, m_pressedNotes.value(midi), 127);
    }

    if (m_autoNotes.contains(midi)) {
        const qint64 currentTick = m_playbackEngine.currentTick();
        const qint64 searchStartTick = qMax<qint64>(0, currentTick - m_playbackEngine.maxNoteDurationTick() - 1);
        const auto first = std::lower_bound(m_notes.begin(), m_notes.end(), searchStartTick,
            [](const NoteEvent &note, qint64 tick) {
                return note.startTick < tick;
            });

        for (auto it = first; it != m_notes.end(); ++it) {
            const NoteEvent &note = *it;
            if (note.startTick > currentTick) break;
            if (note.midi == midi &&
                currentTick >= note.startTick &&
                currentTick <= note.startTick + note.durationTick) {
                return qBound(1, note.velocity, 127);
            }
        }
    }
    return 112;
}
