#include "fm.h"

namespace dco {

FmWrapper::FmWrapper()
    : sampleRate_(48000.0f), frequency_(440.0f), ratio_(2.0f), index_(1.0f), amplitude_(0.3f)
{
}

FmWrapper::~FmWrapper()
{
}

void FmWrapper::Init(float sampleRate, float frequency)
{
    sampleRate_ = sampleRate;
    frequency_  = frequency;

    car_.Init(sampleRate);
    mod_.Init(sampleRate);
    car_.SetWaveform(daisysp::Oscillator::WAVE_SIN);
    mod_.SetWaveform(daisysp::Oscillator::WAVE_SIN);
    car_.SetAmp(1.0f);
    mod_.SetAmp(1.0f);
    car_.SetFreq(frequency_);
    mod_.SetFreq(frequency_ * ratio_);
}

float FmWrapper::GetSample()
{
    float modval = mod_.Process();
    car_.PhaseAdd(modval * index_ * kIndexScalar);
    return car_.Process() * amplitude_;
}

void FmWrapper::SetFrequency(float frequency)
{
    frequency_ = frequency;
    car_.SetFreq(frequency_);
    mod_.SetFreq(frequency_ * ratio_);
}

void FmWrapper::SetRatio(float ratio)
{
    ratio_ = ratio;
    mod_.SetFreq(frequency_ * ratio_);
}

void FmWrapper::SetIndex(float index)
{
    index_ = index;
}

void FmWrapper::SetModWaveform(uint8_t waveform)
{
    mod_.SetWaveform(waveform);
}

void FmWrapper::SetAmplitude(float amp)
{
    amplitude_ = amp;
}

} // namespace dco
