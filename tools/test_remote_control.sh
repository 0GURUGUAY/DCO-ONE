#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cat > "$work/test.cpp" <<'CPP'
#include "shared/remote_control.h"
#include "shared/modulation.h"
#include "shared/pattern_transfer.h"
#include <cassert>
#include <cstdarg>
#include <string>
#include <vector>

constexpr int kMaxPolySteps = 32;
constexpr int POLY_OFF = 0;
struct Param { int value; } kBpmParam{120}, kPlayAutoParam{4};
dco::PatchPolyStep s_poly_steps[32] = {};
bool s_playing = false, s_440hz_test_active = false, s_midi_clock_running = false;
bool s_editing_numeric = false, s_poly_wheel_active = false;
int s_current_patch = 7, s_poly_cursor = 0, s_poly_play_step = -1, s_pingpong_index = 0;
int s_auto_steps_until_change = 0, triggers = 0, releases = 0, panics = 0, dirty = 0;
uint32_t s_last_poly_step_ms = 0;
float s_midi_clock_phase = 1;
unsigned s_remote_pending = 0;
dco::PatternClient s_sd_client;
std::vector<std::string> frames;
struct Midi {
    int starts = 0, stops = 0;
    void SendStart() { ++starts; }
    void SendStop() { ++stops; }
} s_midi_out;
int ClampedPolyStepCount() { return 32; }
void MarkSettingsDirty(uint32_t) { ++dirty; }
void TriggerPolyStep(int) { ++triggers; }
void ReleaseSequencerVoices() { ++releases; }
void PanicSilence() { ++panics; }
void SendPlayStatus() {}
void SendPolyState() {}
void SendRemoteParameters() {}
bool ApplyRemoteParameter(const dco::RemoteCommand& command) { return command.parameter == 0; }
void CopySettingsToPatchData(dco::PatchData& patch) { patch.numPolySteps = 32; }
void DisplayPrintLine(const char* format, ...) {
    char line[512];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    assert(length > 0 && length <= 254);
    frames.emplace_back(line);
}
bool Send(const char*, void*) { return true; }
CPP

for function in SendRemoteState RemoteReply HandleRemoteCommand; do
    awk -v name="$function" '
        $0 ~ "^static .* " name "\\(" { copying = 1 }
        copying { print }
        copying && /^}/ { exit }
    ' "$root/daisy/src/main.cpp" >> "$work/test.cpp"
done

