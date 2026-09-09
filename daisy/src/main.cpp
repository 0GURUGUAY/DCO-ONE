#include "daisy_seed.h"
#include "daisysp.h"
#include "oscillator.h"
#include "filter.h"
#include "input_manager.h"
#include "midi.h"
#include <cmath>
#include <cstring>

using namespace daisy;
using namespace daisysp;
using namespace dco;

// -----------------------------------------------------------------------------
// Phase 1 configuration
// -----------------------------------------------------------------------------
// NOTE: libDaisy's hw.PrintLine() uses a fixed 128-byte internal buffer
// (LOGGER_BUFFER in logger.h) and silently truncates longer lines (marking
// the cut with "$$", never emitting the trailing \r\n). Keep the frame
// ("DCO1,F=###.##,S=" + N * "#.###," ) well under that limit.
static constexpr size_t  kUsbSampleCount        = 12;   // samples per USB frame
static constexpr uint32_t kFreqChangeIntervalMs = 2000; // new random freq every 2s
static constexpr uint32_t kUsbSendIntervalMs    = 100;  // send frame every 100ms
static constexpr float   kFreqMin               = 220.0f;
static constexpr float   kFreqMax               = 880.0f;

// Main menu wheel shown on the ESP32 AMOLED, driven by the MENU encoder
// (D17/D18). Navigation state is streamed to the ESP32 over the USB bridge
// as "NAV,P=<path>", where <path> is a dot-separated list of child indices
// from the root (e.g. "0" = OSC highlighted at root, "0.0" = ONDE highlighted
// inside the OSC submenu, "0.0.1" = SQUARE highlighted inside ONDE's
// waveform list). The ESP32 mirrors the same tree (see esp32/src/main.cpp)
// to resolve the path into labels/colors and always renders it with the
// same wheel format used for the main menu.
static constexpr uint32_t kMenuSendIntervalMs   = 250;  // periodic resync (balanced responsiveness)

// ONDE submenu: waveform types (kept in sync with kWaveformItems below)
enum WaveformType {
    WAVE_SIN = 0,
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_COUNT
};

// Encoder rotation is sampled at audio-block rate (see AudioCallback) and
// consumed once per main-loop tick via GetAndClearSteps(): each returned
// unit is exactly one physical detent, so every consumer below applies it
// 1:1 (1 notch = 1 menu option / 1 poly step / 1 degree), no debounce or
// throttling needed.

// Hardware instance
DaisySeed hw;

// Small voice pool so polymetric-wheel chord steps can sound more than one
// note at once (still just DaisySP Oscillators summed together, no filter/
// envelope per voice yet). Plain single-note steps only ever use voice 0.
static constexpr int kMaxChordVoices = 4;
OscillatorWrapper s_voices[kMaxChordVoices];
static bool        s_voice_active[kMaxChordVoices] = { false, false, false, false };
static uint8_t     s_midi_notes[kMaxChordVoices]   = { 0xFF, 0xFF, 0xFF, 0xFF };
static dco::MidiOut s_midi_out;

// Envelope 1 (amplitude ADSR) using DaisySP
static daisysp::Adsr s_env1_adsr;
static volatile bool s_env1_gate_wanted = false;
static volatile uint32_t s_env1_retrigger_count = 0;
static float s_env1_last_output = 0.0f; // envelope value at the end of the previous audio block, used to control-rate modulate the VCF cutoff below

// VCF block: single post-mix filter (like the single ENV1 ADSR, shared by
// every voice) driven by the DaisySP ladder/SVF filters in filter.h/.cpp.
// Cutoff/Resonance/Drive/Type map straight to the FilterWrapper; Key and Env
// are control-rate modulation amounts applied once per audio block in
// AudioCallback (keytracking against the last triggered note's frequency,
// and against ENV1's own envelope output, since there's no dedicated filter
// envelope yet).
static dco::FilterWrapper s_vcf_filter;
static float s_vcf_keytrack_amount = 0.5f;   // 0..1, from vcf_keytrack (0..100%)
static float s_vcf_env_amount      = 0.0f;   // -1..1, from vcf_env_amt (-100..100%)
static float s_vcf_last_note_freq  = 440.0f; // last note frequency used for keytracking
static float s_vcf_base_cutoff_hz  = 5000.0f; // mirrors kVcfCutoffParam.value, set by ApplyVcfCutoff
static constexpr float kVcfKeytrackRefHz     = 440.0f; // keytrack neutral point (A4)
static constexpr float kVcfEnvOctaveRange    = 4.0f;   // +-100% env amount == +-4 octaves

// Master output volume (set from SYSTEM > Volume)
static float s_master_volume = 1.0f;

// 440Hz tuning test tone state
static bool s_440hz_test_active = false;

// Raw white-noise generator (bypasses voices/ADSR/volume entirely) used to
// test the bare SAI/codec audio path independently of the synth engine --
// see https://github.com/electro-smith/DaisySP/tree/master/Source/Noise.
static daisysp::WhiteNoise s_test_noise;

// Input controls
EncoderReader encoder_main;      // D21/D22 (rotation only, switch separated)
EncoderReader encoder_menu;      // D17/D18 (rotation only, switch separated)
EncoderReader encoder_submenu;   // D19/D20 (rotation only, switch separated)
EncoderReader encoder_prog;      // D15/D16 (rotation only, switch separated)
ButtonReader  button_main_sw;    // D23 (main encoder switch, now standalone)
ButtonReader  button_menu_sw;    // D25 (menu encoder switch, now standalone)
ButtonReader  button_submenu_sw; // D28 (sub-menu encoder switch, now standalone)
ButtonReader  button_prog_sw;    // D24 (prog encoder switch, standalone)
ButtonReader  button_home;       // D12
ButtonReader  button_mute;       // D11
ButtonReader  button_solo;       // D10
ButtonReader  button_trigger;    // D9

// -----------------------------------------------------------------------------
// Lock-free circular sample buffer (single writer in audio IRQ, single reader
// in main loop).  Double the required length to allow snapshot of the most
// recent kUsbSampleCount samples without tearing.
// -----------------------------------------------------------------------------
static float          s_audio_buffer[kUsbSampleCount * 2];
static volatile size_t s_write_idx = 0;
static volatile float  s_current_freq = 440.0f;

// MIDI Clock (0xF8): 24 pulses per quarter note, for DAW sync while playing.
// Ticks are DECIDED sample-accurately inside AudioCallback (a fixed ~1.33ms
// cadence) instead of the main loop, which is NOT fixed-rate (variable-length
// menu/USB sends in between checks caused +-10-30ms jitter on what should be
// a ~20ms period -- more than enough to make a DAW's tempo follower see the
// BPM swinging wildly). AudioCallback only increments a counter (cheap, IRQ-
// safe); the main loop still does the actual SendClock()/PrintLine() work,
// draining the counter every tick (single-writer ISR / single-reader main
// loop, same lock-free pattern as EncoderReader::Poll()/GetAndClearSteps()).
static constexpr float    kAudioSampleRateHz = 48000.0f;
static volatile bool      s_midi_clock_running = false;      // mirrors s_playing, read by ISR
static volatile float     s_midi_clock_samples_per_tick = (60.0f / 120.0f / 24.0f) * kAudioSampleRateHz; // updated from main loop as BPM changes
static float              s_midi_clock_phase = 0.0f;         // ISR-only accumulator
static volatile uint32_t  s_midi_clock_ticks_committed = 0;  // incremented by the ISR
static uint32_t           s_midi_clock_ticks_sent = 0;       // consumed by the main loop

static void PushSample(float sample)
{
    size_t idx = s_write_idx;
    s_audio_buffer[idx] = sample;
    s_write_idx = (idx + 1) % (kUsbSampleCount * 2);
}

static void GetRecentSamples(float* out)
{
    size_t base = (s_write_idx + (kUsbSampleCount * 2) - kUsbSampleCount)
                  % (kUsbSampleCount * 2);
    for (size_t i = 0; i < kUsbSampleCount; ++i)
    {
        out[i] = s_audio_buffer[(base + i) % (kUsbSampleCount * 2)];
    }
}

// Simple deterministic pseudo-random generator (no dynamic allocation).
static float RandomFrequency()
{
    static uint32_t seed = 12345;
    seed = seed * 1103515245u + 12345u;
    float t = static_cast<float>(seed & 0x7FFFu) / 32768.0f;
    return kFreqMin + t * (kFreqMax - kFreqMin);
}

static int32_t RandomInt(int32_t min, int32_t max)
{
    static uint32_t seed = 54321;
    seed = seed * 1103515245u + 12345u;
    if (min >= max)
        return min;
    uint32_t range = static_cast<uint32_t>(max - min + 1);
    return min + static_cast<int32_t>(seed % range);
}

