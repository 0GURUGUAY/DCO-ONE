#include "model.h"
#include <cmath>

namespace dco {

ModelWrapper::ModelWrapper()
    : sampleRate_(48000.0f), baseFrequency_(440.0f), amplitude_(1.0f),
      coarseSemitones_(0), fineCents_(0), pitchModSemitones_(0.0f),
      sustain_(false), pendingTrig_(false), retrigSamplesRemaining_(0.0f)
{
}

ModelWrapper::~ModelWrapper() {}

void ModelWrapper::Init(float sampleRate, float frequency)
{
    sampleRate_    = sampleRate;
    baseFrequency_ = frequency;
    voice_.Init(sampleRate);
    voice_.SetSustain(false); // "Bow" is approximated in GetSample(), never the library's own continuous path
    retrigSamplesRemaining_ = 0.0f;
    UpdateFrequency();
}

void ModelWrapper::UpdateFrequency()
{
    float semis = coarseSemitones_ + fineCents_ / 100.0f + pitchModSemitones_;
    voice_.SetFreq(baseFrequency_ * powf(2.0f, semis / 12.0f));
}

void ModelWrapper::SetFrequency(float frequency)
{
    baseFrequency_ = frequency;
    UpdateFrequency();
}

void ModelWrapper::SetAmplitude(float amp) { amplitude_ = amp; }

void ModelWrapper::SetCoarseTune(int32_t semitones)
{
    coarseSemitones_ = semitones;
    UpdateFrequency();
}

void ModelWrapper::SetFineTune(int32_t cents)
{
    fineCents_ = cents;
    UpdateFrequency();
}

void ModelWrapper::SetPitchModulation(float semitones)
{
    pitchModSemitones_ = semitones;
    UpdateFrequency();
}

void ModelWrapper::SetStructure(float structure)   { voice_.SetStructure(structure); }
void ModelWrapper::SetBrightness(float brightness) { voice_.SetBrightness(brightness); }
void ModelWrapper::SetDamping(float damping)       { voice_.SetDamping(damping); }
void ModelWrapper::SetAccent(float accent)         { voice_.SetAccent(accent); }

void ModelWrapper::SetSustain(bool sustain)
{
    sustain_ = sustain;
    if (!sustain_)
        retrigSamplesRemaining_ = 0.0f;
}

void ModelWrapper::Trig() { pendingTrig_ = true; }

float ModelWrapper::GetSample()
{
    if (sustain_)
    {
        // Cheap "bowed" approximation: re-strike periodically instead of
        // daisysp::StringVoice::SetSustain(true)'s continuous per-sample
        // excitation (see the SetSustain() declaration comment in model.h).
        if (retrigSamplesRemaining_ <= 0.0f)
        {
            pendingTrig_            = true;
            retrigSamplesRemaining_ = kBowRetrigMs * 0.001f * sampleRate_;
        }
        retrigSamplesRemaining_ -= 1.0f;
    }
    float out    = voice_.Process(pendingTrig_) * amplitude_;
    pendingTrig_ = false;
    return out;
}

float ModelWrapper::GetExciter() { return voice_.GetAux() * amplitude_; }

} // namespace dco
