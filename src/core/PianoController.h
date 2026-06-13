#pragma once

#include "audio/MidiSynth.h"
#include "core/HandPractice.h"
#include "core/Song.h"
#include "library/LocalSheetModel.h"
#include "playback/PlaybackEngine.h"
#include "practice/PracticeEngine.h"
#include "practice/PracticeSessionController.h"
#include "storage/PracticeRecordStore.h"

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVector>
#include <QElapsedTimer>

class PianoController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString songTitle READ songTitle NOTIFY songChanged)
    Q_PROPERTY(int bpm READ bpm NOTIFY bpmChanged)
    Q_PROPERTY(int playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
    Q_PROPERTY(double currentBeat READ currentBeat NOTIFY positionChanged)
    Q_PROPERTY(double totalBeats READ totalBeats NOTIFY songChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged)
    Q_PROPERTY(QVariantList activeNotes READ activeNotes NOTIFY activeNotesChanged)
    Q_PROPERTY(QVariantList expectedNotes READ expectedNotes NOTIFY practiceChanged)
    Q_PROPERTY(QVariantList expectedLeftNotes READ expectedLeftNotes NOTIFY practiceChanged)
    Q_PROPERTY(QVariantList expectedRightNotes READ expectedRightNotes NOTIFY practiceChanged)
    Q_PROPERTY(int correctCount READ correctCount NOTIFY statsChanged)
    Q_PROPERTY(int wrongCount READ wrongCount NOTIFY statsChanged)
    Q_PROPERTY(int missedCount READ missedCount NOTIFY statsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY audioStatusChanged)
    Q_PROPERTY(QString soundFontName READ soundFontName NOTIFY audioSettingsChanged)
    Q_PROPERTY(QString soundFontPath READ soundFontPath NOTIFY audioSettingsChanged)
    Q_PROPERTY(QVariantList soundFontFiles READ soundFontFiles NOTIFY audioSettingsChanged)
    Q_PROPERTY(QString velocityCurve READ velocityCurve WRITE setVelocityCurve NOTIFY audioSettingsChanged)
    Q_PROPERTY(QString latencyMode READ latencyMode WRITE setLatencyMode NOTIFY audioSettingsChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool silentPracticeEnabled READ silentPracticeEnabled WRITE setSilentPracticeEnabled NOTIFY silentPracticeChanged)
    Q_PROPERTY(bool handPracticeEnabled READ handPracticeEnabled WRITE setHandPracticeEnabled NOTIFY handPracticeChanged)
    Q_PROPERTY(QString handPracticeSide READ handPracticeSide WRITE setHandPracticeSide NOTIFY handPracticeChanged)
    Q_PROPERTY(int handSplitMidi READ handSplitMidi WRITE setHandSplitMidi NOTIFY handPracticeChanged)
    Q_PROPERTY(QVariantList localMidiFiles READ localMidiFiles NOTIFY localMidiLibraryChanged)
    Q_PROPERTY(QAbstractListModel* localSheetModel READ localSheetModel CONSTANT)
    Q_PROPERTY(QString localMidiLibraryPath READ localMidiLibraryPath NOTIFY localMidiLibraryChanged)
    Q_PROPERTY(QVariantList sheetCategories READ sheetCategories NOTIFY sheetCategoriesChanged)
    Q_PROPERTY(int currentSheetCategoryId READ currentSheetCategoryId NOTIFY sheetCategoryFilterChanged)
    Q_PROPERTY(QVariantMap practiceReport READ practiceReport NOTIFY practiceReportChanged)
    Q_PROPERTY(bool loopPracticeEnabled READ loopPracticeEnabled NOTIFY loopPracticeChanged)
    Q_PROPERTY(bool loopRangeValid READ loopRangeValid NOTIFY loopPracticeChanged)
    Q_PROPERTY(double loopStartBeat READ loopStartBeat NOTIFY loopPracticeChanged)
    Q_PROPERTY(double loopEndBeat READ loopEndBeat NOTIFY loopPracticeChanged)
    Q_PROPERTY(int loopCorrectPasses READ loopCorrectPasses NOTIFY loopPracticeChanged)
    Q_PROPERTY(int loopMistakes READ loopMistakes NOTIFY loopPracticeChanged)
    Q_PROPERTY(QString loopStatus READ loopStatus NOTIFY loopPracticeChanged)
    Q_PROPERTY(bool countdownActive READ countdownActive NOTIFY countdownChanged)
    Q_PROPERTY(QString countdownText READ countdownText NOTIFY countdownChanged)

public:
    explicit PianoController(QObject *parent = nullptr);
    ~PianoController() override;

    QString songTitle() const { return m_songTitle; }
    int bpm() const { return m_playbackEngine.bpm(); }
    int playbackSpeed() const { return m_playbackEngine.playbackSpeed(); }
    double currentBeat() const;
    double totalBeats() const;
    bool isPlaying() const { return m_playing; }
    QString mode() const { return m_mode; }
    QVariantList notes() const;
    QVariantList activeNotes() const;
    QVariantList expectedNotes() const;
    QVariantList expectedLeftNotes() const;
    QVariantList expectedRightNotes() const;
    int correctCount() const { return m_practice.correctCount(); }
    int wrongCount() const { return m_practice.wrongCount(); }
    int missedCount() const { return m_practice.missedCount(); }
    QString statusMessage() const { return m_statusMessage; }
    QString audioStatus() const { return m_synth.statusText(); }
    QString soundFontName() const { return m_synth.soundFontName(); }
    QString soundFontPath() const { return m_synth.soundFontPath(); }
    QVariantList soundFontFiles() const;
    QString velocityCurve() const { return m_synth.velocityCurve(); }
    QString latencyMode() const { return m_synth.latencyMode(); }
    int volume() const { return m_synth.volume(); }
    bool silentPracticeEnabled() const { return m_silentPracticeEnabled; }
    bool handPracticeEnabled() const { return m_handPracticeEnabled; }
    QString handPracticeSide() const { return m_handPracticeSide; }
    int handSplitMidi() const { return m_handSplitMidi; }
    QVariantList localMidiFiles() const { return m_localSheetModel.toVariantList(); }
    QAbstractListModel *localSheetModel() { return &m_localSheetModel; }
    QString localMidiLibraryPath() const { return m_localMidiLibraryPath; }
    QVariantList sheetCategories() const { return m_sheetCategories; }
    int currentSheetCategoryId() const { return int(m_currentSheetCategoryId); }
    QVariantMap practiceReport() const { return m_practiceReport; }
    bool loopPracticeEnabled() const { return m_loopPracticeEnabled; }
    bool loopRangeValid() const;
    double loopStartBeat() const;
    double loopEndBeat() const;
    int loopCorrectPasses() const { return m_loopCorrectPasses; }
    int loopMistakes() const { return m_loopMistakes; }
    QString loopStatus() const;
    bool countdownActive() const { return m_countdownActive; }
    QString countdownText() const { return m_countdownText; }
    const QVector<NoteEvent> &noteEvents() const { return m_notes; }
    qint64 currentTickValue() const { return m_playbackEngine.currentTick(); }
    qint64 totalTickValue() const { return m_playbackEngine.totalTicks(); }
    qint64 expectedTickValue() const;
    int ppq() const { return m_playbackEngine.ppq(); }
    bool noteBelongsToLeftHand(const NoteEvent &note) const;
    HandPractice::NoteDisplayState rollNoteDisplayState(const NoteEvent &note) const;
    bool rollNoteMatchesTarget(const NoteEvent &note) const;
    bool rollNoteExpected(const NoteEvent &note) const;
    bool rollNoteActive(const NoteEvent &note) const;
    bool rollNoteCompleted(const NoteEvent &note) const;
    bool rollNoteReference(const NoteEvent &note) const;

public slots:
    void setPlaybackSpeed(int speed);
    void setMode(const QString &mode);
    void setVolume(int volume);
    void setVelocityCurve(const QString &curve);
    void setLatencyMode(const QString &mode);
    void setSilentPracticeEnabled(bool enabled);
    void setHandPracticeEnabled(bool enabled);
    void setHandPracticeSide(const QString &side);
    void setHandSplitMidi(int midi);
    Q_INVOKABLE void adjustPlaybackSpeed(int delta);

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seekBeat(double beat);
    Q_INVOKABLE void seekNextMeasure();
    Q_INVOKABLE void seekPreviousMeasure();
    Q_INVOKABLE void noteOn(int midi, int velocity = 112);
    Q_INVOKABLE void noteOff(int midi);
    Q_INVOKABLE void loadSoundFont(const QUrl &url);
    Q_INVOKABLE void loadSoundFontPath(const QString &path);
    Q_INVOKABLE void rescanSoundFonts();
    Q_INVOKABLE void previewCurrentSound();
    Q_INVOKABLE void loadDemoSong();
    Q_INVOKABLE void loadSheet(const QUrl &url);
    Q_INVOKABLE void refreshLocalMidiLibrary();
    Q_INVOKABLE void loadLocalMidi(int index);
    Q_INVOKABLE void openLocalMidiLibrary();
    Q_INVOKABLE void setLocalSheetCategory(int categoryId);
    Q_INVOKABLE void createSheetCategory(const QString &name);
    Q_INVOKABLE void toggleLocalMidiCategory(int index, int categoryId);
    Q_INVOKABLE void setLoopStartAtCurrent();
    Q_INVOKABLE void setLoopEndAtCurrent();
    Q_INVOKABLE void toggleLoopPractice();
    Q_INVOKABLE void clearLoopPractice();

signals:
    void songChanged();
    void frameChanged();
    void bpmChanged();
    void playbackSpeedChanged();
    void positionChanged();
    void playbackStateChanged();
    void modeChanged();
    void notesChanged();
    void activeNotesChanged();
    void practiceChanged();
    void statsChanged();
    void statusMessageChanged();
    void audioStatusChanged();
    void audioSettingsChanged();
    void volumeChanged();
    void silentPracticeChanged();
    void handPracticeChanged();
    void localMidiLibraryChanged();
    void sheetCategoriesChanged();
    void sheetCategoryFilterChanged();
    void practiceReportChanged();
    void loopPracticeChanged();
    void countdownChanged();

private slots:
    void onFrame();
    void onCountdownTick();

private:
    QVariantMap noteToVariant(const NoteEvent &note) const;
    QVariantList expectedNotesForHand(bool leftHand) const;
    void setSong(Song song, const QString &sourcePath = QString(), const QString &sourceFormat = QString());
    void loadJsonSheet(const QString &path);
    void loadMidiFile(const QString &path);
    void preparePracticeAtCurrentPosition();
    void evaluatePracticeNote(int midi, int velocity);
    void handleRhythmMisses(qint64 currentTick);
    void resetPracticeState(bool resetStats, bool resetPlayed);
    void rebuildPracticeSongForHand();
    QVector<NoteEvent> practiceNotesForCurrentHand() const;
    HandPractice::Filter currentHandFilter() const;
    bool midiMatchesCurrentHand(int midi) const;
    void markNotesPlayedAtCompletedTick(qint64 completedTick);
    void markNotesPlayedBeforeTick(qint64 tick);
    QString handPracticeLabel() const;
    void refreshActiveNotes();
    void setPlaying(bool playing);
    void setStatusMessage(const QString &message);
    void startPlaybackNow();
    void startRhythmCountdown();
    void cancelCountdown(const QString &message = QString());
    bool isPracticeMode() const;
    bool isRhythmPracticeMode() const;
    RhythmTimingWindows rhythmTimingWindows() const;
    qint64 rhythmWindowTick(int windowMs) const;
    void beginPracticeSession();
    void appendPracticeEvent(const PracticeNoteResult &result);
    void finishPracticeSession(bool completed);
    void refreshPracticeReport();
    QVariantMap practiceReportToVariant(const PracticeReportSummary &report) const;
    void refreshSheetCategories();
    QVariantMap sheetCategoryToVariant(const SheetCategoryInfo &category) const;
    qint64 loopStartTick() const;
    qint64 loopEndTick() const;
    bool tickInsideLoop(qint64 tick) const;
    void resetLoopProgress();
    void resetLoopSegmentBaseline();
    void seekToLoopStart(bool resetStats);
    bool shouldFinishLoopAt(qint64 tick) const;
    void finishLoopIteration();
    bool registerLoopJudgement(const PracticeNoteResult &result);
    void adjustLoopSpeed(int delta, const QString &reason);
    void retriggerAutoNoteStarts(qint64 previousTick, qint64 currentTick);
    qint64 beatToTick(double beat) const;
    double tickToBeat(qint64 tick) const;
    int tickOffsetToMs(qint64 expectedTick, qint64 actualTick) const;
    int velocityForMidi(int midi) const;

    static constexpr int DefaultPpq = 480;

    QString m_songTitle;
    PlaybackEngine m_playbackEngine;
    QVector<NoteEvent> m_notes;

    QString m_mode = QStringLiteral("auto");
    bool m_playing = false;

    QTimer m_timer;
    QTimer m_countdownTimer;
    QElapsedTimer m_frameClock;
    QHash<int, int> m_pressedNotes;
    QSet<int> m_autoNotes;
    QSet<int> m_activeNotes;
    MidiSynth m_synth;
    bool m_silentPracticeEnabled = false;
    bool m_handPracticeEnabled = false;
    QString m_handPracticeSide = QStringLiteral("right");
    int m_handSplitMidi = 60;

    PracticeEngine m_practice;
    PracticeRecordStore m_recordStore;
    PracticeSessionController m_practiceSessions;
    LocalSheetModel m_localSheetModel;
    qint64 m_currentSheetId = -1;
    QVariantMap m_practiceReport;
    QVariantList m_sheetCategories;
    qint64 m_currentSheetCategoryId = 0;

    bool m_loopStartSet = false;
    bool m_loopEndSet = false;
    bool m_loopPracticeEnabled = false;
    qint64 m_loopStartTick = 0;
    qint64 m_loopEndTick = 0;
    int m_loopCorrectPasses = 0;
    int m_loopMistakes = 0;
    int m_loopSegmentMistakeBase = 0;
    bool m_countdownActive = false;
    int m_countdownValue = 0;
    QString m_countdownText;

    QString m_statusMessage;
    qint64 m_positionNotifyAccumulatorMs = 0;
    QString m_localMidiLibraryPath;
};
