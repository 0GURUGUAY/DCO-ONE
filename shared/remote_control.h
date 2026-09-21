#pragma once

#include "pattern_format.h"
#include <cstdio>

namespace dco {

enum class RemoteAction { State, Load, Save, Delete, Play, Stop, Step, Param };

struct RemoteCommand {
    unsigned id = 0;
    RemoteAction action = RemoteAction::State;
    int slot = 0;
    int step = 0;
    int state = 0;
    int degree = 0;
    int transpose = 0;
    int group = 0;
    int parameter = 0;
    int value = 0;
};

inline bool ParseRemoteCommand(const char* line, RemoteCommand& command)
{
    char action[8] = {};
    int consumed = 0;
    if (sscanf(line, "RMC,%u,%7[A-Z]%n", &command.id, action, &consumed) != 2
        || !consumed) return false;
    const char* args = line + consumed;
    if (!strcmp(action, "STATE") || !strcmp(action, "STOP") || !strcmp(action, "PLAY"))
    {
        command.action = !strcmp(action, "STATE") ? RemoteAction::State
            : !strcmp(action, "PLAY") ? RemoteAction::Play : RemoteAction::Stop;
        return !*args;
    }
    consumed = 0;
    if (!strcmp(action, "PARAM"))
    {
        command.action = RemoteAction::Param;
        return sscanf(args, ",%d,%d,%d,%d%n", &command.slot, &command.group,
                      &command.parameter, &command.value, &consumed) == 4
            && consumed && !args[consumed] && command.slot >= 0 && command.slot <= kPatchSlotCount
            && command.group >= 0 && command.group <= 4
            && command.parameter >= 0 && command.parameter < 24
            && command.value >= -100 && command.value <= 20000;
    }
    if (!strcmp(action, "LOAD") || !strcmp(action, "SAVE") || !strcmp(action, "DELETE"))
    {
        command.action = !strcmp(action, "LOAD") ? RemoteAction::Load
            : !strcmp(action, "SAVE") ? RemoteAction::Save : RemoteAction::Delete;
        return sscanf(args, ",%d%n", &command.slot, &consumed) == 1
            && consumed && !args[consumed] && command.slot >= 1 && command.slot <= kPatchSlotCount;
    }
    if (!strcmp(action, "STEP"))
    {
        command.action = RemoteAction::Step;
        return sscanf(args, ",%d,%d,%d,%d,%d%n", &command.slot, &command.step, &command.state,
                      &command.degree, &command.transpose, &consumed) == 5
            && consumed && !args[consumed] && command.slot >= 0 && command.slot <= kPatchSlotCount
            && command.step >= 0 && command.step < kPatchMaxPolySteps
            && command.state >= 0 && command.state <= 4
            && command.degree >= -14 && command.degree <= 14
            && command.transpose >= -24 && command.transpose <= 24;
    }
    return false;
}

}