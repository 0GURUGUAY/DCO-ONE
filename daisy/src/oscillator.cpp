#include "oscillator.h"

namespace dco {

OscillatorWrapper::OscillatorWrapper() 
    : sampleRate_(48000.0f), frequency_(440.0f)
{
}

OscillatorWrapper::~OscillatorWrapper()
{
}

void OscillatorWrapper::Init(float sampleRate, float frequency)
{
    sampleRate_ = sampleRate;
    frequency_ = frequency;
    
    // Initialize DaisySP oscillator
    osc_.Init(sampleRate);
    osc_.SetFreq(frequency_);
    osc_.SetAmp(0.3f);  // 30% amplitude to avoid clipping
    osc_.SetWaveform(daisysp::Oscillator::WAVE_SIN);
}

float OscillatorWrapper::GetSample()
{
    return osc_.Process();
}

void OscillatorWrapper::SetFrequency(float frequency)
{
    frequency_ = frequency;
    osc_.SetFreq(frequency_);
}

void OscillatorWrapper::SetWaveform(uint8_t waveform)
{
    osc_.SetWaveform(waveform);
}

void OscillatorWrapper::SetAmplitude(float amp)
{
    osc_.SetAmp(amp);
}

} // namespace dco