// Audio callback - REAL-TIME SAFE: no allocation, no blocking calls.
void AudioCallback(AudioHandle::InputBuffer in, 
                   AudioHandle::OutputBuffer out, 
                   size_t size)
{
    // Sample encoder quadrature here (~1.3ms @ 48kHz/64) instead of the main
    // loop: the main loop's variable timing (USB sends, flash writes) misses
    // raw quadrature transitions, making "1 notch = 1 step" unreliable.
    encoder_main.Poll();
    encoder_menu.Poll();
    encoder_submenu.Poll();
    encoder_prog.Poll();

    // Sample-accurate MIDI clock tick decision (see declaration above for why
    // this lives here instead of the main loop).
    if (s_midi_clock_running)
    {
        s_midi_clock_phase -= static_cast<float>(size);
        while (s_midi_clock_phase <= 0.0f)
        {
            s_midi_clock_ticks_committed++;
            s_midi_clock_phase += s_midi_clock_samples_per_tick;
        }
    }

    // VCF cutoff modulation (keytrack + ENV1 amount): recomputed once per
    // block instead of per sample -- a ~1.3ms lag on the modulation is
    // inaudible, and this keeps the log2f/powf calls off the per-sample path.
    // Uses s_env1_last_output (the envelope value at the end of the previous
    // block) since there's no dedicated filter envelope yet.
    {
        float keytrackOctaves = log2f(s_vcf_last_note_freq / kVcfKeytrackRefHz) * s_vcf_keytrack_amount;
        float envOctaves = s_vcf_env_amount * s_env1_last_output * kVcfEnvOctaveRange;
        float cutoffHz = s_vcf_base_cutoff_hz * powf(2.0f, keytrackOctaves + envOctaves);
        cutoffHz = cutoffHz < 20.0f ? 20.0f : (cutoffHz > 20000.0f ? 20000.0f : cutoffHz);
        s_vcf_filter.SetCutoff(cutoffHz);
    }

    for (size_t i = 0; i < size; i++)
    {
        // TEST MODE (SYSTEM > 440Hz): raw white noise straight to the DAC,
        // skipping voices/ADSR/master volume, to isolate the SAI/codec path.
        if (s_440hz_test_active)
        {
            float noise = s_test_noise.Process();
            out[0][i] = noise;
            out[1][i] = noise;
            PushSample(noise);
            continue;
        }

        float sample = 0.0f;
        for (int v = 0; v < kMaxChordVoices; ++v)
            if (s_voice_active[v])
                sample += s_voices[v].GetSample();

        // VCF: applied to the raw oscillator mix, before the ENV1/VCA stage.
        sample = s_vcf_filter.Process(sample);

        // Apply ENV1 amplitude envelope (retrigger on new notes)
        bool gate = s_env1_gate_wanted;
        uint32_t retrigger = s_env1_retrigger_count;
        static uint32_t last_retrigger = 0;
        if (retrigger != last_retrigger)
        {
            gate = false;
            last_retrigger = retrigger;
        }
        float envOut = s_env1_adsr.Process(gate);
        s_env1_last_output = envOut;
        sample *= envOut;
        
        // Apply master volume and output to both channels
        sample *= s_master_volume;
        out[0][i] = sample;
        out[1][i] = sample;
        
        // Store for USB display stream
        PushSample(sample);
    }
}

// Map WaveformType enum to DaisySP Oscillator waveform constants
static uint8_t WaveformTypeToDaisySP(WaveformType wave)
{
    switch (wave)
    {
        case WAVE_SIN:
            return daisysp::Oscillator::WAVE_SIN;
        case WAVE_SQUARE:
            return daisysp::Oscillator::WAVE_SQUARE;
        case WAVE_SAW:
            return daisysp::Oscillator::WAVE_RAMP;
        case WAVE_TRIANGLE:
            return daisysp::Oscillator::WAVE_TRI;
        default:
            return daisysp::Oscillator::WAVE_SIN;
    }
}

// -----------------------------------------------------------------------------
// Generic navigable menu tree
// -----------------------------------------------------------------------------
// A single recursive structure drives every level (main menu, submenus,
// value lists like ONDE's waveforms): to add an item at any depth, just add
// an entry to the relevant array below and adjust its count. All levels are
// rendered identically by the ESP32 (same wheel format), which mirrors this
// same tree to resolve the "NAV,P=<path>" frames sent below.
//
// Color ids are shared with the ESP32's palette (0 = default/no override).
// One id per root menu category, matching menu.json's ordering (PLAY, OSC,
// VCF, ENV1, ENV2, LFO, MATRIX, FX, PRESETS, MIDI, SYSTEM).
enum MenuColor : uint8_t {
    COLOR_DEFAULT = 0,
    COLOR_PLAY,
    COLOR_OSC,
    COLOR_VCF,
    COLOR_ENV1,
    COLOR_ENV2,
    COLOR_LFO,
    COLOR_MATRIX,
    COLOR_FX,
    COLOR_PRESETS,
    COLOR_MIDI,
    COLOR_SYSTEM,
};

using MenuApplyFn = void (*)(int32_t selectedIndex);

// A leaf editable numeric value (e.g. Tempo/BPM): edited in place with the
// same encoder_menu/button_menu_sw used for navigation (see the numeric edit
// mode in the main loop below).
struct NumericParam {
    int32_t     minValue;
    int32_t     maxValue;
    int32_t     step;
    int32_t     value;
    const char* unit;
};

struct MenuNode {
    const char*   label;
    uint8_t       color;
    MenuNode*     children;      // nullptr if this node has no submenu yet
    uint8_t       childCount;
    MenuApplyFn   onSelect;      // live-apply callback while this node's children are browsed
    int32_t       selectedIndex; // persists across visits
    NumericParam* numeric;       // non-null => leaf is an editable numeric value (e.g. BPM)
};

// Forward declarations for apply callbacks referenced by the generated menu.
static void ApplyPlayAuto(int32_t index);
static void ApplyOscCoarse(int32_t index);
static void ApplyOscFine(int32_t index);
static void ApplyOscPulse(int32_t index);
static void ApplyOscSub(int32_t index);
static void ApplyOscHard(int32_t index);
static void ApplyVcfType(int32_t index);
static void ApplyVcfCutoff(int32_t index);
static void ApplyVcfResonance(int32_t index);
static void ApplyVcfKey(int32_t index);
static void ApplyVcfDrive(int32_t index);
static void ApplyVcfEnv(int32_t index);
static void ApplyEnv1Attack(int32_t index);
static void ApplyEnv1Decay(int32_t index);
static void ApplyEnv1Sustain(int32_t index);
static void ApplyEnv1Release(int32_t index);
static void ApplyEnv2Attack(int32_t index);
static void ApplyEnv2Decay(int32_t index);
static void ApplyEnv2Sustain(int32_t index);
static void ApplyEnv2Release(int32_t index);
static void ApplyLfo1Rate(int32_t index);
static void ApplyLfo1Amp(int32_t index);
static void ApplyLfo1Phase(int32_t index);
static void ApplyLfo2Rate(int32_t index);
static void ApplyLfo2Amp(int32_t index);
static void ApplyLfo2Phase(int32_t index);
static void ApplyMatSlot1Amt(int32_t index);
static void ApplyMatSlot2Amt(int32_t index);
static void ApplyFxDryWet(int32_t index);
static void ApplyFxTime(int32_t index);
static void ApplyFxFb(int32_t index);
static void ApplyMidiChannel(int32_t index);
static void ApplyMidiBend(int32_t index);
static void ApplySysLuminosite(int32_t index);
static void ApplySysVolume(int32_t index);
static void ApplySys440Hz(int32_t index);

static void ApplyWaveform(int32_t index);

#include "menu_generated.inc"

static void ApplyWaveform(int32_t index)
{
    WaveformType wave = static_cast<WaveformType>(((index % WAVE_COUNT) + WAVE_COUNT) % WAVE_COUNT);
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_voices[v].SetWaveform(WaveformTypeToDaisySP(wave));
}

// Apply callbacks for numeric parameters
static void ApplyPlayAuto(int32_t index) {}
static void ApplyOscCoarse(int32_t index) {}
static void ApplyOscFine(int32_t index) {}
static void ApplyOscPulse(int32_t index) {}
static void ApplyOscSub(int32_t index) {}
static void ApplyOscHard(int32_t index) {}

// VCF: Filter type list (LP24/LP12/BP12/HP12/NOTCH), maps 1:1 to dco::FilterType.
static void ApplyVcfType(int32_t index)
{
    dco::FilterType type = static_cast<dco::FilterType>(
        ((index % 5) + 5) % 5);
    s_vcf_filter.SetType(type);
}

static void ApplyVcfCutoff(int32_t index)
{
    s_vcf_base_cutoff_hz = static_cast<float>(index);
}

