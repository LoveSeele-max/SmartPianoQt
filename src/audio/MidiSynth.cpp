#include "audio/MidiSynth.h"

#include <QAudioDevice>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMediaDevices>
#include <QStringList>
#include <QVector>
#include <QtEndian>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int SampleRate = 44100;
constexpr double TwoPi = 6.28318530717958647692;

double midiToFrequency(int midi)
{
    return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

double softClip(double value)
{
    return std::tanh(value * 1.35);
}

bool isSoundFontFile(const QFileInfo &info)
{
    if (!info.exists() || !info.isFile() || !info.isReadable()) return false;
    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("sf2") || suffix == QStringLiteral("sf3");
}

int soundFontPriority(const QFileInfo &info)
{
    const QString lower = info.fileName().toLower();
    if (lower.contains(QStringLiteral("piano")) ||
        lower.contains(QStringLiteral("grand")) ||
        lower.contains(QStringLiteral("keys"))) {
        return 0;
    }
    return 1;
}

void appendSoundFontCandidate(QStringList &candidates, const QFileInfo &info)
{
    if (!isSoundFontFile(info)) return;

    const QString path = info.absoluteFilePath();
    if (!candidates.contains(path)) {
        candidates.push_back(path);
    }
}

QStringList soundFontsFromDir(const QString &dirPath)
{
    QDir dir(dirPath);
    const QFileInfoList files = dir.entryInfoList(
        { QStringLiteral("*.sf3"), QStringLiteral("*.sf2") },
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    QVector<QFileInfo> sorted;
    sorted.reserve(files.size());
    for (const QFileInfo &file : files) {
        sorted.push_back(file);
    }
    std::sort(sorted.begin(), sorted.end(), [](const QFileInfo &a, const QFileInfo &b) {
        const int pa = soundFontPriority(a);
        const int pb = soundFontPriority(b);
        if (pa != pb) return pa < pb;
        return QString::compare(a.fileName(), b.fileName(), Qt::CaseInsensitive) < 0;
    });

    QStringList candidates;
    for (const QFileInfo &file : sorted) {
        appendSoundFontCandidate(candidates, file);
    }
    return candidates;
}

void appendSoundFontsFromPath(QStringList &candidates, const QString &path)
{
    if (path.trimmed().isEmpty()) return;

    const QFileInfo info(path);
    if (!info.exists()) return;

    if (info.isDir()) {
        const QStringList files = soundFontsFromDir(info.absoluteFilePath());
        for (const QString &file : files) {
            if (!candidates.contains(file)) candidates.push_back(file);
        }
        return;
    }

    appendSoundFontCandidate(candidates, info);
}

QString findSoundFontsDir(QDir dir)
{
    for (int i = 0; i < 6; ++i) {
        const QString candidate = dir.absoluteFilePath(QStringLiteral("soundfonts"));
        if (QDir(candidate).exists()) return QDir(candidate).absolutePath();
        if (!dir.cdUp()) break;
    }
    return {};
}

struct fluid_settings_t;
struct fluid_synth_t;

struct FluidSynthApi {
    using NewSettings = fluid_settings_t *(*)();
    using DeleteSettings = void (*)(fluid_settings_t *);
    using SetStr = int (*)(fluid_settings_t *, const char *, const char *);
    using SetNum = int (*)(fluid_settings_t *, const char *, double);
    using SetInt = int (*)(fluid_settings_t *, const char *, int);
    using NewSynth = fluid_synth_t *(*)(fluid_settings_t *);
    using DeleteSynth = void (*)(fluid_synth_t *);
    using SfLoad = int (*)(fluid_synth_t *, const char *, int);
    using ProgramSelect = int (*)(fluid_synth_t *, int, unsigned int, unsigned int, unsigned int);
    using NoteOn = int (*)(fluid_synth_t *, int, int, int);
    using NoteOff = int (*)(fluid_synth_t *, int, int);
    using Cc = int (*)(fluid_synth_t *, int, int, int);
    using AllNotesOff = int (*)(fluid_synth_t *, int);
    using SystemReset = int (*)(fluid_synth_t *);
    using WriteS16 = int (*)(fluid_synth_t *, int, void *, int, int, void *, int, int);

    NewSettings newSettings = nullptr;
    DeleteSettings deleteSettings = nullptr;
    SetStr setStr = nullptr;
    SetNum setNum = nullptr;
    SetInt setInt = nullptr;
    NewSynth newSynth = nullptr;
    DeleteSynth deleteSynth = nullptr;
    SfLoad sfLoad = nullptr;
    ProgramSelect programSelect = nullptr;
    NoteOn noteOn = nullptr;
    NoteOff noteOff = nullptr;
    Cc cc = nullptr;
    AllNotesOff allNotesOff = nullptr;
    SystemReset systemReset = nullptr;
    WriteS16 writeS16 = nullptr;
};

}

class FluidSynthAudioDevice : public QIODevice {
public:
    FluidSynthAudioDevice(FluidSynthApi api, fluid_settings_t *settings, fluid_synth_t *synth, QObject *parent = nullptr)
        : QIODevice(parent), m_api(api), m_settings(settings), m_synth(synth)
    {
        open(QIODevice::ReadOnly);
    }

    ~FluidSynthAudioDevice() override
    {
        stopAll();
        if (m_synth && m_api.deleteSynth) {
            m_api.deleteSynth(m_synth);
        }
        if (m_settings && m_api.deleteSettings) {
            m_api.deleteSettings(m_settings);
        }
    }

    void noteOn(int midi, int velocity, int volume)
    {
        Q_UNUSED(volume)
        if (!m_synth) return;
        const int scaledVelocity = qBound(1, velocity, 127);
        QMutexLocker locker(&m_mutex);
        m_api.noteOff(m_synth, 0, midi);
        m_api.noteOn(m_synth, 0, midi, scaledVelocity);
    }

    void noteOff(int midi)
    {
        if (!m_synth) return;
        QMutexLocker locker(&m_mutex);
        m_api.noteOff(m_synth, 0, midi);
    }

    void stopAll()
    {
        if (!m_synth) return;
        QMutexLocker locker(&m_mutex);
        if (m_api.allNotesOff) {
            m_api.allNotesOff(m_synth, 0);
        } else if (m_api.cc) {
            m_api.cc(m_synth, 0, 123, 0);
        }
    }

    void setVolume(int volume)
    {
        if (!m_synth || !m_api.cc) return;
        QMutexLocker locker(&m_mutex);
        m_api.cc(m_synth, 0, 7, qBound(0, volume, 127));
    }

    qint64 readData(char *data, qint64 maxSize) override
    {
        if (!m_synth || !m_api.writeS16) {
            std::memset(data, 0, size_t(maxSize));
            return maxSize;
        }

        const qint64 frameBytes = 4;
        const qint64 frames = maxSize / frameBytes;
        auto *out = reinterpret_cast<qint16 *>(data);

        QMutexLocker locker(&m_mutex);
        m_api.writeS16(m_synth, int(frames), out, 0, 2, out, 1, 2);

        const qint64 written = frames * frameBytes;
        if (written < maxSize) {
            std::memset(data + written, 0, size_t(maxSize - written));
        }
        return maxSize;
    }

    qint64 writeData(const char *data, qint64 maxSize) override
    {
        Q_UNUSED(data)
        return maxSize;
    }

    qint64 bytesAvailable() const override
    {
        return 8192 + QIODevice::bytesAvailable();
    }

private:
    QMutex m_mutex;
    FluidSynthApi m_api;
    fluid_settings_t *m_settings = nullptr;
    fluid_synth_t *m_synth = nullptr;
};

PianoAudioDevice::PianoAudioDevice(QObject *parent)
    : QIODevice(parent)
{
    open(QIODevice::ReadOnly);
}

void PianoAudioDevice::noteOn(int midi, int velocity, int volume)
{
    Q_UNUSED(volume)
    if (midi < 0 || midi > 127) return;

    QMutexLocker locker(&m_mutex);

    // A fast note-off before re-trigger gives repeated notes a clear hammer attack.
    for (auto &voice : m_voices) {
        if (voice.midi == midi && !voice.releasing) {
            voice.releasing = true;
            voice.releaseAge = 0.0;
            voice.releaseLevel = envelope(voice);
        }
    }

    Voice voice;
    voice.midi = midi;
    voice.frequency = midiToFrequency(midi);
    voice.phase = 0.0;
    voice.age = 0.0;
    voice.gain = qBound(0.0, velocity / 127.0, 1.25);
    m_voices.push_back(voice);

    if (m_voices.size() > 96) {
        m_voices.erase(m_voices.begin(), m_voices.begin() + int(m_voices.size() - 96));
    }
}

void PianoAudioDevice::noteOff(int midi)
{
    QMutexLocker locker(&m_mutex);
    for (auto &voice : m_voices) {
        if (voice.midi == midi && !voice.releasing) {
            voice.releasing = true;
            voice.releaseAge = 0.0;
            voice.releaseLevel = envelope(voice);
        }
    }
}

void PianoAudioDevice::stopAll()
{
    QMutexLocker locker(&m_mutex);
    m_voices.clear();
    m_reverbL = 0.0;
    m_reverbR = 0.0;
}

void PianoAudioDevice::setVolume(int volume)
{
    QMutexLocker locker(&m_mutex);
    m_masterGain = qBound(0.0, double(volume) / 118.0, 1.25);
}

qint64 PianoAudioDevice::readData(char *data, qint64 maxSize)
{
    const qint64 frameBytes = 4;
    const qint64 frames = maxSize / frameBytes;
    auto *out = reinterpret_cast<qint16 *>(data);

    QMutexLocker locker(&m_mutex);
    for (qint64 frame = 0; frame < frames; ++frame) {
        double sample = 0.0;
        for (auto &voice : m_voices) {
            sample += renderVoice(voice);
        }

        // Tiny stereo spread and short ambience make the synthetic piano less dry.
        const double wetL = m_reverbL * 0.16;
        const double wetR = m_reverbR * 0.16;
        m_reverbL = m_reverbL * 0.985 + sample * 0.020;
        m_reverbR = m_reverbR * 0.982 + sample * 0.018;

        const qint16 left = qint16(qBound(-32767.0, softClip(sample + wetL) * 30000.0, 32767.0));
        const qint16 right = qint16(qBound(-32767.0, softClip(sample + wetR) * 30000.0, 32767.0));
        *out++ = qToLittleEndian(left);
        *out++ = qToLittleEndian(right);
    }

    pruneVoices();

    const qint64 written = frames * frameBytes;
    if (written < maxSize) {
        std::memset(data + written, 0, size_t(maxSize - written));
    }
    return maxSize;
}

qint64 PianoAudioDevice::writeData(const char *data, qint64 maxSize)
{
    Q_UNUSED(data)
    return maxSize;
}

qint64 PianoAudioDevice::bytesAvailable() const
{
    return 4096 + QIODevice::bytesAvailable();
}

double PianoAudioDevice::renderVoice(Voice &voice)
{
    const double env = envelope(voice);
    if (env <= 0.00001) {
        voice.age += 1.0 / SampleRate;
        if (voice.releasing) voice.releaseAge += 1.0 / SampleRate;
        return 0.0;
    }

    const double phase = voice.phase;
    const double bright = std::exp(-voice.age * 2.35);
    const double body = std::exp(-voice.age * 0.72);
    const double hammer = std::exp(-voice.age * 55.0) * std::sin(phase * 13.0);

    double sample =
        std::sin(phase) * (0.82 * body) +
        std::sin(phase * 2.01) * (0.23 * bright) +
        std::sin(phase * 3.005) * (0.12 * bright) +
        std::sin(phase * 4.002) * (0.055 * bright) +
        hammer * 0.08;

    sample *= env * voice.gain * m_masterGain * 0.26;

    const double step = TwoPi * voice.frequency / SampleRate;
    voice.phase += step;
    if (voice.phase > TwoPi) {
        voice.phase = std::fmod(voice.phase, TwoPi);
    }

    voice.age += 1.0 / SampleRate;
    if (voice.releasing) {
        voice.releaseAge += 1.0 / SampleRate;
    }

    return sample;
}

double PianoAudioDevice::envelope(const Voice &voice) const
{
    if (voice.releasing) {
        return voice.releaseLevel * std::exp(-voice.releaseAge * 8.0);
    }

    const double attack = qMin(1.0, voice.age / 0.006);
    const double decay = 0.35 + 0.65 * std::exp(-voice.age * 1.55);
    return attack * decay;
}

void PianoAudioDevice::pruneVoices()
{
    m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(), [](const Voice &voice) {
        if (!voice.releasing) return voice.age > 12.0;
        return voice.releaseAge > 1.2 || voice.releaseLevel * std::exp(-voice.releaseAge * 8.0) < 0.0004;
    }), m_voices.end());
}

