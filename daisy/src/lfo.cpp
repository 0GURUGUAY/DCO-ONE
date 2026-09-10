#include "lfo.h"

namespace dco {

void LfoWrapper::Init(float sampleRate)
{
    osc_.Init(sampleRate);
    osc_.SetAmp(1.0f);
    osc_.SetWaveform(daisysp::Oscillator::WAVE_SIN);
    osc_.SetFreq(rateHz_);

    metro_.Init(rateHz_, sampleRate);
    noise_.Init();
    smoothRandom_.Init(sampleRate);
    smoothRandom_.SetFreq(rateHz_);
}

float LfoWrapper::Process()
{
    float raw = 0.0f;
    switch (shape_)
    {
        case LfoShape::SAMPLE_HOLD:
        {
            bool trig = metro_.Process() != 0;
            raw = sampleHold_.Process(trig, noise_.Process());
            break;
        }
        case LfoShape::RANDOM:
            raw = smoothRandom_.Process();
            break;
        default:
            raw = osc_.Process();
            break;
    }
    lastOutput_ = raw * amount_;
    return lastOutput_;
}

void LfoWrapper::SetShape(LfoShape shape)
{
    shape_ = shape;
    switch (shape)
    {
        case LfoShape::SIN:    osc_.SetWaveform(daisysp::Oscillator::WAVE_SIN); break;
        case LfoShape::TRI:    osc_.SetWaveform(daisysp::Oscillator::WAVE_TRI); break;
        case LfoShape::SAW:    osc_.SetWaveform(daisysp::Oscillator::WAVE_RAMP); break;
        case LfoShape::SQUARE: osc_.SetWaveform(daisysp::Oscillator::WAVE_SQUARE); break;
        default: break; // SAMPLE_HOLD/RANDOM don't use osc_'s waveform
    }
}

void LfoWrapper::SetRate(float hz)
{
    rateHz_ = hz;
    osc_.SetFreq(hz);
    metro_.SetFreq(hz);
    smoothRandom_.SetFreq(hz);
}

void LfoWrapper::SetAmount(float normalized)
{
    if (normalized < 0.0f) normalized = 0.0f;
    else if (normalized > 1.0f) normalized = 1.0f;
    amount_ = normalized;
}

void LfoWrapper::SetPhaseDegrees(float degrees)
{
    float wrapped = fmodf(degrees, 360.0f);
    if (wrapped < 0.0f)
        wrapped += 360.0f;
    osc_.Reset(wrapped / 360.0f);
}

} // namespace dco
