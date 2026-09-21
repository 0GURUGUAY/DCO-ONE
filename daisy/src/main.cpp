#include "daisy_seed.h"
#include "daisysp.h"
#include "oscillator.h"
#include "filter.h"
#include "lfo.h"
#include "fm.h"
#include "model.h"
#include "fx_chain.h"
#include "input_manager.h"
#include "midi.h"
#include "patch_storage.h"
#include "../../shared/pattern_transfer.h"
#include "../../shared/remote_control.h"
#include "../../shared/modulation.h"
#include "util/scopedirqblocker.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include "per/uart.h"
#include "usbd/usbd_desc.h"
#include "usbd_ctlreq.h"

#define DEBUG_LOG(...) ((void)0)

using namespace daisy;
using namespace daisysp;
using namespace dco;

// -----------------------------------------------------------------------------
// Phase 1 configuration
// -----------------------------------------------------------------------------
// Native USB MIDI and UART display frames use separate transports.
static constexpr size_t  kUsbSampleCount        = 12;   // samples per USB frame
static constexpr uint32_t kFreqChangeIntervalMs = 2000; // new random freq every 2s
static constexpr uint32_t kUsbSendIntervalMs    = 100;  // send frame every 100ms
static constexpr float   kFreqMin               = 220.0f;
static constexpr float   kFreqMax               = 880.0f;

// Main menu wheel shown on the ESP32 AMOLED, driven by the MENU encoder
// (D17/D18). Navigation state is streamed to the ESP32 over USART1
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
    WAVE_FM,
    WAVE_MODEL,
    WAVE_COUNT
};

// Encoder rotation is sampled at audio-block rate (see AudioCallback) and
// consumed once per main-loop tick via GetAndClearSteps(): each returned
// unit is exactly one physical detent, so every consumer below applies it
// 1:1 (1 notch = 1 menu option / 1 poly step / 1 degree), no debounce or
// throttling needed.

// Hardware instance
DaisySeed hw;

static UartHandler s_display_uart;
static bool s_display_uart_ready = false;

static void InitDisplayUart()
{
    UartHandler::Config config;
    config.periph = UartHandler::Config::Peripheral::USART_1;
    config.mode = UartHandler::Config::Mode::TX_RX;
    config.pin_config.tx = seed::D13;
    config.pin_config.rx = seed::D14;
    config.baudrate = 921600;
    s_display_uart_ready = s_display_uart.Init(config) == UartHandler::Result::OK;
}

// Main-loop only: at 921600 baud a full frame takes less than 3 ms.
static void DisplayPrintLine(const char* format, ...)
{
    if (!s_display_uart_ready)
        return;
    char frame[256];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(frame, sizeof(frame) - 1, format, args);
    va_end(args);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(frame) - 1)
        return;
    frame[length++] = '\n';
    s_display_uart.BlockingTransmit(reinterpret_cast<uint8_t*>(frame), length, 5);
}

static dco::PatternClient s_sd_client;
static uint8_t DMA_BUFFER_MEM_SECTION s_sd_dma_buffer[256];
static volatile uint8_t s_sd_rx_ring[1024];
static volatile size_t s_sd_rx_write = 0;
static volatile size_t s_sd_rx_read = 0;
static volatile bool s_sd_rx_fault = false;

static void PatternUartRx(uint8_t* data, size_t size, void*, UartHandler::Result result)
{
    if (result != UartHandler::Result::OK)
    {
        s_sd_rx_fault = true;
        return;
    }
    for (size_t index = 0; index < size; ++index)
    {
        size_t next = (s_sd_rx_write + 1) % sizeof(s_sd_rx_ring);
        if (next == s_sd_rx_read)
        {
            s_sd_rx_fault = true;
            return;
        }
        s_sd_rx_ring[s_sd_rx_write] = data[index];
        s_sd_rx_write = next;
    }
}

static bool SendPatternCommand(const char* line, void*)
{
    if (!s_display_uart_ready) return false;
    char frame[200];
    int length = snprintf(frame, sizeof(frame), "%s\n", line);
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(frame)) return false;
    return s_display_uart.BlockingTransmit(reinterpret_cast<uint8_t*>(frame), length, 5)
        == UartHandler::Result::OK;
}

static void InitPatternUart()
{
    s_sd_client.Init(SendPatternCommand, nullptr);
    if (s_display_uart_ready)
        s_display_uart.DmaListenStart(s_sd_dma_buffer, sizeof(s_sd_dma_buffer), PatternUartRx, nullptr);
}

// Small voice pool so polymetric-wheel chord steps can sound more than one
// note at once (still just DaisySP Oscillators summed together, no filter/
// envelope per voice yet). Plain single-note steps only ever use voice 0.
static constexpr int kMaxChordVoices = 4;
OscillatorWrapper s_voices[kMaxChordVoices];
// Parallel FM voice pool (DaisySP Synthesis-style 2-op FM, see fm.h), used
// instead of s_voices when Form == WAVE_FM (see s_osc_is_fm/AudioCallback).
// Frequency/amplitude are always mirrored into both pools regardless of
// which is currently active (see SetVoicePitchAmp), so switching Form is
// click-free.
FmWrapper s_fm_voices[kMaxChordVoices];
static bool s_osc_is_fm = false;
// Physical-modeling voice pool (Karplus-Strong string, see model.h), used
// instead of s_voices/s_fm_voices when Form == WAVE_MODEL. Its raw exciter
// signal (GetExciter()) is mixed in on top of the resonator output, scaled
// by s_model_exciter_amount (OSC > Model > Exciter), mirroring the
// SOURCE+EXCITER->MIX split from the voice architecture diagram.
ModelWrapper s_model_voices[kMaxChordVoices];
static bool s_osc_is_model = false;
static float s_model_exciter_amount = 0.3f;
static bool        s_voice_active[kMaxChordVoices] = { false, false, false, false };
static uint8_t     s_midi_notes[kMaxChordVoices]   = { 0xFF, 0xFF, 0xFF, 0xFF };
static dco::MidiOut s_midi_out;

// Voice source tagging so MIDI IN and the internal sequencer don't fight
// over the same voice pool.
enum class VoiceSource : uint8_t { NONE = 0, SEQUENCER, MIDI_IN };
static VoiceSource s_voice_source[kMaxChordVoices] = {};

// Pushes frequency/amplitude to ALL voice pools (regular waveform + FM +
// Model) so whichever one is active (s_osc_is_fm/s_osc_is_model) always has
// up-to-date pitch/level, keeping Form switches click-free.
static inline void SetVoicePitchAmp(int v, float freq, float amp)
{
    s_voices[v].SetFrequency(freq);
    s_voices[v].SetAmplitude(amp);
    s_fm_voices[v].SetFrequency(freq);
    s_fm_voices[v].SetAmplitude(amp);
    s_model_voices[v].SetFrequency(freq);
    s_model_voices[v].SetAmplitude(amp);
}

// Strikes the Model voice's string on note-on (no-op for VCO/FM); call
// alongside SetVoicePitchAmp wherever a NEW note starts sounding.
static inline void TriggerModelVoice(int v)
{
    if (s_osc_is_model)
        s_model_voices[v].Trig();
}

// Envelope 1 (amplitude ADSR) using DaisySP
static daisysp::Adsr s_env1_adsr;
static daisysp::Adsr s_env2_adsr;
static float s_env2_last_output = 0.0f;
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

static dco::LfoWrapper s_lfo1;
static dco::LfoWrapper s_lfo2;
static int32_t s_lfo2_sync_mode = 0;
static float s_lfo2_free_rate_hz = 8.0f;
static float s_vcf_base_resonance = 0.3f;
static float s_vcf_base_drive = 1.0f;
static float s_osc_base_pulse = 0.5f;
static int32_t s_lfo1_sync_mode    = 0;    // mirrors kLfoSubmenu[2].selectedIndex ( "1 Sync" )
static float   s_lfo1_free_rate_hz = 10.0f; // mirrors kLfo1RateParam.value, set by ApplyLfo1Rate
static float   s_lfo1_last_output  = 0.0f;  // last Process() output, -1..1 scaled by Amp
static float   s_current_bpm_hz    = 120.0f; // mirrors kBpmParam.value, refreshed each main-loop tick

static dco::MatrixSlot s_matrix_slots[dco::kMatrixSlotCount] = {};

// Beats-per-cycle for each "lfoX_sync" option after FREE (index 0), same
// 4-beats-per-whole-note convention as PolyDivisionBeats() above.
static float LfoSyncDivisionBeats(int32_t syncIndex)
{
    static const float kBeatsPerCycle[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f };
    constexpr int n = sizeof(kBeatsPerCycle) / sizeof(kBeatsPerCycle[0]);
    int idx = syncIndex - 1; // index 0 is FREE, handled by the caller
    if (idx < 0 || idx >= n)
        return 1.0f;
    return kBeatsPerCycle[idx];
}

// Master output volume (set from SYSTEM > Volume)
static float s_master_volume = 1.0f;

// Four-slot serial FX chain: FX1 -> FX2 -> FX3 -> FX4.  Each slot can be
// Off/Reverb/Delay/Chorus/Flanger/Phaser/BitCrusher with its own dry/wet mix.
static dco::FxChain s_fx_chain;

// -----------------------------------------------------------------------------
// Native USB MIDI IN / OUT
// -----------------------------------------------------------------------------
static daisy::MidiUsbHandler s_usb_midi;

static uint8_t* MidiProductDescriptor(USBD_SpeedTypeDef, uint16_t* length)
{
    static uint8_t name[] = "DCO-ONE MIDI";
    alignas(4) static uint8_t descriptor[sizeof(name) * 2];
    USBD_GetString(name, descriptor, length);
    return descriptor;
}

static uint8_t          s_midi_in_channel      = 0;   // zero-based, mirrors MIDI Channel menu
static uint8_t          s_midi_in_note_count   = 0;   // number of held MIDI notes
static volatile bool    s_midi_gate_wanted     = false;
static volatile uint32_t s_midi_retrigger_count = 0;

// 440Hz tuning test tone state
static bool s_440hz_test_active = false;

// Clean 440Hz sine generator for the SYSTEM > 440Hz test tone. Bypasses
// voices/ADSR/volume to isolate the SAI/codec audio path, but provides a
// stable tuning reference instead of broadband noise.
static daisysp::Oscillator s_test_osc;

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

// Poly step sequencer clock: the internal OSC/ENV1/VCA note trigger. Same
// "decide sample-accurately in AudioCallback, drain in the main loop" pattern
// as the MIDI clock above -- previously this step advance was decided in the
// main loop via System::GetNow() polling, which is NOT fixed-rate (menu/
// display/flash work in between checks). That let a knob turn (VCO/rhythm
// parameter edit) delay a step trigger by however long that loop iteration
// took, and let the internal step clock slowly drift apart from the
// sample-accurate MIDI clock sent to a DAW (e.g. Ableton's kick, which
// follows the MIDI clock, drifting away from the Daisy's own audible notes).
static volatile bool      s_poly_clock_running = false;      // mirrors s_playing, read by ISR
static volatile float     s_poly_step_samples_per_tick = (60.0f / 120.0f) * kAudioSampleRateHz; // updated from main loop as BPM/Div changes
static float              s_poly_step_phase = 0.0f;          // ISR-only accumulator
static volatile uint32_t  s_poly_step_ticks_committed = 0;   // incremented by the ISR
static uint32_t           s_poly_step_ticks_sent = 0;        // consumed by the main loop

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

// Menu node type forward-declared here so AudioCallback can read the matrix
// menu selections (kMatrixSubmenu is defined in menu_generated.inc below).
using MenuApplyFn = void (*)(int32_t selectedIndex);

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
    MenuApplyFn   onConfirm;     // optional action executed when a numeric edit is confirmed
};

