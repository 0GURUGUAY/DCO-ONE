#pragma once

#include "daisysp.h"
#include <cstdint>

namespace dco {

// Physical-modeling voice (Karplus-Strong string, DaisySP's StringVoice --
// ported from Mutable Instruments Rings/Plaits), mirroring
// OscillatorWrapper/FmWrapper's interface so it slots into the existing
// voice pool / pitch-tracking pattern as the 3rd SOURCE branch (VCO/FM/Model).
// GetSample() is the resonator output (SOURCE); GetExciter() is the raw
// strike/noise excitation signal (EXCITER), meant to be mixed in separately.
class ModelWrapper
{
public:
    ModelWrapper();
    ~ModelWrapper();

    void Init(float sampleRate, float frequency = 440.0f);
    float GetSample();
    float GetExciter();

    void SetFrequency(float frequency);
    void SetAmplitude(float amp);
    void SetCoarseTune(int32_t semitones);
    void SetFineTune(int32_t cents);
    void SetPitchModulation(float semitones);

    void SetStructure(float structure);   // 0..1: curved bridge -> dispersion
    void SetBrightness(float brightness); // 0..1
    void SetDamping(float damping);       // 0..1: decay time
    void SetAccent(float accent);         // 0..1: strike intensity

    // "Bow": approximated by periodically re-triggering the string (see
    // GetSample()) instead of daisysp::StringVoice's own SetSustain(true),
    // which recomputes an expensive filter (incl. a powf() call) EVERY
    // SAMPLE for as long as a note holds -- with several simultaneous voices
    // this overran the audio callback's real-time budget and froze the
    // synth. Periodic Trig() reuses the cheap one-shot Pluck decay instead.
    void SetSustain(bool sustain);

    void Trig(); // strike the string, call once on note-on

private:
    void UpdateFrequency();

    daisysp::StringVoice voice_;
    float sampleRate_;
    float baseFrequency_;
    float amplitude_;
    int32_t coarseSemitones_;
    int32_t fineCents_;
    float pitchModSemitones_;
    bool sustain_;
    bool pendingTrig_;
    float retrigSamplesRemaining_;

    static constexpr float kBowRetrigMs = 60.0f;
};

} // namespace dco
