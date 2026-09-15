#include "oscillator.h"
#include <cmath>
#include <algorithm>

namespace dco {

namespace {
    inline float Clamp01(float v)
    {
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
}

OscillatorWrapper::OscillatorWrapper()
    : sampleRate_(48000.0f),
      baseFrequency_(440.0f),
      effectiveFrequency_(440.0f),
      amplitude_(0.3f),
      waveform_(daisysp::Oscillator::WAVE_SIN),
      coarseSemitones_(0),
      fineCents_(0),
      pitchModSemitones_(0.0f),
      pulseWidth_(0.5f),
      subAmount_(0.0f),
      hardness_(0.0f),
      phase_(0.0f)
{
}

OscillatorWrapper::~OscillatorWrapper()
{
}

void OscillatorWrapper::Init(float sampleRate, float frequency)
{
    sampleRate_     = sampleRate;
    baseFrequency_  = frequency;
    amplitude_      = 0.3f;
    phase_          = 0.0f;

    osc_.Init(sampleRate);
    osc_.SetWaveform(daisysp::Oscillator::WAVE_SIN);
    osc_.SetAmp(1.0f);

    subOsc_.Init(sampleRate);
    subOsc_.SetWaveform(daisysp::Oscillator::WAVE_SQUARE);
    subOsc_.SetAmp(1.0f);

    UpdateFrequency();
}

float OscillatorWrapper::GetSample()
{
    float mainSample;
    if (waveform_ == daisysp::Oscillator::WAVE_SQUARE)
    {
        // Variable pulse width (the menu labels this waveform "Pulse").
        mainSample = ProcessPulse();
    }
    else
    {
        mainSample = osc_.Process();
    }

    // Sub-oscillator: square one octave below.
    float subSample = subOsc_.Process();
    float out = mainSample + subAmount_ * subSample;

    // Hardness: mild waveshaping that grows with the amount.
    if (hardness_ > 0.0f)
    {
        const float drive = 1.0f + hardness_ * 8.0f;
        out = std::tanh(out * drive);
    }

    return out * amplitude_;
}

void OscillatorWrapper::SetFrequency(float frequency)
{
    baseFrequency_ = frequency;
    UpdateFrequency();
}

void OscillatorWrapper::SetWaveform(uint8_t waveform)
{
    waveform_ = waveform;
    if (waveform_ != daisysp::Oscillator::WAVE_SQUARE)
        osc_.SetWaveform(waveform_);
}

void OscillatorWrapper::SetAmplitude(float amp)
{
    amplitude_ = amp;
}

void OscillatorWrapper::SetCoarseTune(int32_t semitones)
{
    coarseSemitones_ = semitones;
    UpdateFrequency();
}

void OscillatorWrapper::SetFineTune(int32_t cents)
{
    fineCents_ = cents;
    UpdateFrequency();
}

void OscillatorWrapper::SetPitchModulation(float semitones)
{
    pitchModSemitones_ = semitones;
    UpdateFrequency();
}

void OscillatorWrapper::SetPulseWidth(float duty)
{
    // Keep a tiny gap at the extremes so the pulse never fully disappears.
    pulseWidth_ = std::max(0.01f, std::min(0.99f, duty));
}

void OscillatorWrapper::SetSubAmount(float amount)
{
    subAmount_ = Clamp01(amount);
}

void OscillatorWrapper::SetHardness(float amount)
{
    hardness_ = Clamp01(amount);
}

void OscillatorWrapper::UpdateFrequency()
{
    const float semitones = static_cast<float>(coarseSemitones_)
                          + static_cast<float>(fineCents_) / 100.0f
                          + pitchModSemitones_;
    effectiveFrequency_ = baseFrequency_ * std::pow(2.0f, semitones / 12.0f);

    osc_.SetFreq(effectiveFrequency_);
    subOsc_.SetFreq(effectiveFrequency_ * 0.5f);
}

float OscillatorWrapper::ProcessPulse()
{
    phase_ += effectiveFrequency_ / sampleRate_;
    while (phase_ >= 1.0f)
        phase_ -= 1.0f;

    return (phase_ < pulseWidth_) ? 1.0f : -1.0f;
}

} // namespace dco
