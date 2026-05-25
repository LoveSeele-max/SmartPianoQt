#include "midi/MidiInputService.h"

#include <QMetaObject>

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

namespace {

constexpr int ActiveSensing = 0xFE;
constexpr int TimingClock = 0xF8;

#ifdef Q_OS_WIN
void CALLBACK midiInputCallback(HMIDIIN, UINT message, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR)
{
    if (message != MIM_DATA || instance == 0) return;

    auto *service = reinterpret_cast<MidiInputService *>(instance);
    const int status = int(param1 & 0xFF);
    const int data1 = int((param1 >> 8) & 0xFF);
    const int data2 = int((param1 >> 16) & 0xFF);
    const MidiInputMessage decoded = MidiInputService::decodeShortMessage(status, data1, data2);

    if (decoded.type == MidiInputMessageType::NoteOn) {
        QMetaObject::invokeMethod(service, [service, decoded]() {
            emit service->noteOn(decoded.midi, decoded.velocity);
        }, Qt::QueuedConnection);
    } else if (decoded.type == MidiInputMessageType::NoteOff) {
        QMetaObject::invokeMethod(service, [service, decoded]() {
            emit service->noteOff(decoded.midi);
        }, Qt::QueuedConnection);
    }
}
#endif

}

MidiInputService::MidiInputService(QObject *parent)
    : QObject(parent)
{
    refreshPorts();
}

MidiInputService::~MidiInputService()
{
    close();
}

void MidiInputService::refreshPorts()
{
    m_inputPorts.clear();

#ifdef Q_OS_WIN
    const UINT count = midiInGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        MIDIINCAPSW caps;
        if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            m_inputPorts.push_back(QString::fromWCharArray(caps.szPname));
        }
    }
    emit portsChanged();
    setStatusText(m_inputPorts.isEmpty()
                      ? QStringLiteral("MIDI 输入：未检测到设备")
                      : QStringLiteral("MIDI 输入：检测到 %1 个设备").arg(m_inputPorts.size()));
#else
    emit portsChanged();
    setStatusText(QStringLiteral("MIDI 输入：当前平台尚未启用 MIDI 后端"));
#endif
}

void MidiInputService::openPort(int index)
{
    if (index < 0 || index >= m_inputPorts.size()) {
        setStatusText(QStringLiteral("MIDI 输入：请选择设备"));
        return;
    }

#ifdef Q_OS_WIN
    close();

    HMIDIIN handle = nullptr;
    const MMRESULT result = midiInOpen(&handle, UINT(index),
                                       reinterpret_cast<DWORD_PTR>(&midiInputCallback),
                                       reinterpret_cast<DWORD_PTR>(this),
                                       CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR || !handle) {
        setStatusText(QStringLiteral("MIDI 输入：设备打开失败"));
        return;
    }

    midiInStart(handle);
    m_nativeHandle = reinterpret_cast<quintptr>(handle);
    m_openPortIndex = index;
    setStatusText(QStringLiteral("MIDI 输入：已连接 %1").arg(m_inputPorts.at(index)));
#else
    Q_UNUSED(index)
    setStatusText(QStringLiteral("MIDI 输入：当前平台尚未启用 MIDI 后端"));
#endif
}

void MidiInputService::close()
{
#ifdef Q_OS_WIN
    if (m_nativeHandle != 0) {
        auto handle = reinterpret_cast<HMIDIIN>(m_nativeHandle);
        midiInStop(handle);
        midiInReset(handle);
        midiInClose(handle);
        m_nativeHandle = 0;
        m_openPortIndex = -1;
    }
#endif
    setStatusText(QStringLiteral("MIDI 输入：已关闭"));
}

MidiInputMessage MidiInputService::decodeShortMessage(int status, int data1, int data2)
{
    const int statusByte = status & 0xFF;
    if (statusByte == ActiveSensing || statusByte == TimingClock) {
        return {};
    }

    const int type = statusByte & 0xF0;
    const int midi = data1 & 0x7F;
    const int velocity = data2 & 0x7F;

    if (type == 0x90) {
        if (velocity == 0) {
            return { MidiInputMessageType::NoteOff, midi, 0 };
        }
        return { MidiInputMessageType::NoteOn, midi, velocity };
    }

    if (type == 0x80) {
        return { MidiInputMessageType::NoteOff, midi, 0 };
    }

    return { MidiInputMessageType::Other, midi, velocity };
}

void MidiInputService::setStatusText(const QString &message)
{
    if (m_statusText == message) return;
    m_statusText = message;
    emit statusChanged();
}
