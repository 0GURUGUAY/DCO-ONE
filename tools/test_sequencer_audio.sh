#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cat > "$work/test.cpp" <<'CPP'
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

constexpr int kMaxChordVoices = 4, kMaxPolySteps = 32;
enum class VoiceSource { NONE, SEQUENCER, MIDI_IN };
enum { POLY_OFF, POLY_NOTE, POLY_ARP, POLY_CHORD, POLY_FIXED };
struct PolyStep { int state = POLY_OFF; int degree = 0; int fixedTranspose = 0; };
struct MenuNode { int selectedIndex = 0; };
struct ChordDef { int count; int intervals[4]; };
const ChordDef kChordDefs[] = {{3, {0, 4, 7, 0}}};
constexpr int kChordDefCount = 1;
PolyStep s_poly_steps[kMaxPolySteps];
MenuNode kPlaySubmenu[8];
bool s_voice_active[4] = {};
VoiceSource s_voice_source[4] = {};
uint8_t s_midi_notes[4] = {255, 255, 255, 255};
uint8_t s_gate_notes[4] = {255, 255, 255, 255};
uint32_t s_gate_note_off_ms[4] = {};
bool s_env1_gate_wanted = false, s_arp_active = false;
uint32_t s_env1_retrigger_count = 0;
int s_gate_length_pct = 100, s_arp_note_count = 0, s_arp_note_index = 0;
int s_arp_intervals[4] = {};
uint32_t s_arp_note_duration_ms = 0, s_arp_last_note_ms = 0;
float s_arp_root_freq = 0, s_vcf_last_note_freq = 440;
float voice_frequency[4] = {}, expected_frequency[4] = {};
uint32_t expected_trigger = 0;
int expected_voices = 0, note_ons = 0, note_offs = 0;
bool check_usb_audio = false, interrupts_blocked = false;
struct ScopedIrqBlocker {
    bool previous = interrupts_blocked;
    ScopedIrqBlocker() { interrupts_blocked = true; }
    ~ScopedIrqBlocker() { interrupts_blocked = previous; }
};
struct System { static uint32_t GetNow() { return 1000; } };
void CheckAudio() {
    assert(!interrupts_blocked);
    if (!check_usb_audio) return;
    assert(s_env1_gate_wanted);
    assert(s_env1_retrigger_count == expected_trigger);
    for (int voice = 0; voice < 4; ++voice) {
        assert(s_voice_active[voice] == (voice < expected_voices));
        if (voice < expected_voices)
            assert(std::fabs(voice_frequency[voice] - expected_frequency[voice]) < 0.01f);
    }
}
struct MidiOut {
    void SendNoteOn(uint8_t, int, int) { ++note_ons; CheckAudio(); }
    void SendNoteOff(uint8_t, int, int) { ++note_offs; assert(!interrupts_blocked); }
    void SendAllNotesOff(int) { assert(!interrupts_blocked); }
} s_midi_out;
void SetVoicePitchAmp(int voice, float frequency, float) { voice_frequency[voice] = frequency; }
int GetPerformanceTransposeDegrees() { return 0; }
float DegreeToFrequencyHz(int degree, int transpose) { return 220 * std::pow(2.f, (degree + transpose) / 12.f); }
uint8_t FrequencyToMidiNote(float frequency) { return std::lround(69 + 12 * std::log2(frequency / 440)); }
uint32_t CurrentStepDurationMs() { return 240; }
CPP

for function in TriggerEnvelope GateLengthMs SilencePolyVoices PlayArpNote TriggerPolyStep; do
    awk -v name="$function" '
        $0 ~ "^static .* " name "\\(" { copying = 1 }
        copying { print }
        copying && /^}/ { exit }
    ' "$root/daisy/src/main.cpp" >> "$work/test.cpp"
done

cat >> "$work/test.cpp" <<'CPP'
void ExpectStep(int state, int voices, int degree) {
    s_poly_steps[0] = {state, degree, 0};
    expected_voices = voices;
    expected_trigger = s_env1_retrigger_count + 1;
    float root_frequency = DegreeToFrequencyHz(degree, 0);
    for (int voice = 0; voice < voices; ++voice)
        expected_frequency[voice] = root_frequency * std::pow(2.f, kChordDefs[0].intervals[voice] / 12.f);
    int previous_ons = note_ons;
    int previous_offs = note_offs;
    int previous_notes = 0;
    for (uint8_t note : s_midi_notes) previous_notes += note != 255;
    check_usb_audio = true;
    TriggerPolyStep(0);
    CheckAudio();
    assert(note_ons - previous_ons == voices);
    assert(note_offs - previous_offs == previous_notes);
}
int main() {
    for (int gate_percent : {0, 50, 100}) {
        s_gate_length_pct = gate_percent;
        ExpectStep(POLY_NOTE, 1, 0);
        ExpectStep(POLY_NOTE, 1, 12);
        ExpectStep(POLY_CHORD, 3, 0);
        ExpectStep(POLY_FIXED, 1, 7);
        ExpectStep(POLY_ARP, 1, 0);
        ++expected_trigger;
        expected_frequency[0] = s_arp_root_freq * std::pow(2.f, 4.f / 12.f);
        PlayArpNote(1);
        CheckAudio();
        uint32_t previous_trigger = s_env1_retrigger_count;
        int previous_messages = note_ons + note_offs;
        s_poly_steps[0].state = POLY_OFF;
        TriggerPolyStep(0);
        assert(!s_env1_gate_wanted);
        assert(s_env1_retrigger_count == previous_trigger);
        assert(note_ons + note_offs == previous_messages);
        assert(s_voice_active[0]);
    }
    std::puts("PASS: atomic note/chord/arp envelope start, one trigger, MIDI counts, OFF release, GT 0/50/100");
}
CPP

clang++ -std=c++17 -include initializer_list -Wall -Wextra -Werror \
    -fsanitize=address,undefined "$work/test.cpp" -o "$work/test"
"$work/test"