// Accessors for matrix menu state (defined after menu_generated.inc).
uint8_t GetMatrixSource(int slot);
uint8_t GetMatrixDestination(int slot);

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

    // Sample-accurate poly step sequencer tick decision -- same reasoning as
    // the MIDI clock above, so the internal note trigger can never be
    // delayed by main-loop menu/display/flash work, and never drifts apart
    // from the MIDI clock sent out to a DAW.
    if (s_poly_clock_running)
    {
        s_poly_step_phase -= static_cast<float>(size);
        while (s_poly_step_phase <= 0.0f)
        {
            s_poly_step_ticks_committed++;
            s_poly_step_phase += s_poly_step_samples_per_tick;
        }
    }

    for (int slot = 0; slot < 2; ++slot)
    {
        s_matrix_slots[slot].source = GetMatrixSource(slot);
        s_matrix_slots[slot].destination = GetMatrixDestination(slot);
    }
    float targets[8] = {};
    dco::AccumulateModulation(s_matrix_slots, s_lfo1.GetLastOutput(),
                             s_lfo2.GetLastOutput(), s_env1_last_output, targets, s_env2_last_output);
    static float modulation[8] = {};
    float smoothing = 1.0f - expf(-static_cast<float>(size) / (hw.AudioSampleRate() * 0.005f));
    for (int destination = 1; destination < 8; ++destination)
        modulation[destination] += smoothing * (targets[destination] - modulation[destination]);

    // VCF cutoff modulation (keytrack + ENV1 amount): recomputed once per
    // block instead of per sample -- a ~1.3ms lag on the modulation is
    // inaudible, and this keeps the log2f/powf calls off the per-sample path.
    // Uses s_env1_last_output (the envelope value at the end of the previous
    // block) since there's no dedicated filter envelope yet.
    {
        float keytrackOctaves = log2f(s_vcf_last_note_freq / kVcfKeytrackRefHz) * s_vcf_keytrack_amount;
        float envOctaves = s_vcf_env_amount * s_env1_last_output * kVcfEnvOctaveRange;
        float cutoffHz = s_vcf_base_cutoff_hz * powf(2.0f, keytrackOctaves + envOctaves
            + dco::LimitModulation(modulation[1], -4.0f, 4.0f) * 4.0f);
        cutoffHz = cutoffHz < 20.0f ? 20.0f : (cutoffHz > 20000.0f ? 20000.0f : cutoffHz);
        s_vcf_filter.SetCutoff(cutoffHz);
        s_vcf_filter.SetResonance(dco::LimitModulation(s_vcf_base_resonance + modulation[6], 0.0f, 1.0f));
        s_vcf_filter.SetDrive(dco::LimitModulation(s_vcf_base_drive + modulation[4], 0.0f, 1.0f));
    }

    // LFO1 rate: BPM-synced division when Sync != FREE, else the free Hz
    // param. Recomputed once per block, same reasoning as the VCF cutoff
    // block above (kBpmParam isn't visible here, s_current_bpm_hz mirrors it,
    // refreshed once per main-loop tick).
    {
        float effectiveHz = s_lfo1_free_rate_hz;
        if (s_lfo1_sync_mode > 0)
        {
            float bpm = s_current_bpm_hz < 1.0f ? 1.0f : s_current_bpm_hz;
            effectiveHz = bpm / (60.0f * LfoSyncDivisionBeats(s_lfo1_sync_mode));
        }
        s_lfo1.SetRate(dco::LimitModulation(effectiveHz * powf(2.0f,
            dco::LimitModulation(modulation[5], -4.0f, 4.0f) * 4.0f), 0.01f, 500.0f));
        float secondHz = s_lfo2_sync_mode > 0
            ? s_current_bpm_hz / (60.0f * LfoSyncDivisionBeats(s_lfo2_sync_mode))
            : s_lfo2_free_rate_hz;
        s_lfo2.SetRate(secondHz);
    }
    for (int voice = 0; voice < kMaxChordVoices; ++voice)
    {
        float pitch = dco::LimitModulation(modulation[2] * 12.0f, -48.0f, 48.0f);
        s_voices[voice].SetPitchModulation(pitch);
        s_fm_voices[voice].SetPitchModulation(pitch);
        s_model_voices[voice].SetPitchModulation(pitch);
        s_voices[voice].SetPulseWidth(dco::LimitModulation(s_osc_base_pulse + modulation[3] * 0.5f, 0.01f, 0.99f));
    }

    // FX slot type changes are requested from the menu thread but must only
    // ever be (re)constructed here, on the audio thread, before Process()
    // runs on any sample this block -- see FxChain::ApplyPendingTypeChanges.
    s_fx_chain.ApplyPendingTypeChanges();

    // DELAY's Time control is tempo-synced (note division, not raw ms), so
    // its effective ms must track live BPM changes even when the knob isn't
    // touched -- same per-block push as the VCF/LFO1 blocks above.
    s_fx_chain.SetTempo(s_current_bpm_hz);

    for (size_t i = 0; i < size; i++)
    {
        // TEST MODE (SYSTEM > 440Hz): clean 440Hz sine straight to the DAC,
        // skipping voices/ADSR/master volume, to isolate the SAI/codec path.
        if (s_440hz_test_active)
        {
            float sine = s_test_osc.Process();
            out[0][i] = sine;
            out[1][i] = sine;
            PushSample(sine);
            continue;
        }

        float sample = 0.0f;
        for (int v = 0; v < kMaxChordVoices; ++v)
            if (s_voice_active[v])
                sample += s_osc_is_fm ? s_fm_voices[v].GetSample()
                        : s_osc_is_model ? (s_model_voices[v].GetSample()
                                            + s_model_voices[v].GetExciter() * s_model_exciter_amount)
                        : s_voices[v].GetSample();

        // VCF: applied to the raw oscillator mix, before the ENV1/VCA stage.
        sample = s_vcf_filter.Process(sample);

        s_lfo1.Process();
        s_lfo2.Process();

        // Apply ENV1 amplitude envelope (retrigger on new notes).
        // The gate is the OR of the sequencer gate and any held MIDI IN notes.
        bool gate = s_env1_gate_wanted || s_midi_gate_wanted;
        uint32_t retrigger = s_env1_retrigger_count + s_midi_retrigger_count;
        static uint32_t last_retrigger = 0;
        if (retrigger != last_retrigger)
        {
            // Soft retrigger: forces mode back to ATTACK without touching the
            // envelope's current level. A hard reset (x_ snapped to 0) or a
            // fake forced-low gate window both create a second, audible
            // dip/rise in level right on top of the real note -- perceived as
            // two separate triggers. Because OFF steps now properly close the
            // gate and let Release run its full course (see TriggerPolyStep),
            // the level is already near 0 by the time a genuinely new note
            // follows a gap, so Attack still shows; back-to-back legato notes
            // (no gap) blend continuously instead of double-triggering, same
            // as a real analog mono synth.
            s_env1_adsr.Retrigger(false);
            s_env2_adsr.Retrigger(false);
            last_retrigger = retrigger;
        }
        float envOut = s_env1_adsr.Process(gate);
        s_env2_last_output = s_env2_adsr.Process(gate);
        s_env1_last_output = envOut;
        sample *= envOut;

        // FX chain: FX1 -> FX2 -> FX3 -> FX4 (each slot has its own
        // type/mix/params, all active simultaneously in series).
        sample = s_fx_chain.Process(sample);

        // Apply master volume and output to both channels
        sample *= s_master_volume * dco::LimitModulation(1.0f + modulation[7], 0.0f, 2.0f);
        out[0][i] = sample;
        out[1][i] = sample;
        
        // Store for USB display stream
        PushSample(sample);
    }
    s_lfo1_last_output = s_lfo1.GetLastOutput();
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
        case WAVE_FM:
            // FM voices are driven separately (s_fm_voices); this value is
            // never actually processed while FM is active.
            return daisysp::Oscillator::WAVE_SIN;
        case WAVE_MODEL:
            // Model voices are driven separately (s_model_voices); this
            // value is never actually processed while Model is active.
            return daisysp::Oscillator::WAVE_SIN;
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

// Forward declarations for apply callbacks referenced by the generated menu.
static void ApplyPlayAuto(int32_t index);
static void ApplyOscCoarse(int32_t index);
static void ApplyOscFine(int32_t index);
static void ApplyOscPulse(int32_t index);
static void ApplyOscSub(int32_t index);
static void ApplyOscHard(int32_t index);
static void ApplyOscFmAmt(int32_t index);
static void ApplyOscFmRatio(int32_t index);
static void ApplyOscFmRatioFine(int32_t index);
static void ApplyOscFmModWave(int32_t index);
static void ApplyOscModelStructure(int32_t index);
static void ApplyOscModelBrightness(int32_t index);
static void ApplyOscModelDamping(int32_t index);
static void ApplyOscModelAccent(int32_t index);
static void ApplyOscModelExciter(int32_t index);
static void ApplyOscModelSustain(int32_t index);
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
static void ApplyLfo1Shape(int32_t index);
static void ApplyLfo1Rate(int32_t index);
static void ApplyLfo1Sync(int32_t index);
static void ApplyLfo1Amp(int32_t index);
static void ApplyLfo1Phase(int32_t index);
static void ApplyLfo2Shape(int32_t index);
static void ApplyLfo2Rate(int32_t index);
static void ApplyLfo2Sync(int32_t index);
static void ApplyLfo2Amp(int32_t index);
static void ApplyLfo2Phase(int32_t index);
static void ApplyMatSlot1Amt(int32_t index);
static void ApplyMatSlot2Amt(int32_t index);
static void ApplyFx1Type(int32_t index);
static void ApplyFx1Mix(int32_t index);
static void ApplyFx1P1(int32_t index);
static void ApplyFx1P2(int32_t index);
static void ApplyFx2Type(int32_t index);
static void ApplyFx2Mix(int32_t index);
static void ApplyFx2P1(int32_t index);
static void ApplyFx2P2(int32_t index);
static void ApplyFx3Type(int32_t index);
static void ApplyFx3Mix(int32_t index);
static void ApplyFx3P1(int32_t index);
static void ApplyFx3P2(int32_t index);
static void ApplyFx4Type(int32_t index);
static void ApplyFx4Mix(int32_t index);
static void ApplyFx4P1(int32_t index);
static void ApplyFx4P2(int32_t index);
static void ApplyMidiChannel(int32_t index);
static void ApplyMidiBend(int32_t index);
static void ApplyAllFxSettings();
static void ApplySysLuminosite(int32_t index);
static void ApplySysVolume(int32_t index);
static void ApplySys440Hz(int32_t index);
static void ApplyPatchLoad(int32_t index);
static void ApplyPatchSave(int32_t index);
static void ApplyInitPatch(int32_t index);

static void ApplyWaveform(int32_t index);

// MIDI IN helpers
static void TriggerMidiNoteOn(uint8_t note, uint8_t velocity);
static void TriggerMidiNoteOff(uint8_t note);
static void TriggerMidiAllNotesOff();
static void ServiceMidiIn();

#include "menu_generated.inc"

uint8_t GetMatrixSource(int slot)
{
    if (slot == 0)
        return static_cast<uint8_t>(kMatrixSubmenu[0].selectedIndex);
    return static_cast<uint8_t>(kMatrixSubmenu[3].selectedIndex);
}

uint8_t GetMatrixDestination(int slot)
{
    if (slot == 0)
        return static_cast<uint8_t>(kMatrixSubmenu[1].selectedIndex);
    return static_cast<uint8_t>(kMatrixSubmenu[4].selectedIndex);
}

static void ApplyWaveform(int32_t index)
{
    WaveformType wave = static_cast<WaveformType>(((index % WAVE_COUNT) + WAVE_COUNT) % WAVE_COUNT);
    s_osc_is_fm    = (wave == WAVE_FM);
    s_osc_is_model = (wave == WAVE_MODEL);
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_voices[v].SetWaveform(WaveformTypeToDaisySP(wave));
}

// Apply callbacks for numeric parameters
static void ApplyPlayAuto(int32_t index) {}

static void ApplyOscCoarse(int32_t index)
{
    (void)index;
    int32_t semitones = kOscCoarseParam.value;
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        s_voices[v].SetCoarseTune(semitones);
        s_fm_voices[v].SetCoarseTune(semitones);
        s_model_voices[v].SetCoarseTune(semitones);
    }
}

static void ApplyOscFine(int32_t index)
{
    (void)index;
    int32_t cents = kOscFineParam.value;
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        s_voices[v].SetFineTune(cents);
        s_fm_voices[v].SetFineTune(cents);
        s_model_voices[v].SetFineTune(cents);
    }
}

static void ApplyOscPulse(int32_t index)
{
    (void)index;
    float duty = static_cast<float>(kOscPulseParam.value) / 100.0f;
    s_osc_base_pulse = duty;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_voices[v].SetPulseWidth(duty);
}

static void ApplyOscSub(int32_t index)
{
    (void)index;
    float level = static_cast<float>(kOscSubParam.value) / 100.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_voices[v].SetSubAmount(level);
}

static void ApplyOscHard(int32_t index)
{
    (void)index;
    float hardness = static_cast<float>(kOscHardParam.value) / 100.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_voices[v].SetHardness(hardness);
}

// FM (OSC > FM submenu): DaisySP Synthesis-style 2-op FM (see fm.h/fm.cpp).
// Ratio is split into a coarse integer part + a fine +/-50% nudge (mirrors
// the main oscillator's Coarse/Fine tuning pattern), combined into one
// float ratio pushed to every FM voice.
static float s_osc_fm_ratio_coarse = 2.0f;
static float s_osc_fm_ratio_fine   = 0.0f; // -50..50, added as a fraction of one ratio step

static void ApplyOscFmAmt(int32_t index)
{
    // 0-100% mapped to DaisySP::Fm2's documented Index range (5 = 2*PI rad).
    float fmIndex = (static_cast<float>(index) / 100.0f) * 5.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_fm_voices[v].SetIndex(fmIndex);
}

