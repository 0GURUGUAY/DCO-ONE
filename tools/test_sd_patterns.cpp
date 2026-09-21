#include "../shared/pattern_format.h"
#include "../shared/pattern_transfer.h"
#include "../shared/remote_control.h"
#include <cassert>
#include <cstdio>
#include <deque>
#include <string>

struct TestDisk {
    dco::PatternRecord saved;
    bool present = false, fail = false;
    int writes = 0;
    bool Read(int slot, dco::PatternRecord& record) {
        if (fail || !present || slot != 7) return false;
        record = saved;
        return true;
    }
    bool Write(int slot, const dco::PatternRecord& record) {
        if (fail || slot != 7) return false;
        saved = record; present = true; ++writes; return true;
    }
    bool List(uint8_t* bitmap) {
        if (fail) return false;
        if (present) bitmap[0] = 64;
        return true;
    }
    bool Delete(int slot) {
        if (fail || slot != 7) return false;
        present = false;
        return true;
    }
};

static bool Enqueue(const char* line, void* context)
{
    assert(strlen(line) < 192);
    static_cast<std::deque<std::string>*>(context)->emplace_back(line);
    return true;
}

static void TestTransfer(const dco::PatchData& data)
{
    TestDisk disk;
    std::deque<std::string> requests, replies;
    dco::PatternClient client;
    client.Init(Enqueue, &requests);
    dco::PatternServer<TestDisk> server(disk, Enqueue, &replies);
    auto pump = [&]() {
        for (int iteration = 0; iteration < 30 && !requests.empty(); ++iteration) {
            auto request = requests.front(); requests.pop_front();
            server.Receive(request.c_str());
            assert(!replies.empty());
            auto reply = replies.front(); replies.pop_front();
            client.Receive(reply.c_str(), 100);
        }
        assert(requests.empty());
        assert(!client.Busy());
    };
    using Op = dco::PatternClient::Operation;
    Op operation;
    int slot;
    bool success;
    assert(client.Start(Op::Save, 7, 100, &data));
    assert(!client.Start(Op::Load, 7, 100));
    pump();
    assert(client.TakeResult(operation, slot, success) && success && operation == Op::Save);
    assert(disk.writes == 1 && slot == 7);
    assert(client.Start(Op::Load, 7, 100)); pump();
    assert(client.TakeResult(operation, slot, success) && success);
    assert(memcmp(&data, &client.record.data, sizeof(data)) == 0);
    assert(client.Start(Op::List, 0, 100)); pump();
    assert(client.TakeResult(operation, slot, success) && success && client.available);
    assert(client.used[0] == 64);
    assert(client.Start(Op::Delete, 7, 100)); pump();
    assert(client.TakeResult(operation, slot, success) && success && operation == Op::Delete);
    assert(!disk.present && client.used[0] == 0);
    disk.present = true;
    disk.fail = true;
    assert(client.Start(Op::Delete, 7, 100)); pump();
    assert(client.TakeResult(operation, slot, success) && !success && disk.present);
    assert(client.Start(Op::Save, 7, 100, &data)); pump();
    assert(client.TakeResult(operation, slot, success) && !success && disk.writes == 1);
    assert(client.Start(Op::List, 0, 100)); pump();
    assert(client.TakeResult(operation, slot, success) && !success && !client.available);
    disk.fail = false;
    disk.saved.data.values[0]++;
    assert(client.Start(Op::Load, 7, 100)); pump();
    assert(client.TakeResult(operation, slot, success) && !success);
    assert(client.Start(Op::Load, 7, 100));
    client.Receive("SDR,0,OK", 200);
    assert(client.Busy());
    client.Tick(2100);
    assert(client.TakeResult(operation, slot, success) && !success);
    assert(!client.Start(Op::Load, 0, 100));
    assert(!client.Start(Op::Save, 129, 100, &data));
    server.Receive("SDC,999,COMMIT,7,640,");
    assert(replies.back() == "SDR,999,ERR,REQUEST");
    assert(disk.writes == 1);
}

int main()
{
    dco::RemoteCommand command;
    assert(dco::ParseRemoteCommand("RMC,1,STATE", command));
    assert(command.action == dco::RemoteAction::State);
    assert(dco::ParseRemoteCommand("RMC,2,LOAD,128", command) && command.slot == 128);
    assert(dco::ParseRemoteCommand("RMC,3,SAVE,1", command));
    assert(dco::ParseRemoteCommand("RMC,4,STOP", command));
    assert(dco::ParseRemoteCommand("RMC,4,PLAY", command));
    assert(command.action == dco::RemoteAction::Play);
    assert(dco::ParseRemoteCommand("RMC,5,DELETE,128", command));
    assert(command.action == dco::RemoteAction::Delete);
    assert(dco::ParseRemoteCommand("RMC,5,STEP,7,31,4,-14,24", command));
    assert(command.step == 31 && command.state == 4 && command.degree == -14 && command.transpose == 24);
    for (const char* invalid : {"RMC,1,LOAD,0", "RMC,1,SAVE,129", "RMC,1,STOP,1",
                               "RMC,1,STEP,7,32,0,0,0", "RMC,1,STEP,7,0,5,0,0",
                               "RMC,1,STEP,7,0,1,-15,0", "RMC,1,STEP,7,0,4,0,25",
                               "RMC,1,STEP,7,0,1,0,0junk", "RMC,1,UNKNOWN", "SDR,1,OK"})
        assert(!dco::ParseRemoteCommand(invalid, command));
    dco::PatternRecord record;
    record.data.numPolySteps = 32;
    record.data.numValues = 64;
    record.data.values[0] = 120;
    record.data.values[63] = 999;
    record.data.gateLengthPct = 25;
    record.data.polySteps[0] = {4, -7, 12};
    record.checksum = dco::PatternChecksum(record);
    assert(dco::ValidPattern(record));
    char hex[sizeof(record) * 2 + 1];
    dco::EncodeHex(reinterpret_cast<const uint8_t*>(&record), sizeof(record), hex);
    dco::PatternRecord restored;
    assert(dco::DecodeHex(hex, reinterpret_cast<uint8_t*>(&restored), sizeof(restored)));
    assert(dco::ValidPattern(restored));
    assert(memcmp(&record, &restored, sizeof(record)) == 0);
    restored.data.values[0]++;
    assert(!dco::ValidPattern(restored));
    restored = record;
    restored.version++;
    restored.checksum = dco::PatternChecksum(restored);
    assert(!dco::ValidPattern(restored));
    restored = record;
    restored.data.numValues = 65;
    restored.checksum = dco::PatternChecksum(restored);
    assert(!dco::ValidPattern(restored));
    hex[0] = 'Z';
    assert(!dco::DecodeHex(hex, reinterpret_cast<uint8_t*>(&restored), sizeof(restored)));
    assert(!dco::DecodeHex("00", reinterpret_cast<uint8_t*>(&restored), sizeof(restored)));
    TestTransfer(record.data);
    puts("SD pattern format: round-trip, corruption, version and bounds OK");
    puts("SD transfer: save/load/list, missing SD, failed write, stale reply and timeout OK");
}