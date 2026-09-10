#pragma once

#include "daisysp.h"

namespace dco {

// Matches menu.json's lfo1_shape/lfo2_shape options: Sin, Tri, Saw, Sq, S&H, Rnd.
enum class LfoShape : uint8_t {
    SIN = 0,
    TRI,
    SAW,
    SQUARE,
    SAMPLE_HOLD,
    RANDOM,
};

// One LFO voice: SIN/TRI/SAW/SQUARE run through a DaisySP Oscillator;
// SAMPLE_HOLD steps a white-noise source through a Metro-triggered
// SampleHold; RANDOM uses DaisySP's SmoothRandomGenerator (slewed random
// wander) since none of those three shapes are "phase" waveforms. Output is
// unipolar-free -1..1 scaled by the Amp (0..1) param; not yet routed to any
// destination -- the upcoming MATRIX block assigns this to VCO/VCF/VCA.
class LfoWrapper
{
public:
    void Init(float sampleRate);
    float Process(); // call once per audio sample; advances phase/state

    void SetShape(LfoShape shape);
    void SetRate(float hz);       // free-running rate, ignored while tempo-synced
    void SetAmount(float normalized); // 0..1, from lfoX_amp (0..100%)
    void SetPhaseDegrees(float degrees); // 0..360, resets the oscillator's phase

    float GetLastOutput() const { return lastOutput_; }

private:
    daisysp::Oscillator             osc_;
    daisysp::Metro                  metro_;
    daisysp::WhiteNoise              noise_;
    daisysp::SampleHold              sampleHold_;
    daisysp::SmoothRandomGenerator   smoothRandom_;
    LfoShape                        shape_       = LfoShape::SIN;
    float                            rateHz_      = 10.0f;
    float                            amount_      = 0.5f;
    float                            lastOutput_  = 0.0f;
};

} // namespace dco
