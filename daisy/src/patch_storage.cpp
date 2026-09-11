#include "patch_storage.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>

#if !UNIT_TEST
extern "C" void dsy_dma_invalidate_cache_for_buffer(uint8_t* buff, size_t size);
#endif

namespace dco {

// QSPI layout: one 4KB sector per patch slot, placed well after the
// PersistentStorage region used for the live settings.
static constexpr uint32_t kPatchStorageBase   = 0x10000; // 64 KiB offset
static constexpr uint32_t kPatchSectorSize    = 4096;
static constexpr uint32_t kPatchHeaderMagic   = 0x44434F31; // "DCO1"
static constexpr uint32_t kPatchFormatVersion = 1;

static daisy::QSPIHandle* s_qspi = nullptr;

bool PatchStorageInit(daisy::QSPIHandle& qspi)
{
    s_qspi = &qspi;
    return true;
}

static uint32_t SlotAddress(int slot)
{
    if (slot < 1)
        slot = 1;
    if (slot > kPatchSlotCount)
        slot = kPatchSlotCount;
    return kPatchStorageBase + static_cast<uint32_t>(slot - 1) * kPatchSectorSize;
}

// -----------------------------------------------------------------------------
// Tiny JSON helpers (format is fully under our control).
// -----------------------------------------------------------------------------

static const char* SkipWhitespace(const char* p)
{
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        ++p;
    return p;
}

static const char* FindKey(const char* text, const char* key)
{
    const char* p = text;
    size_t keyLen = strlen(key);
    while (*p)
    {
        p = SkipWhitespace(p);
        if (*p != '"')
        {
            if (*p == '\0')
                break;
            ++p;
            continue;
        }
        ++p;
        if (strncmp(p, key, keyLen) == 0 && p[keyLen] == '"')
        {
            p += keyLen + 1;
            p = SkipWhitespace(p);
            if (*p == ':')
            {
                ++p;
                return SkipWhitespace(p);
            }
        }
        else
        {
            while (*p && *p != '"')
                ++p;
            if (*p == '"')
                ++p;
        }
    }
    return nullptr;
}

static bool ParseIntArray(const char* in, int32_t* out, uint32_t maxCount, uint32_t& outCount)
{
    outCount = 0;
    const char* p = SkipWhitespace(in);
    if (*p != '[')
        return false;
    ++p;
    while (true)
    {
        p = SkipWhitespace(p);
        if (*p == ']')
        {
            ++p;
            return true;
        }
        if (*p == '\0')
            return false;
        if (outCount < maxCount)
        {
            char* end = nullptr;
            long v = strtol(p, &end, 10);
            if (end == p)
                return false;
            out[outCount++] = static_cast<int32_t>(v);
            p = end;
        }
        else
        {
            // Skip extra values beyond our capacity.
            char* end = nullptr;
            strtol(p, &end, 10);
            if (end == p)
                return false;
            p = end;
        }
        p = SkipWhitespace(p);
        if (*p == ',')
        {
            ++p;
            continue;
        }
        if (*p == ']')
        {
            ++p;
            return true;
        }
        return false;
    }
}

static bool ParsePolyArray(const char* in, PatchPolyStep* out, uint32_t maxCount, uint32_t& outCount)
{
    outCount = 0;
    const char* p = SkipWhitespace(in);
    if (*p != '[')
        return false;
    ++p;
    while (true)
    {
        p = SkipWhitespace(p);
        if (*p == ']')
        {
            ++p;
            return true;
        }
        if (*p != '{')
            return false;
        ++p;

        int values[3] = {0, 0, 0};
        for (int field = 0; field < 3; ++field)
        {
            p = SkipWhitespace(p);
            if (*p != '"')
                return false;
            ++p;
            char key = *p;
            ++p;
            if (*p != '"')
                return false;
            ++p;
            p = SkipWhitespace(p);
            if (*p != ':')
                return false;
            ++p;
            p = SkipWhitespace(p);
            char* end = nullptr;
            long v = strtol(p, &end, 10);
            if (end == p)
                return false;
            if (key == 's')
                values[0] = static_cast<int>(v);
            else if (key == 'd')
                values[1] = static_cast<int>(v);
            else if (key == 't')
                values[2] = static_cast<int>(v);
            p = end;
            p = SkipWhitespace(p);
            if (field < 2 && *p == ',')
            {
                ++p;
                continue;
            }
        }
        p = SkipWhitespace(p);
        if (*p != '}')
            return false;
        ++p;

        if (outCount < maxCount)
        {
            out[outCount].state          = static_cast<uint8_t>(values[0]);
            out[outCount].degree         = static_cast<int8_t>(values[1]);
            out[outCount].fixedTranspose = static_cast<int8_t>(values[2]);
            ++outCount;
        }

        p = SkipWhitespace(p);
        if (*p == ',')
        {
            ++p;
            continue;
        }
        if (*p == ']')
        {
            ++p;
            return true;
        }
        return false;
    }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool PatchStorageSave(int slot, const PatchData& data)
{
    if (!s_qspi || slot < 1 || slot > kPatchSlotCount)
        return false;

    char json[kPatchSectorSize - 16];
    int len = snprintf(json, sizeof(json),
                       "{"
                       "\"magic\":\"DCO1PATCH\","
                       "\"version\":%lu,"
                       "\"name\":\"\","
                       "\"values\":[",
                       static_cast<unsigned long>(kPatchFormatVersion));

    for (uint32_t i = 0; i < data.numValues && len > 0; ++i)
    {
        len += snprintf(json + len, sizeof(json) - len,
                        i == 0 ? "%ld" : ",%ld",
                        static_cast<long>(data.values[i]));
    }

    if (len > 0)
    {
        len += snprintf(json + len, sizeof(json) - len,
                        "],\"selections\":[");
    }

    for (uint32_t i = 0; i < data.numSelections && len > 0; ++i)
    {
        len += snprintf(json + len, sizeof(json) - len,
                        i == 0 ? "%ld" : ",%ld",
                        static_cast<long>(data.selections[i]));
    }

    if (len > 0)
    {
        len += snprintf(json + len, sizeof(json) - len,
                        "],\"poly\":[");
    }

    for (uint32_t i = 0; i < data.numPolySteps && len > 0; ++i)
    {
        len += snprintf(json + len, sizeof(json) - len,
                        i == 0 ? "{\"s\":%d,\"d\":%d,\"t\":%d}"
                               : ",{\"s\":%d,\"d\":%d,\"t\":%d}",
                        static_cast<int>(data.polySteps[i].state),
                        static_cast<int>(data.polySteps[i].degree),
                        static_cast<int>(data.polySteps[i].fixedTranspose));
    }

    if (len > 0)
    {
        len += snprintf(json + len, sizeof(json) - len, "]}");
    }

    if (len <= 0 || static_cast<size_t>(len) >= sizeof(json))
        return false;

    uint32_t addr = SlotAddress(slot);
    struct Header {
        uint32_t magic;
        uint32_t version;
        uint32_t length;
    } header = { kPatchHeaderMagic, kPatchFormatVersion, static_cast<uint32_t>(len + 1) };

    // Compose sector buffer (header + JSON + terminating zero).
    uint8_t sector[kPatchSectorSize];
    memset(sector, 0xFF, sizeof(sector));
    memcpy(sector, &header, sizeof(header));
    memcpy(sector + sizeof(header), json, len + 1);

    if (s_qspi->EraseSector(addr) != daisy::QSPIHandle::Result::OK)
        return false;
    if (s_qspi->Write(addr, sizeof(sector), sector) != daisy::QSPIHandle::Result::OK)
        return false;

    return true;
}

bool PatchStorageLoad(int slot, PatchData& data)
{
    if (!s_qspi || slot < 1 || slot > kPatchSlotCount)
        return false;

    uint32_t addr = SlotAddress(slot);
    const uint8_t* base = static_cast<const uint8_t*>(s_qspi->GetData(addr));
    if (!base)
        return false;

    // Invalidate cache so we read the latest QSPI contents (same logic as
    // PersistentStorage when running from internal flash).
#if !UNIT_TEST
    dsy_dma_invalidate_cache_for_buffer(const_cast<uint8_t*>(base), kPatchSectorSize);
#endif

    const uint8_t* p = base;
    uint32_t magic, version, length;
    memcpy(&magic, p, sizeof(magic));
    p += sizeof(magic);
    memcpy(&version, p, sizeof(version));
    p += sizeof(version);
    memcpy(&length, p, sizeof(length));
    p += sizeof(length);

    if (magic != kPatchHeaderMagic || version != kPatchFormatVersion)
        return false;
    if (length == 0 || length > kPatchSectorSize - sizeof(magic) - sizeof(version) - sizeof(length))
        return false;

    const char* json = reinterpret_cast<const char*>(p);
    // Safety: ensure the JSON is terminated inside the sector.
    bool terminated = false;
    for (uint32_t i = 0; i < length; ++i)
    {
        if (json[i] == '\0')
        {
            terminated = true;
            break;
        }
    }
    if (!terminated)
        return false;

    const char* valuesStart = FindKey(json, "values");
    const char* selectionsStart = FindKey(json, "selections");
    const char* polyStart = FindKey(json, "poly");

    if (!valuesStart || !selectionsStart || !polyStart)
        return false;

    PatchData tmp;
    if (!ParseIntArray(valuesStart, tmp.values, kPatchMaxValues, tmp.numValues))
        return false;
    if (!ParseIntArray(selectionsStart, tmp.selections, kPatchMaxSelections, tmp.numSelections))
        return false;
    if (!ParsePolyArray(polyStart, tmp.polySteps, kPatchMaxPolySteps, tmp.numPolySteps))
        return false;

    data = tmp;
    return true;
}

bool PatchStorageFormat()
{
    if (!s_qspi)
        return false;
    for (int slot = 1; slot <= kPatchSlotCount; ++slot)
    {
        if (s_qspi->EraseSector(SlotAddress(slot)) != daisy::QSPIHandle::Result::OK)
            return false;
    }
    return true;
}

} // namespace dco
