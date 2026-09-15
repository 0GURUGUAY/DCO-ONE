#pragma once

#include "pattern_format.h"
#include <cstdio>

namespace dco {

using PatternSend = bool (*)(const char*, void*);
constexpr size_t kPatternChunk = 64;

class PatternClient {
  public:
    enum class Operation { None, List, Load, Save };
    PatternRecord record;
    uint8_t used[16] = {};
    bool available = false;

    void Init(PatternSend send, void* context) { send_ = send; context_ = context; }
    bool Busy() const { return operation_ != Operation::None; }
    bool Start(Operation operation, int slot, uint32_t now, const PatchData* data = nullptr)
    {
        if (Busy() || completed_ || !send_ || operation == Operation::None
            || (operation != Operation::List && (slot < 1 || slot > kPatchSlotCount)))
            return false;
        if (operation == Operation::Save)
        {
            if (!data) return false;
            record = PatternRecord{};
            record.data = *data;
            record.checksum = PatternChecksum(record);
            if (!ValidPattern(record)) return false;
        }
        operation_ = operation;
        slot_ = slot;
        offset_ = 0;
        stage_ = 0;
        Request(now);
        return true;
    }
    void Tick(uint32_t now)
    {
        if (Busy() && now - sentAt_ >= 2000) Finish(false);
    }
    bool TakeResult(Operation& operation, int& slot, bool& success)
    {
        if (!completed_) return false;
        operation = resultOperation_;
        slot = slot_;
        success = success_;
        completed_ = false;
        return true;
    }
    void Receive(const char* line, uint32_t now)
    {
        unsigned id = 0;
        int consumed = 0;
        if (!Busy() || sscanf(line, "SDR,%u,%n", &id, &consumed) != 1
            || consumed == 0 || id != requestId_) return;
        const char* reply = line + consumed;
        if (operation_ == Operation::List)
        {
            uint8_t bitmap[16];
            bool ok = strncmp(reply, "MAP,", 4) == 0 && DecodeHex(reply + 4, bitmap, 16);
            if (ok) memcpy(used, bitmap, sizeof(used));
            Finish(ok);
        }
        else if (operation_ == Operation::Load)
        {
            size_t count = ChunkSize();
            if (strncmp(reply, "DATA,", 5) != 0
                || !DecodeHex(reply + 5, reinterpret_cast<uint8_t*>(&record) + offset_, count))
            { Finish(false); return; }
            offset_ += count;
            if (offset_ == sizeof(record)) Finish(ValidPattern(record));
            else Request(now);
        }
        else
        {
            if (strcmp(reply, "OK") != 0) { Finish(false); return; }
            if (stage_ == 2) { Finish(true); return; }
            if (stage_ == 0) stage_ = 1;
            else offset_ += ChunkSize();
            if (offset_ == sizeof(record)) stage_ = 2;
            Request(now);
        }
    }

  private:
    PatternSend send_ = nullptr;
    void* context_ = nullptr;
    Operation operation_ = Operation::None;
    Operation resultOperation_ = Operation::None;
    bool completed_ = false, success_ = false;
    unsigned requestId_ = 0;
    uint32_t sentAt_ = 0;
    int slot_ = 0, stage_ = 0;
    size_t offset_ = 0;

    size_t ChunkSize() const
    {
        size_t remaining = sizeof(record) - offset_;
        return remaining < kPatternChunk ? remaining : kPatternChunk;
    }
    void Finish(bool success)
    {
        resultOperation_ = operation_;
        operation_ = Operation::None;
        success_ = success;
        completed_ = true;
        if (resultOperation_ == Operation::List) available = success;
        if (resultOperation_ == Operation::Save && success)
            used[(slot_ - 1) / 8] |= 1U << ((slot_ - 1) % 8);
    }
    void Request(uint32_t now)
    {
        char line[192];
        char hex[kPatternChunk * 2 + 1] = {};
        ++requestId_;
        const char* command = "LIST";
        if (operation_ == Operation::Load) command = "GET";
        if (operation_ == Operation::Save)
        {
            command = stage_ == 0 ? "BEGIN" : stage_ == 1 ? "PUT" : "COMMIT";
            if (stage_ == 1)
                EncodeHex(reinterpret_cast<const uint8_t*>(&record) + offset_, ChunkSize(), hex);
        }
        snprintf(line, sizeof(line), "SDC,%u,%s,%d,%u,%s", requestId_, command, slot_,
                 static_cast<unsigned>(offset_), hex);
        sentAt_ = now;
        if (!send_(line, context_)) Finish(false);
    }
};

template <typename Disk>
class PatternServer {
  public:
    PatternServer(Disk& disk, PatternSend send, void* context)
        : disk_(disk), send_(send), context_(context) {}

    void Receive(const char* line)
    {
        unsigned id = 0, offset = 0;
        int slot = 0, consumed = 0;
        char command[8] = {};
        if (sscanf(line, "SDC,%u,%7[^,],%d,%u,%n", &id, command, &slot, &offset, &consumed) != 4
            || consumed == 0) return;
        const char* payload = line + consumed;
        char reply[160] = "ERR,REQUEST";
        if (strcmp(command, "LIST") == 0 && !*payload)
        {
            uint8_t bitmap[16] = {};
            if (disk_.List(bitmap))
            {
                strcpy(reply, "MAP,");
                EncodeHex(bitmap, sizeof(bitmap), reply + 4);
            }
            else strcpy(reply, "ERR,SD");
        }
        else if (slot >= 1 && slot <= kPatchSlotCount)
        {
            if (strcmp(command, "BEGIN") == 0 && offset == 0 && !*payload)
            {
                writeSlot_ = slot;
                readSlot_ = 0;
                received_ = 0;
                strcpy(reply, "OK");
            }
            else if (strcmp(command, "PUT") == 0 && slot == writeSlot_
                     && offset == received_ && offset < sizeof(record_))
            {
                size_t count = sizeof(record_) - offset;
                if (count > kPatternChunk) count = kPatternChunk;
                if (DecodeHex(payload, reinterpret_cast<uint8_t*>(&record_) + offset, count))
                {
                    received_ += count;
                    strcpy(reply, "OK");
                }
            }
            else if (strcmp(command, "COMMIT") == 0 && slot == writeSlot_
                     && offset == sizeof(record_) && received_ == sizeof(record_) && !*payload)
            {
                strcpy(reply, ValidPattern(record_) && disk_.Write(slot, record_) ? "OK" : "ERR,SD");
                writeSlot_ = 0;
            }
            else if (strcmp(command, "GET") == 0 && offset < sizeof(record_)
                     && offset % kPatternChunk == 0 && !*payload)
            {
                if (offset == 0)
                {
                    writeSlot_ = 0;
                    readSlot_ = disk_.Read(slot, record_) && ValidPattern(record_) ? slot : 0;
                }
                if (readSlot_ == slot)
                {
                    size_t count = sizeof(record_) - offset;
                    if (count > kPatternChunk) count = kPatternChunk;
                    strcpy(reply, "DATA,");
                    EncodeHex(reinterpret_cast<const uint8_t*>(&record_) + offset, count, reply + 5);
                }
                else strcpy(reply, "ERR,SD");
            }
        }
        char response[192];
        snprintf(response, sizeof(response), "SDR,%u,%s", id, reply);
        send_(response, context_);
    }

  private:
    Disk& disk_;
    PatternSend send_;
    void* context_;
    PatternRecord record_;
    int writeSlot_ = 0, readSlot_ = 0;
    size_t received_ = 0;
};

}