MidiSynth::MidiSynth(QObject *parent)
    : QObject(parent)
{
    open();
}

MidiSynth::~MidiSynth()
{
    close();
}

QString MidiSynth::soundFontName() const
{
    return m_soundFontPath.isEmpty() ? QString() : QFileInfo(m_soundFontPath).fileName();
}

QStringList MidiSynth::soundFontCandidates() const
{
    QStringList candidates;

    appendSoundFontsFromPath(candidates, qEnvironmentVariable("SMARTPIANO_SOUNDFONT"));
    appendSoundFontsFromPath(candidates, qEnvironmentVariable("SOUNDFONT_PATH"));

    const QString currentDir = findSoundFontsDir(QDir::current());
    if (!currentDir.isEmpty()) {
        const QStringList files = soundFontsFromDir(currentDir);
        for (const QString &file : files) {
            if (!candidates.contains(file)) candidates.push_back(file);
        }
    }

    const QString appDir = findSoundFontsDir(QDir(QCoreApplication::applicationDirPath()));
    if (!appDir.isEmpty() && appDir != currentDir) {
        const QStringList files = soundFontsFromDir(appDir);
        for (const QString &file : files) {
            if (!candidates.contains(file)) candidates.push_back(file);
        }
    }

    return candidates;
}

void MidiSynth::noteOn(int midi, int velocity)
{
    if (!m_available) return;
    const int mappedVelocity = AudioSettings::mapVelocity(velocity, m_velocityCurve);
    if (m_backend == Backend::FluidSynth && m_fluidDevice) {
        m_fluidDevice->noteOn(midi, mappedVelocity, m_volume);
    } else if (m_backend == Backend::InternalPiano && m_internalDevice) {
        m_internalDevice->noteOn(midi, mappedVelocity, m_volume);
    }
}

