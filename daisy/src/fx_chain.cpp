#include "fx_chain.h"

namespace dco {

void FxChain::Init(float sampleRate) {
    for (int i = 0; i < kNumSlots; ++i)
        slots_[i].Init(sampleRate);
}

void FxChain::ApplyPendingTypeChanges() {
    for (int i = 0; i < kNumSlots; ++i)
        slots_[i].ApplyPendingType();
}

void FxChain::SetTempo(float bpm) {
    for (int i = 0; i < kNumSlots; ++i)
        slots_[i].SetTempo(bpm);
}

float FxChain::Process(float in) {
    float s = in;
    for (int i = 0; i < kNumSlots; ++i)
        s = slots_[i].Process(s);
    return s;
}

void FxChain::SetSlotType(int slot, FxType type) {
    if (slot >= 0 && slot < kNumSlots)
        slots_[slot].RequestType(type);
}

void FxChain::SetSlotMix(int slot, float mix01) {
    if (slot >= 0 && slot < kNumSlots)
        slots_[slot].SetMix(mix01);
}

void FxChain::SetSlotParam1(int slot, float p1_01) {
    if (slot >= 0 && slot < kNumSlots)
        slots_[slot].SetParam1(p1_01);
}

void FxChain::SetSlotParam2(int slot, float p2_01) {
    if (slot >= 0 && slot < kNumSlots)
        slots_[slot].SetParam2(p2_01);
}

} // namespace dco
