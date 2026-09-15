#pragma once

#include <SD_MMC.h>
#include "../../shared/pattern_transfer.h"

class SdPatternDisk {
  public:
    bool List(uint8_t* bitmap)
    {
        if (!Mount()) return false;
        File directory = SD_MMC.open("/DCO-ONE/PATTERNS");
        if (!directory || !directory.isDirectory()) return Fail("directory");
        directory.close();
        memcpy(bitmap, used_, sizeof(used_));
        return true;
    }

    bool Read(int slot, dco::PatternRecord& record)
    {
        if (!Mount()) return false;
        char path[48];
        Path(slot, "dco", path);
        if (ReadFile(path, record)) return true;
        Path(slot, "bak", path);
        return ReadFile(path, record);
    }

    bool Write(int slot, const dco::PatternRecord& record)
    {
        if (!dco::ValidPattern(record) || !Mount()) return false;
        char path[48], temporary[48], backup[48];
        Path(slot, "dco", path);
        Path(slot, "tmp", temporary);
        Path(slot, "bak", backup);
        dco::PatternRecord previous;
        if (!ReadFile(path, previous) && ReadFile(backup, previous))
        {
            if (SD_MMC.exists(path) && !SD_MMC.remove(path)) return Fail("recovery remove");
            if (!SD_MMC.rename(backup, path)) return Fail("recovery rename");
        }
        if (SD_MMC.exists(temporary) && !SD_MMC.remove(temporary)) return Fail("temporary remove");
        File output = SD_MMC.open(temporary, FILE_WRITE);
        if (!output) return Fail("open write");
        size_t written = output.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record));
        output.flush();
        output.close();
        dco::PatternRecord verified;
        if (written != sizeof(record) || !ReadFile(temporary, verified)
            || memcmp(&verified, &record, sizeof(record)) != 0) return Fail("verify write");
        bool hadPrevious = SD_MMC.exists(path);
        if (hadPrevious)
        {
            if (SD_MMC.exists(backup) && !SD_MMC.remove(backup)) return Fail("backup remove");
            if (!SD_MMC.rename(path, backup)) return Fail("backup rename");
        }
        if (!SD_MMC.rename(temporary, path))
        {
            if (hadPrevious) SD_MMC.rename(backup, path);
            return Fail("commit rename");
        }
        used_[(slot - 1) / 8] |= 1U << ((slot - 1) % 8);
        Serial.printf("[SD] Saved pattern %03d (%u bytes)\n", slot, unsigned(sizeof(record)));
        return true;
    }

  private:
    bool mounted_ = false;
    uint8_t used_[16] = {};

    static void Path(int slot, const char* extension, char* path)
    {
        snprintf(path, 48, "/DCO-ONE/PATTERNS/%03d.%s", slot, extension);
    }
    bool Fail(const char* operation)
    {
        Serial.printf("[SD] Error: %s\n", operation);
        SD_MMC.end();
        mounted_ = false;
        return false;
    }
    bool ReadFile(const char* path, dco::PatternRecord& record)
    {
        File input = SD_MMC.open(path, FILE_READ);
        if (!input) return false;
        bool ok = input.size() == sizeof(record)
            && input.read(reinterpret_cast<uint8_t*>(&record), sizeof(record)) == sizeof(record);
        input.close();
        return ok && dco::ValidPattern(record);
    }
    bool Mount()
    {
        if (mounted_) return true;
        SD_MMC.setPins(2, 1, 3);
        if (!SD_MMC.begin("/sdcard", true, false, 10000)) return Fail("mount (FAT32 required)");
        if ((!SD_MMC.exists("/DCO-ONE") && !SD_MMC.mkdir("/DCO-ONE"))
            || (!SD_MMC.exists("/DCO-ONE/PATTERNS") && !SD_MMC.mkdir("/DCO-ONE/PATTERNS")))
            return Fail("mkdir");
        mounted_ = true;
        memset(used_, 0, sizeof(used_));
        for (int slot = 1; slot <= dco::kPatchSlotCount; ++slot)
        {
            dco::PatternRecord record;
            if (Read(slot, record)) used_[(slot - 1) / 8] |= 1U << ((slot - 1) % 8);
        }
        Serial.printf("[SD] Ready: %llu MB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
        return true;
    }
};

static bool SendSdReply(const char* line, void*)
{
    size_t length = strlen(line);
    return Serial1.write(reinterpret_cast<const uint8_t*>(line), length) == length
        && Serial1.write('\n') == 1;
}

static SdPatternDisk s_sd_disk;
static dco::PatternServer<SdPatternDisk> s_sd_server(s_sd_disk, SendSdReply, nullptr);