static void ApplyVcfResonance(int32_t index)
{
    s_vcf_filter.SetResonance(static_cast<float>(index) * 0.01f);
}

// Keytrack amount is applied live in AudioCallback's per-block cutoff
// modulation (see s_vcf_keytrack_amount); nothing to push to the filter here.
static void ApplyVcfKey(int32_t index)
{
    s_vcf_keytrack_amount = static_cast<float>(index) * 0.01f;
}

static void ApplyVcfDrive(int32_t index)
{
    s_vcf_filter.SetDrive(static_cast<float>(index) * 0.01f);
}

// Env amount (-100..100%) is applied live in AudioCallback alongside keytrack.
static void ApplyVcfEnv(int32_t index)
{
    s_vcf_env_amount = static_cast<float>(index) * 0.01f;
}
static void ApplyEnv1Attack(int32_t index)
{
    (void)index;
    s_env1_adsr.SetTime(daisysp::ADSR_SEG_ATTACK, static_cast<float>(kEnv1AttackParam.value) / 1000.0f);
}

static void ApplyEnv1Decay(int32_t index)
{
    (void)index;
    s_env1_adsr.SetTime(daisysp::ADSR_SEG_DECAY, static_cast<float>(kEnv1DecayParam.value) / 1000.0f);
}

static void ApplyEnv1Sustain(int32_t index)
{
    (void)index;
    float level = static_cast<float>(kEnv1SustainParam.value) * 0.01f;
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    s_env1_adsr.SetSustainLevel(level);
}

static void ApplyEnv1Release(int32_t index)
{
    (void)index;
    s_env1_adsr.SetTime(daisysp::ADSR_SEG_RELEASE, static_cast<float>(kEnv1ReleaseParam.value) / 1000.0f);
}
static void ApplyEnv2Attack(int32_t index) {}
static void ApplyEnv2Decay(int32_t index) {}
static void ApplyEnv2Sustain(int32_t index) {}
static void ApplyEnv2Release(int32_t index) {}
static void ApplyLfo1Rate(int32_t index) {}
static void ApplyLfo1Amp(int32_t index) {}
static void ApplyLfo1Phase(int32_t index) {}
static void ApplyLfo2Rate(int32_t index) {}
static void ApplyLfo2Amp(int32_t index) {}
static void ApplyLfo2Phase(int32_t index) {}
static void ApplyMatSlot1Amt(int32_t index) {}
static void ApplyMatSlot2Amt(int32_t index) {}
static void ApplyFxDryWet(int32_t index) {}
static void ApplyFxTime(int32_t index) {}
static void ApplyFxFb(int32_t index) {}
static void ApplyMidiChannel(int32_t index) {}
static void ApplyMidiBend(int32_t index) {}
static void ApplySysLuminosite(int32_t index) {}

// kSysVolumeParam is a 0..100 percentage; map it linearly to gain 0.0..1.0.
// (Previously this treated `index` as a 0..3 enum via `% 4`, which mapped
// most percentage values -- including the default 100 -- to silence.)
static void ApplySysVolume(int32_t index)
{
    float gain = static_cast<float>(index) * 0.01f;
    if (gain < 0.0f) gain = 0.0f;
    else if (gain > 1.0f) gain = 1.0f;
    s_master_volume = gain;
}

// Navigation stack: s_menu_stack[d] is the node whose children are currently
// browsed at depth d; s_menu_depth is the current depth (0 = root wheel).
static constexpr int kMaxMenuDepth = 6;
static MenuNode*     s_menu_stack[kMaxMenuDepth] = { &kRootNode };
static int           s_menu_depth = 0;

// Numeric edit mode (e.g. BPM): overlays the current depth, reusing
// encoder_menu (adjusts the value) and button_menu_sw (confirms) instead of
// browsing/drilling. HOME cancels and restores the pre-edit value.
static bool          s_editing_numeric = false;
static MenuNode*     s_editing_node = nullptr;
static int32_t       s_editing_original_value = 0;

// PLAY/STOP transport state, toggled by button_trigger. Drives the
// polymetric step sequencer below (SendPlayStatus still mirrors it to the
// ESP32's root-wheel hub icon).
static bool          s_playing = false;

// -----------------------------------------------------------------------------
// Polymetric step wheel (overlay, opened by button_main_sw from any screen)
// -----------------------------------------------------------------------------
// Each step has 3 states cycled by button_main_sw: not played (gray), played
// (yellow), played+chord (green). `degree` is a scale-degree offset (0 = the
// PLAY submenu's confirmed Root note), resolved against the active Scale via
// kScaleDefs below. encoder_main moves the edit cursor; encoder_submenu
// adjusts the cursor step's degree; button_submenu_sw confirms and advances
// to the next step. Step count mirrors kPlayStepParam.value (1..32) live, so
// the array is sized to its max instead of being reallocated.
enum PolyStepState : uint8_t { POLY_OFF = 0, POLY_NOTE = 1, POLY_ARP = 2, POLY_CHORD = 3 };

struct PolyStep {
    uint8_t state;
    int8_t  degree;
};

static constexpr int kMaxPolySteps = 32; // == kPlayStepParam.maxValue
static PolyStep      s_poly_steps[kMaxPolySteps] = {};
static bool          s_poly_wheel_active = false; // overlay currently shown on ESP32
static int           s_poly_cursor    = 0;        // step edited by encoder_main/encoder_submenu
static int           s_poly_play_step = -1;       // step currently sounding, -1 = stopped

// Arpeggio state for a CHORD step: its notes are played one at a time in
// sequence (voice 0 reused monophonically) instead of stacked as a chord.
static bool     s_arp_active           = false;
static int8_t   s_arp_intervals[kMaxChordVoices] = {};
static int      s_arp_note_count       = 0;
static int      s_arp_note_index       = 0;
static float    s_arp_root_freq        = 0.0f;
static uint32_t s_arp_note_duration_ms = 0;
static uint32_t s_arp_last_note_ms     = 0;

// Performance transposition in scale degrees: encoder_prog adjusts it when
// AUTO is off; AUTO mode picks a random value every N sequencer steps.
static int32_t       s_prog_transpose_degrees = 0;
static int32_t       s_auto_steps_until_change = 0;
static constexpr int32_t kMaxProgTransposeDegrees = 24;

// -----------------------------------------------------------------------------
// Persistent settings (QSPI flash): every parameter value and every menu
// selection survives a power cycle. A single implicit slot is used for now --
// named, user-selectable presets (multiple slots) are a future PRESETS-menu
// feature, not implemented here.
// -----------------------------------------------------------------------------
static constexpr uint32_t kSettingsMagic          = 0x44434F31; // "DCO1"
static constexpr uint32_t kSettingsSaveDebounceMs = 1500; // idle time before flushing to flash
static constexpr int      kMaxPersistedValues     = 48;
static constexpr int      kMaxPersistedSelections = 48;

struct SynthSettings
{
    uint32_t magic         = kSettingsMagic;
    uint32_t numValues     = 0;
    uint32_t numSelections = 0;
    uint32_t numPolySteps  = 0;  // Persist polymetric step setup
    int32_t  values[kMaxPersistedValues]         = {};
    int32_t  selections[kMaxPersistedSelections] = {};
    PolyStep polySteps[kMaxPolySteps]            = {};  // Full polymetric step state

    bool operator==(const SynthSettings& rhs) const
    {
        return memcmp(this, &rhs, sizeof(SynthSettings)) == 0;
    }
    bool operator!=(const SynthSettings& rhs) const { return !(*this == rhs); }
};

static PersistentStorage<SynthSettings> settings_storage(hw.qspi);
static bool     s_settings_dirty          = false;
static uint32_t s_last_settings_change_ms = 0;

// Walks the whole static menu tree in one fixed, deterministic order so the
// same array slot always corresponds to the same parameter on both save and
// load. kCollect reads live state into `s`; kApply writes `s` back into the
// live state (and re-fires any onSelect callback, e.g. ONDE's waveform, so
// the audio engine reflects the restored value immediately).
enum class SettingsWalkMode { kCollect, kApply };

static void WalkMenuTree(MenuNode* node, SettingsWalkMode mode, SynthSettings& s,
                         uint32_t& valIdx, uint32_t& selIdx)
{
    if (node->numeric != nullptr)
    {
        if (valIdx < kMaxPersistedValues)
        {
            if (mode == SettingsWalkMode::kCollect)
                s.values[valIdx] = node->numeric->value;
            else
                node->numeric->value = s.values[valIdx];
        }
        ++valIdx;
    }
    if (node->childCount > 0)
    {
        if (selIdx < kMaxPersistedSelections)
        {
            if (mode == SettingsWalkMode::kCollect)
            {
                s.selections[selIdx] = node->selectedIndex;
            }
            else
            {
                int32_t idx = s.selections[selIdx];
                node->selectedIndex = idx < 0 ? 0
                                    : (idx >= node->childCount ? node->childCount - 1 : idx);
                if (node->onSelect)
                    node->onSelect(node->selectedIndex);
            }
        }
        ++selIdx;
        for (uint8_t i = 0; i < node->childCount; ++i)
            WalkMenuTree(&node->children[i], mode, s, valIdx, selIdx);
    }
}