void MidiSynth::noteOff(int midi)
{
    if (!m_available) return;
    if (m_backend == Backend::FluidSynth && m_fluidDevice) {
        m_fluidDevice->noteOff(midi);
    } else if (m_backend == Backend::InternalPiano && m_internalDevice) {
        m_internalDevice->noteOff(midi);
    }
}

void MidiSynth::stopAll()
{
    if (m_fluidDevice) {
        m_fluidDevice->stopAll();
    }
    if (m_internalDevice) {
        m_internalDevice->stopAll();
    }
}

void MidiSynth::setVolume(int volume)
{
    m_volume = qBound(0, volume, 127);
    if (m_fluidDevice) {
        m_fluidDevice->setVolume(m_volume);
    }
    if (m_internalDevice) {
        m_internalDevice->setVolume(m_volume);
    }
}

bool MidiSynth::loadSoundFont(const QString &path)
{
    const QString resolved = resolveSoundFontPathFrom(path);
    if (resolved.isEmpty()) {
        m_lastOpenError = QStringLiteral("找不到或无法读取 SoundFont：%1").arg(path);
        m_statusText = QStringLiteral("SoundFont 加载失败：%1；当前音色未切换").arg(m_lastOpenError);
        return false;
    }

    m_soundFontOverridePath = resolved;
    reopen();
    return m_backend == Backend::FluidSynth && QFileInfo(m_soundFontPath).absoluteFilePath() == resolved;
}

