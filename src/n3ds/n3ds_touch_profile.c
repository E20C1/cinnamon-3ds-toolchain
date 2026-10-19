#include "n3ds_touch_profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* N3DSTouchProfile_readText(const char* path) {
    if (path == NULL) return NULL;
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size <= 0 || size > 64 * 1024) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char* text = malloc((size_t) size + 1u);
    if (text == NULL) {
        fclose(file);
        return NULL;
    }
    size_t read = fread(text, 1, (size_t) size, file);
    fclose(file);
    if (read != (size_t) size) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

static const char* N3DSTouchProfile_findValue(const char* json, const char* key) {
    if (json == NULL || key == NULL) return NULL;

    char pattern[96];
    int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written <= 0 || (size_t) written >= sizeof(pattern)) return NULL;

    const char* cursor = strstr(json, pattern);
    if (cursor == NULL) return NULL;
    cursor += strlen(pattern);

    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') cursor++;
    if (*cursor != ':') return NULL;
    cursor++;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') cursor++;
    return cursor;
}

static bool N3DSTouchProfile_readBool(const char* json, const char* key, bool fallback) {
    const char* value = N3DSTouchProfile_findValue(json, key);
    if (value == NULL) return fallback;
    if (strncmp(value, "true", 4) == 0) return true;
    if (strncmp(value, "false", 5) == 0) return false;
    return fallback;
}

static int32_t N3DSTouchProfile_readInt(const char* json, const char* key, int32_t fallback) {
    const char* value = N3DSTouchProfile_findValue(json, key);
    if (value == NULL) return fallback;
    char* end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value) return fallback;
    return (int32_t) parsed;
}

static float N3DSTouchProfile_readFloat(const char* json, const char* key, float fallback) {
    const char* value = N3DSTouchProfile_findValue(json, key);
    if (value == NULL) return fallback;
    char* end = NULL;
    float parsed = strtof(value, &end);
    if (end == value) return fallback;
    return parsed;
}

static void N3DSTouchProfile_readString(
    const char* json,
    const char* key,
    char* out,
    size_t outSize
) {
    if (out == NULL || outSize == 0u) return;
    const char* value = N3DSTouchProfile_findValue(json, key);
    if (value == NULL || *value != '"') return;
    value++;

    size_t cursor = 0u;
    while (*value != '\0' && *value != '"' && cursor + 1u < outSize) {
        if (*value == '\\' && value[1] != '\0') {
            value++;
            switch (*value) {
                case 'n': out[cursor++] = '\n'; break;
                case 'r': out[cursor++] = '\r'; break;
                case 't': out[cursor++] = '\t'; break;
                case '\\': out[cursor++] = '\\'; break;
                case '"': out[cursor++] = '"'; break;
                default: out[cursor++] = *value; break;
            }
            value++;
            continue;
        }
        out[cursor++] = *value++;
    }
    out[cursor] = '\0';
}

void N3DSTouchProfile_setDefaults(N3DSTouchProfile* profile) {
    if (profile == NULL) return;
    memset(profile, 0, sizeof(*profile));
    profile->enabled = false;
    profile->mirrorBottomScreen = false;
    profile->tapKeyCode = 'Z';
    profile->tapMaxMs = 180u;
    profile->tapMaxMovePx = 10.0f;
    profile->virtualStickDeadzonePx = 18.0f;
    profile->followEnabled = false;
    profile->followConditionGreaterThan = 0.0f;
    profile->followTolerance = 4.0f;
}

bool N3DSTouchProfile_load(N3DSTouchProfile* profile, const char* path) {
    if (profile == NULL) return false;
    N3DSTouchProfile_setDefaults(profile);

    char* json = N3DSTouchProfile_readText(path);
    if (json == NULL) return false;

    profile->enabled = N3DSTouchProfile_readBool(json, "touch_enabled", profile->enabled);
    profile->mirrorBottomScreen = N3DSTouchProfile_readBool(json, "mirror_bottom_screen", profile->mirrorBottomScreen);
    profile->tapKeyCode = N3DSTouchProfile_readInt(json, "tap_key_code", profile->tapKeyCode);
    profile->tapMaxMs = (uint32_t) N3DSTouchProfile_readInt(json, "tap_max_ms", (int32_t) profile->tapMaxMs);
    profile->tapMaxMovePx = N3DSTouchProfile_readFloat(json, "tap_max_move_px", profile->tapMaxMovePx);
    profile->virtualStickDeadzonePx = N3DSTouchProfile_readFloat(json, "virtual_stick_deadzone_px", profile->virtualStickDeadzonePx);
    profile->followEnabled = N3DSTouchProfile_readBool(json, "follow_enabled", profile->followEnabled);
    profile->followConditionGreaterThan = N3DSTouchProfile_readFloat(
        json,
        "follow_condition_greater_than",
        profile->followConditionGreaterThan
    );
    profile->followTolerance = N3DSTouchProfile_readFloat(json, "follow_tolerance", profile->followTolerance);
    N3DSTouchProfile_readString(
        json,
        "follow_condition_global",
        profile->followConditionGlobal,
        sizeof(profile->followConditionGlobal)
    );
    N3DSTouchProfile_readString(
        json,
        "follow_target_token",
        profile->followTargetToken,
        sizeof(profile->followTargetToken)
    );

    if (profile->tapKeyCode < 0 || profile->tapKeyCode > 255) profile->tapKeyCode = 'Z';
    if (profile->tapMaxMs < 40u || profile->tapMaxMs > 1000u) profile->tapMaxMs = 180u;
    if (profile->tapMaxMovePx < 1.0f || profile->tapMaxMovePx > 80.0f) profile->tapMaxMovePx = 10.0f;
    if (profile->virtualStickDeadzonePx < 2.0f || profile->virtualStickDeadzonePx > 120.0f) {
        profile->virtualStickDeadzonePx = 18.0f;
    }
    if (profile->followTolerance < 0.5f || profile->followTolerance > 64.0f) profile->followTolerance = 4.0f;

    free(json);
    return true;
}
