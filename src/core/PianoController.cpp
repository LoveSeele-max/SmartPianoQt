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
#include <cmath>

namespace {

constexpr double BeatsPerMeasure = 4.0;
constexpr double MeasureBoundaryEpsilon = 0.02;

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
    , m_practiceSessions(&m_recordStore)
{
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    m_countdownTimer.setInterval(1000);
    m_countdownTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PianoController::onFrame);
    connect(&m_countdownTimer, &QTimer::timeout, this, &PianoController::onCountdownTick);
    m_localMidiLibraryPath = MidiLibraryService::resolveLibraryPath();
    m_recordStore.open();
    m_localSheetModel.setRecordStore(&m_recordStore);
    m_localSheetModel.setLibraryPath(m_localMidiLibraryPath);
    refreshSheetCategories();
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

bool PianoController::loopRangeValid() const
{
    return m_loopStartSet && m_loopEndSet && m_loopEndTick > m_loopStartTick;
}

double PianoController::loopStartBeat() const
{
    return m_loopStartSet ? tickToBeat(m_loopStartTick) : 0.0;
}

double PianoController::loopEndBeat() const
{
    return m_loopEndSet ? tickToBeat(m_loopEndTick) : 0.0;
}

QString PianoController::loopStatus() const
{
    if (!m_loopStartSet && !m_loopEndSet) {
        return QStringLiteral("先设置 A/B 点");
    }
    if (!loopRangeValid()) {
        return QStringLiteral("A/B 区间无效，B 点需要晚于 A 点");
    }

    const QString range = QStringLiteral("A %1 / B %2")
                              .arg(loopStartBeat(), 0, 'f', 1)
                              .arg(loopEndBeat(), 0, 'f', 1);
    if (!m_loopPracticeEnabled) {
        return range + QStringLiteral("  未开启");
    }
    return range + QStringLiteral("  全对 %1/3  错误 %2/2")
                       .arg(m_loopCorrectPasses)
                       .arg(m_loopMistakes);
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

QVariantList PianoController::expectedLeftNotes() const
{
    return expectedNotesForHand(true);
}

QVariantList PianoController::expectedRightNotes() const
{
    return expectedNotesForHand(false);
}

QVariantList PianoController::soundFontFiles() const
{
    const QString currentPath = QFileInfo(m_synth.soundFontPath()).absoluteFilePath();
    const QStringList candidates = m_synth.soundFontCandidates();

    QVariantList list;
    list.reserve(candidates.size());
    for (const QString &path : candidates) {
        const QFileInfo info(path);
        QVariantMap item;
        item.insert(QStringLiteral("name"), info.fileName());
        item.insert(QStringLiteral("path"), info.absoluteFilePath());
        item.insert(QStringLiteral("current"), info.absoluteFilePath() == currentPath);
        list.push_back(item);
    }
    return list;
}

qint64 PianoController::expectedTickValue() const
{
    return isPracticeMode() ? m_practice.expectedTick() : -1;
}

bool PianoController::noteBelongsToLeftHand(const NoteEvent &note) const
{
    return HandPractice::noteIsLeftHand(note, m_handSplitMidi);
}

HandPractice::NoteDisplayState PianoController::rollNoteDisplayState(const NoteEvent &note) const
{
    HandPractice::NoteDisplayContext context;
    context.filter = currentHandFilter();
    context.practiceMode = isPracticeMode();
    context.currentTick = m_playbackEngine.currentTick();
    if (context.practiceMode) {
        context.expectedNotes = m_practice.expectedNotes();
    }
    return HandPractice::displayStateForNote(note, context);
}

bool PianoController::rollNoteMatchesTarget(const NoteEvent &note) const
{
    return rollNoteDisplayState(note).target;
}

bool PianoController::rollNoteExpected(const NoteEvent &note) const
{
    return rollNoteDisplayState(note).expected;
}

bool PianoController::rollNoteActive(const NoteEvent &note) const
{
    return rollNoteDisplayState(note).active;
}

bool PianoController::rollNoteCompleted(const NoteEvent &note) const
{
    return rollNoteDisplayState(note).completed;
}

bool PianoController::rollNoteReference(const NoteEvent &note) const
{
    return rollNoteDisplayState(note).reference;
}

void PianoController::setPlaybackSpeed(int speed)
{
    if (!m_playbackEngine.setPlaybackSpeed(speed)) return;
    emit playbackSpeedChanged();
}

void PianoController::adjustPlaybackSpeed(int delta)
{
    const int before = m_playbackEngine.playbackSpeed();
    setPlaybackSpeed(before + delta);
    const int after = m_playbackEngine.playbackSpeed();
    if (after == before) {
        setStatusMessage(delta > 0
                             ? QStringLiteral("已经是最高速度")
                             : QStringLiteral("已经是最低速度"));
        return;
    }
    setStatusMessage(QStringLiteral("速度：%1%").arg(after));
}

void PianoController::setMode(const QString &mode)
{
    const QString normalized = mode == QStringLiteral("practice") ? QStringLiteral("practice")
                                  : mode == QStringLiteral("rhythm") ? QStringLiteral("rhythm")
                                                                     : QStringLiteral("auto");
    if (m_mode == normalized) return;

    cancelCountdown();
    finishPracticeSession(false);
    m_mode = normalized;
    resetPracticeState(false, true);
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

void PianoController::setVelocityCurve(const QString &curve)
{
    const QString before = m_synth.velocityCurve();
    m_synth.setVelocityCurve(curve);
    if (m_synth.velocityCurve() == before) return;

    emit audioSettingsChanged();
    setStatusMessage(QStringLiteral("力度曲线：%1").arg(m_synth.velocityCurve()));
}

void PianoController::setLatencyMode(const QString &mode)
{
    const QString before = m_synth.latencyMode();
    if (!m_synth.setLatencyMode(mode) || m_synth.latencyMode() == before) return;

    emit audioStatusChanged();
    emit audioSettingsChanged();
    setStatusMessage(QStringLiteral("音频延迟模式：%1").arg(m_synth.latencyMode()));
}

void PianoController::loadSoundFont(const QUrl &url)
{
    const QString path = url.toLocalFile();
    loadSoundFontPath(path);
}

void PianoController::loadSoundFontPath(const QString &path)
{
    if (path.isEmpty()) {
        setStatusMessage(QStringLiteral("SoundFont 路径无效"));
        return;
    }

    const bool loaded = m_synth.loadSoundFont(path);
    emit audioStatusChanged();
    emit audioSettingsChanged();
    setStatusMessage(loaded
                         ? QStringLiteral("已切换 SoundFont：%1").arg(m_synth.soundFontName())
                         : m_synth.statusText());
}

void PianoController::rescanSoundFonts()
{
    m_synth.rescanSoundFonts();
    emit audioStatusChanged();
    emit audioSettingsChanged();
    setStatusMessage(m_synth.soundFontName().isEmpty()
                         ? QStringLiteral("已重扫 soundfonts，当前使用内置音色")
                         : QStringLiteral("已重扫 soundfonts：%1").arg(m_synth.soundFontName()));
}

void PianoController::previewCurrentSound()
{
    if (!m_synth.isAvailable()) {
        setStatusMessage(QStringLiteral("当前没有可用音频输出"));
        return;
    }

    const QVector<int> chord = { 60, 64, 67 };
    for (int i = 0; i < chord.size(); ++i) {
        const int midi = chord.at(i);
        QTimer::singleShot(i * 85, this, [this, midi]() {
            m_synth.noteOn(midi, 106);
        });
        QTimer::singleShot(i * 85 + 520, this, [this, midi]() {
            m_synth.noteOff(midi);
        });
    }
    setStatusMessage(QStringLiteral("试听当前音色"));
}

void PianoController::setSilentPracticeEnabled(bool enabled)
{
    if (m_silentPracticeEnabled == enabled) return;

    m_silentPracticeEnabled = enabled;
    if (m_silentPracticeEnabled) {
        m_synth.stopAll();
    }

    emit silentPracticeChanged();
    setStatusMessage(m_silentPracticeEnabled
                         ? QStringLiteral("静音练习已开启")
                         : QStringLiteral("静音练习已关闭"));
}

void PianoController::setHandPracticeEnabled(bool enabled)
{
    if (m_handPracticeEnabled == enabled) return;

    const bool restartSession = m_playing && isPracticeMode();
    finishPracticeSession(false);
    m_handPracticeEnabled = enabled;
    rebuildPracticeSongForHand();
    resetPracticeState(true, true);
    if (isPracticeMode()) {
        preparePracticeAtCurrentPosition();
    }
    if (restartSession) {
        beginPracticeSession();
    }

    emit handPracticeChanged();
    emit practiceChanged();
    emit notesChanged();
    setStatusMessage(m_handPracticeEnabled
                         ? QStringLiteral("已开启%1练习").arg(handPracticeLabel())
                         : QStringLiteral("已关闭左右手练习"));
}

void PianoController::setHandPracticeSide(const QString &side)
{
    const QString normalized = HandPractice::normalizeSideName(side);
    if (m_handPracticeSide == normalized) return;

    const bool restartSession = m_playing && isPracticeMode() && m_handPracticeEnabled;
    if (m_handPracticeEnabled) {
        finishPracticeSession(false);
    }
    m_handPracticeSide = normalized;
    if (m_handPracticeEnabled) {
        rebuildPracticeSongForHand();
        resetPracticeState(true, true);
        if (isPracticeMode()) {
            preparePracticeAtCurrentPosition();
        }
        if (restartSession) {
            beginPracticeSession();
        }
    }

    emit handPracticeChanged();
    emit practiceChanged();
    emit notesChanged();
    setStatusMessage(QStringLiteral("当前练习：%1").arg(handPracticeLabel()));
}

void PianoController::setHandSplitMidi(int midi)
{
    const int normalized = HandPractice::normalizeSplitMidi(midi);
    if (m_handSplitMidi == normalized) return;

    const bool restartSession = m_playing && isPracticeMode() && m_handPracticeEnabled;
    if (m_handPracticeEnabled) {
        finishPracticeSession(false);
    }
    m_handSplitMidi = normalized;
    if (m_handPracticeEnabled) {
        rebuildPracticeSongForHand();
        resetPracticeState(true, true);
        if (isPracticeMode()) {
            preparePracticeAtCurrentPosition();
        }
        if (restartSession) {
            beginPracticeSession();
        }
    }

    emit handPracticeChanged();
    emit practiceChanged();
    emit notesChanged();
}

void PianoController::playPause()
{
    if (m_countdownActive) {
        cancelCountdown(QStringLiteral("倒计时已取消"));
        return;
    }

    if (m_playing) {
        setPlaying(false);
        m_practiceSessions.pause();
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

    if (isRhythmPracticeMode()) {
        startRhythmCountdown();
        return;
    }

    startPlaybackNow();
}

void PianoController::startPlaybackNow()
{
    if (m_loopPracticeEnabled && loopRangeValid() &&
        !tickInsideLoop(m_playbackEngine.currentTick())) {
        seekToLoopStart(false);
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

void PianoController::startRhythmCountdown()
{
    if (m_notes.isEmpty()) {
        setStatusMessage(QStringLiteral("请先加载曲谱"));
        return;
    }

    if (m_loopPracticeEnabled && loopRangeValid() &&
        !tickInsideLoop(m_playbackEngine.currentTick())) {
        seekToLoopStart(false);
    } else {
        preparePracticeAtCurrentPosition();
    }

    m_countdownTimer.stop();
    m_countdownActive = true;
    m_countdownValue = 3;
    m_countdownText = QString::number(m_countdownValue);
    emit countdownChanged();
    setStatusMessage(QStringLiteral("节奏练习准备：%1").arg(m_countdownText));
    m_countdownTimer.start();
}

void PianoController::cancelCountdown(const QString &message)
{
    if (!m_countdownActive) return;

    m_countdownTimer.stop();
    m_countdownActive = false;
    m_countdownValue = 0;
    m_countdownText.clear();
    emit countdownChanged();

    if (!message.isEmpty()) {
        setStatusMessage(message);
    }
}

void PianoController::onCountdownTick()
{
    if (!m_countdownActive) return;

    if (m_countdownValue > 1) {
        --m_countdownValue;
        m_countdownText = QString::number(m_countdownValue);
        emit countdownChanged();
        setStatusMessage(QStringLiteral("节奏练习准备：%1").arg(m_countdownText));
        return;
    }

    m_countdownTimer.stop();
    m_countdownValue = 0;
    m_countdownText = QStringLiteral("开始");
    emit countdownChanged();
    setStatusMessage(QStringLiteral("开始"));

    QTimer::singleShot(320, this, [this]() {
        if (!m_countdownActive || m_countdownValue != 0) return;

        m_countdownActive = false;
        m_countdownText.clear();
        emit countdownChanged();
        startPlaybackNow();
    });
}

void PianoController::stop()
{
    cancelCountdown();
    finishPracticeSession(false);
    setPlaying(false);
    m_pressedNotes.clear();
    m_synth.stopAll();
    m_playbackEngine.stop();
    resetPracticeState(true, true);
    refreshActiveNotes();
    setStatusMessage(QStringLiteral("已回到曲首"));
    emit positionChanged();
    emit notesChanged();
    emit practiceChanged();
}

void PianoController::seekBeat(double beat)
{
    cancelCountdown();
    const bool restartSession = m_playing && isPracticeMode();
    finishPracticeSession(false);
    m_playbackEngine.seekTick(beatToTick(beat));

    resetPracticeState(false, false);
    if (isPracticeMode()) preparePracticeAtCurrentPosition();
    if (restartSession) beginPracticeSession();
    markNotesPlayedBeforeTick(m_playbackEngine.currentTick());

    refreshActiveNotes();
    emit positionChanged();
    emit notesChanged();
    emit practiceChanged();
}

void PianoController::seekNextMeasure()
{
    if (m_notes.isEmpty()) return;

    const double current = currentBeat();
    double target = (std::floor(current / BeatsPerMeasure) + 1.0) * BeatsPerMeasure;
    if (target <= current + MeasureBoundaryEpsilon) {
        target += BeatsPerMeasure;
    }
    target = qMin(target, totalBeats());

    seekBeat(target);
    setStatusMessage(QStringLiteral("已跳到下一小节：%1 拍").arg(target, 0, 'f', 1));
}

void PianoController::seekPreviousMeasure()
{
    if (m_notes.isEmpty()) return;

    const double current = currentBeat();
    const double currentMeasureStart = std::floor(current / BeatsPerMeasure) * BeatsPerMeasure;
    double target = currentMeasureStart;
    if (current <= currentMeasureStart + MeasureBoundaryEpsilon) {
        target -= BeatsPerMeasure;
    }
    target = qMax(0.0, target);

    seekBeat(target);
    setStatusMessage(QStringLiteral("已回到上一小节：%1 拍").arg(target, 0, 'f', 1));
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
    m_localSheetModel.refresh();
    emit localMidiLibraryChanged();
    setStatusMessage(QStringLiteral("本地 MIDI 库已刷新：%1 首").arg(m_localSheetModel.rowCount()));
}

void PianoController::loadLocalMidi(int index)
{
    const QString path = m_localSheetModel.filePathAt(index);
    if (path.isEmpty()) {
        setStatusMessage(QStringLiteral("请选择本地 MIDI 曲谱"));
        return;
    }

    loadMidiFile(path);
}

void PianoController::openLocalMidiLibrary()
{
    MidiLibraryService::openLibrary(m_localMidiLibraryPath);
}

void PianoController::setLocalSheetCategory(int categoryId)
{
    const qint64 normalized = qMax(0, categoryId);
    if (m_currentSheetCategoryId == normalized) return;

    m_currentSheetCategoryId = normalized;
    m_localSheetModel.setCategoryFilterId(m_currentSheetCategoryId);
    emit localMidiLibraryChanged();
    emit sheetCategoryFilterChanged();

    QString categoryName = QStringLiteral("全部");
    for (const QVariant &item : m_sheetCategories) {
        const QVariantMap category = item.toMap();
        if (category.value(QStringLiteral("id")).toLongLong() == m_currentSheetCategoryId) {
            categoryName = category.value(QStringLiteral("name")).toString();
            break;
        }
    }
    setStatusMessage(QStringLiteral("曲谱分类：%1").arg(categoryName));
}

void PianoController::createSheetCategory(const QString &name)
{
    const qint64 categoryId = m_recordStore.createSheetCategory(name);
    if (categoryId <= 0) {
        setStatusMessage(m_recordStore.lastError().isEmpty()
                             ? QStringLiteral("分类创建失败")
                             : m_recordStore.lastError());
        return;
    }

    refreshSheetCategories();
    setStatusMessage(QStringLiteral("已创建分类：%1").arg(name.simplified().left(32)));
}

void PianoController::toggleLocalMidiCategory(int index, int categoryId)
{
    if (categoryId <= 0) return;

    const qint64 sheetId = m_localSheetModel.sheetIdAt(index);
    if (sheetId <= 0) {
        setStatusMessage(QStringLiteral("请先加载一次曲谱后再分类"));
        return;
    }

    const QVector<qint64> categoryIds = m_localSheetModel.categoryIdsAt(index);
    const bool currentlyInCategory = categoryIds.contains(categoryId);
    if (!m_recordStore.setSheetCategoryMembership(sheetId, categoryId, !currentlyInCategory)) {
        setStatusMessage(m_recordStore.lastError().isEmpty()
                             ? QStringLiteral("分类更新失败")
                             : m_recordStore.lastError());
        return;
    }

    QString categoryName = QStringLiteral("分类");
    for (const QVariant &item : m_sheetCategories) {
        const QVariantMap category = item.toMap();
        if (category.value(QStringLiteral("id")).toLongLong() == categoryId) {
            categoryName = category.value(QStringLiteral("name")).toString();
            break;
        }
    }

    refreshSheetCategories();
    m_localSheetModel.refresh();
    emit localMidiLibraryChanged();
    setStatusMessage(currentlyInCategory
                         ? QStringLiteral("已移出分类：%1").arg(categoryName)
                         : QStringLiteral("已加入分类：%1").arg(categoryName));
}

void PianoController::setLoopStartAtCurrent()
{
    m_loopStartTick = qBound<qint64>(0, m_playbackEngine.currentTick(), m_playbackEngine.totalTicks());
    m_loopStartSet = true;
    if (m_loopPracticeEnabled && !loopRangeValid()) {
        m_loopPracticeEnabled = false;
        resetLoopProgress();
    }
    setStatusMessage(QStringLiteral("已设置循环 A 点：%1 拍").arg(loopStartBeat(), 0, 'f', 1));
    emit loopPracticeChanged();
}

void PianoController::setLoopEndAtCurrent()
{
    m_loopEndTick = qBound<qint64>(0, m_playbackEngine.currentTick(), m_playbackEngine.totalTicks());
    m_loopEndSet = true;
    if (m_loopPracticeEnabled && !loopRangeValid()) {
        m_loopPracticeEnabled = false;
        resetLoopProgress();
    }
    setStatusMessage(QStringLiteral("已设置循环 B 点：%1 拍").arg(loopEndBeat(), 0, 'f', 1));
    emit loopPracticeChanged();
}

void PianoController::toggleLoopPractice()
{
    if (m_loopPracticeEnabled) {
        m_loopPracticeEnabled = false;
        resetLoopProgress();
        setStatusMessage(QStringLiteral("循环练习已关闭"));
        emit loopPracticeChanged();
        return;
    }

    if (!loopRangeValid()) {
        setStatusMessage(QStringLiteral("请先设置有效的 A/B 循环区间"));
        emit loopPracticeChanged();
        return;
    }

    m_loopPracticeEnabled = true;
    resetLoopProgress();
    seekToLoopStart(false);
    resetLoopSegmentBaseline();
    setStatusMessage(QStringLiteral("循环练习已开启：%1").arg(loopStatus()));
    emit loopPracticeChanged();
}

void PianoController::clearLoopPractice()
{
    m_loopPracticeEnabled = false;
    m_loopStartSet = false;
    m_loopEndSet = false;
    m_loopStartTick = 0;
    m_loopEndTick = 0;
    resetLoopProgress();
    setStatusMessage(QStringLiteral("已清除 A/B 循环区间"));
    emit loopPracticeChanged();
}

void PianoController::onFrame()
{
    if (!m_playing) return;
    const qint64 elapsedMs = m_frameClock.restart();

    if (m_mode == QStringLiteral("auto") || isRhythmPracticeMode()) {
        const PlaybackAdvanceResult result = m_playbackEngine.advance(elapsedMs);
        const bool loopBoundaryReached = m_loopPracticeEnabled && loopRangeValid() &&
            result.currentTick >= loopEndTick();
        const qint64 effectiveTick = loopBoundaryReached ? loopEndTick() : result.currentTick;

        if (isRhythmPracticeMode()) {
            const RhythmTimingWindows windows = rhythmTimingWindows();
            handleRhythmMisses(loopBoundaryReached
                                   ? effectiveTick + windows.hitTick
                                   : effectiveTick);
        }

        if (loopBoundaryReached) {
            if (m_mode == QStringLiteral("auto")) {
                retriggerAutoNoteStarts(result.previousTick, effectiveTick);
            }
            m_playbackEngine.seekTick(effectiveTick);
            finishLoopIteration();
            emit positionChanged();
            emit frameChanged();
            refreshActiveNotes();
            return;
        }

        if (result.reachedEnd) {
            if (m_mode == QStringLiteral("auto")) {
                retriggerAutoNoteStarts(result.previousTick, result.currentTick);
            } else {
                const RhythmTimingWindows windows = rhythmTimingWindows();
                handleRhythmMisses(result.currentTick + windows.hitTick + 1);
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
    const HandPractice::NoteDisplayState state = rollNoteDisplayState(note);

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
    map.insert(QStringLiteral("leftHand"), noteBelongsToLeftHand(note));
    map.insert(QStringLiteral("targetHand"), state.target);
    map.insert(QStringLiteral("expected"), state.expected);
    map.insert(QStringLiteral("active"), state.active);
    map.insert(QStringLiteral("completed"), state.completed);
    map.insert(QStringLiteral("reference"), state.reference);
    return map;
}

QVariantList PianoController::expectedNotesForHand(bool leftHand) const
{
    QVariantList list;
    if (!isPracticeMode()) {
        return list;
    }

    const QVector<NoteEvent> expected = m_practice.expectedNotes();
    for (const auto &note : expected) {
        const HandPractice::Side side = leftHand ? HandPractice::Side::Left : HandPractice::Side::Right;
        if (HandPractice::noteMatchesSide(note, side, m_handSplitMidi)) {
            list.push_back(noteToVariant(note));
        }
    }
    return list;
}

void PianoController::setSong(Song song, const QString &sourcePath, const QString &sourceFormat)
{
    cancelCountdown();
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
    m_practiceSessions.setSheetId(m_currentSheetId);
    m_notes = std::move(song.notes);

    rebuildPracticeSongForHand();
    resetPracticeState(true, true);
    m_loopPracticeEnabled = false;
    m_loopStartSet = false;
    m_loopEndSet = false;
    m_loopStartTick = 0;
    m_loopEndTick = 0;
    resetLoopProgress();
    refreshActiveNotes();

    emit songChanged();
    emit bpmChanged();
    emit notesChanged();
    emit positionChanged();
    emit practiceChanged();
    emit loopPracticeChanged();
    refreshLocalMidiLibrary();
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
    if (!midiMatchesCurrentHand(midi)) {
        return;
    }

    QVector<PracticeNoteResult> results;
    if (isRhythmPracticeMode()) {
        results = m_practice.noteOnRhythmDetailed(
            midi, velocity, m_playbackEngine.currentTick(), rhythmTimingWindows());
    } else {
        const PracticeNoteResult result = m_practice.noteOn(midi, velocity);
        if (result.type != PracticeJudgeType::Ignored) {
            results.push_back(result);
        }
    }
    if (results.isEmpty()) return;

    bool anyStatsChanged = false;
    bool loopSpeedAdjusted = false;
    const PracticeNoteResult *completion = nullptr;
    for (const PracticeNoteResult &item : results) {
        appendPracticeEvent(item);
        loopSpeedAdjusted = registerLoopJudgement(item) || loopSpeedAdjusted;
        anyStatsChanged = anyStatsChanged || item.statsChanged;
        if (item.stepComplete) {
            completion = &item;
        }
    }

    const PracticeNoteResult &result = results.first();

    if (result.type == PracticeJudgeType::WrongNote) {
        if (!loopSpeedAdjusted) {
            setStatusMessage(QStringLiteral("错音"));
        }
        if (anyStatsChanged) emit statsChanged();
        return;
    }

    if ((result.type == PracticeJudgeType::Perfect || result.type == PracticeJudgeType::Good) &&
        !loopSpeedAdjusted) {
        setStatusMessage(result.type == PracticeJudgeType::Perfect
                             ? QStringLiteral("Perfect")
                             : QStringLiteral("Good"));
    }

    if (result.type == PracticeJudgeType::Early) {
        if (!loopSpeedAdjusted) {
            setStatusMessage(QStringLiteral("偏早"));
        }
    }

    if (result.type == PracticeJudgeType::Late && !loopSpeedAdjusted) {
        setStatusMessage(QStringLiteral("偏晚"));
    } else if (result.type == PracticeJudgeType::RepeatedNote) {
        setStatusMessage(QStringLiteral("重复音"));
    }

    if (anyStatsChanged) {
        emit statsChanged();
    }

    if (completion) {
        markNotesPlayedAtCompletedTick(completion->completedTick);

        const qint64 nextTick = completion->songComplete
            ? m_playbackEngine.totalTicks()
            : completion->nextTick;
        if (m_loopPracticeEnabled && loopRangeValid() && shouldFinishLoopAt(nextTick)) {
            finishLoopIteration();
            emit positionChanged();
        } else if (completion->songComplete) {
            m_playbackEngine.seekTick(m_playbackEngine.totalTicks());
            setPlaying(false);
            finishPracticeSession(true);
            setStatusMessage(QStringLiteral("练习完成"));
        } else if (!isRhythmPracticeMode()) {
            m_playbackEngine.seekTick(completion->nextTick);
            setStatusMessage(QStringLiteral("正确，继续"));
        } else if (result.type == PracticeJudgeType::Correct) {
            setStatusMessage(QStringLiteral("节奏正确"));
        }
        emit positionChanged();
    } else if (result.type == PracticeJudgeType::Correct ||
               result.type == PracticeJudgeType::Perfect ||
               result.type == PracticeJudgeType::Good) {
        if (result.type == PracticeJudgeType::Perfect || result.type == PracticeJudgeType::Good) {
            setStatusMessage(QStringLiteral("%1，还差当前和弦里的其他音")
                                 .arg(result.type == PracticeJudgeType::Perfect
                                          ? QStringLiteral("Perfect")
                                          : QStringLiteral("Good")));
        } else {
            setStatusMessage(QStringLiteral("很好，还差当前和弦里的其他音"));
        }
    }

    emit notesChanged();
    emit practiceChanged();
}

void PianoController::handleRhythmMisses(qint64 currentTick)
{
    const QVector<PracticeNoteResult> missed = m_practice.markMissedUntil(currentTick, rhythmTimingWindows());
    if (missed.isEmpty()) return;

    bool loopSpeedAdjusted = false;
    for (const PracticeNoteResult &result : missed) {
        appendPracticeEvent(result);
        loopSpeedAdjusted = registerLoopJudgement(result) || loopSpeedAdjusted;
        for (auto &note : m_notes) {
            if (note.startTick == result.completedTick &&
                note.midi == result.expectedMidi &&
                rollNoteMatchesTarget(note)) {
                note.played = true;
            }
        }
    }

    emit statsChanged();
    emit notesChanged();
    emit practiceChanged();
    if (!loopSpeedAdjusted) {
        setStatusMessage(QStringLiteral("漏弹"));
    }
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

void PianoController::rebuildPracticeSongForHand()
{
    m_practice.setSong(practiceNotesForCurrentHand());
}

QVector<NoteEvent> PianoController::practiceNotesForCurrentHand() const
{
    return HandPractice::filterNotes(m_notes, currentHandFilter());
}

HandPractice::Filter PianoController::currentHandFilter() const
{
    HandPractice::Filter filter;
    filter.enabled = m_handPracticeEnabled;
    filter.side = HandPractice::sideFromName(m_handPracticeSide);
    filter.splitMidi = m_handSplitMidi;
    return filter;
}

bool PianoController::midiMatchesCurrentHand(int midi) const
{
    return HandPractice::midiMatchesFilter(midi, currentHandFilter());
}

void PianoController::markNotesPlayedAtCompletedTick(qint64 completedTick)
{
    const HandPractice::Filter filter = currentHandFilter();
    for (auto &note : m_notes) {
        if (HandPractice::noteMatchesCompletedStep(note, completedTick, filter)) {
            note.played = true;
        }
    }
}

void PianoController::markNotesPlayedBeforeTick(qint64 tick)
{
    const HandPractice::Filter filter = currentHandFilter();
    for (auto &note : m_notes) {
        note.played = HandPractice::noteShouldBePlayedBeforeTick(note, tick, filter);
    }
}

QString PianoController::handPracticeLabel() const
{
    return m_handPracticeSide == QStringLiteral("left")
        ? QStringLiteral("左手")
        : QStringLiteral("右手");
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

    if (!m_silentPracticeEnabled) {
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
    } else if (!previous.isEmpty() && combined.isEmpty()) {
        m_synth.stopAll();
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

RhythmTimingWindows PianoController::rhythmTimingWindows() const
{
    RhythmTimingWindows windows;
    windows.perfectTick = rhythmWindowTick(50);
    windows.goodTick = qMax(windows.perfectTick, rhythmWindowTick(100));
    windows.hitTick = qMax(windows.goodTick, rhythmWindowTick(180));
    return windows;
}

qint64 PianoController::rhythmWindowTick(int windowMs) const
{
    const qint64 anchorTick = m_practice.expectedTick() >= 0
        ? m_practice.expectedTick()
        : m_playbackEngine.currentTick();
    const double playbackRate = qMax(0.01, double(m_playbackEngine.playbackSpeed()) / 100.0);
    const qint64 sourceMs = qMax<qint64>(1, qRound64(double(windowMs) * playbackRate));
    const double targetTick = PlaybackClock::advance(double(anchorTick),
                                                     sourceMs,
                                                     m_playbackEngine.tempos(),
                                                     m_playbackEngine.ppq());
    return qMax<qint64>(1, qRound64(targetTick - double(anchorTick)));
}

void PianoController::beginPracticeSession()
{
    m_practiceSessions.begin(m_mode, m_playbackEngine.playbackSpeed(), m_playbackEngine.currentTick());
}

void PianoController::appendPracticeEvent(const PracticeNoteResult &result)
{
    const qint64 actualTick = result.actualTick >= 0 ? result.actualTick : m_playbackEngine.currentTick();
    m_practiceSessions.append(result,
                              actualTick,
                              tickOffsetToMs(result.expectedTick, actualTick));
}

void PianoController::finishPracticeSession(bool completed)
{
    if (m_practiceSessions.finish(completed,
                                  m_playbackEngine.currentTick(),
                                  m_practice.correctCount(),
                                  m_practice.wrongCount(),
                                  m_practice.missedCount())) {
        refreshPracticeReport();
    }
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
    auto sessionToVariant = [](const PracticeSessionRecord &session) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), session.id);
        item.insert(QStringLiteral("mode"), session.mode);
        item.insert(QStringLiteral("score"), session.score);
        item.insert(QStringLiteral("completed"), session.completed);
        item.insert(QStringLiteral("correct"), session.correctCount);
        item.insert(QStringLiteral("wrong"), session.wrongCount);
        item.insert(QStringLiteral("missed"), session.missedCount);
        item.insert(QStringLiteral("durationSeconds"), session.durationSeconds);
        item.insert(QStringLiteral("activeDurationSeconds"), session.activeDurationSeconds);
        const QDateTime started = QDateTime::fromString(session.startedAt, Qt::ISODateWithMs);
        item.insert(QStringLiteral("startedAt"),
                    started.isValid() ? started.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"))
                                      : session.startedAt);
        return item;
    };

    auto mistakeToVariant = [](const PracticeMistakeStat &stat) {
        QVariantMap item;
        item.insert(QStringLiteral("midi"), stat.midi);
        item.insert(QStringLiteral("note"), stat.noteName);
        item.insert(QStringLiteral("wrong"), stat.wrongCount);
        item.insert(QStringLiteral("missed"), stat.missedCount);
        item.insert(QStringLiteral("early"), stat.earlyCount);
        item.insert(QStringLiteral("late"), stat.lateCount);
        item.insert(QStringLiteral("total"), stat.totalCount);
        return item;
    };

    QVariantMap map;
    map.insert(QStringLiteral("hasData"), report.sessionCount > 0);
    map.insert(QStringLiteral("sessionCount"), report.sessionCount);
    map.insert(QStringLiteral("averageScore"), report.averageScore);
    map.insert(QStringLiteral("totalCorrect"), report.totalCorrect);
    map.insert(QStringLiteral("totalWrong"), report.totalWrong);
    map.insert(QStringLiteral("totalMissed"), report.totalMissed);

    QVariantMap latest;
    if (!report.recentSessions.isEmpty()) {
        latest = sessionToVariant(report.recentSessions.first());
    }
    map.insert(QStringLiteral("latest"), latest);

    QVariantList sessions;
    sessions.reserve(report.recentSessions.size());
    for (const PracticeSessionRecord &session : report.recentSessions) {
        sessions.push_back(sessionToVariant(session));
    }
    map.insert(QStringLiteral("sessions"), sessions);

    QVariantList scoreTrend;
    scoreTrend.reserve(report.scoreTrend.size());
    for (const PracticeSessionRecord &session : report.scoreTrend) {
        scoreTrend.push_back(sessionToVariant(session));
    }
    map.insert(QStringLiteral("scoreTrend"), scoreTrend);

    QVariantList mistakes;
    mistakes.reserve(report.mistakeStats.size());
    for (const PracticeMistakeStat &stat : report.mistakeStats) {
        mistakes.push_back(mistakeToVariant(stat));
    }
    map.insert(QStringLiteral("mistakes"), mistakes);

    QVariantList topWrongNotes;
    topWrongNotes.reserve(report.topWrongNotes.size());
    for (const PracticeMistakeStat &stat : report.topWrongNotes) {
        topWrongNotes.push_back(mistakeToVariant(stat));
    }
    map.insert(QStringLiteral("topWrongNotes"), topWrongNotes);

    QVariantList topMissedNotes;
    topMissedNotes.reserve(report.topMissedNotes.size());
    for (const PracticeMistakeStat &stat : report.topMissedNotes) {
        topMissedNotes.push_back(mistakeToVariant(stat));
    }
    map.insert(QStringLiteral("topMissedNotes"), topMissedNotes);

    QString teacherTip = QStringLiteral("完成几次练习后，我会给出更具体的复练建议。");
    if (report.sessionCount > 0) {
        if (!report.topWrongNotes.isEmpty()) {
            teacherTip = QStringLiteral("先慢练最常按错的位置，目标是连续三次稳定命中。");
        } else if (!report.topMissedNotes.isEmpty()) {
            teacherTip = QStringLiteral("注意最容易漏弹的位置，可以单独放慢练。");
        } else if (report.averageScore >= 90) {
            teacherTip = QStringLiteral("整体很稳，可以逐步把速度提高 5%。");
        } else {
            teacherTip = QStringLiteral("先保持当前速度，把正确率稳定到 90% 再加速。");
        }
    }
    map.insert(QStringLiteral("teacherTip"), teacherTip);
    return map;
}

void PianoController::refreshSheetCategories()
{
    QVariantList next;

    QVariantMap all;
    all.insert(QStringLiteral("id"), 0);
    all.insert(QStringLiteral("name"), QStringLiteral("全部"));
    all.insert(QStringLiteral("builtInKey"), QStringLiteral("all"));
    all.insert(QStringLiteral("builtIn"), true);
    all.insert(QStringLiteral("sheetCount"), 0);
    next.push_back(all);

    const QVector<SheetCategoryInfo> categories = m_recordStore.sheetCategories();
    next.reserve(categories.size() + 1);
    for (const SheetCategoryInfo &category : categories) {
        next.push_back(sheetCategoryToVariant(category));
    }

    if (m_sheetCategories == next) return;
    m_sheetCategories = next;
    emit sheetCategoriesChanged();
}

QVariantMap PianoController::sheetCategoryToVariant(const SheetCategoryInfo &category) const
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), category.id);
    item.insert(QStringLiteral("name"), category.name);
    item.insert(QStringLiteral("builtInKey"), category.builtInKey);
    item.insert(QStringLiteral("builtIn"), category.builtIn());
    item.insert(QStringLiteral("sheetCount"), category.sheetCount);
    return item;
}

qint64 PianoController::loopStartTick() const
{
    return m_loopStartSet ? m_loopStartTick : 0;
}

qint64 PianoController::loopEndTick() const
{
    return m_loopEndSet ? m_loopEndTick : m_playbackEngine.totalTicks();
}

bool PianoController::tickInsideLoop(qint64 tick) const
{
    if (!loopRangeValid()) return true;
    return tick >= loopStartTick() && tick < loopEndTick();
}

void PianoController::resetLoopProgress()
{
    m_loopCorrectPasses = 0;
    m_loopMistakes = 0;
    resetLoopSegmentBaseline();
}

void PianoController::resetLoopSegmentBaseline()
{
    m_loopSegmentMistakeBase = m_practice.wrongCount() + m_practice.missedCount();
}

void PianoController::seekToLoopStart(bool resetStats)
{
    if (!loopRangeValid()) return;

    m_playbackEngine.seekTick(loopStartTick());
    resetPracticeState(resetStats, true);
    if (isPracticeMode()) {
        preparePracticeAtCurrentPosition();
    }

    const qint64 currentTick = m_playbackEngine.currentTick();
    markNotesPlayedBeforeTick(currentTick);

    refreshActiveNotes();
    emit positionChanged();
    emit notesChanged();
    emit practiceChanged();
}

bool PianoController::shouldFinishLoopAt(qint64 tick) const
{
    return m_loopPracticeEnabled && loopRangeValid() && tick >= loopEndTick();
}

void PianoController::finishLoopIteration()
{
    if (!m_loopPracticeEnabled || !loopRangeValid()) return;

    const QString previousStatus = m_statusMessage;
    bool adjustedSpeed = false;
    if (isPracticeMode()) {
        const int currentMistakes = m_practice.wrongCount() + m_practice.missedCount();
        const int segmentMistakes = qMax(0, currentMistakes - m_loopSegmentMistakeBase);
        if (segmentMistakes == 0) {
            ++m_loopCorrectPasses;
            m_loopMistakes = 0;
            if (m_loopCorrectPasses >= 3) {
                m_loopCorrectPasses = 0;
                adjustLoopSpeed(5, QStringLiteral("连续 3 次全对，自动升速"));
                adjustedSpeed = true;
            }
        } else {
            m_loopCorrectPasses = 0;
        }
    }

    seekToLoopStart(false);
    resetLoopSegmentBaseline();
    if (!adjustedSpeed && !previousStatus.contains(QStringLiteral("自动降速"))) {
        setStatusMessage(isPracticeMode()
                             ? QStringLiteral("已回到 A 点，继续局部练习")
                             : QStringLiteral("已回到 A 点循环播放"));
    }
    emit loopPracticeChanged();
}

bool PianoController::registerLoopJudgement(const PracticeNoteResult &result)
{
    if (!m_loopPracticeEnabled || !loopRangeValid() || !isPracticeMode() || !result.statsChanged) {
        return false;
    }

    const bool isMistake = result.type == PracticeJudgeType::WrongNote ||
        result.type == PracticeJudgeType::Early ||
        result.type == PracticeJudgeType::Late ||
        result.type == PracticeJudgeType::Missed;
    if (!isMistake) return false;

    ++m_loopMistakes;
    m_loopCorrectPasses = 0;
    if (m_loopMistakes >= 2) {
        m_loopMistakes = 0;
        adjustLoopSpeed(-5, QStringLiteral("错 2 次，自动降速"));
        return true;
    }
    emit loopPracticeChanged();
    return false;
}

void PianoController::adjustLoopSpeed(int delta, const QString &reason)
{
    const int before = m_playbackEngine.playbackSpeed();
    setPlaybackSpeed(before + delta);
    const int after = m_playbackEngine.playbackSpeed();
    if (after == before) {
        setStatusMessage(delta > 0
                             ? QStringLiteral("已经到最高速度，继续保持稳定")
                             : QStringLiteral("已经到最低速度，先稳住正确率"));
    } else {
        setStatusMessage(QStringLiteral("%1：%2%").arg(reason).arg(after));
    }
    emit loopPracticeChanged();
}

void PianoController::retriggerAutoNoteStarts(qint64 previousTick, qint64 currentTick)
{
    if (m_silentPracticeEnabled) return;
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
