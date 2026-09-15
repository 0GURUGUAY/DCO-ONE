#pragma once

#include "hid/midi.h"
#include <cstddef>
#include <cstdint>

namespace dco {

/**
 * @class MidiOut
 * @brief MIDI output through the shared native USB MIDI handler.
 */
class MidiOut
{
public:
    /// MIDI channel 1 in zero-based form.
    static constexpr uint8_t kDefaultChannel = 0;

    MidiOut();
    ~MidiOut();

    /**
    * Bind the MIDI output to an initialized USB MIDI handler.
    * @param midi     USB MIDI handler shared with the input.
     * @param channel  Zero-based MIDI channel (0..15 => MIDI ch. 1..16).
     */
    void Init(daisy::MidiUsbHandler& midi, uint8_t channel = kDefaultChannel);

    /** Change the target MIDI channel at runtime. */
    void SetChannel(uint8_t channel);

    /** Send a Note On message.  velocity == 0 is handled as Note Off. */
    void SendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);

    /** Send a Note Off message. */
    void SendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);

    /** Send CC 123 (All Notes Off) on the given channel. */
    void SendAllNotesOff(uint8_t channel);

    /** Send MIDI START transport command (0xFA). */
    void SendStart();

    /** Send MIDI STOP transport command (0xFC). */
    void SendStop();

    /** Send MIDI Clock timing message (0xF8) for synchronization. */
    void SendClock();

    /**
    * Send a raw MIDI message over native USB MIDI.
     * Pass data0/data1 as 0xFF to omit those bytes.
     */
    void SendMessage(uint8_t status, uint8_t data0 = 0xFF, uint8_t data1 = 0xFF);

private:
    daisy::MidiUsbHandler* midi_;
    uint8_t           channel_;
};

} // namespace dco