void MidiSynth::rescanSoundFonts()
{
    m_soundFontOverridePath.clear();
    reopen();
}

void MidiSynth::setVelocityCurve(const QString &curve)
{
    m_velocityCurve = AudioSettings::velocityCurveFromName(curve);
}

bool MidiSynth::setLatencyMode(const QString &mode)
{
    const QString normalized = AudioSettings::normalizeLatencyMode(mode);
    if (m_latencyMode == normalized) return false;

    m_latencyMode = normalized;
    reopen();
    return true;
}

void MidiSynth::open()
{
    m_lastOpenError.clear();
    if (openFluidSynth()) {
        return;
    }
    openInternalPiano();
}

void MidiSynth::close()
{
    stopAll();
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink.reset();
    }
    m_fluidDevice.reset();
    m_internalDevice.reset();
    if (m_fluidLibrary.isLoaded()) {
        m_fluidLibrary.unload();
    }
    m_backend = Backend::None;
    m_available = false;
}

bool MidiSynth::reopen()
{
    close();
    open();
    return m_available;
}

bool MidiSynth::openFluidSynth()
{
    const QString soundFontPath = resolveSoundFontPath();
    if (soundFontPath.isEmpty()) {
        m_lastOpenError = m_soundFontOverridePath.isEmpty()
            ? QStringLiteral("未找到 .sf2 / .sf3 文件")
            : QStringLiteral("找不到或无法读取 SoundFont：%1").arg(m_soundFontOverridePath);
        return false;
    }

    const QString libraryPath = resolveFluidSynthLibraryPath();
    if (libraryPath.isEmpty()) {
        m_lastOpenError = QStringLiteral("未找到 FluidSynth 动态库");
        return false;
    }

#ifdef Q_OS_WIN
    const QString libraryDir = QFileInfo(libraryPath).absolutePath();
    const QString nativeLibraryDir = QDir::toNativeSeparators(libraryDir);
    SetDllDirectoryW(reinterpret_cast<LPCWSTR>(nativeLibraryDir.utf16()));
#endif

    m_fluidLibrary.setFileName(libraryPath);
    if (!m_fluidLibrary.load()) {
        m_lastOpenError = QStringLiteral("FluidSynth 动态库加载失败：%1").arg(m_fluidLibrary.errorString());
        return false;
    }

    auto load = [this](const char *name) {
        return m_fluidLibrary.resolve(name);
    };

    FluidSynthApi api;
    api.newSettings = reinterpret_cast<FluidSynthApi::NewSettings>(load("new_fluid_settings"));
    api.deleteSettings = reinterpret_cast<FluidSynthApi::DeleteSettings>(load("delete_fluid_settings"));
    api.setStr = reinterpret_cast<FluidSynthApi::SetStr>(load("fluid_settings_setstr"));
    api.setNum = reinterpret_cast<FluidSynthApi::SetNum>(load("fluid_settings_setnum"));
    api.setInt = reinterpret_cast<FluidSynthApi::SetInt>(load("fluid_settings_setint"));
    api.newSynth = reinterpret_cast<FluidSynthApi::NewSynth>(load("new_fluid_synth"));
    api.deleteSynth = reinterpret_cast<FluidSynthApi::DeleteSynth>(load("delete_fluid_synth"));
    api.sfLoad = reinterpret_cast<FluidSynthApi::SfLoad>(load("fluid_synth_sfload"));
    api.programSelect = reinterpret_cast<FluidSynthApi::ProgramSelect>(load("fluid_synth_program_select"));
    api.noteOn = reinterpret_cast<FluidSynthApi::NoteOn>(load("fluid_synth_noteon"));
    api.noteOff = reinterpret_cast<FluidSynthApi::NoteOff>(load("fluid_synth_noteoff"));
    api.cc = reinterpret_cast<FluidSynthApi::Cc>(load("fluid_synth_cc"));
    api.allNotesOff = reinterpret_cast<FluidSynthApi::AllNotesOff>(load("fluid_synth_all_notes_off"));
    api.systemReset = reinterpret_cast<FluidSynthApi::SystemReset>(load("fluid_synth_system_reset"));
    api.writeS16 = reinterpret_cast<FluidSynthApi::WriteS16>(load("fluid_synth_write_s16"));

    if (!api.newSettings || !api.deleteSettings || !api.setNum || !api.setInt ||
        !api.newSynth || !api.deleteSynth || !api.sfLoad || !api.programSelect ||
        !api.noteOn || !api.noteOff || !api.writeS16) {
        m_lastOpenError = QStringLiteral("FluidSynth 动态库接口不完整");
        m_fluidLibrary.unload();
        return false;
    }

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        m_lastOpenError = QStringLiteral("没有可用音频输出设备");
        m_fluidLibrary.unload();
        return false;
    }

    m_format.setSampleRate(SampleRate);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Int16);

    if (!device.isFormatSupported(m_format)) {
        m_lastOpenError = QStringLiteral("音频设备不支持 44.1kHz / 16-bit / stereo 输出");
        m_fluidLibrary.unload();
        return false;
    }

    fluid_settings_t *settings = api.newSettings();
    if (!settings) {
        m_lastOpenError = QStringLiteral("FluidSynth 设置创建失败");
        m_fluidLibrary.unload();
        return false;
    }

    api.setNum(settings, "synth.sample-rate", SampleRate);
    api.setNum(settings, "synth.gain", 0.85);
    api.setInt(settings, "synth.polyphony", 256);
    if (api.setStr) {
        api.setStr(settings, "synth.midi-bank-select", "gm");
    }

    fluid_synth_t *synth = api.newSynth(settings);
    if (!synth) {
        m_lastOpenError = QStringLiteral("FluidSynth 合成器创建失败");
        api.deleteSettings(settings);
        m_fluidLibrary.unload();
        return false;
    }

    const int sfid = api.sfLoad(synth, soundFontPath.toUtf8().constData(), 0);
    if (sfid < 0) {
        m_lastOpenError = QStringLiteral("SoundFont 文件加载失败：%1").arg(soundFontPath);
        api.deleteSynth(synth);
        api.deleteSettings(settings);
        m_fluidLibrary.unload();
        return false;
    }
    api.programSelect(synth, 0, unsigned(sfid), 0, 0);
    if (api.cc) {
        api.cc(synth, 0, 7, m_volume);
        api.cc(synth, 0, 10, 64);
    }

    m_fluidDevice = std::make_unique<FluidSynthAudioDevice>(api, settings, synth);
    m_audioSink = std::make_unique<QAudioSink>(device, m_format);
    m_audioSink->setBufferSize(outputBufferSize());
    m_audioSink->start(m_fluidDevice.get());

    m_available = true;
    m_backend = Backend::FluidSynth;
    m_soundFontPath = soundFontPath;
    m_statusText = QStringLiteral("钢琴音色：SoundFont 采样 - %1").arg(QFileInfo(soundFontPath).fileName());
    return true;
}

