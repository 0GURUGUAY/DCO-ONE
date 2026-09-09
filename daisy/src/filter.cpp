#include "filter.h"

namespace dco {

void FilterWrapper::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    ladder_.Init(sampleRate);
    svf_.Init(sampleRate);
}

float FilterWrapper::Process(float in)
{
    if (type_ == FilterType::NOTCH)
    {
        svf_.Process(in);
        return svf_.Notch();
    }
    return ladder_.Process(in);
}

void FilterWrapper::SetType(FilterType type)
{
    type_ = type;
    switch (type)
    {
        case FilterType::LP24:
            ladder_.SetFilterMode(daisysp::LadderFilter::FilterMode::LP24);
            break;
        case FilterType::LP12:
            ladder_.SetFilterMode(daisysp::LadderFilter::FilterMode::LP12);
            break;
        case FilterType::BP12:
            ladder_.SetFilterMode(daisysp::LadderFilter::FilterMode::BP12);
            break;
        case FilterType::HP12:
            ladder_.SetFilterMode(daisysp::LadderFilter::FilterMode::HP12);
            break;
        case FilterType::NOTCH:
            // Handled by the SVF's Notch() output instead of the ladder.
            break;
    }
}

void FilterWrapper::SetCutoff(float hz)
{
    ladder_.SetFreq(hz);
    // Svf::SetFreq requires f < sample_rate / 3 to remain stable.
    float svfMax = sampleRate_ / 3.0f;
    svf_.SetFreq(hz > svfMax ? svfMax : hz);
}

void FilterWrapper::SetResonance(float normalized)
{
    if (normalized < 0.0f) normalized = 0.0f;
    else if (normalized > 1.0f) normalized = 1.0f;
    ladder_.SetRes(normalized * 1.8f); // LadderFilter's stable self-osc range is 0..1.8
    svf_.SetRes(normalized * 0.98f);   // Svf requires < 1.0 to remain stable
}

void FilterWrapper::SetDrive(float normalized)
{
    if (normalized < 0.0f) normalized = 0.0f;
    else if (normalized > 1.0f) normalized = 1.0f;
    ladder_.SetInputDrive(normalized * 4.0f); // LadderFilter's valid drive range is 0..4
    svf_.SetDrive(normalized);
}

} // namespace dco