// Persist/restore the full polymetric step wheel state (all steps, all states & degrees)
static void SyncPolyStepsState(SettingsWalkMode mode, SynthSettings& s)
{
    if (mode == SettingsWalkMode::kCollect)
    {
        s.numPolySteps = kMaxPolySteps;
        for (int i = 0; i < kMaxPolySteps; ++i)
            s.polySteps[i] = s_poly_steps[i];
    }
    else
    {
        // Sanitize: flash data saved by an older firmware layout (before
        // these fields existed) can leave garbage bytes here, which must
        // never be interpreted as a valid state/degree (see TriggerPolyStep).
        // Legacy state '2' (old CHORD) is migrated to the new CHORD value.
        for (int i = 0; i < kMaxPolySteps && i < s.numPolySteps; ++i)
        {
            PolyStep loaded = s.polySteps[i];
            if (loaded.state == 2)
                loaded.state = POLY_CHORD;
            else if (loaded.state > POLY_CHORD)
                loaded.state = POLY_OFF;
            if (loaded.degree < -14 || loaded.degree > 14)
                loaded.degree = 0;
            s_poly_steps[i] = loaded;
        }
    }
}

static void MarkSettingsDirty(uint32_t now)
{
    s_settings_dirty          = true;
    s_last_settings_change_ms = now;
}

// Flushes the current parameter/selection state to flash a short idle period
// after the last change (avoids wearing the flash on every encoder step).
static void SyncSettingsIfDirty(uint32_t now)
{
    if (!s_settings_dirty || now - s_last_settings_change_ms < kSettingsSaveDebounceMs)
        return;
    SynthSettings& live = settings_storage.GetSettings();
    uint32_t vi = 0, si = 0;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kCollect, live, vi, si);
    SyncPolyStepsState(SettingsWalkMode::kCollect, live);  // Also save poly steps
    live.magic         = kSettingsMagic;
    live.numValues     = vi;
    live.numSelections = si;
    settings_storage.Save();
    s_settings_dirty = false;
}

// -----------------------------------------------------------------------------
// Poly step wheel: scale/chord tables + note-triggering helpers
// -----------------------------------------------------------------------------
// Semitone offsets from the scale's own root, one row per kPlayScaleOptions
// entry (same order/count). Chromatic uses all 12 semitones as its "degrees".
struct ScaleDef {
    int8_t  semitones[12];
    uint8_t length;
};
static const ScaleDef kScaleDefs[] = {
    { { 0, 2, 4, 5, 7, 9, 11 },                        7 }, // M (Majeure)
    { { 0, 2, 3, 5, 7, 8, 10 },                        7 }, // m (mineure naturelle)
    { { 0, 2, 3, 5, 7, 8, 11 },                        7 }, // m harm
    { { 0, 2, 3, 5, 7, 9, 11 },                        7 }, // m melo
    { { 0, 2, 4, 7, 9 },                               5 }, // Pent M
    { { 0, 3, 5, 7, 10 },                              5 }, // Pent m
    { { 0, 3, 5, 6, 7, 10 },                           6 }, // Blues
    { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },        12 }, // Chrom
};
static constexpr int kScaleDefCount = sizeof(kScaleDefs) / sizeof(kScaleDefs[0]);

// Semitone intervals above a chord's own root, one row per kPlayChordsOptions
// entry (same order/count). "Mono" is a single note (chord state falls back
// to it only if it were ever selected, which the UI doesn't expose).
struct ChordDef {
    int8_t  intervals[4];
    uint8_t count;
};
static const ChordDef kChordDefs[] = {
    { { 0, 0, 0, 0 },  1 }, // Mono
    { { 0, 4, 7, 0 },  3 }, // Triad
    { { 0, 4, 7, 10 }, 4 }, // 7 dom
    { { 0, 4, 7, 11 }, 4 }, // M7
    { { 0, 3, 7, 10 }, 4 }, // m7
    { { 0, 2, 7, 0 },  3 }, // Sus2
    { { 0, 5, 7, 0 },  3 }, // Sus4
    { { 0, 3, 6, 0 },  3 }, // Dim
    { { 0, 4, 8, 0 },  3 }, // Augm
    { { 0, 7, 0, 0 },  2 }, // Power
};
static constexpr int kChordDefCount = sizeof(kChordDefs) / sizeof(kChordDefs[0]);

// Returns the current performance transpose in scale degrees.
// In manual mode (AUTO == 0) this follows encoder_prog; in AUTO mode the
// sequencer updates it periodically to a fresh random value.
static int32_t GetPerformanceTransposeDegrees()
{
    return s_prog_transpose_degrees;
}

// Resolves a scale-degree offset (0 = confirmed PLAY Root note) against the
// confirmed PLAY Scale + Root + the global Oct (semitone) transpose, wrapping
// extra degrees into further octaves the same way array indices wrap.
// The per-step degree is added to the performance transpose (manual or AUTO).
static float DegreeToFrequencyHz(int32_t degree)
{
    int32_t rootIdx  = kPlaySubmenu[1].selectedIndex; // Root: 0=C..11=B
    int32_t scaleIdx = kPlaySubmenu[2].selectedIndex; // Scale
    const ScaleDef& scale = kScaleDefs[((scaleIdx % kScaleDefCount) + kScaleDefCount) % kScaleDefCount];
    int32_t len = scale.length;
    int32_t totalDegree = degree + GetPerformanceTransposeDegrees();
    int32_t idx = totalDegree % len;
    int32_t oct = totalDegree / len;
    if (idx < 0)
    {
        idx += len;
        oct -= 1;
    }
    int32_t semitoneFromA4 = (rootIdx - 9) + scale.semitones[idx] + 12 * oct
                            + static_cast<int32_t>(kPlayOctParam.value);
    return 440.0f * powf(2.0f, static_cast<float>(semitoneFromA4) / 12.0f);
}

// Convert a frequency in Hz to the nearest MIDI note number (A4 = 69).
static uint8_t FrequencyToMidiNote(float freq)
{
    if (freq <= 0.0f)
        return 0;

    float note_f = 69.0f + 12.0f * log2f(freq / 440.0f);
    int   note   = static_cast<int>(note_f + 0.5f);
    if (note < 0)
        note = 0;
    else if (note > 127)
        note = 127;

    return static_cast<uint8_t>(note);
}

static void TriggerEnvelope(bool on)
{
    if (on)
    {
        s_env1_gate_wanted = true;
        s_env1_retrigger_count++;
    }
    else
    {
        s_env1_gate_wanted = false;
    }
}

static void SilencePolyVoices()
{
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_voice_active[v] = false;
}

// Hard silence + MIDI panic: turns off every voice, sends NoteOff for all
// notes we think are active, and emits an All Notes Off controller message.
// Called on transport stop and on gray (OFF) steps to guarantee no stuck note.
static void PanicSilence()
{
    TriggerEnvelope(false);
    s_arp_active = false;
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        if (s_midi_notes[v] != 0xFF)
        {
            s_midi_out.SendNoteOff(s_midi_notes[v], 0, 0);
            s_midi_notes[v] = 0xFF;
        }
    }
    s_midi_out.SendAllNotesOff(0);
    SilencePolyVoices();
}

static void SendPlayStatus();  // forward declaration for ApplySys440Hz

// SYSTEM > 440Hz action: toggle a raw white-noise test tone straight to the
// DAC (see AudioCallback), bypassing voices/ADSR/volume entirely to isolate
// the bare SAI/codec audio path. If a sequence is playing, it is stopped first.
static void ApplySys440Hz(int32_t index)
{
    (void)index;

    if (s_440hz_test_active)
    {
        s_440hz_test_active = false;
        return;
    }

    if (s_playing)
    {
        s_playing = false;
        s_midi_clock_running = false;
        s_midi_out.SendStop();
        SendPlayStatus();
    }
    PanicSilence();

    s_440hz_test_active = true;
}

static float PolyDivisionBeats(int32_t divIndex); // defined below; needed by CurrentStepDurationMs

// Current sequencer step length in ms (BPM + the PLAY submenu's Div setting),
// used to spread a CHORD step's arpeggio notes evenly across the step.
static uint32_t CurrentStepDurationMs()
{
    int32_t  divIdx = kPlaySubmenu[7].selectedIndex; // Div
    float    bpm    = static_cast<float>(kBpmParam.value);
    uint32_t ms     = static_cast<uint32_t>((60000.0f / bpm) * PolyDivisionBeats(divIdx));
    return ms < 1 ? 1 : ms;
}

