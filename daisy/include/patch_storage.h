#pragma once

#include "../../shared/pattern_format.h"
#include "daisy_seed.h"

namespace dco {

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
