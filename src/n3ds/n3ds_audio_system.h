#pragma once

#include "../audio_system.h"

typedef struct N3DSAudioSystem N3DSAudioSystem;

typedef struct {
    bool valid;
    bool workerEnabled;
    uint32_t cachedBytesBefore;
    uint32_t cachedBytesAfter;
    uint32_t pendingSoundCountAfter;
    uint32_t pendingByteBudgetAfter;
} N3DSAudioRoomPrewarmPerf;

N3DSAudioSystem* N3DSAudioSystem_create(void);
bool N3DSAudioSystem_isReady(AudioSystem* base);
const char* N3DSAudioSystem_getStartupError(AudioSystem* base);
bool N3DSAudioSystem_getLastRoomPrewarmPerf(AudioSystem* base, N3DSAudioRoomPrewarmPerf* outPerf);
void N3DSAudioSystem_getCacheStats(
    AudioSystem* base,
    uint32_t* outCachedSounds,
    uint32_t* outTotalSounds,
    uint32_t* outCachedBytes,
    uint32_t* outCacheLimitBytes
);