// Sounds one note of the currently-armed arpeggio (voice 0 reused
// monophonically), turning off whatever note voice 0 was previously playing.
static void PlayArpNote(int index)
{
    if (s_midi_notes[0] != 0xFF)
        s_midi_out.SendNoteOff(s_midi_notes[0], 0, 0);

    float freq = s_arp_root_freq * powf(2.0f, s_arp_intervals[index] / 12.0f);
    s_voices[0].SetFrequency(freq);
    s_voice_active[0] = true;
    s_midi_notes[0]   = FrequencyToMidiNote(freq);
    s_midi_out.SendNoteOn(s_midi_notes[0], 100, 0);
    s_vcf_last_note_freq = freq; // VCF keytrack reference (see AudioCallback)
}

// Sounds `stepIndex` on the voice pool: a plain NOTE step uses voice 0 only;
// an ARPEGGIO step cycles the PLAY submenu's currently-selected Chords
// intervals one note at a time over the step's duration (see the main loop's
// arpeggio advance); a CHORD step plays all of those intervals simultaneously.
// An OFF step silences every voice.
static void TriggerPolyStep(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= kMaxPolySteps)
        return;
    const PolyStep& step = s_poly_steps[stepIndex];

    // Snapshot currently-sounding notes and mark the internal state as "none
    // active" before sending NoteOffs. This guarantees a NoteOff is emitted
    // for every note that was previously triggered, even if this step is OFF.
    uint8_t prev_notes[kMaxChordVoices];
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        prev_notes[v] = s_midi_notes[v];
        s_midi_notes[v] = 0xFF;
    }
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        if (prev_notes[v] != 0xFF)
            s_midi_out.SendNoteOff(prev_notes[v], 0, 0);
    }

    s_arp_active = false;

    if (step.state == POLY_OFF)
    {
        // Explicit MIDI panic for gray/inactive steps: some hosts need the
        // All Notes Off controller to fully clear a hanging note.
        s_midi_out.SendAllNotesOff(0);
        SilencePolyVoices();
        TriggerEnvelope(false);
        return;
    }

    float rootFreq = DegreeToFrequencyHz(step.degree);
    s_vcf_last_note_freq = rootFreq; // VCF keytrack reference (see AudioCallback)
    if (step.state == POLY_CHORD)
    {
        int32_t chordIdx = kPlaySubmenu[3].selectedIndex;
        const ChordDef& chord = kChordDefs[((chordIdx % kChordDefCount) + kChordDefCount) % kChordDefCount];
        int noteCount = chord.count < kMaxChordVoices ? chord.count : kMaxChordVoices;

        for (int v = 0; v < kMaxChordVoices; ++v)
        {
            if (v < noteCount)
            {
                float freq = rootFreq * powf(2.0f, chord.intervals[v] / 12.0f);
                s_voices[v].SetFrequency(freq);
                s_voice_active[v] = true;
                s_midi_notes[v]   = FrequencyToMidiNote(freq);
                s_midi_out.SendNoteOn(s_midi_notes[v], 100, 0);
            }
            else
            {
                s_voice_active[v] = false;
                s_midi_notes[v]   = 0xFF;
            }
        }
        TriggerEnvelope(true);
    }
    else if (step.state == POLY_ARP)
    {
        int32_t chordIdx = kPlaySubmenu[3].selectedIndex;
        const ChordDef& chord = kChordDefs[((chordIdx % kChordDefCount) + kChordDefCount) % kChordDefCount];
        int noteCount = chord.count < kMaxChordVoices ? chord.count : kMaxChordVoices;

        for (int v = 1; v < kMaxChordVoices; ++v)
        {
            s_voice_active[v] = false;
            s_midi_notes[v]   = 0xFF;
        }

        s_arp_root_freq  = rootFreq;
        s_arp_note_count = noteCount;
        for (int i = 0; i < noteCount; ++i)
            s_arp_intervals[i] = chord.intervals[i];
        s_arp_note_duration_ms = CurrentStepDurationMs() / static_cast<uint32_t>(noteCount);
        if (s_arp_note_duration_ms < 1)
            s_arp_note_duration_ms = 1;
        s_arp_note_index   = 0;
        s_arp_last_note_ms = System::GetNow();
        s_arp_active       = noteCount > 1; // a 1-note arpeggio just plays like a NOTE step

        PlayArpNote(0);
        TriggerEnvelope(true);
    }
    else if (step.state == POLY_NOTE)
    {
        s_voices[0].SetFrequency(rootFreq);
        s_voice_active[0] = true;
        s_midi_notes[0]   = FrequencyToMidiNote(rootFreq);
        s_midi_out.SendNoteOn(s_midi_notes[0], 100, 0);
        for (int v = 1; v < kMaxChordVoices; ++v)
        {
            s_voice_active[v] = false;
            s_midi_notes[v]   = 0xFF;
        }
        TriggerEnvelope(true);
    }
    else
    {
        // Unrecognized/corrupt state (e.g. stale flash data): stay silent
        // instead of defaulting to a note.
        s_midi_out.SendAllNotesOff(0);
        SilencePolyVoices();
        TriggerEnvelope(false);
    }
}

// Beats-per-step for each kPlayPolyDivOptions entry (same order/count),
// assuming a 4/4 whole note = 4 beats.
static float PolyDivisionBeats(int32_t divIndex)
{
    static const float kBeatsPerStep[] = {
        4.0f, 2.0f, 1.0f, 2.0f / 3.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f
    };
    constexpr int n = sizeof(kBeatsPerStep) / sizeof(kBeatsPerStep[0]);
    if (divIndex < 0 || divIndex >= n)
        return 1.0f;
    return kBeatsPerStep[divIndex];
}

static int ClampedPolyStepCount()
{
    int32_t count = kPlayStepParam.value;
    if (count < 1)
        count = 1;
    if (count > kMaxPolySteps)
        count = kMaxPolySteps;
    return static_cast<int>(count);
}

// Streams the values shown in the ESP32's root-wheel center hub: current
// BPM, the confirmed root note / scale (kPlaySubmenu[1]/[2].selectedIndex,
// persisted across visits like any other value-list node), the PLAY/STOP
// transport state and the currently-sounding MIDI note. Sent alongside every
// NAV/EDIT frame below so it never needs its own send cadence.
static void SendPlayStatus()
{
    int note = (s_midi_notes[0] != 0xFF) ? static_cast<int>(s_midi_notes[0]) : 255;
    hw.PrintLine("STAT,BPM=%ld,ROOT=%ld,SCALE=%ld,PLAY=%d,TRNS=%ld,AUTO=%ld,NOTE=%d,T440=%d,EA=%ld,ED=%ld,ES=%ld,ER=%ld",
                 static_cast<long>(kBpmParam.value),
                 static_cast<long>(kPlaySubmenu[1].selectedIndex),
                 static_cast<long>(kPlaySubmenu[2].selectedIndex),
                 s_playing ? 1 : 0,
                 static_cast<long>(s_prog_transpose_degrees),
                 static_cast<long>(kPlayAutoParam.value),
                 note,
                 s_440hz_test_active ? 1 : 0,
                 static_cast<long>(kEnv1AttackParam.value),
                 static_cast<long>(kEnv1DecayParam.value),
                 static_cast<long>(kEnv1SustainParam.value),
                 static_cast<long>(kEnv1ReleaseParam.value));
    // VCF fields used to be appended to the STAT line above, but that pushed
    // a single hw.PrintLine() call past libDaisy's hard 128-byte
    // LOGGER_BUFFER limit (see the truncation note at the top of this file):
    // the line got silently cut with no trailing "\n" and fused with
    // whatever was sent right after, corrupting/dropping the next NAV/EDIT
    // frame -- this is what caused the long lag when turning the MENU
    // encoder or pressing HOME. Kept in their own short frame instead.
    hw.PrintLine("VCF,VFT=%ld,VC=%ld,VR=%ld,VK=%ld,VD=%ld,VE=%ld",
                 static_cast<long>(kVcfSubmenu[0].selectedIndex),
                 static_cast<long>(kVcfCutoffParam.value),
                 static_cast<long>(kVcfResonanceParam.value),
                 static_cast<long>(kVcfKeyParam.value),
                 static_cast<long>(kVcfDriveParam.value),
                 static_cast<long>(kVcfEnvParam.value));
}