static void ApplyOscFmRatio(int32_t index)
{
    s_osc_fm_ratio_coarse = static_cast<float>(index);
    float ratio = s_osc_fm_ratio_coarse + s_osc_fm_ratio_fine / 100.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_fm_voices[v].SetRatio(ratio);
}

static void ApplyOscFmRatioFine(int32_t index)
{
    s_osc_fm_ratio_fine = static_cast<float>(index);
    float ratio = s_osc_fm_ratio_coarse + s_osc_fm_ratio_fine / 100.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_fm_voices[v].SetRatio(ratio);
}

static void ApplyOscFmModWave(int32_t index)
{
    static constexpr uint8_t kModWaves[4] = {
        daisysp::Oscillator::WAVE_SIN,
        daisysp::Oscillator::WAVE_TRI,
        daisysp::Oscillator::WAVE_RAMP,
        daisysp::Oscillator::WAVE_SQUARE,
    };
    uint8_t wf = kModWaves[((index % 4) + 4) % 4];
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_fm_voices[v].SetModWaveform(wf);
}

// Model (OSC > Model submenu): Karplus-Strong string voice (see model.h/.cpp).
static void ApplyOscModelStructure(int32_t index)
{
    float structure = static_cast<float>(index) / 100.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_model_voices[v].SetStructure(structure);
}

static void ApplyOscModelBrightness(int32_t index)
{
    float brightness = static_cast<float>(index) / 100.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_model_voices[v].SetBrightness(brightness);
}

static void ApplyOscModelDamping(int32_t index)
{
    float damping = static_cast<float>(index) / 100.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_model_voices[v].SetDamping(damping);
}

static void ApplyOscModelAccent(int32_t index)
{
    float accent = static_cast<float>(index) / 100.0f;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_model_voices[v].SetAccent(accent);
}

static void ApplyOscModelExciter(int32_t index)
{
    s_model_exciter_amount = static_cast<float>(index) / 100.0f;
}

// Sustain (Pluck=0/Bow=1): Bow keeps the string continuously excited by
// noise (bowed-instrument character) instead of a single struck decay.
static void ApplyOscModelSustain(int32_t index)
{
    bool sustain = index != 0;
    for (int v = 0; v < kMaxChordVoices; ++v)
        s_model_voices[v].SetSustain(sustain);
}

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
    s_vcf_base_resonance = static_cast<float>(index) * 0.01f;
}

// Keytrack amount is applied live in AudioCallback's per-block cutoff
// modulation (see s_vcf_keytrack_amount); nothing to push to the filter here.
static void ApplyVcfKey(int32_t index)
{
    s_vcf_keytrack_amount = static_cast<float>(index) * 0.01f;
}

