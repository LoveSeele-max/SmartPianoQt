#pragma once

#include "parser/MidiFileParser.h"

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
    Q_PROPERTY(int bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
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

public:
    explicit PianoController(QObject *parent = nullptr);

    QString songTitle() const { return m_songTitle; }
    int bpm() const { return m_bpm; }
    double currentBeat() const;
    double totalBeats() const;
    bool isPlaying() const { return m_playing; }
    QString mode() const { return m_mode; }
    QVariantList notes() const;
    QVariantList activeNotes() const;
    QVariantList expectedNotes() const;
    int correctCount() const { return m_correctCount; }
    int wrongCount() const { return m_wrongCount; }
    int missedCount() const { return m_missedCount; }
    QString statusMessage() const { return m_statusMessage; }
    const QVector<NoteEvent> &noteEvents() const { return m_notes; }
    qint64 currentTickValue() const { return m_currentTick; }
    qint64 totalTickValue() const { return m_totalTicks; }
    qint64 expectedTickValue() const;
    int ppq() const { return m_ppq; }

public slots:
    void setBpm(int bpm);
    void setMode(const QString &mode);

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seekBeat(double beat);
    Q_INVOKABLE void noteOn(int midi);
    Q_INVOKABLE void noteOff(int midi);
    Q_INVOKABLE void loadDemoSong();
    Q_INVOKABLE void loadSheet(const QUrl &url);

signals:
    void songChanged();
    void frameChanged();
    void bpmChanged();
    void positionChanged();
    void playbackStateChanged();
    void modeChanged();
    void notesChanged();
    void activeNotesChanged();
    void practiceChanged();
    void statsChanged();
    void statusMessageChanged();

private slots:
    void onFrame();

private:
    QVariantMap noteToVariant(const NoteEvent &note) const;
    void setSong(const QString &title, int bpm, int ppq, QVector<NoteEvent> notes);
    void loadJsonSheet(const QString &path);
    void rebuildPracticeTicks();
    void preparePracticeAtCurrentPosition();
    void evaluatePracticeNote(int midi);
    void advancePracticeTick();
    void resetPracticeState(bool resetStats);
    void refreshActiveNotes();
    void setPlaying(bool playing);
    void setStatusMessage(const QString &message);
    void clampPosition();
    qint64 beatToTick(double beat) const;
    double tickToBeat(qint64 tick) const;
    qint64 msToTicks(qint64 elapsedMs) const;

    static constexpr int DefaultPpq = 480;

    QString m_songTitle;
    int m_bpm = 100;
    int m_ppq = DefaultPpq;
    QVector<NoteEvent> m_notes;
    QVector<qint64> m_practiceTicks;

    qint64 m_currentTick = 0;
    qint64 m_totalTicks = DefaultPpq * 8;
    qint64 m_maxNoteDurationTick = DefaultPpq * 2;

    QString m_mode = QStringLiteral("auto");
    bool m_playing = false;

    QTimer m_timer;
    QElapsedTimer m_frameClock;
    QSet<int> m_pressedNotes;
    QSet<int> m_autoNotes;
    QSet<int> m_activeNotes;

    int m_waitTickIndex = 0;
    QSet<int> m_matchedPracticeNotes;
    int m_correctCount = 0;
    int m_wrongCount = 0;
    int m_missedCount = 0;

    QString m_statusMessage;
    qint64 m_positionNotifyAccumulatorMs = 0;
};
