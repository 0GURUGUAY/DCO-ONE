#pragma once

#include "daisysp.h"

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
    
private:
    daisysp::Oscillator osc_;
    float sampleRate_;
    float frequency_;
};

} // namespace dco
