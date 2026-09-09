#pragma once

#include "daisy_seed.h"
#include <cstddef>
#include <cstdint>

namespace dco {

/**
 * @class MidiOut
 * @brief Minimal MIDI output over the existing USB CDC link.
 *
 * Messages are emitted as text frames so the Daisy keeps its CDC connection
 * to the ESP32 bridge alive.  A host can parse lines starting with "MIDI,"
 * and forward the hex bytes to a real MIDI sink.
 *
 * For this first phase only channel voice messages are implemented, fixed
 * on a single MIDI channel (default = channel 1, zero-indexed = 0).
 */
class MidiOut
{
public:
    /// MIDI channel 1 in zero-based form.
    static constexpr uint8_t kDefaultChannel = 0;

    MidiOut();
    ~MidiOut();

    /**
     * Bind the MIDI output to a DaisySeed hardware instance.
     * @param hw       Daisy hardware used for USB CDC output.
     * @param channel  Zero-based MIDI channel (0..15 => MIDI ch. 1..16).
     */
    void Init(daisy::DaisySeed& hw, uint8_t channel = kDefaultChannel);

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
     * Send a raw MIDI message over CDC as a text frame.
     * Format: "MIDI,<status>[,<data0>[,<data1>]]" with hex bytes.
     * Pass data0/data1 as 0xFF to omit those bytes.
     */
    void SendMessage(uint8_t status, uint8_t data0 = 0xFF, uint8_t data1 = 0xFF);

private:
    daisy::DaisySeed* hw_;
    uint8_t           channel_;
};

} // namespace dco
