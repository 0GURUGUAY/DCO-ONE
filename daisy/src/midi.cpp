#include "midi.h"
#include <cmath>

namespace dco {

MidiOut::MidiOut()
    : midi_(nullptr),
      channel_(kDefaultChannel)
{
}

MidiOut::~MidiOut()
{
}

void MidiOut::Init(daisy::MidiUsbHandler& midi, uint8_t channel)
{
    midi_ = &midi;
    SetChannel(channel);
}

void MidiOut::SetChannel(uint8_t channel)
{
    channel_ = channel & 0x0F;
}

void MidiOut::SendMessage(uint8_t status, uint8_t data0, uint8_t data1)
{
    if (midi_ == nullptr)
        return;

    uint8_t bytes[3] = {status, data0, data1};
    const size_t size = data0 == 0xFF ? 1 : (data1 == 0xFF ? 2 : 3);
    midi_->SendMessage(bytes, size);
}

void MidiOut::SendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel)
{
    if (velocity == 0)
    {
        SendNoteOff(note, 0, channel);
        return;
    }

    SendMessage(0x90 | (channel & 0x0F), note & 0x7F, velocity & 0x7F);
}

void MidiOut::SendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel)
{
    SendMessage(0x80 | (channel & 0x0F), note & 0x7F, velocity & 0x7F);
}

void MidiOut::SendAllNotesOff(uint8_t channel)
{
    // MIDI CC 123 = All Notes Off.
    SendMessage(0xB0 | (channel & 0x0F), 123, 0);
}

void MidiOut::SendStart()
{
    // MIDI transport START (0xFA) - system real-time message, no data bytes
    SendMessage(0xFA);
}

void MidiOut::SendStop()
{
    // MIDI transport STOP (0xFC) - system real-time message, no data bytes
    SendMessage(0xFC);
}

void MidiOut::SendClock()
{
    // MIDI timing clock (0xF8) - sent 24 times per quarter note for synchronization.
    // No data bytes.
    SendMessage(0xF8);
}

} // namespace dco
