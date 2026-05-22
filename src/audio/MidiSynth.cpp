#include "audio/MidiSynth.h"

#include <QtGlobal>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

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
    if (!m_available || midi < 0 || midi > 127) return;

    const int scaledVelocity = qBound(1, velocity * m_volume / 127, 127);
    if (m_soundingNotes.contains(midi)) {
        sendShortMessage(0x80, static_cast<unsigned char>(midi), 0);
    }
    m_soundingNotes.insert(midi);
    sendShortMessage(0x90, static_cast<unsigned char>(midi), static_cast<unsigned char>(scaledVelocity));
}

void MidiSynth::noteOff(int midi)
{
    if (!m_available || midi < 0 || midi > 127) return;
    if (!m_soundingNotes.remove(midi)) return;
    sendShortMessage(0x80, static_cast<unsigned char>(midi), 0);
}

void MidiSynth::stopAll()
{
    if (!m_available) return;
    for (int midi : std::as_const(m_soundingNotes)) {
        sendShortMessage(0x80, static_cast<unsigned char>(midi), 0);
    }
    m_soundingNotes.clear();
    sendShortMessage(0xB0, 0x7B, 0);
}

void MidiSynth::setVolume(int volume)
{
    m_volume = qBound(0, volume, 127);
    if (m_available) {
        sendShortMessage(0xB0, 0x07, static_cast<unsigned char>(m_volume));
    }
}

void MidiSynth::open()
{
#ifdef Q_OS_WIN
    HMIDIOUT output = nullptr;
    MMRESULT result = midiOutOpen(&output, MIDI_MAPPER, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        m_available = false;
        m_statusText = QStringLiteral("系统 MIDI 输出不可用");
        return;
    }

    m_output = output;
    m_available = true;
    m_statusText = QStringLiteral("钢琴音色：Windows General MIDI");
    setVolume(m_volume);
    setProgram(0);
#else
    m_available = false;
    m_statusText = QStringLiteral("当前平台暂未接入 MIDI 音源");
#endif
}

void MidiSynth::close()
{
    stopAll();
#ifdef Q_OS_WIN
    if (m_output) {
        midiOutReset(static_cast<HMIDIOUT>(m_output));
        midiOutClose(static_cast<HMIDIOUT>(m_output));
        m_output = nullptr;
    }
#endif
    m_available = false;
}

void MidiSynth::sendShortMessage(unsigned char status, unsigned char data1, unsigned char data2)
{
#ifdef Q_OS_WIN
    if (!m_output) return;
    const DWORD message = DWORD(status) | (DWORD(data1) << 8) | (DWORD(data2) << 16);
    midiOutShortMsg(static_cast<HMIDIOUT>(m_output), message);
#else
    Q_UNUSED(status)
    Q_UNUSED(data1)
    Q_UNUSED(data2)
#endif
}

void MidiSynth::setProgram(int program)
{
    sendShortMessage(0xC0, static_cast<unsigned char>(qBound(0, program, 127)), 0);
}