bool MidiSynth::openInternalPiano()
{
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        m_available = false;
        m_backend = Backend::None;
        m_statusText = QStringLiteral("没有可用音频输出设备");
        return false;
    }

    m_format.setSampleRate(SampleRate);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Int16);

    if (!device.isFormatSupported(m_format)) {
        m_available = false;
        m_backend = Backend::None;
        m_statusText = QStringLiteral("音频设备不支持内置钢琴音色格式");
        return false;
    }

    m_internalDevice = std::make_unique<PianoAudioDevice>();
    m_internalDevice->setVolume(m_volume);
    m_audioSink = std::make_unique<QAudioSink>(device, m_format);
    m_audioSink->setBufferSize(outputBufferSize());
    m_audioSink->start(m_internalDevice.get());

    m_available = true;
    m_backend = Backend::InternalPiano;
    m_statusText = m_lastOpenError.isEmpty()
        ? QStringLiteral("钢琴音色：内置柔和钢琴（未加载 SoundFont）")
        : QStringLiteral("SoundFont 加载失败：%1；已使用内置柔和钢琴").arg(m_lastOpenError);
    return true;
}

QString MidiSynth::resolveSoundFontPath() const
{
    if (!m_soundFontOverridePath.isEmpty()) {
        return resolveSoundFontPathFrom(m_soundFontOverridePath);
    }

    const QStringList candidates = soundFontCandidates();
    if (!candidates.isEmpty()) {
        return candidates.first();
    }

    QString dirPath = findSoundFontsDir(QDir::current());
    if (dirPath.isEmpty()) {
        dirPath = findSoundFontsDir(QDir(QCoreApplication::applicationDirPath()));
    }
    if (dirPath.isEmpty()) {
        QDir dir(QDir::current());
        dir.mkpath(QStringLiteral("soundfonts"));
        dirPath = dir.absoluteFilePath(QStringLiteral("soundfonts"));
    }

    Q_UNUSED(dirPath)
    return {};
}

