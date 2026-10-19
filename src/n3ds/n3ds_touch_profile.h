#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool mirrorBottomScreen;
    int32_t tapKeyCode;
    uint32_t tapMaxMs;
    float tapMaxMovePx;
    float virtualStickDeadzonePx;

    bool followEnabled;
    char followConditionGlobal[64];
    float followConditionGreaterThan;
    char followTargetToken[64];
    float followTolerance;
} N3DSTouchProfile;

void N3DSTouchProfile_setDefaults(N3DSTouchProfile* profile);
bool N3DSTouchProfile_load(N3DSTouchProfile* profile, const char* path);
