#pragma once

#include "audio/MidiSynth.h"
#include "core/Song.h"
#include "practice/PracticeEngine.h"

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
    Q_PROPERTY(int correctCount READ correctCount NOTIFY statsChanged)
    Q_PROPERTY(int wrongCount READ wrongCount NOTIFY statsChanged)
    Q_PROPERTY(int missedCount READ missedCount NOTIFY statsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY audioStatusChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QVariantList localMidiFiles READ localMidiFiles NOTIFY localMidiLibraryChanged)
    Q_PROPERTY(QString localMidiLibraryPath READ localMidiLibraryPath NOTIFY localMidiLibraryChanged)

public:
    explicit PianoController(QObject *parent = nullptr);

    QString songTitle() const { return m_songTitle; }
    int bpm() const { return m_bpm; }
    int playbackSpeed() const { return m_playbackSpeed; }
    double currentBeat() const;
    double totalBeats() const;
    bool isPlaying() const { return m_playing; }
    QString mode() const { return m_mode; }
    QVariantList notes() const;
    QVariantList activeNotes() const;
    QVariantList expectedNotes() const;
    int correctCount() const { return m_practice.correctCount(); }
    int wrongCount() const { return m_practice.wrongCount(); }
    int missedCount() const { return m_practice.missedCount(); }
    QString statusMessage() const { return m_statusMessage; }
    QString audioStatus() const { return m_synth.statusText(); }
    int volume() const { return m_synth.volume(); }
    QVariantList localMidiFiles() const { return m_localMidiFiles; }
    QString localMidiLibraryPath() const { return m_localMidiLibraryPath; }
    const QVector<NoteEvent> &noteEvents() const { return m_notes; }
    qint64 currentTickValue() const { return m_currentTick; }
    qint64 totalTickValue() const { return m_totalTicks; }
    qint64 expectedTickValue() const;
    int ppq() const { return m_ppq; }

public slots:
    void setPlaybackSpeed(int speed);
    void setMode(const QString &mode);
    void setVolume(int volume);

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seekBeat(double beat);
    Q_INVOKABLE void noteOn(int midi);
    Q_INVOKABLE void noteOff(int midi);
    Q_INVOKABLE void loadDemoSong();
    Q_INVOKABLE void loadSheet(const QUrl &url);
    Q_INVOKABLE void refreshLocalMidiLibrary();
    Q_INVOKABLE void loadLocalMidi(int index);
    Q_INVOKABLE void openLocalMidiLibrary();

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
    void volumeChanged();
    void localMidiLibraryChanged();

private slots:
    void onFrame();

private:
    QVariantMap noteToVariant(const NoteEvent &note) const;
    void setSong(Song song);
    void loadJsonSheet(const QString &path);
    void loadMidiFile(const QString &path);
    void preparePracticeAtCurrentPosition();
    void evaluatePracticeNote(int midi);
    void resetPracticeState(bool resetStats, bool resetPlayed = true);
    void refreshActiveNotes();
    void setPlaying(bool playing);
    void setStatusMessage(const QString &message);
    void clampPosition();
    void retriggerAutoNoteStarts(qint64 previousTick, qint64 currentTick);
    qint64 beatToTick(double beat) const;
    double tickToBeat(qint64 tick) const;
    int velocityForMidi(int midi) const;

    static constexpr int DefaultPpq = 480;

    QString m_songTitle;
    int m_bpm = 100;
    int m_playbackSpeed = 100;
    double m_playbackRate = 1.0;
    double m_playbackMsRemainder = 0.0;
    int m_ppq = DefaultPpq;
    QVector<TempoEvent> m_tempos;
    QVector<NoteEvent> m_notes;

    qint64 m_currentTick = 0;
    double m_preciseTick = 0.0;
    qint64 m_totalTicks = DefaultPpq * 8;
    qint64 m_maxNoteDurationTick = DefaultPpq * 2;

    QString m_mode = QStringLiteral("auto");
    bool m_playing = false;

    QTimer m_timer;
    QElapsedTimer m_frameClock;
    QSet<int> m_pressedNotes;
    QSet<int> m_autoNotes;
    QSet<int> m_activeNotes;
    MidiSynth m_synth;

    PracticeEngine m_practice;

    QString m_statusMessage;
    qint64 m_positionNotifyAccumulatorMs = 0;
    QString m_localMidiLibraryPath;
    QVariantList m_localMidiFiles;
};
