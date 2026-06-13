#pragma once

#include "audio/AudioSettings.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QLibrary>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

class PianoAudioDevice : public QIODevice {
    Q_OBJECT

public:
    explicit PianoAudioDevice(QObject *parent = nullptr);

    void noteOn(int midi, int velocity, int volume);
    void noteOff(int midi);
    void stopAll();
    void setVolume(int volume);

    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    qint64 bytesAvailable() const override;

private:
    struct Voice {
        int midi = 60;
        double frequency = 261.625565;
        double phase = 0.0;
        double age = 0.0;
        double releaseAge = 0.0;
        double releaseLevel = 0.0;
        double gain = 0.8;
        bool releasing = false;
    };

    double renderVoice(Voice &voice);
    double envelope(const Voice &voice) const;
    void pruneVoices();

    mutable QMutex m_mutex;
    std::vector<Voice> m_voices;
    double m_reverbL = 0.0;
    double m_reverbR = 0.0;
    double m_masterGain = 1.0;
};

class FluidSynthAudioDevice;

class MidiSynth : public QObject {
    Q_OBJECT

public:
    explicit MidiSynth(QObject *parent = nullptr);
    ~MidiSynth() override;

    bool isAvailable() const { return m_available; }
    QString statusText() const { return m_statusText; }
    QString soundFontPath() const { return m_soundFontPath; }
    QString soundFontName() const;
    QStringList soundFontCandidates() const;
    QString velocityCurve() const { return AudioSettings::velocityCurveName(m_velocityCurve); }
    QString latencyMode() const { return m_latencyMode; }
    int volume() const { return m_volume; }

    void noteOn(int midi, int velocity = 92);
    void noteOff(int midi);
    void stopAll();
    void setVolume(int volume);
    bool loadSoundFont(const QString &path);
    void rescanSoundFonts();
    void setVelocityCurve(const QString &curve);
    bool setLatencyMode(const QString &mode);

private:
    enum class Backend {
        None,
        FluidSynth,
        InternalPiano
    };

    void open();
    void close();
    bool openFluidSynth();
    bool openInternalPiano();
    bool reopen();
    QString resolveSoundFontPath() const;
    QString resolveSoundFontPathFrom(const QString &path) const;
    QString resolveFluidSynthLibraryPath() const;
    int outputBufferSize() const;

    bool m_available = false;
    int m_volume = 118;
    QString m_statusText;
    QString m_soundFontPath;
    QString m_soundFontOverridePath;
    QString m_lastOpenError;
    AudioSettings::VelocityCurve m_velocityCurve = AudioSettings::VelocityCurve::Linear;
    QString m_latencyMode = QStringLiteral("stable");
    Backend m_backend = Backend::None;
    QAudioFormat m_format;
    std::unique_ptr<QAudioSink> m_audioSink;
    std::unique_ptr<PianoAudioDevice> m_internalDevice;
    std::unique_ptr<FluidSynthAudioDevice> m_fluidDevice;
    QLibrary m_fluidLibrary;
};
