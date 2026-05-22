#include "audio/MidiSynth.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QtEndian>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>

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

}

PianoAudioDevice::PianoAudioDevice(QObject *parent)
    : QIODevice(parent)
{
    open(QIODevice::ReadOnly);
}

void PianoAudioDevice::noteOn(int midi, int velocity, int volume)
{
    if (midi < 0 || midi > 127 || volume <= 0) return;

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
    voice.gain = qBound(0.0, (velocity / 127.0) * (volume / 118.0), 1.25);
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

    sample *= env * voice.gain * 0.26;

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

void MidiSynth::noteOn(int midi, int velocity)
{
    if (!m_available || !m_audioDevice) return;
    m_audioDevice->noteOn(midi, qBound(1, velocity, 127), m_volume);
}

void MidiSynth::noteOff(int midi)
{
    if (!m_available || !m_audioDevice) return;
    m_audioDevice->noteOff(midi);
}

void MidiSynth::stopAll()
{
    if (m_audioDevice) {
        m_audioDevice->stopAll();
    }
}

void MidiSynth::setVolume(int volume)
{
    m_volume = qBound(0, volume, 127);
}

void MidiSynth::open()
{
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        m_available = false;
        m_statusText = QStringLiteral("没有可用音频输出设备");
        return;
    }

    m_format.setSampleRate(SampleRate);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Int16);

    if (!device.isFormatSupported(m_format)) {
        m_format = device.preferredFormat();
        if (m_format.sampleFormat() != QAudioFormat::Int16 || m_format.channelCount() < 1) {
            m_available = false;
            m_statusText = QStringLiteral("音频设备不支持内置钢琴音色格式");
            return;
        }
    }

    m_audioDevice = std::make_unique<PianoAudioDevice>();
    m_audioSink = std::make_unique<QAudioSink>(device, m_format);
    m_audioSink->setBufferSize(4096);
    m_audioSink->start(m_audioDevice.get());

    m_available = true;
    m_statusText = QStringLiteral("钢琴音色：内置柔和钢琴");
}

void MidiSynth::close()
{
    stopAll();
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink.reset();
    }
    m_audioDevice.reset();
    m_available = false;
}
