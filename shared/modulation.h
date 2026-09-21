#pragma once

#include <cstdint>

namespace dco {

constexpr int kMatrixSlotCount = 8;
struct MatrixSlot {
    uint8_t source = 0;
    uint8_t destination = 0;
    float amount = 0;
};

inline bool MatrixSourceSupported(int source)
{
    return source == 0 || source == 1 || source == 2 || source == 3 || source == 7;
}

inline int32_t PackMatrixSlot(const MatrixSlot& slot)
{
    return 0x4D000000 | (slot.source << 16) | (slot.destination << 8)
        | static_cast<uint8_t>(static_cast<int>(slot.amount * 100.0f + (slot.amount >= 0 ? 0.5f : -0.5f)) + 100);
}

inline MatrixSlot UnpackMatrixSlot(int32_t packed)
{
    int source = (packed >> 16) & 255;
    int destination = (packed >> 8) & 255;
    int amount = (packed & 255) - 100;
    if ((packed & 0xFF000000) != 0x4D000000 || !MatrixSourceSupported(source)
        || destination > 7 || amount < -100 || amount > 100) return {};
    return {static_cast<uint8_t>(source), static_cast<uint8_t>(destination), amount * 0.01f};
}

inline float LimitModulation(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

inline void AccumulateModulation(const MatrixSlot* slots, float lfo1, float lfo2,
                                 float envelope, float (&destinations)[8], float envelope2 = 0.0f)
{
    for (int slot = 0; slot < kMatrixSlotCount; ++slot)
    {
        const auto& route = slots[slot];
        float source = route.source == 1 ? lfo1 : route.source == 2 ? lfo2
            : route.source == 3 ? envelope2 : route.source == 7 ? envelope : 0.0f;
        if (route.destination > 0 && route.destination < 8)
            destinations[route.destination] += source * route.amount;
    }
}

}