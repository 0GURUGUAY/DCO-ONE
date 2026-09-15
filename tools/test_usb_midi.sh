#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/hid"

cat > "$work/hid/midi.h" <<'CPP'
#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>
namespace daisy {
enum MidiMessageType { NoteOff, NoteOn, ControlChange, PitchBend, SystemRealTime };
struct MidiEvent {
    MidiMessageType type;
    uint8_t channel;
    uint8_t data[2];
};
struct MidiUsbHandler {
    std::vector<std::vector<uint8_t>> sent;
    std::deque<MidiEvent> events;
    bool listened = false;
    void SendMessage(uint8_t* bytes, size_t size) {
        sent.emplace_back(bytes, bytes + size);
    }
    void Listen() { listened = true; }
    bool HasEvents() const { return !events.empty(); }
    MidiEvent PopEvent() {
        auto event = events.front();
        events.pop_front();
        return event;
    }
};
}
CPP

cat > "$work/test.cpp" <<'CPP'
#include "midi.h"
#include <cassert>
#include <cstdio>
static daisy::MidiUsbHandler s_usb_midi;
static uint8_t s_midi_in_channel = 2;
static std::vector<std::vector<uint8_t>> received;
static void TriggerMidiNoteOn(uint8_t note, uint8_t velocity) {
    received.push_back({0x90, note, velocity});
}
static void TriggerMidiNoteOff(uint8_t note) {
    received.push_back({0x80, note});
}
static void TriggerMidiAllNotesOff() { received.push_back({0xB0, 123}); }
CPP

sed -n '/^static void ProcessMidiInMessage(uint8_t status, uint8_t data0, uint8_t data1)$/,/^}/p' "$root/daisy/src/main.cpp" >> "$work/test.cpp"
sed -n '/^static void ServiceMidiIn()$/,/^}/p' "$root/daisy/src/main.cpp" >> "$work/test.cpp"

cat >> "$work/test.cpp" <<'CPP'
int main() {
    dco::MidiOut output;
    output.SendClock();
    assert(s_usb_midi.sent.empty());
    output.Init(s_usb_midi);
    output.SendNoteOn(60, 100, 2);
    output.SendNoteOn(60, 0, 2);
    output.SendNoteOff(61, 64, 15);
    output.SendAllNotesOff(2);
    output.SendStart();
    output.SendClock();
    output.SendStop();
    output.SendMessage(0xC2, 7);
    const std::vector<std::vector<uint8_t>> expected = {
        {0x92, 60, 100}, {0x82, 60, 0}, {0x8F, 61, 64}, {0xB2, 123, 0},
        {0xFA}, {0xF8}, {0xFC}, {0xC2, 7}
    };
    assert(s_usb_midi.sent == expected);
    s_usb_midi.events = {
        {daisy::NoteOn, 2, {60, 100}},
        {daisy::NoteOn, 2, {60, 0}},
        {daisy::NoteOff, 2, {61, 64}},
        {daisy::NoteOn, 1, {62, 100}},
        {daisy::ControlChange, 2, {123, 0}},
        {daisy::ControlChange, 1, {123, 0}},
        {daisy::ControlChange, 2, {1, 127}},
        {daisy::PitchBend, 2, {0, 64}},
        {daisy::SystemRealTime, 0, {0, 0}}
    };
    ServiceMidiIn();
    const std::vector<std::vector<uint8_t>> expected_input = {
        {0x90, 60, 100}, {0x80, 60}, {0x80, 61}, {0xB0, 123}
    };
    assert(received == expected_input);
    assert(s_usb_midi.listened && s_usb_midi.events.empty());
    assert(s_usb_midi.sent == expected);
    ServiceMidiIn();
    assert(received == expected_input);
    puts("USB MIDI: raw output, transport/clock, input dispatch, channel filter, velocity-zero note-off and no echo OK");
}
CPP

clang++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined \
    -I"$work" -I"$root/daisy/include" "$root/daisy/src/midi.cpp" "$work/test.cpp" -o "$work/test"
"$work/test"
if grep -Eq 'hw\.(StartLog|PrintLine)|MidiInCdcCallback|ParseMidiInLine' "$root/daisy/src/main.cpp"; then
    echo 'Conflicting CDC transport still present' >&2
    exit 1
fi
grep -q 'midi_config.transport_config.periph = daisy::MidiUsbTransport::Config::INTERNAL;' "$root/daisy/src/main.cpp"
grep -q 's_usb_midi.Init(midi_config);' "$root/daisy/src/main.cpp"
grep -q 's_usb_midi.StartReceive();' "$root/daisy/src/main.cpp"
grep -q 's_midi_out.Init(s_usb_midi, 0);' "$root/daisy/src/main.cpp"
grep -q '        ServiceMidiIn();' "$root/daisy/src/main.cpp"