cat >> "$work/test.cpp" <<'CPP'
int main() {
    dco::MatrixSlot routes[dco::kMatrixSlotCount] = {};
    routes[0] = {2, 6, 0.35f};
    routes[7] = {1, 6, -0.2f};
    float destinations[8] = {};
    dco::AccumulateModulation(routes, 0.5f, 1.0f, 0.0f, destinations);
    assert(destinations[6] > 0.249f && destinations[6] < 0.251f);
    assert(destinations[2] == 0);
    routes[1] = {3, 1, 0.5f};
    dco::AccumulateModulation(routes, 0, 0, 0, destinations, 0.8f);
    assert(destinations[1] > 0.399f && destinations[1] < 0.401f);
    assert(dco::UnpackMatrixSlot(dco::PackMatrixSlot(routes[1])).source == 3);
    for (int amount = -100; amount <= 100; ++amount) {
        auto restored = dco::UnpackMatrixSlot(dco::PackMatrixSlot({2, 6, amount * 0.01f}));
        assert(restored.source == 2 && restored.destination == 6);
        assert(restored.amount > amount * 0.01f - 0.001f && restored.amount < amount * 0.01f + 0.001f);
    }
    assert(dco::UnpackMatrixSlot(0).source == 0);
    assert(dco::UnpackMatrixSlot(0x4D0206FF).source == 0);
    assert(dco::LimitModulation(2, 0, 1) == 1);
    dco::RemoteCommand parameter;
    assert(dco::ParseRemoteCommand("RMC,9,PARAM,7,2,6,4", parameter));
    assert(parameter.action == dco::RemoteAction::Param && parameter.group == 2
        && parameter.parameter == 6 && parameter.value == 4);
    assert(dco::ParseRemoteCommand("RMC,9,PARAM,7,4,7,5000", parameter));
    assert(parameter.group == 4 && parameter.parameter == 7 && parameter.value == 5000);
    for (const char* invalid : {"RMC,9,PARAM,7,5,0,0", "RMC,9,PARAM,7,0,24,0",
         "RMC,9,PARAM,129,0,0,0", "RMC,9,PARAM,7,0,0,20001", "RMC,9,PARAM,7,0,0,1x"})
        assert(!dco::ParseRemoteCommand(invalid, parameter));
    s_sd_client.Init(Send, nullptr);
    HandleRemoteCommand("RMC,0,STATE", 100);
    assert(frames.back().find("RMS,7,0,32,-1,120,0,") == 0);
    HandleRemoteCommand("RMC,1,PLAY", 200);
    assert(s_playing && s_midi_clock_running && s_last_poly_step_ms == 200);
    assert(s_poly_play_step == 0 && triggers == 1 && s_midi_out.starts == 1);
    assert(frames.back() == "RMR,1,OK");
    HandleRemoteCommand("RMC,2,STEP,7,0,4,-14,24", 300);
    assert(s_poly_steps[0].state == 4 && s_poly_steps[0].degree == -14);
    assert(s_poly_steps[0].fixedTranspose == 24 && triggers == 2 && dirty == 1);
    assert(s_poly_wheel_active && !s_editing_numeric);
    HandleRemoteCommand("RMC,3,STEP,7,0,0,0,0", 400);
    assert(releases == 1 && triggers == 2 && dirty == 2);
    HandleRemoteCommand("RMC,4,STEP,8,0,1,0,0", 500);
    assert(frames.back() == "RMR,4,ERR,STALE" && dirty == 2);
    HandleRemoteCommand("RMC,5,STEP,7,32,1,0,0", 500);
    assert(frames.back() == "RMR,5,ERR,REQUEST" && dirty == 2);
    HandleRemoteCommand("RMC,6,STOP", 600);
    assert(!s_playing && !s_midi_clock_running && s_poly_play_step == -1);
    assert(panics == 1 && s_midi_out.stops == 1);
    for (const char* action : {"LOAD", "SAVE", "DELETE"}) {
        s_sd_client = dco::PatternClient{};
        s_sd_client.Init(Send, nullptr);
        s_remote_pending = 0;
        char line[64];
        snprintf(line, sizeof(line), "RMC,7,%s,7", action);
        HandleRemoteCommand(line, 700);
        assert(s_sd_client.Busy() && s_remote_pending == 7);
        HandleRemoteCommand("RMC,8,STEP,7,0,1,0,0", 800);
        assert(frames.back() == "RMR,8,ERR,BUSY" && dirty == 2);
        HandleRemoteCommand("RMC,0,STATE", 900);
        assert(frames.back().find("RMS,7,0,32,-1,120,1,") == 0);
    }
    puts("Remote: actual Daisy handlers, play/stop, live steps, stale slot, busy transfers and frame bounds OK");
}
CPP
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I "$root" "$work/test.cpp" -o "$work/test"
"$work/test"

cat > "$work/parameters.cpp" <<'CPP'
#include "shared/remote_control.h"
#include "shared/modulation.h"
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <string>
#include <vector>
int callbackValue = -999, s_current_patch = 7;
struct ScopedIrqBlocker {};
dco::MatrixSlot s_matrix_slots[dco::kMatrixSlotCount] = {};
enum class SettingsWalkMode { kCollect, kApply };
struct SessionSettings {
    int32_t values[48] = {}, selections[48] = {};
};
std::vector<std::string> frames;
void DisplayPrintLine(const char* format, ...) {
    char line[255]; va_list args; va_start(args, format);
    int size = vsnprintf(line, sizeof(line), format, args); va_end(args);
    assert(size > 0 && size < 255); frames.emplace_back(line);
}
CPP
sed -n '/^using MenuApplyFn =/,/^\/\/ Accessors/{ /^\/\/ Accessors/!p; }' "$root/daisy/src/main.cpp" >> "$work/parameters.cpp"
sed -n '/^enum MenuColor/,/^};/p' "$root/daisy/src/main.cpp" >> "$work/parameters.cpp"
sed -n 's/^static void \(Apply[A-Za-z0-9]*\)(int32_t index);/static void \1(int32_t index) { callbackValue = index; }/p' "$root/daisy/src/main.cpp" >> "$work/parameters.cpp"
printf '#include "daisy/src/menu_generated.inc"\n' >> "$work/parameters.cpp"
for function in RemoteParameterNode ApplyRemoteParameter SendRemoteParameters WalkMenuTree; do
    if [ "$function" = WalkMenuTree ]; then printf 'template <typename Settings>\n' >> "$work/parameters.cpp"; fi
    awk -v name="$function" '
        $0 ~ "^static .* " name "\\(" { copying = 1 }
        copying { print }
        copying && /^}/ { exit }
    ' "$root/daisy/src/main.cpp" >> "$work/parameters.cpp"
