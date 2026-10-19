#pragma once

#include "../audio_system.h"

typedef struct N3DSAudioSystem N3DSAudioSystem;

N3DSAudioSystem* N3DSAudioSystem_create(void);
bool N3DSAudioSystem_isReady(AudioSystem* base);
const char* N3DSAudioSystem_getStartupError(AudioSystem* base);
void N3DSAudioSystem_getCacheStats(
    AudioSystem* base,
    uint32_t* outCachedSounds,
    uint32_t* outTotalSounds,
    uint32_t* outCachedBytes,
    uint32_t* outCacheLimitBytes
);
