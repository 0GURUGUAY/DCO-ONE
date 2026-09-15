#pragma once

#include "daisysp.h"
#include <cstdint>

namespace dco {

class OscillatorWrapper
{
public:
    OscillatorWrapper();
    ~OscillatorWrapper();

    void Init(float sampleRate, float frequency = 440.0f);
    float GetSample();

    void SetFrequency(float frequency);
    void SetWaveform(uint8_t waveform);  // Pass DaisySP waveform constants
    void SetAmplitude(float amp);

    // Pitch tuning, applied on top of the base frequency set by SetFrequency.
    void SetCoarseTune(int32_t semitones);
    void SetFineTune(int32_t cents);

    // Real-time pitch modulation (e.g. LFO/matrix), in semitones.
    void SetPitchModulation(float semitones);

    // Pulse width (0..1). Only affects the "Pulse" waveform.
    void SetPulseWidth(float duty);

    // Sub-oscillator square level one octave below (0..1).
    void SetSubAmount(float amount);

    // Waveshaper hardness (0..1) applied to the mixed output.
    void SetHardness(float amount);

private:
    void UpdateFrequency();
    float ProcessPulse();

    daisysp::Oscillator osc_;
    daisysp::Oscillator subOsc_;

    float sampleRate_;
    float baseFrequency_;
    float effectiveFrequency_;
    float amplitude_;
    uint8_t waveform_;

    int32_t coarseSemitones_;
    int32_t fineCents_;
    float   pitchModSemitones_;
    float   pulseWidth_;
    float   subAmount_;
    float   hardness_;

    float   phase_; // 0..1, for the custom pulse generator
};

} // namespace dco