done
cat >> "$work/parameters.cpp" <<'CPP'
int main() {
    dco::RemoteCommand command;
    command.group = 1; command.parameter = 2; command.value = 70;
    assert(ApplyRemoteParameter(command) && kVcfResonanceParam.value == 70 && callbackValue == 70);
    command.value = 101;
    assert(!ApplyRemoteParameter(command) && kVcfResonanceParam.value == 70);
    command.group = 2; command.parameter = 5; command.value = 5;
    assert(ApplyRemoteParameter(command) && kLfoSubmenu[5].selectedIndex == 5 && callbackValue == 5);
    command.value = 6; assert(!ApplyRemoteParameter(command));
    command.group = 0; command.parameter = 9; command.value = 3;
    assert(ApplyRemoteParameter(command) && kOscFmSubmenu[3].selectedIndex == 3);
    command.parameter = 10; assert(!ApplyRemoteParameter(command));
    command.group = 3; command.parameter = 21; command.value = 2;
    assert(ApplyRemoteParameter(command));
    command.parameter = 22; command.value = 6; assert(ApplyRemoteParameter(command));
    command.parameter = 23; command.value = -35; assert(ApplyRemoteParameter(command));
    command.parameter = 21; command.value = 4; assert(!ApplyRemoteParameter(command));
    assert(s_matrix_slots[7].source == 2 && s_matrix_slots[7].destination == 6);
    command.group = 4;
    for (int parameter = 0; parameter < 8; ++parameter) {
        command.parameter = parameter;
        command.value = parameter % 4 == 2 ? 65 : 450;
        assert(ApplyRemoteParameter(command) && callbackValue == command.value);
        assert(RemoteParameterNode(4, parameter)->numeric->value == command.value);
        command.value = parameter % 4 == 2 ? 101 : 5001;
        assert(!ApplyRemoteParameter(command));
    }
    command.parameter = 8; assert(!ApplyRemoteParameter(command));
    dco::PatchData patch;
    uint32_t values = 0, selections = 0;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kCollect, patch, values, selections);
    kEnv1AttackParam.value = 1;
    kEnv2ReleaseParam.value = 1;
    assert(selections <= 42 && values <= 64);
    SessionSettings session;
    values = selections = 0;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kCollect, session, values, selections);
    s_matrix_slots[7] = {};
    values = selections = 0;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kApply, session, values, selections);
    assert(s_matrix_slots[7].source == 2 && s_matrix_slots[7].amount < -0.349f);
    s_matrix_slots[7] = {};
    values = selections = 0;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kApply, patch, values, selections);
    assert(s_matrix_slots[7].source == 2 && s_matrix_slots[7].destination == 6);
    assert(kVcfResonanceParam.value == 70 && kOscFmSubmenu[3].selectedIndex == 3);
    assert(kEnv1AttackParam.value == 450 && kEnv2ReleaseParam.value == 450);
    for (int index = 58; index < 64; ++index) patch.selections[index] = 0;
    values = selections = 0;
    WalkMenuTree(&kRootNode, SettingsWalkMode::kApply, patch, values, selections);
    assert(s_matrix_slots[7].source == 0);
    SendRemoteParameters();
    assert(frames.size() == 5 && frames[4].find("RMP,7,4,") == 0);
    int32_t decoded[24];
    assert(dco::DecodeHex(frames[1].c_str() + 8, reinterpret_cast<uint8_t*>(decoded), sizeof(decoded)));
    assert(decoded[2] == 70);
    assert(dco::DecodeHex(frames[4].c_str() + 8, reinterpret_cast<uint8_t*>(decoded), sizeof(decoded)));
    assert(decoded[0] == 450 && decoded[2] == 65 && decoded[6] == 65 && decoded[7] == 450);
    puts("Synth: actual parameter setters, menu bounds, LFO2, eight routes, QSPI/SD roundtrip, old presets, UART frames OK");
}
CPP
c++ -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -Wno-unused-variable -Wno-unused-function \
    -fsanitize=address,undefined -I "$root" "$work/parameters.cpp" -o "$work/parameters"
"$work/parameters"