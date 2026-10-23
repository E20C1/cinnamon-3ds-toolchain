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

typedef struct {
    bool valid;
    bool active;
    bool ready;
    bool success;
    uint32_t requestId;
    uint32_t bytesRead;
    uint32_t totalBytes;
    uint32_t chunksRead;
} N3DSAudioWorkerReadStatus;

N3DSAudioSystem* N3DSAudioSystem_create(void);
bool N3DSAudioSystem_isReady(AudioSystem* base);
const char* N3DSAudioSystem_getStartupError(AudioSystem* base);
bool N3DSAudioSystem_getLastRoomPrewarmPerf(AudioSystem* base, N3DSAudioRoomPrewarmPerf* outPerf);
uint32_t N3DSAudioSystem_beginWorkerRead(
    AudioSystem* base,
    const char* path,
    uint32_t offset,
    uint32_t size
);
bool N3DSAudioSystem_getWorkerReadStatus(
    AudioSystem* base,
    uint32_t requestId,
    N3DSAudioWorkerReadStatus* outStatus
);
bool N3DSAudioSystem_waitWorkerRead(AudioSystem* base, uint32_t requestId);
bool N3DSAudioSystem_takeWorkerRead(
    AudioSystem* base,
    uint32_t requestId,
    uint8_t** outData,
    uint32_t* outSize
);
void N3DSAudioSystem_cancelWorkerRead(AudioSystem* base, uint32_t requestId);
void N3DSAudioSystem_getCacheStats(
    AudioSystem* base,
    uint32_t* outCachedSounds,
    uint32_t* outTotalSounds,
    uint32_t* outCachedBytes,
    uint32_t* outCacheLimitBytes
);
