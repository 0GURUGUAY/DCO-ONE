#pragma once

#include "daisysp.h"

namespace dco {

// Simple 2-operator FM voice (carrier phase-modulated by a modulator
// oscillator), based on DaisySP's Synthesis/fm2.h. Reimplemented on top of
// two plain daisysp::Oscillator instances (instead of using daisysp::Fm2
// directly) so the modulator's waveform can be exposed/changed, which Fm2
// hardcodes to a sine internally.
class FmWrapper
{
public:
    FmWrapper();
    ~FmWrapper();

    void Init(float sampleRate, float frequency = 440.0f);
    float GetSample();

    void SetFrequency(float frequency); // carrier frequency in Hz
    void SetRatio(float ratio);         // modulator freq = carrier freq * ratio
    void SetIndex(float index);         // FM depth, 5.0 = 2*PI rad phase swing
    void SetModWaveform(uint8_t waveform); // daisysp::Oscillator::WAVE_* constant
    void SetAmplitude(float amp);

private:
    daisysp::Oscillator car_;
    daisysp::Oscillator mod_;
    float sampleRate_;
    float frequency_;
    float ratio_;
    float index_;
    float amplitude_;

    static constexpr float kIndexScalar = 0.2f; // matches DaisySP::Fm2's convention
};

} // namespace dco