static void ApplyVcfDrive(int32_t index)
{
    s_vcf_base_drive = static_cast<float>(index) * 0.01f;
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
static void ApplyEnv2Attack(int32_t index)
{
    s_env2_adsr.SetTime(daisysp::ADSR_SEG_ATTACK, static_cast<float>(index) / 1000.0f);
}
static void ApplyEnv2Decay(int32_t index)
{
    s_env2_adsr.SetTime(daisysp::ADSR_SEG_DECAY, static_cast<float>(index) / 1000.0f);
}
static void ApplyEnv2Sustain(int32_t index)
{
    s_env2_adsr.SetSustainLevel(dco::LimitModulation(static_cast<float>(index) * 0.01f, 0.0f, 1.0f));
}
static void ApplyEnv2Release(int32_t index)
{
    s_env2_adsr.SetTime(daisysp::ADSR_SEG_RELEASE, static_cast<float>(index) / 1000.0f);
}

static void InitEnvelope2()
{
    s_env2_adsr.Init(hw.AudioSampleRate());
    s_env2_last_output = 0.0f;
    ApplyEnv2Attack(kEnv2AttackParam.value);
    ApplyEnv2Decay(kEnv2DecayParam.value);
    ApplyEnv2Sustain(kEnv2SustainParam.value);
    ApplyEnv2Release(kEnv2ReleaseParam.value);
}

// LFO1: Shape list maps 1:1 to dco::LfoShape; Sync only stores the selected
// division (index 0 = FREE) -- the actual effective rate, free-running Hz vs.
// BPM-derived, is recomputed once per audio block in AudioCallback so it
// always tracks live BPM changes without needing its own onSelect wiring.
static void ApplyLfo1Shape(int32_t index)
{
    dco::LfoShape shape = static_cast<dco::LfoShape>(((index % 6) + 6) % 6);
    s_lfo1.SetShape(shape);
}

static void ApplyLfo1Rate(int32_t index)
{
    s_lfo1_free_rate_hz = static_cast<float>(index);
}

static void ApplyLfo1Sync(int32_t index)
{
    s_lfo1_sync_mode = index;
}

static void ApplyLfo1Amp(int32_t index)
{
    s_lfo1.SetAmount(static_cast<float>(index) * 0.01f);
}

static void ApplyLfo1Phase(int32_t index)
{
    s_lfo1.SetPhaseDegrees(static_cast<float>(index));
}

static void ApplyLfo2Shape(int32_t index) { s_lfo2.SetShape(static_cast<dco::LfoShape>(index)); }
static void ApplyLfo2Rate(int32_t index) { s_lfo2_free_rate_hz = static_cast<float>(index); }
static void ApplyLfo2Sync(int32_t index) { s_lfo2_sync_mode = index; }
static void ApplyLfo2Amp(int32_t index) { s_lfo2.SetAmount(static_cast<float>(index) * 0.01f); }
static void ApplyLfo2Phase(int32_t index) { s_lfo2.SetPhaseDegrees(static_cast<float>(index)); }
static void ApplyMatSlot1Amt(int32_t index)
{
    s_matrix_slots[0].amount = static_cast<float>(index) * 0.01f;
}

static void ApplyMatSlot2Amt(int32_t index)
{
    s_matrix_slots[1].amount = static_cast<float>(index) * 0.01f;
}

// FX helpers: map raw menu values to the FX chain.  Param1 is normalised
// from the 10..1000 ms UI range; Param2 is already 0..100.
static inline float FxNormMix(int32_t index)  { return static_cast<float>(index) * 0.01f; }
static inline float FxNormP1(int32_t index)   { return static_cast<float>(index - 10) / 490.0f; }
static inline float FxNormP2(int32_t index)   { return static_cast<float>(index) * 0.01f; }

// One "Type" node's selectedIndex per FX slot -- lets the exclusivity check
// below both read and correct another slot's menu display in sync with the
// engine.
static int32_t* const kFxTypeSelectedIndex[dco::FxChain::kNumSlots] = {
    &kFxFx1Submenu[0].selectedIndex, &kFxFx2Submenu[0].selectedIndex,
    &kFxFx3Submenu[0].selectedIndex, &kFxFx4Submenu[0].selectedIndex,
};

// Enforces "an effect type can only be active in a single FX slot at a
// time" (e.g. no 2 Reverb simultaneously). If another slot already has the
// requested (non-Off) type, that slot is bumped back to Off -- both in the
// engine and in its own menu selection -- before this slot's request is
// applied.
static void ApplyFxSlotType(int slot, int32_t index)
{
    dco::FxType type = static_cast<dco::FxType>(index);
    if (type != dco::FxType::OFF)
    {
        for (int other = 0; other < dco::FxChain::kNumSlots; ++other)
        {
            if (other != slot && *kFxTypeSelectedIndex[other] == index)
            {
                *kFxTypeSelectedIndex[other] = 0;
                s_fx_chain.SetSlotType(other, dco::FxType::OFF);
            }
        }
    }
    s_fx_chain.SetSlotType(slot, type);
}

static void ApplyFx1Type(int32_t index) { ApplyFxSlotType(0, index); }
static void ApplyFx1Mix (int32_t index) { s_fx_chain.SetSlotMix(0, FxNormMix(index)); }
static void ApplyFx1P1  (int32_t index) { s_fx_chain.SetSlotParam1(0, FxNormP1(index)); }
static void ApplyFx1P2  (int32_t index) { s_fx_chain.SetSlotParam2(0, FxNormP2(index)); }

static void ApplyFx2Type(int32_t index) { ApplyFxSlotType(1, index); }
static void ApplyFx2Mix (int32_t index) { s_fx_chain.SetSlotMix(1, FxNormMix(index)); }
static void ApplyFx2P1  (int32_t index) { s_fx_chain.SetSlotParam1(1, FxNormP1(index)); }
static void ApplyFx2P2  (int32_t index) { s_fx_chain.SetSlotParam2(1, FxNormP2(index)); }

static void ApplyFx3Type(int32_t index) { ApplyFxSlotType(2, index); }
static void ApplyFx3Mix (int32_t index) { s_fx_chain.SetSlotMix(2, FxNormMix(index)); }
static void ApplyFx3P1  (int32_t index) { s_fx_chain.SetSlotParam1(2, FxNormP1(index)); }
static void ApplyFx3P2  (int32_t index) { s_fx_chain.SetSlotParam2(2, FxNormP2(index)); }

static void ApplyFx4Type(int32_t index) { ApplyFxSlotType(3, index); }
static void ApplyFx4Mix (int32_t index) { s_fx_chain.SetSlotMix(3, FxNormMix(index)); }
static void ApplyFx4P1  (int32_t index) { s_fx_chain.SetSlotParam1(3, FxNormP1(index)); }
static void ApplyFx4P2  (int32_t index) { s_fx_chain.SetSlotParam2(3, FxNormP2(index)); }

// Push the full FX menu state into the audio engine.  Called once at boot
// after settings are restored so that numeric leaves (mix/p1/p2) are applied
// in addition to the type selections already re-fired by WalkMenuTree.
static void ApplyAllFxSettings()
{
    ApplyFx1Type(kFxFx1Submenu[0].selectedIndex);
    ApplyFx1Mix (kFx1MixParam.value);
    ApplyFx1P1  (kFx1P1Param.value);
    ApplyFx1P2  (kFx1P2Param.value);

    ApplyFx2Type(kFxFx2Submenu[0].selectedIndex);
    ApplyFx2Mix (kFx2MixParam.value);
    ApplyFx2P1  (kFx2P1Param.value);
    ApplyFx2P2  (kFx2P2Param.value);

    ApplyFx3Type(kFxFx3Submenu[0].selectedIndex);
    ApplyFx3Mix (kFx3MixParam.value);
    ApplyFx3P1  (kFx3P1Param.value);
    ApplyFx3P2  (kFx3P2Param.value);

    ApplyFx4Type(kFxFx4Submenu[0].selectedIndex);
    ApplyFx4Mix (kFx4MixParam.value);
    ApplyFx4P1  (kFx4P1Param.value);
    ApplyFx4P2  (kFx4P2Param.value);
}

static void ApplyMidiChannel(int32_t index)
{
    // kMidiChannelParam is 1..16 in the UI; MIDI channels are zero-based.
    s_midi_in_channel = static_cast<uint8_t>((index - 1) & 0x0F);
}

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
// Each step has 5 states cycled by button_main_sw: not played (gray), played
// (yellow), arpeggio (cyan), chord (green), fixed note (rose). `degree` is a
// scale-degree offset (0 = the PLAY submenu's confirmed Root note), resolved
// against the active Scale via kScaleDefs below. encoder_main moves the edit
// cursor; encoder_submenu adjusts the cursor step's degree; encoder_prog sets
// the absolute transposition of a ROSE (FIXED) step, which is stored per-step
// and never modified by AUTO. button_submenu_sw confirms and advances to the
// next step. Step count mirrors kPlayStepParam.value (1..32) live, so the
// array is sized to its max instead of being reallocated.
enum PolyStepState : uint8_t { POLY_OFF = 0, POLY_NOTE = 1, POLY_ARP = 2, POLY_CHORD = 3, POLY_FIXED = 4 };

struct PolyStep {
    uint8_t state;
    int8_t  degree;
    int8_t  fixedTranspose; // POLY_FIXED: prog transpose captured by encoder_prog, immune to AUTO
};

static constexpr int kMaxPolySteps = 32; // == kPlayStepParam.maxValue
static PolyStep      s_poly_steps[kMaxPolySteps] = {};
static bool          s_poly_wheel_active = false; // overlay currently shown on ESP32
static int           s_poly_cursor    = 0;        // step edited by encoder_main/encoder_submenu
static int           s_poly_play_step = -1;       // step currently sounding, -1 = stopped

// Direction de lecture du séquenceur, cyclée par button_prog_sw en mode PLAY :
// 0 = avant, 1 = arrière, 2 = aller-retour (ping-pong)
static int           s_playback_direction = 0;
static int32_t       s_pingpong_index = 0;        // position dans le cycle ping-pong 0..2*(N-1)-1

// Arpeggio state for a CHORD step: its notes are played one at a time in
// sequence (voice 0 reused monophonically) instead of stacked as a chord.
static bool     s_arp_active           = false;
static int8_t   s_arp_intervals[kMaxChordVoices] = {};
static int      s_arp_note_count       = 0;
static int      s_arp_note_index       = 0;
static float    s_arp_root_freq        = 0.0f;
static uint32_t s_arp_note_duration_ms = 0;
static uint32_t s_arp_last_note_ms     = 0;

// Gate length for the sequencer's MIDI output: percentage of the step
// duration during which a Note On stays open before a Note Off is emitted.
// Edited with encoder_prog while the poly wheel is shown (25/50/75/100).
static int32_t  s_gate_length_pct      = 100; // 0..100 %, persisted in flash/presets
static uint8_t  s_gate_notes[kMaxChordVoices] = { 0xFF, 0xFF, 0xFF, 0xFF };
static uint32_t s_gate_note_off_ms[kMaxChordVoices] = {};

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
static constexpr uint32_t kSettingsMagic          = 0x44434F34; // "DCO4" (4-slot FX chain)
static constexpr uint32_t kSettingsSaveDebounceMs = 1500; // idle time before flushing to flash
static constexpr int      kMaxPersistedValues     = 48;
static constexpr int      kMaxPersistedSelections = 48;

struct SynthSettings
{
    uint32_t magic         = kSettingsMagic;
    uint32_t numValues     = 0;
    uint32_t numSelections = 0;
    uint32_t numPolySteps  = 0;  // Persist polymetric step setup
    int32_t  gateLengthPct = 100; // Global sequencer MIDI gate length %
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
static SynthSettings s_factory_settings;    // compiled-in defaults, captured at boot
static bool     s_settings_dirty          = false;
static uint32_t s_last_settings_change_ms = 0;

// Walks the whole static menu tree in one fixed, deterministic order so the
// same array slot always corresponds to the same parameter on both save and
// load. kCollect reads live state into `s`; kApply writes `s` back into the
// live state (and re-fires any onSelect callback, e.g. ONDE's waveform, so
// the audio engine reflects the restored value immediately).
enum class SettingsWalkMode { kCollect, kApply };

template <typename Settings>
static void WalkMenuTree(MenuNode* node, SettingsWalkMode mode, Settings& s,
                         uint32_t& valIdx, uint32_t& selIdx)
{
    if (node->numeric != nullptr)
    {
        if (valIdx < sizeof(s.values) / sizeof(s.values[0]))
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
        if (selIdx < sizeof(s.selections) / sizeof(s.selections[0]))
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
    if (node == &kRootNode)
    {
        constexpr size_t firstExtra = sizeof(s.selections) / sizeof(s.selections[0]) - 6;
        if (selIdx <= firstExtra)
            for (int slot = 2; slot < dco::kMatrixSlotCount; ++slot)
            {
                auto& packed = s.selections[firstExtra + slot - 2];
                if (mode == SettingsWalkMode::kCollect)
                    packed = dco::PackMatrixSlot(s_matrix_slots[slot]);
                else
                    s_matrix_slots[slot] = dco::UnpackMatrixSlot(packed);
            }
    }
}

// Persist/restore the full polymetric step wheel state (all steps, all states & degrees)
static void SyncPolyStepsState(SettingsWalkMode mode, SynthSettings& s)
{
    if (mode == SettingsWalkMode::kCollect)
    {
        s.numPolySteps  = kMaxPolySteps;
        s.gateLengthPct = s_gate_length_pct;
        for (int i = 0; i < kMaxPolySteps; ++i)
            s.polySteps[i] = s_poly_steps[i];
    }
    else
    {
        // Sanitize: flash data saved by an older firmware layout (before
        // these fields existed) can leave garbage bytes here, which must
        // never be interpreted as a valid state/degree/transpose (see
        // TriggerPolyStep). kSettingsMagic already rejects truly legacy
        // layouts, so any state reaching here uses the current 5-state
        // encoding (OFF/NOTE/ARP/CHORD/FIXED) and must be left untouched.
        if (s.gateLengthPct < 0)
            s.gateLengthPct = 0;
        else if (s.gateLengthPct > 100)
            s.gateLengthPct = 100;
        s_gate_length_pct = s.gateLengthPct;

        for (int i = 0; i < kMaxPolySteps && i < s.numPolySteps; ++i)
        {
            PolyStep loaded = s.polySteps[i];
            if (loaded.state > POLY_FIXED)
                loaded.state = POLY_OFF;
            if (loaded.degree < -14 || loaded.degree > 14)
                loaded.degree = 0;
            if (loaded.fixedTranspose < -kMaxProgTransposeDegrees
                || loaded.fixedTranspose > kMaxProgTransposeDegrees)
                loaded.fixedTranspose = 0;
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
// confirmed PLAY Scale + Root + an explicit transpose (in scale degrees) +
// the global Oct (semitone) transpose, wrapping extra degrees into further
// octaves the same way array indices wrap.
static float DegreeToFrequencyHz(int32_t degree, int32_t transposeDegrees)
{
    int32_t rootIdx  = kPlaySubmenu[1].selectedIndex; // Root: 0=C..11=B
    int32_t scaleIdx = kPlaySubmenu[2].selectedIndex; // Scale
    const ScaleDef& scale = kScaleDefs[((scaleIdx % kScaleDefCount) + kScaleDefCount) % kScaleDefCount];
    int32_t len = scale.length;
    int32_t totalDegree = degree + transposeDegrees;
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

// Convenience overload using the live performance transpose (manual or AUTO).
static float DegreeToFrequencyHz(int32_t degree)
{
    return DegreeToFrequencyHz(degree, GetPerformanceTransposeDegrees());
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

// Convert a step/note duration into a gate duration using the current GT%.
// MIDI-only: this shapes the external Note On/Off pair sent to s_midi_out
// (see the scheduled Note Off loop in the main loop). It has no effect on
// the internal OSC/ENV1/VCA chain, which is triggered once per step and
// runs its own ADSR shape regardless of GT% (see TriggerEnvelope).
// GT=100 leaves the gate open for the full duration; GT=0 is treated as a
// minimal trigger (1 ms) so an external gear's own envelope still gets a
// chance to sound.
static uint32_t GateLengthMs(uint32_t durationMs)
{
    if (s_gate_length_pct >= 100)
        return durationMs;
    uint32_t gateMs = durationMs * static_cast<uint32_t>(s_gate_length_pct) / 100u;
    return gateMs < 1 ? 1 : gateMs;
}

static void SilencePolyVoices()
{
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        s_voice_active[v] = false;
        s_voice_source[v] = VoiceSource::NONE;
        s_midi_notes[v]   = 0xFF;
    }
}

static void SilenceSequencerVoices()
{
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        if (s_voice_source[v] == VoiceSource::SEQUENCER)
        {
            s_voice_active[v] = false;
            s_voice_source[v] = VoiceSource::NONE;
            s_midi_notes[v]   = 0xFF;
        }
    }
}

// Hard silence + MIDI panic: turns off every voice, sends NoteOff for all
// sequencer notes we think are active, and emits an All Notes Off controller
// message. Called on transport stop and other panic situations to guarantee
// no stuck note.
static void PanicSilence()
{
    TriggerEnvelope(false);
    s_arp_active = false;
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        if (s_voice_source[v] == VoiceSource::SEQUENCER && s_midi_notes[v] != 0xFF)
        {
            s_midi_out.SendNoteOff(s_midi_notes[v], 0, 0);
        }
        s_voice_active[v] = false;
        s_voice_source[v] = VoiceSource::NONE;
        s_midi_notes[v]   = 0xFF;
        s_gate_notes[v]   = 0xFF;
    }
    s_midi_in_note_count = 0;
    s_midi_gate_wanted   = false;
    s_midi_out.SendAllNotesOff(0);
}

// Release only the internal envelope when a gray step is entered manually
// (e.g. editing the current playing step to OFF). MIDI notes are left
// ringing because the user may want the tie effect. The oscillator itself is
// NOT cut here: only ENV1's gate closes, so its Release time actually plays
// out through the VCA instead of being silenced instantly.
static void ReleaseSequencerVoices()
{
    s_arp_active = false;
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        if (s_voice_source[v] == VoiceSource::SEQUENCER)
        {
            s_voice_source[v] = VoiceSource::NONE;
            s_midi_notes[v]   = 0xFF;
            s_gate_notes[v]   = 0xFF;
        }
    }
    TriggerEnvelope(false);
}

static void SendPlayStatus();  // forward declaration for ApplySys440Hz

// SYSTEM > 440Hz action: toggle a clean 440Hz sine test tone straight to the
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
        s_poly_clock_running = false;
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

static void CycleGateLength(int32_t delta)
{
    int32_t newValue = s_gate_length_pct + delta * 5;
    if (newValue < 0)
        newValue = 0;
    else if (newValue > 100)
        newValue = 100;
    s_gate_length_pct = newValue;
}

// Sounds one note of the currently-armed arpeggio (voice 0 reused
// monophonically), turning off whatever note voice 0 was previously playing.
static void PlayArpNote(int index)
{
    if (s_midi_notes[0] != 0xFF)
        s_midi_out.SendNoteOff(s_midi_notes[0], 0, 0);
    s_gate_notes[0] = 0xFF;

    float freq = s_arp_root_freq * powf(2.0f, s_arp_intervals[index] / 12.0f);
    s_midi_notes[0]   = FrequencyToMidiNote(freq);
    {
        ScopedIrqBlocker audioUpdate;
        SetVoicePitchAmp(0, freq, 0.3f / kMaxChordVoices);
        s_voice_active[0] = true;
        TriggerModelVoice(0);
        s_voice_source[0] = VoiceSource::SEQUENCER;
        for (int voice = 1; voice < kMaxChordVoices; ++voice)
        {
            s_voice_active[voice] = false;
            s_voice_source[voice] = VoiceSource::NONE;
            s_midi_notes[voice] = 0xFF;
        }
        s_vcf_last_note_freq = freq;
        TriggerEnvelope(true);
    }
    s_midi_out.SendNoteOn(s_midi_notes[0], 100, 0);

    if (s_gate_length_pct < 100)
    {
        uint32_t gate_ms = GateLengthMs(s_arp_note_duration_ms);
        s_gate_notes[0] = s_midi_notes[0];
        s_gate_note_off_ms[0] = System::GetNow() + gate_ms;
    }
}

// Sounds `stepIndex` on the voice pool: a plain NOTE step uses voice 0 only;
// an ARPEGGIO step cycles the PLAY submenu's currently-selected Chords
// intervals one note at a time over the step's duration (see the main loop's
// arpeggio advance); a CHORD step plays all of those intervals simultaneously.
// An OFF step sends no trigger to ENV1/VCA: the oscillator only sounds when a
// note/arp/chord actually plays. MIDI note tracking (s_midi_notes/tie) is
// left untouched here so the external MIDI note keeps ringing until the next
// sounding step turns it off -- this OFF handling only affects the internal
// OSC/ENV1/VCA chain, never the MIDI output.
static void TriggerPolyStep(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= kMaxPolySteps)
        return;
    const PolyStep& step = s_poly_steps[stepIndex];

    // Gray / inactive step: no note is played, so no trigger is sent. Close
    // ENV1's gate so the oscillator fades out via its own Release time
    // instead of continuing to sound (MIDI note tracking is untouched: the
    // next sounding step still sends its proper Note Off, same tie as before).
    if (step.state == POLY_OFF)
    {
        s_arp_active = false;
        TriggerEnvelope(false);
        return;
    }

    // Sounding step: first cut any previous sequencer notes (and cancel their
    // scheduled gate Note Offs), then start the new notes.
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
        s_gate_notes[v] = 0xFF;
    }

    s_arp_active = false;

    // FIXED steps carry their own encoder_prog transpose and ignore AUTO.
    int32_t stepTranspose = (step.state == POLY_FIXED)
                            ? step.fixedTranspose
                            : GetPerformanceTransposeDegrees();
    float rootFreq = DegreeToFrequencyHz(step.degree, stepTranspose);
    if (step.state == POLY_CHORD)
    {
        int32_t chordIdx = kPlaySubmenu[3].selectedIndex;
        const ChordDef& chord = kChordDefs[((chordIdx % kChordDefCount) + kChordDefCount) % kChordDefCount];
        int noteCount = chord.count < kMaxChordVoices ? chord.count : kMaxChordVoices;

        uint32_t chord_step_ms = CurrentStepDurationMs();
        uint32_t chord_gate_ms = GateLengthMs(chord_step_ms);

        float frequencies[kMaxChordVoices];
        for (int voice = 0; voice < noteCount; ++voice)
        {
            frequencies[voice] = rootFreq * powf(2.0f, chord.intervals[voice] / 12.0f);
            s_midi_notes[voice] = FrequencyToMidiNote(frequencies[voice]);
        }
        {
            ScopedIrqBlocker audioUpdate;
            for (int voice = 0; voice < kMaxChordVoices; ++voice)
            {
                if (voice < noteCount)
                {
                    SetVoicePitchAmp(voice, frequencies[voice], 0.3f / kMaxChordVoices);
                    s_voice_active[voice] = true;
                    TriggerModelVoice(voice);
                    s_voice_source[voice] = VoiceSource::SEQUENCER;
                }
                else
                {
                    s_voice_active[voice] = false;
                    s_voice_source[voice] = VoiceSource::NONE;
                }
            }
            s_vcf_last_note_freq = rootFreq;
            TriggerEnvelope(true);
        }
        for (int voice = 0; voice < noteCount; ++voice)
        {
            s_midi_out.SendNoteOn(s_midi_notes[voice], 100, 0);
            if (s_gate_length_pct < 100)
            {
                s_gate_notes[voice] = s_midi_notes[voice];
                s_gate_note_off_ms[voice] = System::GetNow() + chord_gate_ms;
            }
        }
    }
    else if (step.state == POLY_ARP)
    {
        int32_t chordIdx = kPlaySubmenu[3].selectedIndex;
        const ChordDef& chord = kChordDefs[((chordIdx % kChordDefCount) + kChordDefCount) % kChordDefCount];
        int noteCount = chord.count < kMaxChordVoices ? chord.count : kMaxChordVoices;

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
    }
    else if (step.state == POLY_NOTE || step.state == POLY_FIXED)
    {
        uint32_t note_step_ms = CurrentStepDurationMs();
        uint32_t note_gate_ms = GateLengthMs(note_step_ms);

        s_midi_notes[0]   = FrequencyToMidiNote(rootFreq);
        {
            ScopedIrqBlocker audioUpdate;
            SetVoicePitchAmp(0, rootFreq, 0.3f / kMaxChordVoices);
            s_voice_active[0] = true;
            TriggerModelVoice(0);
            s_voice_source[0] = VoiceSource::SEQUENCER;
            for (int voice = 1; voice < kMaxChordVoices; ++voice)
            {
                s_voice_active[voice] = false;
                s_voice_source[voice] = VoiceSource::NONE;
                s_midi_notes[voice] = 0xFF;
            }
            s_vcf_last_note_freq = rootFreq;
            TriggerEnvelope(true);
        }
        s_midi_out.SendNoteOn(s_midi_notes[0], 100, 0);

        if (s_gate_length_pct < 100)
        {
            s_gate_notes[0] = s_midi_notes[0];
            s_gate_note_off_ms[0] = System::GetNow() + note_gate_ms;
        }
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

// -----------------------------------------------------------------------------
// MIDI IN helpers
// -----------------------------------------------------------------------------

static float MidiNoteToFrequency(uint8_t note)
{
    return 440.0f * powf(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

static int FindMidiVoiceForNote(uint8_t note)
{
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        if (s_voice_active[v]
            && s_voice_source[v] == VoiceSource::MIDI_IN
            && s_midi_notes[v] == note)
        {
            return v;
        }
    }
    return -1;
}

static int AllocateMidiVoice()
{
    // Checks ownership (source), not voice_active: a sequencer voice that was
    // just released (see ReleaseSequencerVoices) stays voice_active=true
    // while ENV1 tails off, but is unowned (source==NONE) and safe to steal.
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        if (s_voice_source[v] == VoiceSource::NONE)
            return v;
    }
    return 0;
}

static void TriggerMidiNoteOn(uint8_t note, uint8_t velocity)
{
    if (velocity == 0)
    {
        TriggerMidiNoteOff(note);
        return;
    }

    int v = FindMidiVoiceForNote(note);
    if (v < 0)
        v = AllocateMidiVoice();

    float freq = MidiNoteToFrequency(note);
    float velRatio    = static_cast<float>(velocity) / 127.0f;
    float sensitivity = static_cast<float>(kEnv1VelocityParam.value) / 100.0f;
    float amp = (0.3f / kMaxChordVoices) * (1.0f - sensitivity + velRatio * sensitivity);

    SetVoicePitchAmp(v, freq, amp);
    s_voice_active[v] = true;
    TriggerModelVoice(v);
    s_voice_source[v] = VoiceSource::MIDI_IN;
    s_midi_notes[v]   = note;
    s_vcf_last_note_freq = freq;

    if (s_midi_in_note_count == 0)
    {
        s_midi_gate_wanted = true;
        s_midi_retrigger_count++;
    }
    s_midi_in_note_count++;
}

static void TriggerMidiNoteOff(uint8_t note)
{
    int v = FindMidiVoiceForNote(note);
    if (v < 0)
        return;

    s_voice_active[v] = false;
    s_voice_source[v] = VoiceSource::NONE;
    s_midi_notes[v]   = 0xFF;

    if (s_midi_in_note_count > 0)
        s_midi_in_note_count--;

    if (s_midi_in_note_count == 0)
        s_midi_gate_wanted = false;
}

static void TriggerMidiAllNotesOff()
{
    for (int v = 0; v < kMaxChordVoices; ++v)
    {
        if (s_voice_source[v] == VoiceSource::MIDI_IN)
        {
            s_voice_active[v] = false;
            s_voice_source[v] = VoiceSource::NONE;
            s_midi_notes[v]   = 0xFF;
        }
    }
    s_midi_in_note_count = 0;
    s_midi_gate_wanted   = false;
}

static void ProcessMidiInMessage(uint8_t status, uint8_t data0, uint8_t data1)
{
    uint8_t channel = status & 0x0F;
    if (channel != s_midi_in_channel)
        return;

    uint8_t type = status & 0xF0;
    if (type == 0x90) // Note On
    {
        if (data1 == 0)
            TriggerMidiNoteOff(data0);
        else
            TriggerMidiNoteOn(data0, data1);
    }
    else if (type == 0x80) // Note Off
    {
        TriggerMidiNoteOff(data0);
    }
    else if (type == 0xB0 && data0 == 123) // CC 123 All Notes Off
    {
        TriggerMidiAllNotesOff();
    }
}

static void ServiceMidiIn()
{
    s_usb_midi.Listen();
    while (s_usb_midi.HasEvents())
    {
        const auto event = s_usb_midi.PopEvent();
        uint8_t status;
        switch (event.type)
        {
            case daisy::NoteOn: status = 0x90; break;
            case daisy::NoteOff: status = 0x80; break;
            case daisy::ControlChange: status = 0xB0; break;
            default: continue;
        }
        ProcessMidiInMessage(status | event.channel, event.data[0], event.data[1]);
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
// Status frames are emitted one per call to avoid USB/LOGGER buffer
// congestion. SendNextStatusFrame() rotates through STAT -> VCF -> LFO -> MAT
// -> EN2 -> LF2 so no single main-loop iteration buries the outgoing FIFO
// with back-to-back lines. This keeps NAV/EDIT frames responsive and avoids
// the historical frame-fusion bug. SendPlayStatus() just restarts the
// rotation from STAT.
static uint8_t s_status_frame_index = 0;

static void SendNextStatusFrame()
{
    int note = (s_midi_notes[0] != 0xFF) ? static_cast<int>(s_midi_notes[0]) : 255;

    switch (s_status_frame_index)
    {
        case 0:
            DisplayPrintLine("STAT,BPM=%ld,ROOT=%ld,SCALE=%ld,PLAY=%d,DIR=%d,TRNS=%ld,AUTO=%ld,NOTE=%d,T440=%d,EA=%ld,ED=%ld,ES=%ld,ER=%ld",
                         static_cast<long>(kBpmParam.value),
                         static_cast<long>(kPlaySubmenu[1].selectedIndex),
                         static_cast<long>(kPlaySubmenu[2].selectedIndex),
                         s_playing ? 1 : 0,
                         static_cast<int>(s_playback_direction),
                         static_cast<long>(s_prog_transpose_degrees),
                         static_cast<long>(kPlayAutoParam.value),
                         note,
                         s_440hz_test_active ? 1 : 0,
                         static_cast<long>(kEnv1AttackParam.value),
                         static_cast<long>(kEnv1DecayParam.value),
                         static_cast<long>(kEnv1SustainParam.value),
                         static_cast<long>(kEnv1ReleaseParam.value));
            break;
        case 1:
            DisplayPrintLine("VCF,VFT=%ld,VC=%ld,VR=%ld,VK=%ld,VD=%ld,VE=%ld",
                         static_cast<long>(kVcfSubmenu[0].selectedIndex),
                         static_cast<long>(kVcfCutoffParam.value),
                         static_cast<long>(kVcfResonanceParam.value),
                         static_cast<long>(kVcfKeyParam.value),
                         static_cast<long>(kVcfDriveParam.value),
                         static_cast<long>(kVcfEnvParam.value));
            break;
        case 2:
            DisplayPrintLine("LFO,LS=%ld,LR=%ld,LSY=%ld,LA=%ld,LP=%ld",
                         static_cast<long>(kLfoSubmenu[0].selectedIndex),
                         static_cast<long>(kLfo1RateParam.value),
                         static_cast<long>(kLfoSubmenu[2].selectedIndex),
                         static_cast<long>(kLfo1AmpParam.value),
                         static_cast<long>(kLfo1PhaseParam.value));
            break;
        case 3:
            DisplayPrintLine("MAT,S1=%ld,D1=%ld,A1=%ld,S2=%ld,D2=%ld,A2=%ld",
                         static_cast<long>(kMatrixSubmenu[0].selectedIndex),
                         static_cast<long>(kMatrixSubmenu[1].selectedIndex),
                         static_cast<long>(kMatSlot1AmtParam.value),
                         static_cast<long>(kMatrixSubmenu[3].selectedIndex),
                         static_cast<long>(kMatrixSubmenu[4].selectedIndex),
                         static_cast<long>(kMatSlot2AmtParam.value));
            break;
        case 4:
            DisplayPrintLine("EN2,EA=%ld,ED=%ld,ES=%ld,ER=%ld",
                         static_cast<long>(kEnv2AttackParam.value),
                         static_cast<long>(kEnv2DecayParam.value),
                         static_cast<long>(kEnv2SustainParam.value),
                         static_cast<long>(kEnv2ReleaseParam.value));
            break;
        case 5:
            DisplayPrintLine("LF2,LS=%ld,LR=%ld,LSY=%ld,LA=%ld,LP=%ld",
                         static_cast<long>(kLfoSubmenu[5].selectedIndex),
                         static_cast<long>(kLfo2RateParam.value),
                         static_cast<long>(kLfoSubmenu[7].selectedIndex),
                         static_cast<long>(kLfo2AmpParam.value),
                         static_cast<long>(kLfo2PhaseParam.value));
            break;
                case 6:
                    DisplayPrintLine("OSC,W=%ld,A=%ld,R=%ld,F=%ld,M=%ld,C=%ld,I=%ld,P=%ld,S=%ld,H=%ld",
                         static_cast<long>(kOscSubmenu[0].selectedIndex),
                         static_cast<long>(kOscFmAmtParam.value),
                         static_cast<long>(kOscFmRatioParam.value),
                         static_cast<long>(kOscFmRatioFineParam.value),
                         static_cast<long>(kOscFmSubmenu[3].selectedIndex),
                         static_cast<long>(kOscCoarseParam.value),
                         static_cast<long>(kOscFineParam.value),
                         static_cast<long>(kOscPulseParam.value),
                         static_cast<long>(kOscSubParam.value),
                         static_cast<long>(kOscHardParam.value));
                    break;
                case 7:
                    DisplayPrintLine("FXS,T1=%ld,T2=%ld,T3=%ld,T4=%ld",
                         static_cast<long>(kFxFx1Submenu[0].selectedIndex),
                         static_cast<long>(kFxFx2Submenu[0].selectedIndex),
                         static_cast<long>(kFxFx3Submenu[0].selectedIndex),
                         static_cast<long>(kFxFx4Submenu[0].selectedIndex));
                    break;
                case 8:
                    DisplayPrintLine("MDL,ST=%ld,BR=%ld,DM=%ld,AC=%ld,EX=%ld,SU=%ld",
                         static_cast<long>(kOscModelStructureParam.value),
                         static_cast<long>(kOscModelBrightnessParam.value),
                         static_cast<long>(kOscModelDampingParam.value),
                         static_cast<long>(kOscModelAccentParam.value),
                         static_cast<long>(kOscModelExciterParam.value),
                         static_cast<long>(kOscModelSubmenu[5].selectedIndex));
                    break;
    }
                s_status_frame_index = (s_status_frame_index + 1) % 9;
}

static void SendPlayStatus()
{
    const uint8_t next_frame = s_status_frame_index;
    s_status_frame_index = 0;
    SendNextStatusFrame();
    s_status_frame_index = next_frame;
}

// Sends the full navigation path as "NAV,P=<idx0>.<idx1>...[,V=<n>]": one
// index per depth level, from the root sentinel down to the currently
// active node, plus an optional trailing preview of the highlighted item's
// current value (e.g. ONDE's waveform) when it is itself a live-value list
// -- lets the ESP32 show that value in its center hub without requiring the
// user to drill into it first.
static bool s_patch_used[dco::kPatchSlotCount] = {};
static int32_t s_current_patch = 0;
static bool s_patch_error = false;

static int32_t NextUsedPatchSlot(int32_t slot, int32_t direction)
{
    int32_t slots[dco::kPatchSlotCount];
    int32_t count = 0;
    int32_t position = 0;
    for (int32_t candidate = 1; candidate <= dco::kPatchSlotCount; ++candidate)
    {
        if (!s_patch_used[candidate - 1])
            continue;
        if (candidate == slot)
            position = count;
        slots[count++] = candidate;
    }
    if (count == 0)
        return 0;
    position += direction;
    position = position < 0 ? 0 : (position >= count ? count - 1 : position);
    return slots[position];
}

static void SendPresetState(int mode, int32_t slot)
{
    char used[dco::kPatchSlotCount / 4 + 1];
    for (int group = 0; group < dco::kPatchSlotCount / 4; ++group)
    {
        int bits = 0;
        for (int bit = 0; bit < 4; ++bit)
            if (s_patch_used[group * 4 + bit])
                bits |= 1 << bit;
        used[group] = "0123456789ABCDEF"[bits];
    }
    used[sizeof(used) - 1] = '\0';
    DisplayPrintLine("PRST,C=%ld,M=%d,S=%ld,U=%s,W=%ld,B=%ld,R=%ld,G=%ld,F=%ld,H=%ld,N=%d,E=%d,SD=%d,IO=%d",
                 static_cast<long>(s_current_patch), mode, static_cast<long>(slot), used,
                 static_cast<long>(kOscSubmenu[0].selectedIndex),
                 static_cast<long>(kBpmParam.value),
                 static_cast<long>(kPlaySubmenu[1].selectedIndex),
                 static_cast<long>(kPlaySubmenu[2].selectedIndex),
                 static_cast<long>(kVcfSubmenu[0].selectedIndex),
                 static_cast<long>(kVcfCutoffParam.value), ClampedPolyStepCount(),
                 s_patch_error ? 1 : 0, s_sd_client.available ? 1 : 0,
                 s_sd_client.Busy() ? 1 : 0);
}

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

    if (cur->children == kPresetsSubmenu)
        SendPresetState(0, 0);
    DisplayPrintLine("NAV,P=%s", path);
}

// Streams the numeric value currently being edited (e.g. BPM) so the ESP32
// can render the ring-gauge parameter-edit screen; sent on entry, on every
// value change, and on the periodic heartbeat while editing.
static void SendEditState()
{
    if (s_editing_node == nullptr || s_editing_node->numeric == nullptr)
        return;
    const NumericParam* p = s_editing_node->numeric;
    if (p == &kPatchLoadParam || p == &kPatchSaveParam)
    {
        SendPresetState(p == &kPatchLoadParam ? 1 : 2, p->value);
        return;
    }
    DisplayPrintLine("EDIT,V=%ld,MIN=%ld,MAX=%ld,U=%s",
                 static_cast<long>(p->value), static_cast<long>(p->minValue),
                 static_cast<long>(p->maxValue), p->unit ? p->unit : "");
}

// Streams the polymetric step wheel overlay: step count, edit cursor, one
// state digit per step ('0'=off,'1'=note,'2'=arpeggio,'3'=chord,'4'=fixed), the cursor step's
// degree (for the ESP32 to show while encoder_submenu adjusts it), the
// currently-sounding playhead step (-1 while stopped), the MIDI note
// number currently being played (255 = none) and the global gate length %.
// Sent on entry, on every cursor/state/degree change, on every playhead
// advance, and on the periodic heartbeat while the wheel is shown.
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

    DisplayPrintLine("POLY,N=%d,C=%d,ST=%s,DEG=%d,PLAY=%d,NOTE=%d,GATE=%d",
                 stepCount, s_poly_cursor, states,
                 static_cast<int>(s_poly_steps[s_poly_cursor].degree), s_poly_play_step, note,
                 static_cast<int>(s_gate_length_pct));
}

// -----------------------------------------------------------------------------
// Patch preset storage (SD card on ESP32 via UART)
// -----------------------------------------------------------------------------

// Re-apply every audio engine block after a patch load or factory reset.
// WalkMenuTree only fires onSelect callbacks for list nodes; numeric leaves
// (cutoff, envelope times, etc.) must be pushed explicitly.
static void ApplyAllEngineSettings()
{
    InitEnvelope2();
    s_env1_adsr.Init(hw.AudioSampleRate());
    ApplyEnv1Attack(kEnv1AttackParam.value);
    ApplyEnv1Decay(kEnv1DecayParam.value);
    ApplyEnv1Sustain(kEnv1SustainParam.value);
    ApplyEnv1Release(kEnv1ReleaseParam.value);

    s_vcf_filter.Init(hw.AudioSampleRate());
    ApplyVcfType(kVcfSubmenu[0].selectedIndex);
    ApplyVcfCutoff(kVcfCutoffParam.value);
    ApplyVcfResonance(kVcfResonanceParam.value);
    ApplyVcfKey(kVcfKeyParam.value);
    ApplyVcfDrive(kVcfDriveParam.value);
    ApplyVcfEnv(kVcfEnvParam.value);

    s_lfo1.Init(hw.AudioSampleRate());
    ApplyLfo1Shape(kLfoSubmenu[0].selectedIndex);
    ApplyLfo1Rate(kLfo1RateParam.value);
    ApplyLfo1Sync(kLfoSubmenu[2].selectedIndex);
    ApplyLfo1Amp(kLfo1AmpParam.value);
    ApplyLfo1Phase(kLfo1PhaseParam.value);

    s_lfo2.Init(hw.AudioSampleRate());
    ApplyLfo2Shape(kLfoSubmenu[5].selectedIndex);
    ApplyLfo2Rate(kLfo2RateParam.value);
    ApplyLfo2Sync(kLfoSubmenu[7].selectedIndex);
    ApplyLfo2Amp(kLfo2AmpParam.value);
    ApplyLfo2Phase(kLfo2PhaseParam.value);
    ApplyMatSlot1Amt(kMatSlot1AmtParam.value);
    ApplyMatSlot2Amt(kMatSlot2AmtParam.value);
    ApplyOscCoarse(kOscCoarseParam.value);
    ApplyOscFine(kOscFineParam.value);
    ApplyOscPulse(kOscPulseParam.value);
    ApplyOscSub(kOscSubParam.value);
    ApplyOscHard(kOscHardParam.value);

    ApplySysVolume(kSysVolumeParam.value);

    ApplyOscFmModWave(kOscFmSubmenu[3].selectedIndex);
    ApplyOscFmRatio(kOscFmRatioParam.value);
    ApplyOscFmRatioFine(kOscFmRatioFineParam.value);
    ApplyOscFmAmt(kOscFmAmtParam.value);

    ApplyAllFxSettings();

    ApplyMidiChannel(kMidiChannelParam.value);
}

static void CopySettingsToPatchData(dco::PatchData& patch)
{
    SynthSettings tmp;
    uint32_t vi = 0, si = 0;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kCollect, patch, vi, si);
    SyncPolyStepsState(SettingsWalkMode::kCollect, tmp);

    patch.numValues     = vi;
    patch.numSelections = si;
    patch.numPolySteps  = kMaxPolySteps;
    patch.gateLengthPct = tmp.gateLengthPct;
    memcpy(patch.polySteps, tmp.polySteps, sizeof(patch.polySteps));
}

static void ApplyPatchData(const dco::PatchData& patch)
{
    SynthSettings tmp;
    tmp.numValues     = patch.numValues;
    tmp.numSelections = patch.numSelections;
    tmp.numPolySteps  = patch.numPolySteps;
    tmp.gateLengthPct = patch.gateLengthPct;
    memcpy(tmp.polySteps, patch.polySteps, sizeof(tmp.polySteps));

    uint32_t vi = 0, si = 0;
    dco::PatchData restored = patch;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kApply, restored, vi, si);
    SyncPolyStepsState(SettingsWalkMode::kApply, tmp);
}

static void ApplyPatchSave(int32_t slot)
{
    DEBUG_LOG("[PATCH] SAVE requested slot %ld", static_cast<long>(slot));
    if (slot < 1 || slot > dco::kPatchSlotCount)
    {
        DEBUG_LOG("[PATCH] SAVE invalid slot");
        return;
    }
    dco::PatchData patch;
    CopySettingsToPatchData(patch);
    DEBUG_LOG("[PATCH] SAVE copying BPM=%ld", static_cast<long>(kBpmParam.value));
    s_patch_error = !s_sd_client.Start(dco::PatternClient::Operation::Save, slot,
                                     System::GetNow(), &patch);
    SendMenuPath();
}

static void ApplyPatchLoad(int32_t slot)
{
    DEBUG_LOG("[PATCH] LOAD requested slot %ld", static_cast<long>(slot));
    if (slot < 1 || slot > dco::kPatchSlotCount)
    {
        DEBUG_LOG("[PATCH] LOAD invalid slot");
        return;
    }

    s_patch_error = !s_sd_client.Start(dco::PatternClient::Operation::Load, slot, System::GetNow());
    SendMenuPath();
}

static void CompletePatchLoad(int32_t slot, const dco::PatchData& patch)
{
    s_current_patch = slot;
    DEBUG_LOG("[PATCH] LOAD slot %ld OK", static_cast<long>(slot));

    if (s_playing)
    {
        s_playing = false;
        s_midi_clock_running = false;
        s_poly_clock_running = false;
        s_midi_out.SendStop();
    }
    PanicSilence();
    s_440hz_test_active = false;
    s_poly_wheel_active = false;
    s_poly_cursor       = 0;
    s_poly_play_step    = -1;

    ApplyPatchData(patch);
    s_prog_transpose_degrees  = 0;
    s_auto_steps_until_change = kPlayAutoParam.value;

    ApplyAllEngineSettings();

    MarkSettingsDirty(System::GetNow());

    // Start playback immediately after a successful load.
    s_playing = true;
    s_poly_play_step = 0;
    s_auto_steps_until_change = kPlayAutoParam.value;
    TriggerPolyStep(s_poly_play_step);
    s_midi_out.SendStart();
    s_midi_clock_phase = 0.0f;
    s_midi_clock_running = true;
    s_poly_step_phase = 0.0f;
    s_poly_clock_running = true;

    s_poly_wheel_active = true;
    SendPlayStatus();
    SendPolyState();
}

static unsigned s_remote_pending = 0;

static MenuNode* RemoteParameterNode(int group, int parameter)
{
    if (parameter < 0) return nullptr;
    if (group == 0 && parameter < 10)
        return parameter < 6 ? &kOscSubmenu[parameter] : &kOscFmSubmenu[parameter - 6];
    if (group == 1 && parameter < 6) return &kVcfSubmenu[parameter];
    if (group == 2 && parameter < 10) return &kLfoSubmenu[parameter];
    if (group == 3 && parameter < 6) return &kMatrixSubmenu[parameter];
    if (group == 4 && parameter < 8)
        return parameter < 4 ? &kEnv1Submenu[parameter] : &kEnv2Submenu[parameter - 4];
    return nullptr;
}

static bool ApplyRemoteParameter(const dco::RemoteCommand& command)
{
    if (command.group == 3)
    {
        int field = command.parameter % 3;
        if ((field == 0 && !dco::MatrixSourceSupported(command.value))
            || (field == 1 && (command.value < 0 || command.value > 7))
            || (field == 2 && (command.value < -100 || command.value > 100))) return false;
        if (command.parameter >= 6)
        {
            ScopedIrqBlocker blocker;
            auto& route = s_matrix_slots[command.parameter / 3];
            if (field == 0) route.source = command.value;
            else if (field == 1) route.destination = command.value;
            else route.amount = command.value * 0.01f;
            return true;
        }
    }
    MenuNode* node = RemoteParameterNode(command.group, command.parameter);
    if (!node) return false;
    if (node->numeric)
    {
        if (command.value < node->numeric->minValue || command.value > node->numeric->maxValue) return false;
    }
    else if (command.value < 0 || command.value >= node->childCount) return false;
    ScopedIrqBlocker blocker;
    if (node->numeric) node->numeric->value = command.value;
    else node->selectedIndex = command.value;
    if (node->onSelect) node->onSelect(command.value);
    return true;
}

static void SendRemoteParameters()
{
    for (int group = 0; group < 5; ++group)
    {
        int32_t values[24] = {};
        for (int parameter = 0; parameter < 24; ++parameter)
        {
            MenuNode* node = RemoteParameterNode(group, parameter);
            if (node) values[parameter] = node->numeric ? node->numeric->value : node->selectedIndex;
            else if (group == 3 && parameter >= 6)
            {
                const auto& route = s_matrix_slots[parameter / 3];
                values[parameter] = parameter % 3 == 0 ? route.source : parameter % 3 == 1
                    ? route.destination : static_cast<int32_t>(roundf(route.amount * 100.0f));
            }
        }
        char hex[sizeof(values) * 2 + 1];
        dco::EncodeHex(reinterpret_cast<const uint8_t*>(values), sizeof(values), hex);
        DisplayPrintLine("RMP,%d,%d,%s", s_current_patch, group, hex);
    }
}

static void SendRemoteState()
{
    SendRemoteParameters();
    dco::PatchPolyStep steps[kMaxPolySteps];
    for (int index = 0; index < kMaxPolySteps; ++index)
        steps[index] = {s_poly_steps[index].state, s_poly_steps[index].degree,
                        s_poly_steps[index].fixedTranspose};
    char hex[sizeof(steps) * 2 + 1];
    dco::EncodeHex(reinterpret_cast<const uint8_t*>(steps), sizeof(steps), hex);
    DisplayPrintLine("RMS,%d,%d,%d,%d,%ld,%d,%s", s_current_patch, s_playing ? 1 : 0,
                     ClampedPolyStepCount(), s_poly_play_step, static_cast<long>(kBpmParam.value),
                     s_sd_client.Busy() ? 1 : 0, hex);
}

static void RemoteReply(unsigned id, const char* result)
{
    SendRemoteState();
    DisplayPrintLine("RMR,%u,%s", id, result);
}

static void HandleRemoteCommand(const char* line, uint32_t now)
{
    dco::RemoteCommand command;
    if (!dco::ParseRemoteCommand(line, command))
    {
        RemoteReply(command.id, "ERR,REQUEST");
        return;
    }
    if (command.action == dco::RemoteAction::State)
    {
        SendRemoteState();
        return;
    }
    if (!command.id || s_sd_client.Busy() || s_remote_pending)
    {
        RemoteReply(command.id, "ERR,BUSY");
        return;
    }
    if (command.action == dco::RemoteAction::Load || command.action == dco::RemoteAction::Save
        || command.action == dco::RemoteAction::Delete)
    {
        auto operation = command.action == dco::RemoteAction::Load ? dco::PatternClient::Operation::Load
            : command.action == dco::RemoteAction::Save ? dco::PatternClient::Operation::Save
            : dco::PatternClient::Operation::Delete;
        dco::PatchData patch;
        if (operation == dco::PatternClient::Operation::Save) CopySettingsToPatchData(patch);
        if (s_sd_client.Start(operation, command.slot, now, &patch)) s_remote_pending = command.id;
        else RemoteReply(command.id, "ERR,BUSY");
        return;
    }
    if (command.action == dco::RemoteAction::Param)
    {
        if (command.slot != s_current_patch) { RemoteReply(command.id, "ERR,STALE"); return; }
        if (s_editing_numeric) { RemoteReply(command.id, "ERR,BUSY"); return; }
        if (!ApplyRemoteParameter(command)) { RemoteReply(command.id, "ERR,REQUEST"); return; }
        MarkSettingsDirty(now);
    }
    else if (command.action == dco::RemoteAction::Play)
    {
        s_440hz_test_active = false;
        s_playing = true;
        s_poly_play_step = 0;
        s_pingpong_index = 0;
        s_auto_steps_until_change = kPlayAutoParam.value;
        TriggerPolyStep(0);
        s_midi_out.SendStart();
        s_midi_clock_phase = 0.0f;
        s_midi_clock_running = true;
        s_poly_step_phase = 0.0f;
        s_poly_clock_running = true;
    }
    else if (command.action == dco::RemoteAction::Stop)
    {
        s_playing = false;
        s_440hz_test_active = false;
        s_midi_clock_running = false;
        s_poly_clock_running = false;
        s_poly_play_step = -1;
        PanicSilence();
        s_midi_out.SendStop();
    }
    else
    {
        if (command.slot != s_current_patch || command.step >= ClampedPolyStepCount())
        {
            RemoteReply(command.id, "ERR,STALE");
            return;
        }
        s_poly_cursor = command.step;
        s_poly_steps[command.step] = {static_cast<uint8_t>(command.state),
            static_cast<int8_t>(command.degree), static_cast<int8_t>(command.transpose)};
        MarkSettingsDirty(now);
        if (s_playing && s_poly_play_step == command.step)
        {
            if (command.state == POLY_OFF) ReleaseSequencerVoices();
            else TriggerPolyStep(s_poly_play_step);
        }
        s_editing_numeric = false;
        s_poly_wheel_active = true;
    }
    SendPlayStatus();
    if (s_poly_wheel_active) SendPolyState();
    RemoteReply(command.id, "OK");
}

static void ServicePatternStorage(uint32_t now)
{
    static char line[192];
    static size_t length = 0;
    static bool discard = false;
    static uint32_t lastScan = 0;
    if (s_sd_rx_fault)
    {
        ScopedIrqBlocker blocker;
        s_sd_rx_read = s_sd_rx_write;
        s_sd_rx_fault = false;
        length = 0;
        discard = true;
    }
    while (s_sd_rx_read != s_sd_rx_write)
    {
        char value = static_cast<char>(s_sd_rx_ring[s_sd_rx_read]);
        s_sd_rx_read = (s_sd_rx_read + 1) % sizeof(s_sd_rx_ring);
        if (value == '\r') continue;
        if (value == '\n')
        {
            line[length] = '\0';
            if (!discard)
            {
                if (strncmp(line, "RMC,", 4) == 0) HandleRemoteCommand(line, now);
                else s_sd_client.Receive(line, now);
            }
            length = 0;
            discard = false;
        }
        else if (!discard)
        {
            if (length < sizeof(line) - 1) line[length++] = value;
            else { length = 0; discard = true; }
        }
    }
    s_sd_client.Tick(now);
    dco::PatternClient::Operation operation;
    int slot;
    bool success;
    if (s_sd_client.TakeResult(operation, slot, success))
    {
        if (operation == dco::PatternClient::Operation::List)
        {
            for (int index = 0; index < dco::kPatchSlotCount; ++index)
                s_patch_used[index] = success && (s_sd_client.used[index / 8] & (1U << (index % 8)));
        }
        else
        {
            if (success && operation == dco::PatternClient::Operation::Load)
                success = s_sd_client.record.data.numValues == s_factory_settings.numValues
                    && s_sd_client.record.data.numSelections == s_factory_settings.numSelections;
            s_patch_error = !success;
            if (success && operation == dco::PatternClient::Operation::Save)
            {
                s_patch_used[slot - 1] = true;
                s_current_patch = slot;
            }
            if (success && operation == dco::PatternClient::Operation::Delete)
            {
                s_patch_used[slot - 1] = false;
                if (s_current_patch == slot) s_current_patch = 0;
            }
            DEBUG_LOG("[SD] %s slot %d: %s",
                         operation == dco::PatternClient::Operation::Save ? "SAVE" : "LOAD",
                         slot, success ? "OK" : "FAILED");
            if (success && operation == dco::PatternClient::Operation::Load)
            {
                s_editing_numeric = false;
                CompletePatchLoad(slot, s_sd_client.record.data);
                if (s_remote_pending) RemoteReply(s_remote_pending, "OK");
                s_remote_pending = 0;
                return;
            }
            if (s_remote_pending) RemoteReply(s_remote_pending, success ? "OK" : "ERR,SD");
            s_remote_pending = 0;
        }
        if (s_menu_stack[s_menu_depth]->children == kPresetsSubmenu)
        {
            if (s_editing_numeric) SendEditState();
            else SendMenuPath();
        }
    }
    if (!s_sd_client.Busy() && !s_editing_numeric && now - lastScan >= 5000)
    {
        lastScan = now;
        s_sd_client.Start(dco::PatternClient::Operation::List, 0, now);
    }
    if (s_display_uart_ready && !s_display_uart.IsListening())
        s_display_uart.DmaListenStart(s_sd_dma_buffer, sizeof(s_sd_dma_buffer), PatternUartRx, nullptr);
}

static void ApplyInitPatch(int32_t index)
{
    (void)index;
    s_current_patch = 0;
    s_patch_error = false;
    if (s_playing)
    {
        s_playing = false;
        s_midi_clock_running = false;
        s_poly_clock_running = false;
        s_midi_out.SendStop();
    }
    PanicSilence();
    s_440hz_test_active = false;
    s_poly_wheel_active = false;
    s_poly_cursor       = 0;
    s_poly_play_step    = -1;
    s_prog_transpose_degrees  = 0;
    s_auto_steps_until_change = 0;

    uint32_t vi = 0, si = 0;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kApply, s_factory_settings, vi, si);
    SyncPolyStepsState(SettingsWalkMode::kApply, s_factory_settings);

    ApplyAllEngineSettings();

    MarkSettingsDirty(System::GetNow());
    SendPlayStatus();
    SendPolyState();
    SendMenuPath();
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
    
    DisplayPrintLine("%s", buf);
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
        s_fm_voices[v].Init(hw.AudioSampleRate());
        s_fm_voices[v].SetAmplitude(0.3f / kMaxChordVoices);
        s_model_voices[v].Init(hw.AudioSampleRate());
        s_model_voices[v].SetAmplitude(0.3f / kMaxChordVoices);
    }

    // Test-mode 440Hz sine generator (SYSTEM > 440Hz), kept quiet at 0.5 amplitude
    s_test_osc.Init(hw.AudioSampleRate());
    s_test_osc.SetFreq(440.0f);
    s_test_osc.SetAmp(0.5f);
    s_test_osc.SetWaveform(daisysp::Oscillator::WAVE_SIN);

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
        s_factory_settings     = defaults;  // keep factory defaults for CMD_INIT_PATCH
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
    InitEnvelope2();
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

    // Initialize LFO1 the same way: Shape/Sync are list nodes, so
    // WalkMenuTree's restore already re-fires ApplyLfo1Shape/ApplyLfo1Sync via
    // onSelect; Rate/Amp/Phase are numeric leaves and need an explicit push.
    s_lfo1.Init(hw.AudioSampleRate());
    ApplyLfo1Shape(kLfoSubmenu[0].selectedIndex);
    ApplyLfo1Rate(kLfo1RateParam.value);
    ApplyLfo1Sync(kLfoSubmenu[2].selectedIndex);
    ApplyLfo1Amp(kLfo1AmpParam.value);
    ApplyLfo1Phase(kLfo1PhaseParam.value);

    s_lfo2.Init(hw.AudioSampleRate());
    ApplyLfo2Shape(kLfoSubmenu[5].selectedIndex);
    ApplyLfo2Rate(kLfo2RateParam.value);
    ApplyLfo2Sync(kLfoSubmenu[7].selectedIndex);
    ApplyLfo2Amp(kLfo2AmpParam.value);
    ApplyLfo2Phase(kLfo2PhaseParam.value);
    ApplyMatSlot1Amt(kMatSlot1AmtParam.value);
    ApplyMatSlot2Amt(kMatSlot2AmtParam.value);

    // Initialize the four-slot FX chain and push the restored/default values.
    s_fx_chain.Init(hw.AudioSampleRate());
    ApplyAllFxSettings();

    // Master volume also isn't re-applied by WalkMenuTree's restore (it only
    // re-fires onSelect for parent/list nodes, not numeric leaves), so without
    // this it silently stays at its 1.0f default regardless of the persisted
    // Volume value.
    ApplySysVolume(kSysVolumeParam.value);

    // OSC > FM: Amount/Ratio/Ratio Fine are numeric leaves (explicit push
    // needed, same reasoning as VCF/LFO above); Mod Wave is a list node so
    // WalkMenuTree's restore already re-fires ApplyOscFmModWave via
    // onSelect -- still pushed explicitly here to cover the very first boot
    // (no valid flash yet), matching ApplyVcfType/ApplyLfo1Shape's pattern.
    ApplyOscFmModWave(kOscFmSubmenu[3].selectedIndex);
    ApplyOscFmRatio(kOscFmRatioParam.value);
    ApplyOscFmRatioFine(kOscFmRatioFineParam.value);
    ApplyOscFmAmt(kOscFmAmtParam.value);

    // OSC > Model: same reasoning -- Structure/Brightness/Damping/Accent/
    // Exciter are numeric leaves (explicit push needed); Sustain is a list
    // node so WalkMenuTree's restore already re-fires ApplyOscModelSustain,
    // still pushed explicitly here to cover the very first boot.
    ApplyOscModelStructure(kOscModelStructureParam.value);
    ApplyOscModelBrightness(kOscModelBrightnessParam.value);
    ApplyOscModelDamping(kOscModelDampingParam.value);
    ApplyOscModelAccent(kOscModelAccentParam.value);
    ApplyOscModelExciter(kOscModelExciterParam.value);
    ApplyOscModelSustain(kOscModelSubmenu[5].selectedIndex);

    // OSC main parameters are also numeric leaves: push restored/default values.
    ApplyOscCoarse(kOscCoarseParam.value);
    ApplyOscFine(kOscFineParam.value);
    ApplyOscPulse(kOscPulseParam.value);
    ApplyOscSub(kOscSubParam.value);
    ApplyOscHard(kOscHardParam.value);

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
    
    InitDisplayUart();
    InitPatternUart();

    daisy::MidiUsbHandler::Config midi_config;
    midi_config.transport_config.periph = daisy::MidiUsbTransport::Config::INTERNAL;
    FS_Desc.GetProductStrDescriptor = MidiProductDescriptor;
    s_usb_midi.Init(midi_config);
    s_usb_midi.StartReceive();
    s_midi_out.Init(s_usb_midi, 0);

    // Apply the persisted MIDI channel (kMidiChannelParam is 1..16).
    ApplyMidiChannel(kMidiChannelParam.value);
    
    // Print startup message
    DEBUG_LOG("=== DCO-ONE Phase 1 ===");
    DEBUG_LOG("Daisy Seed 3 Audio Engine");
    DEBUG_LOG("Native USB MIDI IN / OUT");
    DEBUG_LOG("  Sample Rate: 48 kHz");
    DEBUG_LOG("  Amplitude: 30%%");
    DEBUG_LOG("Audio output: ACTIVE");
    DEBUG_LOG("=======================");
    DEBUG_LOG("Navigation (Simplified UI):");
    DEBUG_LOG("  Encoder menu (D17/D18): Browse menu & submenu");
    DEBUG_LOG("  Button menu sw (D25): Enter submenu / Exit submenu");
    DEBUG_LOG("  Button home (D12): Go back one menu level");
    DEBUG_LOG("Other encoders reserved for future features.");
    DEBUG_LOG("Monitoring input...");
    
    // Set up audio callback
    hw.SetAudioBlockSize(64);  // 64 samples per block
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    
    hw.StartAudio(AudioCallback);
    
    uint32_t last_menu_send = System::GetNow();
    uint32_t last_status_send = System::GetNow();
    uint32_t last_audio_send = System::GetNow();
    uint32_t last_poly_send = System::GetNow();

    // Send the initial state once so the ESP32 doesn't wait a full heartbeat
    SendMenuPath();

    // Main loop (non real-time)
    while (true)
    {
        uint32_t now = System::GetNow();
        ServicePatternStorage(now);

        ServiceMidiIn();

        // Mirror BPM into a plain float global: AudioCallback runs before the
        // #include "menu_generated.inc" point where kBpmParam is declared, so
        // it can't reference it directly (same reasoning as s_vcf_base_cutoff_hz).
        s_current_bpm_hz = static_cast<float>(kBpmParam.value);
        
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
                s_auto_steps_until_change = kPlayAutoParam.value; // reset AUTO counter
                TriggerPolyStep(s_poly_play_step);
                s_midi_out.SendStart();
                s_midi_clock_phase = 0.0f; // fire the first tick promptly on play
                s_midi_clock_running = true;
                s_poly_step_phase = 0.0f; // fire the first step promptly on play
                s_poly_clock_running = true;
            }
            else
            {
                PanicSilence();
                s_poly_play_step = -1;
                s_midi_out.SendStop();
                s_midi_clock_running = false;
                s_poly_clock_running = false;
            }
            }
            SendPlayStatus();
            if (s_poly_wheel_active)
            {
                SendPolyState();
                last_poly_send = now;
            }
        }

        // ---- button_prog_sw: en mode PLAY (root page ou lecture active),
        //      cycle la direction de lecture : avant -> arrière -> ping-pong -> avant ----
        bool in_play_mode = s_playing
                            || (s_menu_depth == 0 && !s_editing_numeric && !s_poly_wheel_active);
        if (prog_sw_changed && prog_sw_pressed && in_play_mode)
        {
            if (s_playback_direction == 0)
                s_playback_direction = 1;
            else if (s_playback_direction == 1)
            {
                s_playback_direction = 2;
                // Initialise l'index ping-pong sur le step actuel pour ne pas sauter de note
                int stepCount = ClampedPolyStepCount();
                if (stepCount > 1 && s_poly_play_step >= 0 && s_poly_play_step < stepCount)
                    s_pingpong_index = s_poly_play_step;
                else
                    s_pingpong_index = 0;
            }
            else
            {
                s_playback_direction = 0;
            }
        }

        // ---- encoder_prog: adjust the focused ENV1/VCF/LFO parameter directly
        //      (no need to enter the submenu). In the poly wheel, a ROSE
        //      (FIXED) step's stored transpose is edited here. Outside those
        //      it transposes the playing sequence when AUTO is off. ----
        int32_t dir_prog = encoder_prog.GetAndClearSteps();
        if (dir_prog != 0 && s_poly_wheel_active
            && s_poly_steps[s_poly_cursor].state == POLY_FIXED)
        {
            PolyStep& step = s_poly_steps[s_poly_cursor];
            int32_t newFixed = static_cast<int32_t>(step.fixedTranspose) + dir_prog;
            if (newFixed > kMaxProgTransposeDegrees)
                newFixed = kMaxProgTransposeDegrees;
            else if (newFixed < -kMaxProgTransposeDegrees)
                newFixed = -kMaxProgTransposeDegrees;
            if (newFixed != step.fixedTranspose)
            {
                step.fixedTranspose = static_cast<int8_t>(newFixed);
                MarkSettingsDirty(now);
                if (s_playing && s_poly_cursor == s_poly_play_step)
                {
                    TriggerPolyStep(s_poly_play_step);
                }
                SendPolyState();
                last_poly_send = now;
            }
        }
        else if (dir_prog != 0 && s_poly_wheel_active)
        {
            // Outside FIXED steps, encoder_prog adjusts the global gate length.
            int32_t prevGate = s_gate_length_pct;
            CycleGateLength(dir_prog);
            if (s_gate_length_pct != prevGate)
                MarkSettingsDirty(now);
            SendPolyState();
            last_poly_send = now;
        }
        else if (dir_prog != 0)
        {
            bool in_env1 = (s_menu_depth == 1 && s_menu_stack[1]->children == kEnv1Submenu);
            bool in_env2 = (s_menu_depth == 1 && s_menu_stack[1]->children == kEnv2Submenu);
            bool in_vcf  = (s_menu_depth == 1 && s_menu_stack[1]->children == kVcfSubmenu);
            bool in_lfo  = (s_menu_depth == 1 && s_menu_stack[1]->children == kLfoSubmenu);
            if (in_env1 || in_env2 || in_vcf || in_lfo)
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
                else if (in_env2)
                {
                    switch (focusIdx)
                    {
                        case 0: p = &kEnv2AttackParam;  applyFn = ApplyEnv2Attack;  editNode = &kEnv2Submenu[0]; break;
                        case 1: p = &kEnv2DecayParam;   applyFn = ApplyEnv2Decay;   editNode = &kEnv2Submenu[1]; break;
                        case 2: p = &kEnv2SustainParam; applyFn = ApplyEnv2Sustain; editNode = &kEnv2Submenu[2]; break;
                        case 3: p = &kEnv2ReleaseParam; applyFn = ApplyEnv2Release; editNode = &kEnv2Submenu[3]; break;
                    }
                }
                else if (in_vcf) // kVcfSubmenu[0] is the Filter type list, not numeric
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
                else // in_lfo -- kLfoSubmenu[0]/[2]/[5]/[7] (Shape/Sync lists) are not numeric
                {
                    switch (focusIdx)
                    {
                        case 1: p = &kLfo1RateParam;  applyFn = ApplyLfo1Rate;  editNode = &kLfoSubmenu[1]; break;
                        case 3: p = &kLfo1AmpParam;   applyFn = ApplyLfo1Amp;   editNode = &kLfoSubmenu[3]; break;
                        case 4: p = &kLfo1PhaseParam; applyFn = ApplyLfo1Phase; editNode = &kLfoSubmenu[4]; break;
                        case 6: p = &kLfo2RateParam;  applyFn = ApplyLfo2Rate;  editNode = &kLfoSubmenu[6]; break;
                        case 8: p = &kLfo2AmpParam;   applyFn = ApplyLfo2Amp;   editNode = &kLfoSubmenu[8]; break;
                        case 9: p = &kLfo2PhaseParam; applyFn = ApplyLfo2Phase; editNode = &kLfoSubmenu[9]; break;
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
        //      state of the step under the cursor (off -> note -> arpeggio -> chord -> fixed) ----
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
                step.state = static_cast<uint8_t>((step.state + 1) % 5);
                if (step.state == POLY_FIXED)
                    step.fixedTranspose = s_prog_transpose_degrees; // capture current prog transpose
                MarkSettingsDirty(now);  // Mark poly state change for persistence
                // If the edited step is currently sounding, apply it live so a
                // note/chord/fixed step starts on the spot. Switching to gray
                // only releases the internal audio voices (tie: MIDI keeps ringing).
                if (s_playing && s_poly_cursor == s_poly_play_step)
                {
                    if (step.state == POLY_OFF)
                        ReleaseSequencerVoices();
                    else
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

            // Keep the ISR's clock periods in sync with the live BPM/Div,
            // then drain whatever ticks it already decided were due (sample-
            // accurate; see s_midi_clock_*/s_poly_step_* declarations above).
            s_midi_clock_samples_per_tick = (60.0f / bpm / 24.0f) * kAudioSampleRateHz;
            uint32_t clockTicksCommitted = s_midi_clock_ticks_committed;
            while (s_midi_clock_ticks_sent != clockTicksCommitted)
            {
                s_midi_out.SendClock();
                s_midi_clock_ticks_sent++;
            }

            s_poly_step_samples_per_tick = (60.0f / bpm) * PolyDivisionBeats(divIdx) * kAudioSampleRateHz;
            if (s_poly_step_samples_per_tick < 1.0f)
                s_poly_step_samples_per_tick = 1.0f;
            uint32_t polyTicksCommitted = s_poly_step_ticks_committed;
            while (s_poly_step_ticks_sent != polyTicksCommitted)
            {
                s_poly_step_ticks_sent++;
                int stepCount = ClampedPolyStepCount();

                // Advance the playhead according to the selected direction
                if (s_playback_direction == 0)
                {
                    // Forward
                    s_poly_play_step = (s_poly_play_step + 1) % stepCount;
                }
                else if (s_playback_direction == 1)
                {
                    // Backward
                    s_poly_play_step = (s_poly_play_step - 1 + stepCount) % stepCount;
                }
                else
                {
                    // Ping-pong: 0,1,...,N-2,N-1,N-2,...,1,0,1,...
                    // Un index monotonique garantit que chaque step est visité
                    // et que les extrémités ne sont pas sautées au changement de mode.
                    if (stepCount <= 2)
                    {
                        s_poly_play_step = (s_poly_play_step + 1) % stepCount;
                    }
                    else
                    {
                        int32_t period = 2 * (stepCount - 1);
                        s_pingpong_index = (s_pingpong_index + 1) % period;
                        if (s_pingpong_index < stepCount)
                            s_poly_play_step = s_pingpong_index;
                        else
                            s_poly_play_step = period - s_pingpong_index;
                    }
                }

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

        // ---- Scheduled MIDI Note Offs driven by gate length (< 100 %).
        //      MIDI-only: this closes the external Note On/Off pair after
        //      GateLengthMs() but never touches the internal OSC/ENV1/VCA
        //      chain, which is triggered once per step (TriggerPolyStep/
        //      PlayArpNote) and runs its own ADSR shape independently of the
        //      gate percentage (see AudioCallback's ENV1 gate).
        for (int v = 0; v < kMaxChordVoices; ++v)
        {
            if (s_gate_notes[v] != 0xFF && System::GetNow() >= s_gate_note_off_ms[v])
            {
                s_midi_out.SendNoteOff(s_gate_notes[v], 0, 0);
                s_gate_notes[v] = 0xFF;
            }
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
                if (p == &kPatchLoadParam)
                    clamped = NextUsedPatchSlot(p->value, dir_menu);
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

        // ---- Periodic status heartbeat: one short status frame per iteration.
        // This keeps the ESP32 hubs up to date without bursting four lines in
        // a single loop tick and without blocking for UART flush.
        if (now - last_status_send >= kUsbSendIntervalMs)
        {
            SendNextStatusFrame();
            last_status_send = now;
        }

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
                if (s_editing_node != nullptr && s_editing_node->onConfirm != nullptr)
                    s_editing_node->onConfirm(s_editing_node->numeric->value);
                s_editing_numeric = false;
                s_editing_node = nullptr;
                if (!s_poly_wheel_active)
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
                        if (selected->numeric == &kPatchLoadParam)
                            kPatchLoadParam.value = NextUsedPatchSlot(kPatchLoadParam.value, 0);
                        s_patch_error = false;
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
