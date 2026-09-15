#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace dco {

constexpr int kPatchMaxValues = 64;
constexpr int kPatchMaxSelections = 64;
constexpr int kPatchMaxPolySteps = 32;
constexpr int kPatchSlotCount = 128;

struct PatchPolyStep {
    uint8_t state;
    int8_t degree;
    int8_t fixedTranspose;
};

struct PatchData {
    int32_t values[kPatchMaxValues] = {};
    uint32_t numValues = 0;
    int32_t selections[kPatchMaxSelections] = {};
    uint32_t numSelections = 0;
    PatchPolyStep polySteps[kPatchMaxPolySteps] = {};
    uint32_t numPolySteps = 0;
    int32_t gateLengthPct = 100;
};

struct PatternRecord {
    uint32_t magic = 0x534F4344;
    uint32_t version = 3;
    uint32_t length = sizeof(PatchData);
    PatchData data;
    uint32_t checksum = 0;
};

static_assert(sizeof(PatchData) == 624, "Pattern layout changed: bump format version");
static_assert(sizeof(PatternRecord) == 640, "Unexpected pattern padding");

inline uint32_t PatternChecksum(const PatternRecord& record)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t index = 0; index < offsetof(PatternRecord, checksum); ++index)
    {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

inline bool ValidPattern(const PatternRecord& record)
{
    if (record.magic != 0x534F4344 || record.version != 3
        || record.length != sizeof(PatchData)
        || record.data.numValues > kPatchMaxValues
        || record.data.numSelections > kPatchMaxSelections
        || record.data.numPolySteps != kPatchMaxPolySteps
        || record.data.gateLengthPct < 0 || record.data.gateLengthPct > 100
        || record.checksum != PatternChecksum(record))
        return false;
    for (const auto& step : record.data.polySteps)
        if (step.state > 4 || step.degree < -14 || step.degree > 14
            || step.fixedTranspose < -24 || step.fixedTranspose > 24)
            return false;
    return true;
}

inline int HexDigit(char digit)
{
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    return -1;
}

inline void EncodeHex(const uint8_t* bytes, size_t count, char* output)
{
    const char* digits = "0123456789ABCDEF";
    for (size_t index = 0; index < count; ++index)
    {
        output[index * 2] = digits[bytes[index] >> 4];
        output[index * 2 + 1] = digits[bytes[index] & 15];
    }
    output[count * 2] = '\0';
}

inline bool DecodeHex(const char* text, uint8_t* output, size_t count)
{
    if (strlen(text) != count * 2) return false;
    for (size_t index = 0; index < count; ++index)
    {
        int high = HexDigit(text[index * 2]);
        int low = HexDigit(text[index * 2 + 1]);
        if (high < 0 || low < 0) return false;
        output[index] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

}