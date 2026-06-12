#include "test_support.h"

#include "midi/MidiInputService.h"

namespace SmartPianoTest {

void testMidiInputMessageFiltering()
{
    const MidiInputMessage activeSensing = MidiInputService::decodeShortMessage(0xFE, 0, 0);
    expect(activeSensing.type == MidiInputMessageType::Ignored, "MIDI input should ignore active sensing messages");

    const MidiInputMessage timingClock = MidiInputService::decodeShortMessage(0xF8, 0, 0);
    expect(timingClock.type == MidiInputMessageType::Ignored, "MIDI input should ignore timing clock messages");

    const MidiInputMessage noteOn = MidiInputService::decodeShortMessage(0x90, 64, 77);
    expect(noteOn.type == MidiInputMessageType::NoteOn, "MIDI input should decode note-on messages");
    expect(noteOn.midi == 64, "MIDI input should preserve note-on pitch");
    expect(noteOn.velocity == 77, "MIDI input should preserve note-on velocity");

    const MidiInputMessage zeroVelocity = MidiInputService::decodeShortMessage(0x90, 64, 0);
    expect(zeroVelocity.type == MidiInputMessageType::NoteOff, "MIDI input should treat note-on velocity zero as note-off");
}

}
