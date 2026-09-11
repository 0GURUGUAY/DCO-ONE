#pragma once

#include <cstdint>
#include "daisy_seed.h"

namespace dco {

// Maximum sizes, kept in sync with daisy/src/main.cpp.
constexpr int kPatchMaxValues     = 48;
constexpr int kPatchMaxSelections = 48;
constexpr int kPatchMaxPolySteps  = 32;
constexpr int kPatchSlotCount     = 128;

struct PatchPolyStep {
    uint8_t state;
    int8_t  degree;
    int8_t  fixedTranspose;
};

struct PatchData {
    int32_t      values[kPatchMaxValues]         = {};
    uint32_t     numValues                       = 0;
    int32_t      selections[kPatchMaxSelections] = {};
    uint32_t     numSelections                   = 0;
    PatchPolyStep polySteps[kPatchMaxPolySteps]  = {};
    uint32_t     numPolySteps                    = 0;
};

// Initialize the patch storage layer (must be called after hw.Init()).
bool PatchStorageInit(daisy::QSPIHandle& qspi);

// Save a patch to the given slot (1..kPatchSlotCount).
// Returns true on success.
bool PatchStorageSave(int slot, const PatchData& data);

// Load a patch from the given slot (1..kPatchSlotCount).
// Returns true if a valid patch was found and parsed.
bool PatchStorageLoad(int slot, PatchData& data);

// Erase all patch slots.
bool PatchStorageFormat();

} // namespace dco