// Sends the full navigation path as "NAV,P=<idx0>.<idx1>...[,V=<n>]": one
// index per depth level, from the root sentinel down to the currently
// active node, plus an optional trailing preview of the highlighted item's
// current value (e.g. ONDE's waveform) when it is itself a live-value list
// -- lets the ESP32 show that value in its center hub without requiring the
// user to drill into it first.
static void SendMenuPath()
{
    char path[64];
    int  offset = 0;
    for (int d = 0; d <= s_menu_depth && offset < static_cast<int>(sizeof(path)); ++d)
    {
        offset += snprintf(path + offset, sizeof(path) - offset,
                            d == 0 ? "%ld" : ".%ld",
                            static_cast<long>(s_menu_stack[d]->selectedIndex));
    }

    MenuNode* cur = s_menu_stack[s_menu_depth];
    if (cur->childCount > 0)
    {
        MenuNode* highlighted = &cur->children[cur->selectedIndex];
        if (highlighted->childCount > 0 && highlighted->onSelect != nullptr
            && offset < static_cast<int>(sizeof(path)))
        {
            offset += snprintf(path + offset, sizeof(path) - offset,
                                ",V=%ld", static_cast<long>(highlighted->selectedIndex));
        }
    }

    SendPlayStatus();
    hw.PrintLine("NAV,P=%s", path);
}

// Streams the numeric value currently being edited (e.g. BPM) so the ESP32
// can render the ring-gauge parameter-edit screen; sent on entry, on every
// value change, and on the periodic heartbeat while editing.
static void SendEditState()
{
    if (s_editing_node == nullptr || s_editing_node->numeric == nullptr)
        return;
    const NumericParam* p = s_editing_node->numeric;
    hw.PrintLine("EDIT,V=%ld,MIN=%ld,MAX=%ld,U=%s",
                 static_cast<long>(p->value), static_cast<long>(p->minValue),
                 static_cast<long>(p->maxValue), p->unit ? p->unit : "");
    SendPlayStatus();
}

// Streams the polymetric step wheel overlay: step count, edit cursor, one
// state digit per step ('0'=off,'1'=note,'2'=arpeggio,'3'=chord), the cursor step's
// degree (for the ESP32 to show while encoder_submenu adjusts it), the
// currently-sounding playhead step (-1 while stopped) and the MIDI note
// number currently being played (255 = none). Sent on entry, on every
// cursor/state/degree change, on every playhead advance, and on the
// periodic heartbeat while the wheel is shown.
static void SendPolyState()
{
    int stepCount = ClampedPolyStepCount();
    if (s_poly_cursor >= stepCount)
        s_poly_cursor = stepCount - 1;
    if (s_poly_cursor < 0)
        s_poly_cursor = 0;

    char states[kMaxPolySteps + 1];
    for (int i = 0; i < stepCount; ++i)
        states[i] = static_cast<char>('0' + s_poly_steps[i].state);
    states[stepCount] = '\0';

    int note = (s_midi_notes[0] != 0xFF) ? static_cast<int>(s_midi_notes[0]) : 255;

    hw.PrintLine("POLY,N=%d,C=%d,ST=%s,DEG=%d,PLAY=%d,NOTE=%d",
                 stepCount, s_poly_cursor, states,
                 static_cast<int>(s_poly_steps[s_poly_cursor].degree), s_poly_play_step, note);
}

// Sends recent audio samples for waveform display on the ESP32.
// Format: "WAVE,S=<s0>,<s1>,...,<sN>" where each sample is scaled to -127..127.
static void SendAudioSamples()
{
    float samples[kUsbSampleCount];
    GetRecentSamples(samples);
    
    char buf[256];
    int offset = snprintf(buf, sizeof(buf), "WAVE,S=");
    
    for (size_t i = 0; i < kUsbSampleCount; i++)
    {
        // Scale float [-1.0, 1.0] to int8_t [-127, 127]
        int8_t sample_int = static_cast<int8_t>(samples[i] * 127.0f);
        offset += snprintf(buf + offset, sizeof(buf) - offset,
                          i == 0 ? "%d" : ",%d", sample_int);
    }
    
    hw.PrintLine("%s", buf);
}