QString MidiSynth::resolveSoundFontPathFrom(const QString &path) const
{
    QStringList candidates;
    appendSoundFontsFromPath(candidates, path);
    return candidates.isEmpty() ? QString() : candidates.first();
}

int MidiSynth::outputBufferSize() const
{
    return AudioSettings::bufferSizeForLatencyMode(m_latencyMode);
}

QString MidiSynth::resolveFluidSynthLibraryPath() const
{
    const QString envPath = qEnvironmentVariable("FLUIDSYNTH_DLL");
    if (!envPath.isEmpty() && QFileInfo::exists(envPath)) return envPath;

    const QStringList dllNames = {
        QStringLiteral("libfluidsynth-3.dll"),
        QStringLiteral("libfluidsynth-2.dll"),
        QStringLiteral("libfluidsynth.dll")
    };

    auto findInDir = [&dllNames](const QString &dirPath) -> QString {
        if (dirPath.isEmpty()) return {};
        const QDir dir(dirPath);
        for (const QString &name : dllNames) {
            const QString path = dir.absoluteFilePath(name);
            if (QFileInfo::exists(path)) return path;
        }
        return {};
    };

    const QString envPrefix = qEnvironmentVariable("FLUIDSYNTH_PREFIX");
    if (!envPrefix.isEmpty()) {
        const QString direct = findInDir(envPrefix);
        if (!direct.isEmpty()) return direct;

        const QString binPath = QDir(envPrefix).absoluteFilePath(QStringLiteral("bin"));
        const QString fromBin = findInDir(binPath);
        if (!fromBin.isEmpty()) return fromBin;
    }

    const QStringList candidateDirs = {
        QCoreApplication::applicationDirPath(),
        QDir::currentPath()
    };

    for (const QString &dir : candidateDirs) {
        const QString path = findInDir(dir);
        if (!path.isEmpty()) return path;
    }

    const QStringList libraryNames = {
        QStringLiteral("libfluidsynth-3"),
        QStringLiteral("libfluidsynth-2"),
        QStringLiteral("libfluidsynth"),
        QStringLiteral("fluidsynth")
    };
    for (const QString &name : libraryNames) {
        QLibrary probe(name);
        if (probe.load()) {
            const QString path = probe.fileName();
            probe.unload();
            return path;
        }
    }

    return {};
}
