#include "midi.h"
#include <cmath>

namespace dco {

MidiOut::MidiOut()
    : hw_(nullptr),
      channel_(kDefaultChannel)
{
}

MidiOut::~MidiOut()
{
}

void MidiOut::Init(daisy::DaisySeed& hw, uint8_t channel)
{
    hw_ = &hw;
    SetChannel(channel);
}

void MidiOut::SetChannel(uint8_t channel)
{
    channel_ = channel & 0x0F;
}

void MidiOut::SendMessage(uint8_t status, uint8_t data0, uint8_t data1)
{
    if (hw_ == nullptr)
        return;

    const bool has_data0 = (data0 != 0xFF);
    const bool has_data1 = (data1 != 0xFF);

    if (has_data0 && has_data1)
    {
        hw_->PrintLine("MIDI,%02X,%02X,%02X", status, data0, data1);
    }
    else if (has_data0)
    {
        hw_->PrintLine("MIDI,%02X,%02X", status, data0);
    }
    else
    {
        hw_->PrintLine("MIDI,%02X", status);
    }
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