int main(void)
{
    // Initialize hardware
    hw.Init();
    
    // Initialize the chord voice pool (all voices share the same waveform,
    // kept quiet enough per-voice that a full chord doesn't clip)
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        s_voices[v].Init(hw.AudioSampleRate());
        s_voices[v].SetAmplitude(0.3f / kMaxChordVoices);
    }

    // Test-mode raw noise generator (SYSTEM > 440Hz), kept quiet at 0.5 amplitude
    s_test_noise.Init();
    s_test_noise.SetAmp(0.5f);

    // ---- Load persisted settings (parameter values + menu selections + poly steps) ----
    // Restores everything saved on a previous boot; falls back to the
    // compiled-in defaults (already live in the menu tree above) on the very
    // first boot or if the menu layout changed since the last save.
    {
        SynthSettings defaults;
        uint32_t vi = 0, si = 0;
        WalkMenuTree(&kRootNode, SettingsWalkMode::kCollect, defaults, vi, si);
        SyncPolyStepsState(SettingsWalkMode::kCollect, defaults);  // Collect default poly state
        defaults.numValues     = vi;
        defaults.numSelections = si;
        settings_storage.Init(defaults);

        SynthSettings& loaded = settings_storage.GetSettings();
        if (loaded.magic == kSettingsMagic && loaded.numValues == defaults.numValues
            && loaded.numSelections == defaults.numSelections)
        {
            uint32_t lvi = 0, lsi = 0;
            WalkMenuTree(&kRootNode, SettingsWalkMode::kApply, loaded, lvi, lsi);
            SyncPolyStepsState(SettingsWalkMode::kApply, loaded);  // Restore poly steps
        }
    }

    // Initialize ENV1 amplitude envelope after settings are loaded so the
    // restored A/D/S/R values are active immediately.
    s_env1_adsr.Init(hw.AudioSampleRate());
    ApplyEnv1Attack(kEnv1AttackParam.value);
    ApplyEnv1Decay(kEnv1DecayParam.value);
    ApplyEnv1Sustain(kEnv1SustainParam.value);
    ApplyEnv1Release(kEnv1ReleaseParam.value);

    // Initialize the VCF the same way: Cutoff/Resonance/Key/Drive/Env are
    // numeric leaves, so (like ENV1's) they aren't re-applied by WalkMenuTree's
    // restore and need an explicit push here. Filter type is a list node, so
    // WalkMenuTree already re-fired ApplyVcfType via its onSelect callback.
    s_vcf_filter.Init(hw.AudioSampleRate());
    ApplyVcfType(kVcfSubmenu[0].selectedIndex);
    ApplyVcfCutoff(kVcfCutoffParam.value);
    ApplyVcfResonance(kVcfResonanceParam.value);
    ApplyVcfKey(kVcfKeyParam.value);
    ApplyVcfDrive(kVcfDriveParam.value);
    ApplyVcfEnv(kVcfEnvParam.value);

    // Master volume also isn't re-applied by WalkMenuTree's restore (it only
    // re-fires onSelect for parent/list nodes, not numeric leaves), so without
    // this it silently stays at its 1.0f default regardless of the persisted
    // Volume value.
    ApplySysVolume(kSysVolumeParam.value);
    
    // Initialize main encoder rotation only (D21/D22), with inverted direction for sync with menu encoder
    encoder_main.Init(daisy::seed::D21,  // pin A
                      daisy::seed::D22,  // pin B
                      daisy::Pin(),      // no switch
                      true);             // invert_direction: match encoder_menu's rotation sense
    
    // Initialize menu encoder rotation only (D17/D18), with inverted direction
    encoder_menu.Init(daisy::seed::D17,  // pin A
                      daisy::seed::D18,  // pin B
                      daisy::Pin(),      // no switch
                      true);             // invert_direction

    // Initialize sub-menu encoder rotation only (D19/D20)
    encoder_submenu.Init(daisy::seed::D19,  // pin A
                         daisy::seed::D20); // pin B

    // Initialize programming encoder rotation only (D15/D16)
    encoder_prog.Init(daisy::seed::D15,  // pin A
                      daisy::seed::D16); // pin B
    
    // Initialize MAIN encoder switch (D23) as a standalone active-high button
    // (encoder switches are wired to +3V when pressed, so use pull-down)
    button_main_sw.Init(daisy::seed::D23, true);
    
    // Initialize MENU encoder switch (D25) as a standalone active-high button
    button_menu_sw.Init(daisy::seed::D25, true);
    
    // Initialize SUB-MENU encoder switch (D28) as a standalone active-high button
    button_submenu_sw.Init(daisy::seed::D28, true);
    
    // Initialize PROG encoder switch (D24) as a standalone active-high button
    button_prog_sw.Init(daisy::seed::D24, true);
    
    // Initialize HOME button (D12)
    button_home.Init(daisy::seed::D12);
    
    // Initialize MUTE button (D11)
    button_mute.Init(daisy::seed::D11);
    
    // Initialize SOLO button (D10)
    button_solo.Init(daisy::seed::D10);
    
    // Initialize TRIGGER button (D9)
    button_trigger.Init(daisy::seed::D9);
    
    // Start USB serial for debugging / display stream
    hw.StartLog(false);

    // Initialize MIDI output on the same CDC link (channel 1 = zero-indexed 0).
    s_midi_out.Init(hw, 0);
    
    // Print startup message
    hw.PrintLine("=== DCO-ONE Phase 1 ===");
    hw.PrintLine("Daisy Seed 3 Audio Engine");
    hw.PrintLine("Random-frequency sine wave -> USB CDC");
    hw.PrintLine("  Sample Rate: 48 kHz");
    hw.PrintLine("  Amplitude: 30%");
    hw.PrintLine("Audio output: ACTIVE");
    hw.PrintLine("=======================");
    hw.PrintLine("Navigation (Simplified UI):");
    hw.PrintLine("  Encoder menu (D17/D18): Browse menu & submenu");
    hw.PrintLine("  Button menu sw (D25): Enter submenu / Exit submenu");
    hw.PrintLine("  Button home (D12): Go back one menu level");
    hw.PrintLine("Other encoders reserved for future features.");
    hw.PrintLine("Monitoring input...");
    
    // Set up audio callback
    hw.SetAudioBlockSize(64);  // 64 samples per block
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    
    hw.StartAudio(AudioCallback);
    
    uint32_t last_menu_send = System::GetNow();
    uint32_t last_audio_send = System::GetNow();
    uint32_t last_poly_send = System::GetNow();
    uint32_t last_poly_step_ms = System::GetNow();

    // Send the initial state once so the ESP32 doesn't wait a full heartbeat
    SendMenuPath();

    // Main loop (non real-time)
    while (true)
    {
        uint32_t now = System::GetNow();
        
        // Update all buttons and encoders
        bool main_sw_changed = button_main_sw.Update();
        bool main_sw_pressed = button_main_sw.IsPressed();
        bool menu_sw_changed = button_menu_sw.Update();
        bool menu_sw_pressed = button_menu_sw.IsPressed();
        bool submenu_sw_changed = button_submenu_sw.Update();
        bool submenu_sw_pressed = button_submenu_sw.IsPressed();
        bool prog_sw_changed = button_prog_sw.Update();
        bool prog_sw_pressed = button_prog_sw.IsPressed();
        bool home_changed = button_home.Update();
        bool home_pressed = button_home.IsPressed();
        bool trigger_changed = button_trigger.Update();
        bool trigger_pressed = button_trigger.IsPressed();
        
        // Visual test aid: LED reflects encoder switch states
        hw.SetLed(main_sw_pressed || menu_sw_pressed || submenu_sw_pressed || prog_sw_pressed);

        // ---- trigger: toggle PLAY/STOP transport state (poly wheel sequencer) ----
        if (trigger_changed && trigger_pressed)
        {
            if (s_440hz_test_active)
            {
                PanicSilence();
                s_440hz_test_active = false;
                SendPlayStatus();
            }
            else
            {
                s_playing = !s_playing;
            if (s_playing)
            {
                s_poly_play_step = 0;
                last_poly_step_ms = now;
                s_auto_steps_until_change = kPlayAutoParam.value; // reset AUTO counter
                TriggerPolyStep(s_poly_play_step);
                s_midi_out.SendStart();
                s_midi_clock_phase = 0.0f; // fire the first tick promptly on play
                s_midi_clock_running = true;
            }
            else
            {
                PanicSilence();
                s_poly_play_step = -1;
                s_midi_out.SendStop();
                s_midi_clock_running = false;
            }
            }
            SendPlayStatus();
            if (s_poly_wheel_active)
            {
                SendPolyState();
                last_poly_send = now;
            }
        }

        // ---- encoder_prog: adjust the focused ENV1 parameter directly
        //      (no need to enter the submenu). Outside ENV1/VCF it transposes
        //      the playing sequence when AUTO is off. ----
        int32_t dir_prog = encoder_prog.GetAndClearSteps();
        if (dir_prog != 0)
        {
            bool in_env1 = (s_menu_depth == 1 && s_menu_stack[1]->children == kEnv1Submenu);
            bool in_vcf  = (s_menu_depth == 1 && s_menu_stack[1]->children == kVcfSubmenu);
            if (in_env1 || in_vcf)
            {
                int focusIdx = s_menu_stack[s_menu_depth]->selectedIndex;
                NumericParam* p = nullptr;
                MenuApplyFn applyFn = nullptr;
                MenuNode* editNode = nullptr;
                if (in_env1)
                {
                    switch (focusIdx)
                    {
                        case 0: p = &kEnv1AttackParam;  applyFn = ApplyEnv1Attack;  editNode = &kEnv1Submenu[0]; break;
                        case 1: p = &kEnv1DecayParam;   applyFn = ApplyEnv1Decay;   editNode = &kEnv1Submenu[1]; break;
                        case 2: p = &kEnv1SustainParam; applyFn = ApplyEnv1Sustain; editNode = &kEnv1Submenu[2]; break;
                        case 3: p = &kEnv1ReleaseParam; applyFn = ApplyEnv1Release; editNode = &kEnv1Submenu[3]; break;
                    }
                }
                else // in_vcf -- kVcfSubmenu[0] is the Filter type list, not numeric
                {
                    switch (focusIdx)
                    {
                        case 1: p = &kVcfCutoffParam;    applyFn = ApplyVcfCutoff;    editNode = &kVcfSubmenu[1]; break;
                        case 2: p = &kVcfResonanceParam; applyFn = ApplyVcfResonance; editNode = &kVcfSubmenu[2]; break;
                        case 3: p = &kVcfKeyParam;       applyFn = ApplyVcfKey;       editNode = &kVcfSubmenu[3]; break;
                        case 4: p = &kVcfDriveParam;     applyFn = ApplyVcfDrive;     editNode = &kVcfSubmenu[4]; break;
                        case 5: p = &kVcfEnvParam;       applyFn = ApplyVcfEnv;       editNode = &kVcfSubmenu[5]; break;
                    }
                }
                if (p != nullptr)
                {
                    int32_t newValue = p->value + dir_prog * p->step;
                    if (newValue < p->minValue)
                        newValue = p->minValue;
                    else if (newValue > p->maxValue)
                        newValue = p->maxValue;
                    if (newValue != p->value)
                    {
                        p->value = newValue;
                        applyFn(newValue);
                        MarkSettingsDirty(now);
                        if (s_editing_numeric && s_editing_node == editNode)
                        {
                            SendEditState();
                            last_menu_send = now;
                        }
                        else
                        {
                            SendMenuPath();
                            last_menu_send = now;
                        }
                    }
                }
            }
            else if (kPlayAutoParam.value == 0)
            {
                int32_t newTranspose = s_prog_transpose_degrees + dir_prog;
                if (newTranspose > kMaxProgTransposeDegrees)
                    newTranspose = kMaxProgTransposeDegrees;
                else if (newTranspose < -kMaxProgTransposeDegrees)
                    newTranspose = -kMaxProgTransposeDegrees;
                s_prog_transpose_degrees = newTranspose;
                SendPlayStatus(); // live transpose update on the ESP32 hub
            }
        }

        // =====================================================================
        // POLY STEP WHEEL (overlay opened by button_main_sw, any screen)
        // =====================================================================

        // ---- main_sw: first press opens the wheel, further presses cycle the
        //      state of the step under the cursor (off -> note -> arpeggio -> chord) ----
        if (main_sw_changed && main_sw_pressed)
        {
            if (!s_poly_wheel_active)
            {
                s_poly_wheel_active = true;
                s_poly_cursor = 0;
            }
            else
            {
                PolyStep& step = s_poly_steps[s_poly_cursor];
                step.state = static_cast<uint8_t>((step.state + 1) % 4);
                MarkSettingsDirty(now);  // Mark poly state change for persistence
                // If the edited step is currently sounding, apply it live so a
                // gray step immediately becomes a rest and a note/chord step
                // starts/stops on the spot.
                if (s_playing && s_poly_cursor == s_poly_play_step)
                {
                    TriggerPolyStep(s_poly_play_step);
                }
            }
            SendPolyState();
            last_poly_send = now;
        }

        if (s_poly_wheel_active)
        {
            // ---- encoder_main: move the edit cursor across the steps, 1:1 ----
            int32_t dir_main = encoder_main.GetAndClearSteps();
            if (dir_main != 0)
            {
                int stepCount = ClampedPolyStepCount();
                s_poly_cursor = ((s_poly_cursor + dir_main) % stepCount + stepCount) % stepCount;
                SendPolyState();
                last_poly_send = now;
            }

            // ---- encoder_submenu: live-adjust the cursor step's degree, 1:1 ----
            int32_t dir_submenu = encoder_submenu.GetAndClearSteps();
            if (dir_submenu != 0)
            {
                PolyStep& step = s_poly_steps[s_poly_cursor];
                int32_t newDegree = step.degree + dir_submenu;
                newDegree = newDegree < -14 ? -14 : (newDegree > 14 ? 14 : newDegree);
                step.degree = static_cast<int8_t>(newDegree);
                MarkSettingsDirty(now);  // Mark poly degree change for persistence
                // Retrigger if we are tweaking the step that is currently playing.
                if (s_playing && s_poly_cursor == s_poly_play_step)
                {
                    TriggerPolyStep(s_poly_play_step);
                }
                SendPolyState();
                last_poly_send = now;
            }

            // ---- button_submenu_sw: confirm the note, advance to the next step ----
            if (submenu_sw_changed && submenu_sw_pressed)
            {
                int stepCount = ClampedPolyStepCount();
                s_poly_cursor = (s_poly_cursor + 1) % stepCount;
                SendPolyState();
                last_poly_send = now;
            }

            // ---- Periodic heartbeat resync ----
            if (now - last_poly_send >= kMenuSendIntervalMs)
            {
                SendPolyState();
                last_poly_send = now;
            }
        }
        else
        {
            // Discard stray rotation while the wheel isn't shown, so it
            // doesn't jump the cursor on the next entry.
            encoder_main.GetAndClearSteps();
            encoder_submenu.GetAndClearSteps();
        }

        // ---- Poly sequencer clock: advances at tempo while s_playing,
        //      independent of whether the wheel is currently shown ----
        if (s_playing)
        {
            int32_t divIdx = kPlaySubmenu[7].selectedIndex; // Div
            float   bpm    = static_cast<float>(kBpmParam.value);
            if (bpm < 1.0f)
                bpm = 1.0f;

            // Keep the ISR's clock period in sync with the live BPM, then
            // drain whatever ticks it already decided were due (sample-
            // accurate; see s_midi_clock_* declarations above).
            s_midi_clock_samples_per_tick = (60.0f / bpm / 24.0f) * kAudioSampleRateHz;
            uint32_t clockTicksCommitted = s_midi_clock_ticks_committed;
            while (s_midi_clock_ticks_sent != clockTicksCommitted)
            {
                s_midi_out.SendClock();
                s_midi_clock_ticks_sent++;
            }

            uint32_t stepDurationMs =
                static_cast<uint32_t>((60000.0f / bpm) * PolyDivisionBeats(divIdx));
            if (stepDurationMs < 1)
                stepDurationMs = 1;
            if (now - last_poly_step_ms >= stepDurationMs)
            {
                int stepCount = ClampedPolyStepCount();
                s_poly_play_step = (s_poly_play_step + 1) % stepCount;

                // AUTO mode: change the global transposition every N steps,
                // constrained to degrees of the current scale/root/octave.
                int32_t autoSteps = kPlayAutoParam.value;
                if (autoSteps > 0)
                {
                    s_auto_steps_until_change--;
                    if (s_auto_steps_until_change <= 0)
                    {
                        s_auto_steps_until_change = autoSteps;
                        int32_t scaleIdx = kPlaySubmenu[2].selectedIndex;
                        const ScaleDef& scale = kScaleDefs[((scaleIdx % kScaleDefCount) + kScaleDefCount) % kScaleDefCount];
                        int32_t len = scale.length;
                        s_prog_transpose_degrees = RandomInt(-2 * len, 2 * len);
                        SendPlayStatus(); // reflect new random transpose on the ESP32 hub
                    }
                }

                TriggerPolyStep(s_poly_play_step);
                last_poly_step_ms = now;
                if (s_poly_wheel_active)
                {
                    SendPolyState();
                    last_poly_send = now;
                }
            }
        }

        // ---- Arpeggio advance: step through a CHORD step's notes one at a
        //      time over its duration (armed by TriggerPolyStep above) ----
        if (s_playing && s_arp_active && now - s_arp_last_note_ms >= s_arp_note_duration_ms)
        {
            s_arp_note_index   = (s_arp_note_index + 1) % s_arp_note_count;
            PlayArpNote(s_arp_note_index);
            s_arp_last_note_ms = now;
        }

        // =====================================================================
        // NAVIGATION STATE MACHINE (generic tree, same logic at every depth)
        // Suspended while the poly step wheel overlay is shown.
        // =====================================================================
        if (!s_poly_wheel_active)
        {
        // ---- Rotate: move selection within whichever level is active, 1:1 ----
        int32_t dir_menu = encoder_menu.GetAndClearSteps();
        if (dir_menu != 0)
        {
            if (s_editing_numeric && s_editing_node != nullptr && s_editing_node->numeric != nullptr)
            {
                NumericParam* p = s_editing_node->numeric;
                int32_t newValue = p->value + dir_menu * p->step;
                int32_t clamped = newValue < p->minValue ? p->minValue
                                : (newValue > p->maxValue ? p->maxValue : newValue);
                if (clamped != p->value)
                {
                    p->value = clamped;
                    if (s_editing_node->onSelect)
                        s_editing_node->onSelect(p->value);
                    MarkSettingsDirty(now);
                    SendEditState();
                    last_menu_send = now;
                }
            }
            else
            {
                MenuNode* cur = s_menu_stack[s_menu_depth];
                if (cur->childCount > 0)
                {
                    cur->selectedIndex = ((cur->selectedIndex + dir_menu) % cur->childCount
                                          + cur->childCount) % cur->childCount;
                    if (cur->onSelect)
                        cur->onSelect(cur->selectedIndex);
                    MarkSettingsDirty(now);
                    SendMenuPath();
                    last_menu_send = now;
                }
            }
        }

        // ---- Periodic heartbeat resync ----
        if (now - last_menu_send >= kMenuSendIntervalMs)
        {
            if (s_editing_numeric)
                SendEditState();
            else
                SendMenuPath();
            last_menu_send = now;
        }
        }
        else
        {
            // Discard stray rotation while a different overlay is shown.
            encoder_menu.GetAndClearSteps();
        } // !s_poly_wheel_active (navigation rotate + heartbeat)

        // ---- Periodic audio sample stream ----
        if (now - last_audio_send >= kUsbSendIntervalMs)
        {
            SendAudioSamples();
            last_audio_send = now;
        }

        // ---- menu_sw: drill into the selected item, edit a numeric value, or confirm/pop a value list ----
        if (!s_poly_wheel_active && menu_sw_changed && menu_sw_pressed)
        {
            if (s_editing_numeric)
            {
                // Confirm the edited value and return to the submenu wheel
                s_editing_numeric = false;
                s_editing_node = nullptr;
                SendMenuPath();
                last_menu_send = now;
            }
            else
            {
                MenuNode* cur = s_menu_stack[s_menu_depth];
                if (cur->childCount > 0)
                {
                    MenuNode* selected = &cur->children[cur->selectedIndex];
                    if (selected->numeric != nullptr)
                    {
                        // Enter numeric edit mode (e.g. BPM): reuses this same encoder/button
                        s_editing_numeric = true;
                        s_editing_node = selected;
                        s_editing_original_value = selected->numeric->value;
                        SendEditState();
                    }
                    else if (selected->childCount > 0 && s_menu_depth + 1 < kMaxMenuDepth)
                    {
                        // Has children: drill one level down
                        s_menu_depth++;
                        s_menu_stack[s_menu_depth] = selected;
                        SendMenuPath();
                    }
                    else if (selected->onSelect != nullptr)
                    {
                        // Action leaf (e.g. 440Hz test tone): execute immediately
                        selected->onSelect(0);
                    }
                    else if (cur->onSelect != nullptr && s_menu_depth > 0)
                    {
                        // Leaf of a live-value list (e.g. a waveform choice): confirm and go back up
                        s_menu_depth--;
                        SendMenuPath();
                    }
                    else
                    {
                        // Leaf with no live-apply yet (not implemented) -> no-op
                        SendMenuPath();
                    }
                    last_menu_send = now;
                }
            }
        }
        
        // ---- HOME button: exit the poly wheel, cancel a numeric edit, else go back up one level ----
        if (home_changed && home_pressed)
        {
            if (s_poly_wheel_active)
            {
                s_poly_wheel_active = false;
            }
            else if (s_editing_numeric)
            {
                s_editing_node->numeric->value = s_editing_original_value;
                s_editing_numeric = false;
                s_editing_node = nullptr;
            }
            else if (s_menu_depth > 0)
            {
                s_menu_depth--;
            }
            SendMenuPath();
            last_menu_send = now;
        }

        SyncSettingsIfDirty(now);

        hw.DelayMs(2);
    }
}
