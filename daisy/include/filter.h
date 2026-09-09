#pragma once

#include "daisysp.h"

namespace dco {

// Matches menu.json's vcf_type options: LP24, LP12, BP12, HP12, NOTCH.
enum class FilterType : uint8_t {
    LP24 = 0,
    LP12,
    BP12,
    HP12,
    NOTCH,
};

// VCF block: a 4-pole ladder filter (LP24/LP12/BP12/HP12, with drive and
// self-oscillating resonance) plus a state-variable filter used only for
// its Notch output, which the ladder model doesn't provide. Both run in
// parallel behind a single Cutoff/Resonance/Drive/Type interface so
// switching Filter type is click-free.
class FilterWrapper
{
public:
    void Init(float sampleRate);
    float Process(float in);

    void SetType(FilterType type);
    void SetCutoff(float hz);
    void SetResonance(float normalized); // 0..1
    void SetDrive(float normalized);     // 0..1

private:
    daisysp::LadderFilter ladder_;
    daisysp::Svf          svf_;
    float                 sampleRate_ = 48000.0f;
    FilterType             type_ = FilterType::LP24;
};

} // namespace dco
