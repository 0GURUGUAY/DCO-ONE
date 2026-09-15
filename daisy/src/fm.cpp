#include "fm.h"
#include <cmath>

namespace dco {

FmWrapper::FmWrapper()
    : sampleRate_(48000.0f),
      baseFrequency_(440.0f),
      frequency_(440.0f),
      ratio_(2.0f),
      index_(1.0f),
      amplitude_(0.3f),
      coarseSemitones_(0),
      fineCents_(0),
      pitchModSemitones_(0.0f)
{
}

FmWrapper::~FmWrapper()
{
}

void FmWrapper::Init(float sampleRate, float frequency)
{
    sampleRate_    = sampleRate;
    baseFrequency_ = frequency;

    car_.Init(sampleRate);
    mod_.Init(sampleRate);
    car_.SetWaveform(daisysp::Oscillator::WAVE_SIN);
    mod_.SetWaveform(daisysp::Oscillator::WAVE_SIN);
    car_.SetAmp(1.0f);
    mod_.SetAmp(1.0f);

    UpdateFrequency();
}

float FmWrapper::GetSample()
{
    float modval = mod_.Process();
    car_.PhaseAdd(modval * index_ * kIndexScalar);
    return car_.Process() * amplitude_;
}

void FmWrapper::SetFrequency(float frequency)
{
    baseFrequency_ = frequency;
    UpdateFrequency();
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

void FmWrapper::SetCoarseTune(int32_t semitones)
{
    coarseSemitones_ = semitones;
    UpdateFrequency();
}

void FmWrapper::SetFineTune(int32_t cents)
{
    fineCents_ = cents;
    UpdateFrequency();
}

void FmWrapper::SetPitchModulation(float semitones)
{
    pitchModSemitones_ = semitones;
    UpdateFrequency();
}

void FmWrapper::UpdateFrequency()
{
    const float semitones = static_cast<float>(coarseSemitones_)
                          + static_cast<float>(fineCents_) / 100.0f
                          + pitchModSemitones_;
    frequency_ = baseFrequency_ * std::pow(2.0f, semitones / 12.0f);

    car_.SetFreq(frequency_);
    mod_.SetFreq(frequency_ * ratio_);
}

} // namespace dco
