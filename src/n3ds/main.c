#include "../data_win.h"
#include "../runner.h"
#include "../runner_keyboard.h"
#include "../utils.h"
#include "../vm.h"

#include "n3ds_audio_system.h"
#include "n3ds_file_system.h"
#include "n3ds_renderer.h"

#include <3ds.h>
#include <citro2d.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define N3DS_LOADING_TEXT_SCALE 0.42f
#define N3DS_BOOT_LOG_MAX_LINES 6
#define N3DS_BOOT_LOG_LINE_CHARS 88
#define N3DS_TOTAL_VRAM_BYTES (6u * 1024u * 1024u)
#define N3DS_DEBUG_ASRIEL_ROOM 330
#define N3DS_DUST_PERF_PRE_FRAMES 12u
#define N3DS_DUST_PERF_POST_FRAMES 8u
#define N3DS_DUST_PERF_MAX_FRAMES 256u
#define N3DS_PERF_IMPORT_PAGE_CAP 16u
#define N3DS_PERF_PENDING_EVENT_CAP 8u
#define N3DS_BATTLE_ENTRY_POST_FRAMES 4u
#define N3DS_DUST_PERF_LOG_PATH "sdmc:/3ds/cinnamon/dust_perf.log"

void N3DS_tryTriggerAsrielLed(Runner* runner);

typedef struct {
    bool useCitro2D;
    C3D_RenderTarget* target;
    C2D_TextBuf textBuf;
    PrintConsole console;
    char statusLine[128];
    int chunkIndex;
    int totalChunks;
} N3DSLoadingScreen;

static char gN3DSBootLogLines[N3DS_BOOT_LOG_MAX_LINES][N3DS_BOOT_LOG_LINE_CHARS];
static int gN3DSBootLogLineCount = 0;

typedef struct {
    C2D_TextBuf textBuf;
    double displayedFps;
    double displayedRenderMs;
    double sampledRenderMs;
    uint32_t sampledFrames;
    u64 sampleStartMs;
    N3DSBattleTouchStatus touchFollowStatus;
    float touchFollowActivity;
    float touchFollowCurrentX;
    float touchFollowCurrentY;
    float touchFollowTargetX;
    float touchFollowTargetY;
} N3DSDebugMonitor;

typedef struct {
    int32_t frame;
    int32_t phase;
    uint32_t dustCount;
    uint32_t instancesCreated;
    uint32_t instancesDestroyed;
    uint32_t drawInstances;
    uint32_t spriteDrawCalls;
    uint32_t spritePartDrawCalls;
    uint32_t c2dFlushes;
    uint32_t residentAtlasVRAMBytes;
    uint32_t residentAtlasVRAMLimitBytes;
    uint32_t residentAtlasPageCount;
    uint32_t residentAtlasPageLimit;
    uint32_t residentDirectVRAMBytes;
    uint32_t residentDirectVRAMLimitBytes;
    uint32_t atlasBlobReads;
    uint32_t atlasBlobCacheBytes;
    uint32_t atlasBlobReadPageCount;
    uint16_t atlasBlobReadPages[N3DS_PERF_IMPORT_PAGE_CAP];
    uint32_t frameImportEventCount;
    uint16_t frameImportEventPages[N3DS_PERF_IMPORT_PAGE_CAP];
    int32_t frameImportEventTPAGs[N3DS_PERF_IMPORT_PAGE_CAP];
    uint32_t directBlobReads;
    uint32_t pageImports;
    uint32_t pageEvictions;
    uint32_t importFailures;
    uint32_t directAssetLoads;
    uint32_t directLastLoadKind;
    int32_t directLastLoadIndex;
    int32_t directLastLoadFrame;
    uint32_t directLastLoadVRAMBytes;
    int32_t directLastLoadUsedCachedBlob;
    uint32_t textureSwitches;
    int32_t atlasImportObjectIndex;
    uint32_t atlasImportInstanceId;
    int32_t atlasImportInstanceSpriteIndex;
    int32_t atlasImportInstanceSubimg;
    int32_t atlasImportInstanceTPAGIndex;
    int32_t atlasImportTriggerTPAGIndex;
    int32_t atlasImportTriggerSpriteIndex;
    int32_t atlasImportTriggerFrameIndex;
    uint32_t atlasImportCount;
    uint32_t atlasImportEvictions;
    uint32_t atlasImportPass;
    uint32_t atlasImportPageCount;
    uint16_t atlasImportPages[N3DS_PERF_IMPORT_PAGE_CAP];
    int32_t targetChoiceObjectIndex;
    uint32_t targetChoiceInstanceId;
    int32_t targetChoiceSpriteIndex;
    int32_t targetChoiceSubimg;
    int32_t targetChoiceTPAGIndex;
    float targetChoiceImageIndex;
    float targetChoiceImageSpeed;
    int32_t battleActive;
    int32_t myFight;
    int32_t mnFight;
    double stepMs;
    double prepMs;
    double eventMs;
    double motionMs;
    double collisionMs;
    double cleanupMs;
    double finalizeMs;
    double drawInstancesMs;
    double battleReplayMs;
    double battlePrewarmTickMs;
    int32_t battleTransitionPrewarmActive;
    uint32_t battlePrewarmTargetKind;
    int32_t battlePrewarmTargetRoomIndex;
    int32_t battlePrewarmStageStart;
    int32_t battlePrewarmStageEnd;
    int32_t battlePrewarmAssetStage;
    uint32_t battlePrewarmJobBytesCompleted;
    uint32_t battlePrewarmJobBytesTotal;
    uint32_t battlePrewarmTickChunks;
    uint32_t battlePrewarmTickBytes;
    double renderMs;
    double frameBeginMs;
    double rendererBeginMs;
    double drawViewsMs;
    double overlayMs;
    double rendererEndMs;
    double frameEndMs;
    double c3dProcessingMs;
    double previousGpuDrawingMs;
    double cmdBufUsagePercent;
    int32_t topStepObjectIndex;
    double topStepObjectMs;
    uint32_t topStepObjectCalls;
    int32_t roomTransitioned;
    int32_t roomFromIndex;
    int32_t roomToIndex;
    double roomTransitionMs;
    double roomInitMs;
    double roomStartMs;
    double rendererPrewarmMs;
    double audioPrewarmMs;
    int32_t prewarmBattleRoom;
    double prewarmTotalMs;
    double prewarmClearPinsMs;
    double prewarmFlushDirectMs;
    double prewarmBattleDirectMs;
    uint32_t prewarmBattleDirectReads;
    uint64_t prewarmBattleDirectReadBytes;
    double prewarmJapaneseFontsMs;
    uint32_t prewarmJapaneseFontReads;
    uint64_t prewarmJapaneseFontReadBytes;
    double prewarmRoomAtlasMs;
    uint32_t prewarmRoomAtlasReads;
    uint64_t prewarmRoomAtlasReadBytes;
    uint32_t prewarmRoomAtlasReadPageCount;
    uint16_t prewarmRoomAtlasReadPages[N3DS_ROOM_PREWARM_PAGE_DIAG_CAP];
    int32_t prewarmRoomAtlasReadPagesTruncated;
    double prewarmBattleCriticalMs;
    uint32_t prewarmBattleCriticalReads;
    uint64_t prewarmBattleCriticalReadBytes;
    uint32_t prewarmBattleCriticalReadPageCount;
    uint16_t prewarmBattleCriticalReadPages[N3DS_ROOM_PREWARM_PAGE_DIAG_CAP];
    int32_t prewarmBattleCriticalReadPagesTruncated;
    int32_t battleManifestLoaded;
    int32_t battleManifestAttempted;
    int32_t battleManifestFound;
    uint32_t battleManifestPageCount;
    uint32_t battleManifestColdReads;
    uint64_t battleManifestReadBytes;
    uint32_t prewarmAtlasCacheBytesBefore;
    uint32_t prewarmAtlasCacheBytesAfter;
    uint32_t prewarmDirectCacheBytesBefore;
    uint32_t prewarmDirectCacheBytesAfter;
    uint32_t prewarmDirectVRAMBytesBefore;
    uint32_t prewarmDirectVRAMBytesAfterFlush;
    uint32_t prewarmDirectVRAMBytesAfter;
    int32_t audioPrewarmWorkerEnabled;
    uint32_t audioPrewarmCacheBytesBefore;
    uint32_t audioPrewarmCacheBytesAfter;
    uint32_t audioPrewarmPendingSounds;
    uint32_t audioPrewarmPendingBytes;
} N3DSDustPerfFrame;

typedef struct {
    double totalMs;
    double frameBeginMs;
    double rendererBeginMs;
    double drawViewsMs;
    double overlayMs;
    double rendererEndMs;
    double frameEndMs;
    double c3dProcessingMs;
    double previousGpuDrawingMs;
    double cmdBufUsagePercent;
} N3DSRenderPerfSample;

typedef struct {
    bool active;
    bool truncated;
    uint32_t eventId;
    uint32_t postFramesRemaining;
    uint32_t preCount;
    uint32_t preWrite;
    uint32_t sampleCount;
    N3DSDustPerfFrame preFrames[N3DS_DUST_PERF_PRE_FRAMES];
    N3DSDustPerfFrame samples[N3DS_DUST_PERF_MAX_FRAMES];
} N3DSDustPerfSampler;

static N3DSDustPerfSampler gN3DSDustPerfSampler;
static N3DSDustPerfSampler gN3DSAttackPerfSampler;
static N3DSDustPerfSampler gN3DSBattleEntryPerfSampler;

typedef struct {
    N3DSDustPerfSampler sampler;
    char eventName[16];
    char activePhaseName[16];
} N3DSPerfPendingEvent;

static N3DSPerfPendingEvent gN3DSPerfPendingEvents[N3DS_PERF_PENDING_EVENT_CAP];
static uint32_t gN3DSPerfPendingEventCount;
static bool gN3DSPerfBattleRoomActive;

typedef struct {
    bool touchEnabled;
    int32_t tapKeyCode;
    uint32_t tapMaxMs;
    int32_t tapMaxMovePx;
    int32_t dragDeadzonePx;
    bool battleMenuTouchEnabled;
    bool battleFollowEnabled;
    char battleFollowTargetToken[64];
    int32_t battleFollowTargetObjectIndex;
    char battleFollowActiveSelfVar[64];
    int32_t battleFollowActiveGreaterThan;
    int32_t battleFollowTolerance;
    uint32_t battleFollowActivationMs;
    float battleFollowStepHint;
} N3DSInputProfile;

typedef struct {
    bool active;
    bool tapCancelled;
    touchPosition start;
    touchPosition current;
    u64 startMs;
    int32_t maxMoveSquared;
    uint8_t tapPulseFrames;
} N3DSTouchState;

typedef struct {
    bool engaged;
    bool havePreviousSample;
    float previousX;
    float previousY;
    float estimatedStepX;
    float estimatedStepY;
    bool lastCommandX;
    bool lastCommandY;
    int32_t lastDirectionX;
    int32_t lastDirectionY;
    int32_t majorAxis;
    float minorPhase;
} N3DSBattleFollowController;

typedef enum {
    N3DS_BATTLE_MENU_TARGET_NONE = 0,
    N3DS_BATTLE_MENU_TARGET_TOP = 1,
    N3DS_BATTLE_MENU_TARGET_MONSTER = 2,
    N3DS_BATTLE_MENU_TARGET_ACT = 3,
    N3DS_BATTLE_MENU_TARGET_ITEM = 4,
    N3DS_BATTLE_MENU_TARGET_MERCY = 5,
    N3DS_BATTLE_MENU_TARGET_SPECIAL = 6
} N3DSBattleMenuTargetKind;

typedef struct {
    bool active;
    bool releaseFrame;
    N3DSBattleMenuTargetKind kind;
    int32_t modeCode;
    int32_t targetIndex;
    uint32_t pulseCount;
} N3DSBattleMenuTouchController;

typedef enum {
    N3DS_BATTLE_MENU_TOUCH_PASS_THROUGH = 0,
    N3DS_BATTLE_MENU_TOUCH_CONSUMED = 1,
    N3DS_BATTLE_MENU_TOUCH_ACTION = 2,
    N3DS_BATTLE_MENU_TOUCH_CANCEL = 3
} N3DSBattleMenuTouchResult;

static int32_t N3DSBattleMenu_modeCode(float menuNo) {
    return (int32_t) lroundf(menuNo * 2.0f);
}

static void N3DSBattleMenuTouchController_reset(N3DSBattleMenuTouchController* controller) {
    if (controller == NULL) return;
    memset(controller, 0, sizeof(*controller));
}

static bool N3DSBattleMenu_pointInRect(float x, float y, const N3DSBattleMenuButtonRect* rect) {
    return rect != NULL && rect->present && x >= rect->left && x <= rect->right &&
        y >= rect->top && y <= rect->bottom;
}

static bool N3DSBattleMenu_pointInRows(
    const N3DSBattleMenuState* state,
    float x,
    float y,
    float firstY,
    int32_t rowCount
) {
    if (state == NULL || rowCount <= 0) return false;
    float left = state->idealBorder[0];
    float right = state->idealBorder[1];
    if (right <= left) return false;
    float top = firstY - 16.0f;
    float bottom = firstY + (float) (rowCount - 1) * 32.0f + 16.0f;
    return x >= left && x <= right && y >= top && y <= bottom;
}

static int32_t N3DSBattleMenu_nearestRow(float y, float firstY, int32_t rowCount) {
    if (rowCount <= 0) return -1;
    int32_t row = (int32_t) floorf(((y - firstY) + 16.0f) / 32.0f);
    if (row < 0) row = 0;
    if (row >= rowCount) row = rowCount - 1;
    return row;
}

static N3DSBattleMenuTouchResult N3DSBattleMenuTouchController_capture(
    N3DSBattleMenuTouchController* controller,
    const N3DSBattleMenuState* state,
    float gameX,
    float gameY
) {
    if (controller == NULL || state == NULL || !state->battleActive || state->dodgingBullets) {
        return N3DS_BATTLE_MENU_TOUCH_PASS_THROUGH;
    }
    N3DSBattleMenuTouchController_reset(controller);

    float borderLeft = state->idealBorder[0];
    float borderTop = state->idealBorder[2];

    // myfight==3 is still a modal battle choice. Own every tap while it is
    // visible so blank-space taps cannot leak through as a generic Z press.
    if (state->myFight == 3) {
        float firstY = borderTop + 92.0f;
        if (!N3DSBattleMenu_pointInRows(state, gameX, gameY, firstY, 1)) {
            return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
        }
        float splitX = borderLeft + 158.0f;
        controller->active = true;
        controller->kind = N3DS_BATTLE_MENU_TARGET_SPECIAL;
        controller->targetIndex = gameX < splitX ? 0 : 1;
        return N3DS_BATTLE_MENU_TOUCH_ACTION;
    }

    // Attack timing, dialogue/turn transitions, and other non-menu phases
    // keep the existing generic tap semantics.
    if (state->myFight != 0 || state->mnFight != 0) {
        return N3DS_BATTLE_MENU_TOUCH_PASS_THROUGH;
    }

    int32_t modeCode = N3DSBattleMenu_modeCode(state->menuNo);
    bool menuActive =
        modeCode == 0 ||
        modeCode == 2 ||
        modeCode == 4 ||
        modeCode == 6 ||
        modeCode == 7 ||
        modeCode == 8 ||
        modeCode == 20 ||
        modeCode == 22;
    if (!menuActive) return N3DS_BATTLE_MENU_TOUCH_PASS_THROUGH;

    controller->modeCode = modeCode;

    // Inside a submenu, tapping one of the four persistent battle buttons is
    // UNDERTALE's normal cancel action. Use X/SHIFT rather than rewriting
    // bmenuno so nested 10->2 and 11->3 behavior remains game-owned.
    if (modeCode != 0) {
        repeat(4, i) {
            if (N3DSBattleMenu_pointInRect(gameX, gameY, &state->topButtons[i])) {
                return N3DS_BATTLE_MENU_TOUCH_CANCEL;
            }
        }
    }

    if (modeCode == 0) {
        repeat(4, i) {
            if (!N3DSBattleMenu_pointInRect(gameX, gameY, &state->topButtons[i])) continue;
            controller->active = true;
            controller->kind = N3DS_BATTLE_MENU_TARGET_TOP;
            controller->targetIndex = i;
            return N3DS_BATTLE_MENU_TOUCH_ACTION;
        }
        return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
    }

    if (modeCode == 2 || modeCode == 4 || modeCode == 22) {
        float firstY = borderTop + 28.0f;
        if (!N3DSBattleMenu_pointInRows(state, gameX, gameY, firstY, 3)) {
            return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
        }
        int32_t row = N3DSBattleMenu_nearestRow(gameY, firstY, 3);
        if (row < 0 || row >= 3 || state->monsterPresent[row] == 0) {
            return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
        }
        controller->active = true;
        controller->kind = N3DS_BATTLE_MENU_TARGET_MONSTER;
        controller->targetIndex = row;
        return N3DS_BATTLE_MENU_TOUCH_ACTION;
    }

    if (modeCode == 20) {
        float firstY = borderTop + 28.0f;
        if (!N3DSBattleMenu_pointInRows(state, gameX, gameY, firstY, 3)) {
            return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
        }
        int32_t row = N3DSBattleMenu_nearestRow(gameY, firstY, 3);
        int32_t col = gameX < borderLeft + 162.0f ? 0 : 1;
        int32_t index = col * 3 + row;
        if (index < 0 || index >= 6 || state->choicePresent[index] == 0) {
            return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
        }
        controller->active = true;
        controller->kind = N3DS_BATTLE_MENU_TARGET_ACT;
        controller->targetIndex = index;
        return N3DS_BATTLE_MENU_TOUCH_ACTION;
    }

    if (modeCode == 6 || modeCode == 7) {
        float firstY = borderTop + 28.0f;
        if (!N3DSBattleMenu_pointInRows(state, gameX, gameY, firstY, 2)) {
            return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
        }
        int32_t row = N3DSBattleMenu_nearestRow(gameY, firstY, 2);
        int32_t col = gameX < borderLeft + 156.0f ? 0 : 1;
        int32_t index = row * 2 + col;
        int32_t pageOffset = modeCode == 7 ? 4 : 0;
        if (index < 0 || index >= 4 || state->itemPresent[pageOffset + index] == 0) {
            return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
        }
        controller->active = true;
        controller->kind = N3DS_BATTLE_MENU_TARGET_ITEM;
        controller->targetIndex = index;
        return N3DS_BATTLE_MENU_TOUCH_ACTION;
    }

    if (modeCode == 8) {
        float firstY = borderTop + 28.0f;
        if (!N3DSBattleMenu_pointInRows(state, gameX, gameY, firstY, 2)) {
            return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
        }
        controller->active = true;
        controller->kind = N3DS_BATTLE_MENU_TARGET_MERCY;
        controller->targetIndex = N3DSBattleMenu_nearestRow(gameY, firstY, 2);
        return N3DS_BATTLE_MENU_TOUCH_ACTION;
    }

    return N3DS_BATTLE_MENU_TOUCH_CONSUMED;
}
static void N3DSBattleMenuTouchController_step(
    N3DSBattleMenuTouchController* controller,
    Runner* runner,
    bool* outLeft,
    bool* outRight,
    bool* outUp,
    bool* outDown,
    bool* outConfirm
) {
    if (outLeft != NULL) *outLeft = false;
    if (outRight != NULL) *outRight = false;
    if (outUp != NULL) *outUp = false;
    if (outDown != NULL) *outDown = false;
    if (outConfirm != NULL) *outConfirm = false;
    if (controller == NULL || !controller->active || runner == NULL) return;

    if (controller->releaseFrame) {
        controller->releaseFrame = false;
        return;
    }
    if (controller->pulseCount >= 24u) {
        N3DSBattleMenuTouchController_reset(controller);
        return;
    }

    N3DSBattleMenuState state;
    if (!Runner_get3DSBattleMenuState(runner, &state) || state.dodgingBullets) {
        N3DSBattleMenuTouchController_reset(controller);
        return;
    }

    int32_t modeCode = N3DSBattleMenu_modeCode(state.menuNo);
    int32_t current = -1;
    int32_t command = 0;

    switch (controller->kind) {
        case N3DS_BATTLE_MENU_TARGET_TOP: {
            if (state.myFight != 0 || state.mnFight != 0 || modeCode != 0) break;
            current = state.menuCoord[0];
            if (current == controller->targetIndex) {
                if (outConfirm != NULL) *outConfirm = true;
                N3DSBattleMenuTouchController_reset(controller);
                return;
            }
            int32_t rightDistance = (controller->targetIndex - current + 4) % 4;
            command = rightDistance > 0 && rightDistance <= 2 ? 2 : 1;
            break;
        }
        case N3DS_BATTLE_MENU_TARGET_MONSTER:
            if (state.myFight != 0 || state.mnFight != 0 || modeCode != controller->modeCode) break;
            current = state.menuCoord[1];
            if (current == controller->targetIndex) {
                if (outConfirm != NULL) *outConfirm = true;
                N3DSBattleMenuTouchController_reset(controller);
                return;
            }
            command = controller->targetIndex < current ? 3 : 4;
            break;
        case N3DS_BATTLE_MENU_TARGET_ACT: {
            if (state.myFight != 0 || state.mnFight != 0 || modeCode != 20) break;
            current = state.menuCoord[2];
            if (current == controller->targetIndex) {
                if (outConfirm != NULL) *outConfirm = true;
                N3DSBattleMenuTouchController_reset(controller);
                return;
            }
            int32_t currentCol = current >= 3 ? 1 : 0;
            int32_t targetCol = controller->targetIndex >= 3 ? 1 : 0;
            int32_t currentRow = current % 3;
            int32_t targetRow = controller->targetIndex % 3;
            if (currentCol != targetCol) command = targetCol > currentCol ? 2 : 1;
            else if (currentRow != targetRow) command = targetRow < currentRow ? 3 : 4;
            break;
        }
        case N3DS_BATTLE_MENU_TARGET_ITEM: {
            if (state.myFight != 0 || state.mnFight != 0 || (modeCode != 6 && modeCode != 7)) break;
            current = state.menuCoord[3];
            if (modeCode != controller->modeCode) {
                command = controller->modeCode == 7 ? 2 : 1;
                break;
            }
            if (current == controller->targetIndex) {
                if (outConfirm != NULL) *outConfirm = true;
                N3DSBattleMenuTouchController_reset(controller);
                return;
            }
            int32_t currentRow = current / 2;
            int32_t targetRow = controller->targetIndex / 2;
            int32_t currentCol = current % 2;
            int32_t targetCol = controller->targetIndex % 2;
            if (currentRow != targetRow) command = targetRow < currentRow ? 3 : 4;
            else if (currentCol != targetCol) command = targetCol < currentCol ? 1 : 2;
            break;
        }
        case N3DS_BATTLE_MENU_TARGET_MERCY:
            if (state.myFight != 0 || state.mnFight != 0 || modeCode != 8) break;
            current = state.menuCoord[4];
            if (current == controller->targetIndex) {
                if (outConfirm != NULL) *outConfirm = true;
                N3DSBattleMenuTouchController_reset(controller);
                return;
            }
            command = controller->targetIndex < current ? 3 : 4;
            break;
        case N3DS_BATTLE_MENU_TARGET_SPECIAL:
            if (state.myFight != 3) break;
            current = state.menuCoord[6];
            if (current == controller->targetIndex) {
                if (outConfirm != NULL) *outConfirm = true;
                N3DSBattleMenuTouchController_reset(controller);
                return;
            }
            command = controller->targetIndex < current ? 1 : 2;
            break;
        default:
            break;
    }

    if (command == 0) {
        N3DSBattleMenuTouchController_reset(controller);
        return;
    }

    if (command == 1 && outLeft != NULL) *outLeft = true;
    else if (command == 2 && outRight != NULL) *outRight = true;
    else if (command == 3 && outUp != NULL) *outUp = true;
    else if (command == 4 && outDown != NULL) *outDown = true;

    controller->pulseCount++;
    controller->releaseFrame = true;
}

static int32_t N3DS_signFloat(float value) {
    if (value > 0.0f) return 1;
    if (value < 0.0f) return -1;
    return 0;
}

static void N3DSBattleFollowController_reset(N3DSBattleFollowController* controller, float stepHint) {
    if (controller == NULL) return;
    memset(controller, 0, sizeof(*controller));
    if (!isfinite(stepHint) || stepHint <= 0.0f) stepHint = 4.0f;
    controller->estimatedStepX = stepHint;
    controller->estimatedStepY = stepHint;
    controller->minorPhase = 0.5f;
}

static void N3DSBattleFollowController_observe(
    N3DSBattleFollowController* controller,
    float currentX,
    float currentY
) {
    if (controller == NULL) return;

    if (controller->havePreviousSample) {
        float deltaX = currentX - controller->previousX;
        float deltaY = currentY - controller->previousY;
        if (controller->lastCommandX && controller->lastDirectionX != 0 &&
            deltaX * (float) controller->lastDirectionX > 0.05f) {
            float observed = fabsf(deltaX);
            if (observed >= 0.25f && observed <= 16.0f) {
                controller->estimatedStepX = controller->estimatedStepX * 0.75f + observed * 0.25f;
            }
        }
        if (controller->lastCommandY && controller->lastDirectionY != 0 &&
            deltaY * (float) controller->lastDirectionY > 0.05f) {
            float observed = fabsf(deltaY);
            if (observed >= 0.25f && observed <= 16.0f) {
                controller->estimatedStepY = controller->estimatedStepY * 0.75f + observed * 0.25f;
            }
        }
    }

    controller->previousX = currentX;
    controller->previousY = currentY;
    controller->havePreviousSample = true;
}

static bool N3DSBattleFollowController_axisShouldMove(float error, float estimatedStep, float tolerance) {
    float absError = fabsf(error);
    if (!isfinite(absError) || absError <= 0.0f) return false;
    if (!isfinite(estimatedStep) || estimatedStep < 0.25f) estimatedStep = 4.0f;
    if (!isfinite(tolerance) || tolerance < 0.5f) tolerance = 0.5f;

    // A digital input can only move in whole per-frame steps.  Stop at the
    // closest reachable point instead of overshooting and correcting forever.
    float arrivalWindow = fminf(tolerance, fmaxf(0.75f, estimatedStep * 0.5f));
    if (absError <= arrivalWindow) return false;

    float projectedError = fabsf(absError - estimatedStep);
    return projectedError + 0.05f < absError;
}

static void N3DSBattleFollowController_compute(
    N3DSBattleFollowController* controller,
    float activity,
    float currentX,
    float currentY,
    float targetX,
    float targetY,
    float tolerance,
    bool* outLeft,
    bool* outRight,
    bool* outUp,
    bool* outDown
) {
    if (controller == NULL || outLeft == NULL || outRight == NULL || outUp == NULL || outDown == NULL) return;

    *outLeft = false;
    *outRight = false;
    *outUp = false;
    *outDown = false;

    N3DSBattleFollowController_observe(controller, currentX, currentY);

    float errorX = targetX - currentX;
    float errorY = targetY - currentY;
    int32_t directionX = N3DS_signFloat(errorX);
    int32_t directionY = N3DS_signFloat(errorY);
    bool canMoveX = directionX != 0 && N3DSBattleFollowController_axisShouldMove(
        errorX,
        controller->estimatedStepX,
        tolerance
    );
    bool canMoveY = directionY != 0 && N3DSBattleFollowController_axisShouldMove(
        errorY,
        controller->estimatedStepY,
        tolerance
    );

    bool commandX = false;
    bool commandY = false;

    // movement==1 is UNDERTALE's free red-soul movement.  For gravity/special
    // modes preserve continuous digital semantics rather than pulse keys that
    // may have jump/guard meaning.
    if (fabsf(activity - 1.0f) > 0.25f) {
        commandX = canMoveX;
        commandY = canMoveY;
    } else if (canMoveX || canMoveY) {
        float framesX = canMoveX ? fabsf(errorX) / fmaxf(controller->estimatedStepX, 0.25f) : 0.0f;
        float framesY = canMoveY ? fabsf(errorY) / fmaxf(controller->estimatedStepY, 0.25f) : 0.0f;
        int32_t majorAxis = framesX >= framesY ? 1 : 2;
        if (!canMoveX) majorAxis = 2;
        if (!canMoveY) majorAxis = 1;

        if (controller->majorAxis != majorAxis) {
            controller->majorAxis = majorAxis;
            controller->minorPhase = 0.5f;
        }

        if (majorAxis == 1) {
            commandX = canMoveX;
            if (canMoveY && framesX > 0.0001f) {
                float ratio = fminf(1.0f, framesY / framesX);
                controller->minorPhase += ratio;
                if (controller->minorPhase >= 1.0f) {
                    commandY = true;
                    controller->minorPhase -= 1.0f;
                }
            }
        } else {
            commandY = canMoveY;
            if (canMoveX && framesY > 0.0001f) {
                float ratio = fminf(1.0f, framesX / framesY);
                controller->minorPhase += ratio;
                if (controller->minorPhase >= 1.0f) {
                    commandX = true;
                    controller->minorPhase -= 1.0f;
                }
            }
        }
    }

    if (commandX) {
        if (directionX < 0) *outLeft = true;
        else if (directionX > 0) *outRight = true;
    }
    if (commandY) {
        if (directionY < 0) *outUp = true;
        else if (directionY > 0) *outDown = true;
    }

    controller->lastCommandX = commandX;
    controller->lastCommandY = commandY;
    controller->lastDirectionX = commandX ? directionX : 0;
    controller->lastDirectionY = commandY ? directionY : 0;
}

static void N3DSInputProfile_setDefaults(N3DSInputProfile* profile) {
    if (profile == NULL) return;
    memset(profile, 0, sizeof(*profile));
    profile->tapKeyCode = 'Z';
    profile->tapMaxMs = 180u;
    profile->tapMaxMovePx = 10;
    profile->dragDeadzonePx = 18;
    profile->battleFollowTargetObjectIndex = -1;
    profile->battleFollowTolerance = 4;
    profile->battleFollowActivationMs = 50u;
    profile->battleFollowStepHint = 4.0f;
}

static void N3DSInputProfile_load(N3DSInputProfile* profile, const char* path) {
    if (profile == NULL) return;
    N3DSInputProfile_setDefaults(profile);
    if (path == NULL) return;

    FILE* file = fopen(path, "rb");
    if (file == NULL) return;

    char line[160];
    while (fgets(line, sizeof(line), file) != NULL) {
        char key[64];
        char valueText[80];
        if (sscanf(line, " %63[^=]=%79[^\r\n]", key, valueText) != 2) continue;
        int value = atoi(valueText);
        if (strcmp(key, "touch_enabled") == 0) profile->touchEnabled = value != 0;
        else if (strcmp(key, "tap_key_code") == 0) profile->tapKeyCode = value;
        else if (strcmp(key, "tap_max_ms") == 0 && value > 0) profile->tapMaxMs = (uint32_t) value;
        else if (strcmp(key, "tap_max_move_px") == 0 && value > 0) profile->tapMaxMovePx = value;
        else if (strcmp(key, "drag_deadzone_px") == 0 && value > 0) profile->dragDeadzonePx = value;
        else if (strcmp(key, "battle_menu_touch_enabled") == 0) profile->battleMenuTouchEnabled = value != 0;
        else if (strcmp(key, "battle_follow_enabled") == 0) profile->battleFollowEnabled = value != 0;
        else if (strcmp(key, "battle_follow_target_token") == 0) {
            snprintf(profile->battleFollowTargetToken, sizeof(profile->battleFollowTargetToken), "%s", valueText);
        }
        else if (strcmp(key, "battle_follow_target_object_index") == 0) profile->battleFollowTargetObjectIndex = value;
        else if (strcmp(key, "battle_follow_active_self_var") == 0) {
            snprintf(profile->battleFollowActiveSelfVar, sizeof(profile->battleFollowActiveSelfVar), "%s", valueText);
        }
        else if (strcmp(key, "battle_follow_active_greater_than") == 0) profile->battleFollowActiveGreaterThan = value;
        else if (strcmp(key, "battle_follow_tolerance") == 0 && value > 0) profile->battleFollowTolerance = value;
        else if (strcmp(key, "battle_follow_activation_ms") == 0 && value >= 0) profile->battleFollowActivationMs = (uint32_t) value;
        else if (strcmp(key, "battle_follow_step_hint") == 0 && value > 0) profile->battleFollowStepHint = (float) value;
    }
    fclose(file);

    if (profile->tapKeyCode < 0 || profile->tapKeyCode > 255) profile->tapKeyCode = 'Z';
    if (profile->tapMaxMs < 40u || profile->tapMaxMs > 1000u) profile->tapMaxMs = 180u;
    if (profile->tapMaxMovePx < 2 || profile->tapMaxMovePx > 64) profile->tapMaxMovePx = 10;
    if (profile->dragDeadzonePx < profile->tapMaxMovePx || profile->dragDeadzonePx > 120) profile->dragDeadzonePx = 18;
    if (profile->battleFollowTolerance < 1 || profile->battleFollowTolerance > 64) profile->battleFollowTolerance = 4;
    if (profile->battleFollowActivationMs > 500u) profile->battleFollowActivationMs = 50u;
    if (!isfinite(profile->battleFollowStepHint) || profile->battleFollowStepHint < 0.25f || profile->battleFollowStepHint > 16.0f) {
        profile->battleFollowStepHint = 4.0f;
    }
    if (profile->battleFollowTargetToken[0] == '\0') profile->battleFollowEnabled = false;
}

static void N3DSTouchState_update(
    N3DSTouchState* state,
    const N3DSInputProfile* profile,
    u32 held,
    u32 down,
    u32 up
) {
    if (state == NULL || profile == NULL || !profile->touchEnabled) return;

    u64 nowMs = osGetTime();
    if (down & KEY_TOUCH) {
        memset(state, 0, sizeof(*state));
        state->active = true;
        state->startMs = nowMs;
        hidTouchRead(&state->start);
        state->current = state->start;
    }

    if (state->active && (held & KEY_TOUCH)) {
        hidTouchRead(&state->current);
        int32_t dx = (int32_t) state->current.px - (int32_t) state->start.px;
        int32_t dy = (int32_t) state->current.py - (int32_t) state->start.py;
        int32_t moveSquared = dx * dx + dy * dy;
        if (moveSquared > state->maxMoveSquared) state->maxMoveSquared = moveSquared;

        int32_t tapMoveSquared = profile->tapMaxMovePx * profile->tapMaxMovePx;
        if (state->maxMoveSquared > tapMoveSquared || nowMs - state->startMs > profile->tapMaxMs) {
            state->tapCancelled = true;
        }
    }

    if (state->active && (up & KEY_TOUCH)) {
        u64 elapsedMs = nowMs - state->startMs;
        int32_t tapMoveSquared = profile->tapMaxMovePx * profile->tapMaxMovePx;
        if (!state->tapCancelled && elapsedMs <= profile->tapMaxMs && state->maxMoveSquared <= tapMoveSquared) {
            state->tapPulseFrames = 1u;
        }
        state->active = false;
    }
}

static bool fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void N3DS_formatDebugSize(char* out, size_t outSize, uint32_t bytes) {
    if (out == NULL || outSize == 0) return;

    double kib = (double) bytes / 1024.0;
    if (kib < 1024.0) {
        snprintf(out, outSize, "%.0f KB", kib);
        return;
    }

    snprintf(out, outSize, "%.2f MB", kib / 1024.0);
}

static double N3DSPerf_ticksToMs(u64 ticks) {
    return (double) ticks * 1000.0 / (double) SYSCLOCK_ARM11;
}

static Instance* N3DSDustPerf_findTargetChoice(Runner* runner) {
    if (runner == NULL || runner->dataWin == NULL || runner->instances == NULL) return NULL;

    repeat(arrlen(runner->instances), i) {
        Instance* inst = runner->instances[i];
        if (inst == NULL || inst->destroyed || !inst->active) continue;
        if (inst->objectIndex < 0 || (uint32_t) inst->objectIndex >= runner->dataWin->objt.count) continue;

        const char* objectName = runner->dataWin->objt.objects[inst->objectIndex].name;
        if (objectName != NULL && strstr(objectName, "targetchoice") != NULL) return inst;
    }
    return NULL;
}

static N3DSDustPerfFrame N3DSDustPerf_captureFrame(
    Runner* runner,
    Renderer* renderer,
    double stepMs,
    double battlePrewarmTickMs,
    bool battleTransitionPrewarmActive,
    const N3DSRenderPerfSample* renderPerf
) {
    N3DSDustPerfFrame sample;
    memset(&sample, 0, sizeof(sample));
    sample.frame = runner != NULL ? runner->frameCount : -1;
    sample.atlasImportObjectIndex = -1;
    sample.atlasImportInstanceSpriteIndex = -1;
    sample.atlasImportInstanceSubimg = -1;
    sample.atlasImportInstanceTPAGIndex = -1;
    sample.atlasImportTriggerTPAGIndex = -1;
    sample.atlasImportTriggerSpriteIndex = -1;
    sample.atlasImportTriggerFrameIndex = -1;
    sample.directLastLoadIndex = -1;
    sample.directLastLoadFrame = -1;
    sample.targetChoiceObjectIndex = -1;
    sample.targetChoiceSpriteIndex = -1;
    sample.targetChoiceSubimg = -1;
    sample.targetChoiceTPAGIndex = -1;
    sample.myFight = -1;
    sample.mnFight = -1;
    sample.roomFromIndex = -1;
    sample.roomToIndex = -1;
    sample.battlePrewarmTargetRoomIndex = -1;
    sample.battlePrewarmStageStart = -1;
    sample.battlePrewarmStageEnd = -1;
    sample.battlePrewarmAssetStage = -1;
    sample.dustCount = Runner_get3DSObjectInstanceCountByName(runner, "obj_whtpxlgrav");
    if (runner != NULL) {
        sample.instancesCreated = runner->frameInstancesCreated;
        sample.instancesDestroyed = runner->frameInstancesDestroyed;
        sample.drawInstances = runner->frameDrawInstances;
        sample.stepMs = stepMs;
        sample.prepMs = runner->frameStepPrepMs;
        sample.eventMs = runner->frameStepEventMs;
        sample.motionMs = runner->frameStepMotionMs;
        sample.collisionMs = runner->frameStepCollisionMs;
        sample.cleanupMs = runner->frameStepCleanupMs;
        sample.finalizeMs = runner->frameStepFinalizeMs;
        sample.drawInstancesMs = runner->frameDrawInstancesMs;
        sample.battleReplayMs = runner->frameDrawBattleReplayMs;
        sample.battlePrewarmTickMs = battlePrewarmTickMs;
        sample.battleTransitionPrewarmActive = battleTransitionPrewarmActive ? 1 : 0;
        N3DSBattlePrewarmTickDiag prewarmTickDiag;
        if (N3DSRenderer_getLastBattlePrewarmTickDiag(renderer, &prewarmTickDiag)) {
            sample.battlePrewarmTargetKind = (uint32_t) prewarmTickDiag.targetKind;
            sample.battlePrewarmTargetRoomIndex = prewarmTickDiag.targetRoomIndex;
            sample.battlePrewarmStageStart = prewarmTickDiag.stageStart;
            sample.battlePrewarmStageEnd = prewarmTickDiag.stageEnd;
            sample.battlePrewarmAssetStage = prewarmTickDiag.assetStage;
            sample.battlePrewarmJobBytesCompleted = prewarmTickDiag.jobBytesCompleted;
            sample.battlePrewarmJobBytesTotal = prewarmTickDiag.jobBytesTotal;
            sample.battlePrewarmTickChunks = prewarmTickDiag.chunksReadThisTick;
            sample.battlePrewarmTickBytes = prewarmTickDiag.bytesReadThisTick;
        }
        if (renderPerf != NULL) {
            sample.renderMs = renderPerf->totalMs;
            sample.frameBeginMs = renderPerf->frameBeginMs;
            sample.rendererBeginMs = renderPerf->rendererBeginMs;
            sample.drawViewsMs = renderPerf->drawViewsMs;
            sample.overlayMs = renderPerf->overlayMs;
            sample.rendererEndMs = renderPerf->rendererEndMs;
            sample.frameEndMs = renderPerf->frameEndMs;
            sample.c3dProcessingMs = renderPerf->c3dProcessingMs;
            sample.previousGpuDrawingMs = renderPerf->previousGpuDrawingMs;
            sample.cmdBufUsagePercent = renderPerf->cmdBufUsagePercent;
        }
        sample.topStepObjectIndex = runner->frameStepTopObjectIndex;
        sample.topStepObjectMs = runner->frameStepTopObjectMs;
        sample.topStepObjectCalls = runner->frameStepTopObjectCalls;

        if (runner->frameRoomTransitioned) {
            sample.roomTransitioned = 1;
            sample.roomFromIndex = runner->frameRoomTransitionFrom;
            sample.roomToIndex = runner->frameRoomTransitionTo;
            sample.roomTransitionMs = runner->frameRoomTransitionMs;
            sample.roomInitMs = runner->frameRoomInitMs;
            sample.roomStartMs = runner->frameRoomStartMs;
            sample.rendererPrewarmMs = runner->frameRendererPrewarmMs;
            sample.audioPrewarmMs = runner->frameAudioPrewarmMs;

            N3DSRoomPrewarmPerf prewarmPerf;
            if (N3DSRenderer_getLastRoomPrewarmPerf(renderer, &prewarmPerf) &&
                prewarmPerf.roomIndex == runner->frameRoomTransitionTo) {
                sample.prewarmBattleRoom = prewarmPerf.battleRoom ? 1 : 0;
                sample.prewarmTotalMs = prewarmPerf.totalMs;
                sample.prewarmClearPinsMs = prewarmPerf.clearPinsMs;
                sample.prewarmFlushDirectMs = prewarmPerf.flushDirectMs;
                sample.prewarmBattleDirectMs = prewarmPerf.battleDirectMs;
                sample.prewarmBattleDirectReads = prewarmPerf.battleDirectReads;
                sample.prewarmBattleDirectReadBytes = prewarmPerf.battleDirectReadBytes;
                sample.prewarmJapaneseFontsMs = prewarmPerf.japaneseFontsMs;
                sample.prewarmJapaneseFontReads = prewarmPerf.japaneseFontReads;
                sample.prewarmJapaneseFontReadBytes = prewarmPerf.japaneseFontReadBytes;
                sample.prewarmRoomAtlasMs = prewarmPerf.roomAtlasMs;
                sample.prewarmRoomAtlasReads = prewarmPerf.roomAtlasReads;
                sample.prewarmRoomAtlasReadBytes = prewarmPerf.roomAtlasReadBytes;
                sample.prewarmRoomAtlasReadPageCount = prewarmPerf.roomAtlasReadPageCount;
                repeat(sample.prewarmRoomAtlasReadPageCount, pageIndex) {
                    sample.prewarmRoomAtlasReadPages[pageIndex] = prewarmPerf.roomAtlasReadPages[pageIndex];
                }
                sample.prewarmRoomAtlasReadPagesTruncated = prewarmPerf.roomAtlasReadPagesTruncated ? 1 : 0;
                sample.prewarmBattleCriticalMs = prewarmPerf.battleCriticalMs;
                sample.prewarmBattleCriticalReads = prewarmPerf.battleCriticalReads;
                sample.prewarmBattleCriticalReadBytes = prewarmPerf.battleCriticalReadBytes;
                sample.prewarmBattleCriticalReadPageCount = prewarmPerf.battleCriticalReadPageCount;
                repeat(sample.prewarmBattleCriticalReadPageCount, pageIndex) {
                    sample.prewarmBattleCriticalReadPages[pageIndex] = prewarmPerf.battleCriticalReadPages[pageIndex];
                }
                sample.prewarmBattleCriticalReadPagesTruncated = prewarmPerf.battleCriticalReadPagesTruncated ? 1 : 0;
                sample.battleManifestLoaded = prewarmPerf.battleManifestLoaded ? 1 : 0;
                sample.battleManifestAttempted = prewarmPerf.battleManifestAttempted ? 1 : 0;
                sample.battleManifestFound = prewarmPerf.battleManifestFound ? 1 : 0;
                sample.battleManifestPageCount = prewarmPerf.battleManifestPageCount;
                sample.battleManifestColdReads = prewarmPerf.battleManifestColdReads;
                sample.battleManifestReadBytes = prewarmPerf.battleManifestReadBytes;
                sample.prewarmAtlasCacheBytesBefore = prewarmPerf.atlasCacheBytesBefore;
                sample.prewarmAtlasCacheBytesAfter = prewarmPerf.atlasCacheBytesAfter;
                sample.prewarmDirectCacheBytesBefore = prewarmPerf.directCacheBytesBefore;
                sample.prewarmDirectCacheBytesAfter = prewarmPerf.directCacheBytesAfter;
                sample.prewarmDirectVRAMBytesBefore = prewarmPerf.directVRAMBytesBefore;
                sample.prewarmDirectVRAMBytesAfterFlush = prewarmPerf.directVRAMBytesAfterFlush;
                sample.prewarmDirectVRAMBytesAfter = prewarmPerf.directVRAMBytesAfter;
            }

            N3DSAudioRoomPrewarmPerf audioPrewarmPerf;
            if (N3DSAudioSystem_getLastRoomPrewarmPerf(runner->audioSystem, &audioPrewarmPerf)) {
                sample.audioPrewarmWorkerEnabled = audioPrewarmPerf.workerEnabled ? 1 : 0;
                sample.audioPrewarmCacheBytesBefore = audioPrewarmPerf.cachedBytesBefore;
                sample.audioPrewarmCacheBytesAfter = audioPrewarmPerf.cachedBytesAfter;
                sample.audioPrewarmPendingSounds = audioPrewarmPerf.pendingSoundCountAfter;
                sample.audioPrewarmPendingBytes = audioPrewarmPerf.pendingByteBudgetAfter;
            }
        }

        sample.atlasImportObjectIndex = runner->frameDrawTopImportObjectIndex;
        sample.atlasImportInstanceId = runner->frameDrawTopImportInstanceId;
        sample.atlasImportInstanceSpriteIndex = runner->frameDrawTopImportInstanceSpriteIndex;
        sample.atlasImportInstanceSubimg = runner->frameDrawTopImportInstanceSubimg;
        sample.atlasImportInstanceTPAGIndex = runner->frameDrawTopImportInstanceTPAGIndex;
        sample.atlasImportTriggerTPAGIndex = runner->frameDrawTopImportTriggerTPAGIndex;
        sample.atlasImportTriggerSpriteIndex = N3DSRenderer_getSpriteIndexForTPAG(renderer, sample.atlasImportTriggerTPAGIndex);
        sample.atlasImportTriggerFrameIndex = N3DSRenderer_getSpriteFrameIndexForTPAG(renderer, sample.atlasImportTriggerTPAGIndex);
        sample.atlasImportCount = runner->frameDrawTopImportCount;
        sample.atlasImportEvictions = runner->frameDrawTopImportEvictions;
        sample.atlasImportPass = runner->frameDrawTopImportPass;
        sample.atlasImportPageCount = runner->frameDrawTopImportPageCount;
        if (sample.atlasImportPageCount > N3DS_PERF_IMPORT_PAGE_CAP) sample.atlasImportPageCount = N3DS_PERF_IMPORT_PAGE_CAP;
        repeat(sample.atlasImportPageCount, pageIndex) sample.atlasImportPages[pageIndex] = runner->frameDrawTopImportPages[pageIndex];

        N3DSBattleMenuState battleState;
        if (Runner_get3DSBattleMenuState(runner, &battleState)) {
            sample.battleActive = battleState.battleActive ? 1 : 0;
            sample.myFight = battleState.myFight;
            sample.mnFight = battleState.mnFight;
        }

        Instance* targetChoice = N3DSDustPerf_findTargetChoice(runner);
        if (targetChoice != NULL) {
            sample.targetChoiceObjectIndex = targetChoice->objectIndex;
            sample.targetChoiceInstanceId = targetChoice->instanceId;
            sample.targetChoiceSpriteIndex = targetChoice->spriteIndex;
            sample.targetChoiceImageIndex = targetChoice->imageIndex;
            sample.targetChoiceImageSpeed = targetChoice->imageSpeed;
            if (targetChoice->spriteIndex >= 0 &&
                (uint32_t) targetChoice->spriteIndex < runner->dataWin->sprt.count) {
                sample.targetChoiceSubimg = (int32_t) targetChoice->imageIndex;
                sample.targetChoiceTPAGIndex = Renderer_resolveTPAGIndex(
                    runner->dataWin, targetChoice->spriteIndex, sample.targetChoiceSubimg);
            }
        }
    }
    sample.spriteDrawCalls = N3DSRenderer_getFrameSpriteDrawCalls(renderer);
    sample.spritePartDrawCalls = N3DSRenderer_getFrameSpritePartDrawCalls(renderer);
    sample.c2dFlushes = N3DSRenderer_getFrameC2DFlushes(renderer);
    sample.residentAtlasVRAMBytes = N3DSRenderer_getResidentAtlasVRAMBytes(renderer);
    sample.residentAtlasVRAMLimitBytes = N3DSRenderer_getResidentAtlasVRAMLimitBytes(renderer);
    sample.residentAtlasPageCount = N3DSRenderer_getResidentAtlasPageCount(renderer);
    sample.residentAtlasPageLimit = N3DSRenderer_getResidentAtlasPageLimit(renderer);
    sample.residentDirectVRAMBytes = N3DSRenderer_getResidentDirectAssetVRAMBytes(renderer);
    sample.residentDirectVRAMLimitBytes = N3DSRenderer_getResidentDirectAssetVRAMLimitBytes(renderer);
    sample.atlasBlobReads = N3DSRenderer_getFrameBlobReads(renderer);
    sample.atlasBlobCacheBytes = N3DSRenderer_getCachedAtlasT3xBytes(renderer);
    sample.atlasBlobReadPageCount = N3DSRenderer_getFrameBlobReadEventCount(renderer);
    if (sample.atlasBlobReadPageCount > N3DS_PERF_IMPORT_PAGE_CAP) sample.atlasBlobReadPageCount = N3DS_PERF_IMPORT_PAGE_CAP;
    repeat(sample.atlasBlobReadPageCount, eventIndex) {
        uint16_t pageIndex = UINT16_MAX;
        if (N3DSRenderer_getFrameBlobReadEvent(renderer, eventIndex, &pageIndex)) {
            sample.atlasBlobReadPages[eventIndex] = pageIndex;
        }
    }
    sample.frameImportEventCount = N3DSRenderer_getFramePageImportEventCount(renderer);
    if (sample.frameImportEventCount > N3DS_PERF_IMPORT_PAGE_CAP) sample.frameImportEventCount = N3DS_PERF_IMPORT_PAGE_CAP;
    repeat(sample.frameImportEventCount, eventIndex) {
        uint16_t pageIndex = UINT16_MAX;
        int32_t tpagIndex = -1;
        if (N3DSRenderer_getFramePageImportEvent(renderer, eventIndex, &pageIndex, &tpagIndex)) {
            sample.frameImportEventPages[eventIndex] = pageIndex;
            sample.frameImportEventTPAGs[eventIndex] = tpagIndex;
        }
    }
    sample.directBlobReads = N3DSRenderer_getFrameDirectBlobReads(renderer);
    sample.pageImports = N3DSRenderer_getFramePageImports(renderer);
    sample.pageEvictions = N3DSRenderer_getFramePageEvictions(renderer);
    sample.importFailures = N3DSRenderer_getFrameImportFailures(renderer);
    sample.directAssetLoads = N3DSRenderer_getFrameDirectAssetLoads(renderer);
    sample.directLastLoadKind = N3DSRenderer_getFrameDirectLastLoadKind(renderer);
    sample.directLastLoadIndex = N3DSRenderer_getFrameDirectLastLoadIndex(renderer);
    sample.directLastLoadFrame = N3DSRenderer_getFrameDirectLastLoadFrame(renderer);
    sample.directLastLoadVRAMBytes = N3DSRenderer_getFrameDirectLastLoadVRAMBytes(renderer);
    sample.directLastLoadUsedCachedBlob = N3DSRenderer_getFrameDirectLastLoadUsedCachedBlob(renderer) ? 1 : 0;
    sample.textureSwitches = N3DSRenderer_getFrameTextureSwitches(renderer);
    return sample;
}

static void N3DSDustPerf_appendSample(N3DSDustPerfSampler* sampler, const N3DSDustPerfFrame* sample) {
    if (sampler == NULL || sample == NULL) return;
    if (sampler->sampleCount >= N3DS_DUST_PERF_MAX_FRAMES) {
        sampler->truncated = true;
        return;
    }
    sampler->samples[sampler->sampleCount++] = *sample;
}

static void N3DSDustPerf_pushPreFrame(N3DSDustPerfSampler* sampler, const N3DSDustPerfFrame* sample) {
    if (sampler == NULL || sample == NULL) return;
    N3DSDustPerfFrame copy = *sample;
    copy.phase = -1;
    sampler->preFrames[sampler->preWrite] = copy;
    sampler->preWrite = (sampler->preWrite + 1u) % N3DS_DUST_PERF_PRE_FRAMES;
    if (sampler->preCount < N3DS_DUST_PERF_PRE_FRAMES) sampler->preCount++;
}

static void N3DSDustPerf_copyPreFrames(N3DSDustPerfSampler* sampler) {
    if (sampler == NULL || sampler->preCount == 0) return;
    uint32_t start = sampler->preCount < N3DS_DUST_PERF_PRE_FRAMES ? 0u : sampler->preWrite;
    repeat(sampler->preCount, i) {
        uint32_t index = (start + (uint32_t) i) % N3DS_DUST_PERF_PRE_FRAMES;
        N3DSDustPerf_appendSample(sampler, &sampler->preFrames[index]);
    }
}

static const char* N3DSPerf_phaseName(int32_t phase, const char* activePhaseName) {
    if (phase < 0) return "pre";
    if (phase > 0) return activePhaseName != NULL ? activePhaseName : "active";
    return "post";
}

static const char* N3DSPerf_directLoadKindName(uint32_t kind) {
    switch (kind) {
        case 1u: return "sprite_sheet";
        case 2u: return "sprite_frame";
        case 3u: return "background";
        case 4u: return "font";
        default: return "";
    }
}

static const char* N3DSPerf_battlePrewarmTargetName(uint32_t kind) {
    switch ((N3DSBattlePrewarmTargetKind) kind) {
        case N3DS_BATTLE_PREWARM_TARGET_NORMAL: return "normal";
        case N3DS_BATTLE_PREWARM_TARGET_QUICK: return "quick";
        case N3DS_BATTLE_PREWARM_TARGET_STORY: return "story";
        case N3DS_BATTLE_PREWARM_TARGET_FLOWEY: return "flowey";
        case N3DS_BATTLE_PREWARM_TARGET_UNKNOWN: return "unknown";
        default: return "";
    }
}

static const char* N3DSPerf_battlePrewarmAssetName(int32_t stage) {
    switch (stage) {
        case 0: return "spr_targetchoice";
        case 1: return "spr_target";
        case 2: return "spr_strike";
        case 3: return "fnt_ja_curs";
        case 4: return "fnt_ja_main";
        case 5: return "fnt_ja_plain";
        case 6: return "manifest";
        default: return "";
    }
}

static void N3DSPerf_formatPageList(char* out, size_t outSize, const uint16_t* pages, uint32_t pageCount) {
    if (out == NULL || outSize == 0) return;
    out[0] = '\0';
    if (pages == NULL) return;

    size_t cursor = 0;
    repeat(pageCount, pageIndex) {
        int written = snprintf(out + cursor, outSize - cursor, "%s%u",
            pageIndex == 0 ? "" : "|", (unsigned int) pages[pageIndex]);
        if (written <= 0 || (size_t) written >= outSize - cursor) break;
        cursor += (size_t) written;
    }
}

static void N3DSPerf_queueEvent(
    const N3DSDustPerfSampler* sampler,
    const char* eventName,
    const char* activePhaseName
) {
    if (sampler == NULL || sampler->sampleCount == 0) return;
    if (gN3DSPerfPendingEventCount >= N3DS_PERF_PENDING_EVENT_CAP) {
        fprintf(stderr, "N3DS perf: pending event queue full; dropping %s-event %lu\n",
            eventName != NULL ? eventName : "perf", (unsigned long) sampler->eventId);
        return;
    }

    N3DSPerfPendingEvent* pending = &gN3DSPerfPendingEvents[gN3DSPerfPendingEventCount++];
    pending->sampler = *sampler;
    snprintf(pending->eventName, sizeof(pending->eventName), "%s", eventName != NULL ? eventName : "perf");
    snprintf(pending->activePhaseName, sizeof(pending->activePhaseName), "%s",
        activePhaseName != NULL ? activePhaseName : "active");
}

static void N3DSPerf_writeEventToFile(
    FILE* file,
    N3DSDustPerfSampler* sampler,
    Runner* runner,
    const char* eventName,
    const char* activePhaseName
) {
    if (file == NULL || sampler == NULL || sampler->sampleCount == 0) return;

    fprintf(file, "# %s-event %lu samples=%lu truncated=%d\n",
        eventName != NULL ? eventName : "perf",
        (unsigned long) sampler->eventId, (unsigned long) sampler->sampleCount, sampler->truncated ? 1 : 0);
    fputs(
        "frame,phase,dust,created,destroyed,draw_instances,step_ms,prep_ms,event_ms,motion_ms,"
        "collision_ms,cleanup_ms,finalize_ms,draw_instances_ms,battle_replay_ms,battle_prewarm_tick_ms,render_ms,"
        "frame_begin_ms,renderer_begin_ms,draw_views_ms,overlay_ms,renderer_end_ms,frame_end_ms,"
        "c3d_cpu_ms,gpu_prev_ms,cmd_buf_pct,sprite_calls,part_calls,c2d_flushes,"
        "atlas_vram_bytes,atlas_vram_limit,atlas_resident_pages,atlas_page_limit,direct_vram_bytes,direct_vram_limit,"
        "atlas_blob_reads,atlas_blob_read_pages,atlas_blob_cache_bytes,frame_import_events,direct_blob_reads,page_imports,page_evictions,import_failures,"
        "direct_asset_loads,direct_load_kind,direct_load_index,direct_load_frame,direct_load_vram_bytes,"
        "direct_load_cached_blob,texture_switches,atlas_object,atlas_object_name,atlas_instance,"
        "atlas_instance_sprite,atlas_instance_sprite_name,atlas_instance_subimg,atlas_instance_tpag,"
        "atlas_trigger_tpag,atlas_trigger_sprite,atlas_trigger_sprite_name,atlas_trigger_frame,"
        "atlas_imports,atlas_evictions,atlas_pass,atlas_pages,atlas_page_info,target_object,target_name,target_instance,target_sprite,"
        "target_subimg,target_tpag,target_image_index,target_image_speed,battle_active,myfight,mnfight,battle_transition_prewarm_active,battle_prewarm_target,battle_prewarm_target_room,battle_prewarm_stage_start,battle_prewarm_stage_end,battle_prewarm_asset,battle_prewarm_job_bytes,battle_prewarm_job_total,battle_prewarm_tick_chunks,battle_prewarm_tick_bytes,"
        "top_step_object,top_step_name,top_step_ms,top_step_calls,"
        "room_transitioned,room_from,room_to,room_transition_ms,room_init_ms,room_start_ms,renderer_prewarm_ms,audio_prewarm_ms,"
        "prewarm_battle_room,prewarm_total_ms,prewarm_clear_pins_ms,prewarm_flush_direct_ms,"
        "prewarm_battle_direct_ms,prewarm_battle_direct_reads,prewarm_battle_direct_bytes,"
        "prewarm_japanese_fonts_ms,prewarm_japanese_font_reads,prewarm_japanese_font_bytes,"
        "prewarm_room_atlas_ms,prewarm_room_atlas_reads,prewarm_room_atlas_bytes,prewarm_room_atlas_pages,prewarm_room_atlas_pages_truncated,"
        "prewarm_battle_critical_ms,prewarm_battle_critical_reads,prewarm_battle_critical_bytes,prewarm_battle_critical_pages,prewarm_battle_critical_pages_truncated,"
        "battle_manifest_loaded,battle_manifest_attempted,battle_manifest_found,battle_manifest_page_count,battle_manifest_cold_reads,battle_manifest_read_bytes,"
        "prewarm_atlas_cache_before,prewarm_atlas_cache_after,prewarm_direct_cache_before,prewarm_direct_cache_after,"
        "prewarm_direct_vram_before,prewarm_direct_vram_after_flush,prewarm_direct_vram_after,"
        "audio_prewarm_worker,audio_cache_before,audio_cache_after,audio_pending_sounds,audio_pending_bytes\n", file);

    repeat(sampler->sampleCount, i) {
        const N3DSDustPerfFrame* sample = &sampler->samples[i];
        const char* topName = "";
        if (runner != NULL && sample->topStepObjectIndex >= 0 &&
            (uint32_t) sample->topStepObjectIndex < runner->dataWin->objt.count) {
            const char* candidate = runner->dataWin->objt.objects[sample->topStepObjectIndex].name;
            if (candidate != NULL) topName = candidate;
        }

        const char* targetName = "";
        if (runner != NULL && sample->targetChoiceObjectIndex >= 0 &&
            (uint32_t) sample->targetChoiceObjectIndex < runner->dataWin->objt.count) {
            const char* candidate = runner->dataWin->objt.objects[sample->targetChoiceObjectIndex].name;
            if (candidate != NULL) targetName = candidate;
        }
        const char* atlasObjectName = "";
        if (runner != NULL && sample->atlasImportObjectIndex >= 0 && (uint32_t) sample->atlasImportObjectIndex < runner->dataWin->objt.count) {
            const char* candidate = runner->dataWin->objt.objects[sample->atlasImportObjectIndex].name;
            if (candidate != NULL) atlasObjectName = candidate;
        }
        const char* atlasInstanceSpriteName = "";
        if (runner != NULL && sample->atlasImportInstanceSpriteIndex >= 0 && (uint32_t) sample->atlasImportInstanceSpriteIndex < runner->dataWin->sprt.count) {
            const char* candidate = runner->dataWin->sprt.sprites[sample->atlasImportInstanceSpriteIndex].name;
            if (candidate != NULL) atlasInstanceSpriteName = candidate;
        }
        const char* atlasTriggerSpriteName = "";
        if (runner != NULL && sample->atlasImportTriggerSpriteIndex >= 0 && (uint32_t) sample->atlasImportTriggerSpriteIndex < runner->dataWin->sprt.count) {
            const char* candidate = runner->dataWin->sprt.sprites[sample->atlasImportTriggerSpriteIndex].name;
            if (candidate != NULL) atlasTriggerSpriteName = candidate;
        }
        const char* atlasPassName = sample->atlasImportPass == 1u ? "main" : sample->atlasImportPass == 2u ? "replay" : "";

        char atlasBlobReadPages[128];
        atlasBlobReadPages[0] = '\0';
        size_t atlasBlobReadPagesCursor = 0;
        repeat(sample->atlasBlobReadPageCount, eventIndex) {
            int written = snprintf(
                atlasBlobReadPages + atlasBlobReadPagesCursor,
                sizeof(atlasBlobReadPages) - atlasBlobReadPagesCursor,
                "%s%u",
                eventIndex == 0 ? "" : "|",
                (unsigned int) sample->atlasBlobReadPages[eventIndex]
            );
            if (written <= 0 || (size_t) written >= sizeof(atlasBlobReadPages) - atlasBlobReadPagesCursor) break;
            atlasBlobReadPagesCursor += (size_t) written;
        }

        char frameImportEvents[256];
        frameImportEvents[0] = '\0';
        size_t frameImportEventsCursor = 0;
        repeat(sample->frameImportEventCount, eventIndex) {
            int written = snprintf(
                frameImportEvents + frameImportEventsCursor,
                sizeof(frameImportEvents) - frameImportEventsCursor,
                "%s%u:%ld",
                eventIndex == 0 ? "" : "|",
                (unsigned int) sample->frameImportEventPages[eventIndex],
                (long) sample->frameImportEventTPAGs[eventIndex]
            );
            if (written <= 0 || (size_t) written >= sizeof(frameImportEvents) - frameImportEventsCursor) break;
            frameImportEventsCursor += (size_t) written;
        }

        char prewarmRoomAtlasPages[128];
        char prewarmBattleCriticalPages[128];
        N3DSPerf_formatPageList(prewarmRoomAtlasPages, sizeof(prewarmRoomAtlasPages),
            sample->prewarmRoomAtlasReadPages, sample->prewarmRoomAtlasReadPageCount);
        N3DSPerf_formatPageList(prewarmBattleCriticalPages, sizeof(prewarmBattleCriticalPages),
            sample->prewarmBattleCriticalReadPages, sample->prewarmBattleCriticalReadPageCount);

        char atlasPages[128];
        atlasPages[0] = '\0';
        size_t atlasPagesCursor = 0;
        char atlasPageInfo[256];
        atlasPageInfo[0] = '\0';
        size_t atlasPageInfoCursor = 0;
        repeat(sample->atlasImportPageCount, pageIndex) {
            uint32_t importedPage = sample->atlasImportPages[pageIndex];
            int written = snprintf(atlasPages + atlasPagesCursor, sizeof(atlasPages) - atlasPagesCursor, "%s%u", pageIndex == 0 ? "" : "|", (unsigned int) importedPage);
            if (written > 0 && (size_t) written < sizeof(atlasPages) - atlasPagesCursor) {
                atlasPagesCursor += (size_t) written;
            }
            const char* formatName = runner != NULL && runner->renderer != NULL
                ? N3DSRenderer_getAtlasPageFormatName(runner->renderer, importedPage)
                : "unknown";
            uint32_t pageVRAMBytes = runner != NULL && runner->renderer != NULL
                ? N3DSRenderer_getAtlasPageVRAMBytes(runner->renderer, importedPage)
                : 0u;
            written = snprintf(
                atlasPageInfo + atlasPageInfoCursor,
                sizeof(atlasPageInfo) - atlasPageInfoCursor,
                "%s%u:%s:%lu",
                pageIndex == 0 ? "" : "|",
                (unsigned int) importedPage,
                formatName,
                (unsigned long) pageVRAMBytes
            );
            if (written <= 0 || (size_t) written >= sizeof(atlasPageInfo) - atlasPageInfoCursor) break;
            atlasPageInfoCursor += (size_t) written;
        }

        fprintf(file,
            "%ld,%s,%lu,%lu,%lu,%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%lu,%lu,%lu",
            (long) sample->frame, N3DSPerf_phaseName(sample->phase, activePhaseName),
            (unsigned long) sample->dustCount, (unsigned long) sample->instancesCreated,
            (unsigned long) sample->instancesDestroyed, (unsigned long) sample->drawInstances,
            sample->stepMs, sample->prepMs, sample->eventMs, sample->motionMs, sample->collisionMs,
            sample->cleanupMs, sample->finalizeMs, sample->drawInstancesMs, sample->battleReplayMs,
            sample->battlePrewarmTickMs, sample->renderMs, sample->frameBeginMs, sample->rendererBeginMs, sample->drawViewsMs,
            sample->overlayMs, sample->rendererEndMs, sample->frameEndMs, sample->c3dProcessingMs,
            sample->previousGpuDrawingMs, sample->cmdBufUsagePercent,
            (unsigned long) sample->spriteDrawCalls, (unsigned long) sample->spritePartDrawCalls,
            (unsigned long) sample->c2dFlushes);
        fprintf(file,
            ",%lu,%lu,%lu,%lu,%lu,%lu",
            (unsigned long) sample->residentAtlasVRAMBytes,
            (unsigned long) sample->residentAtlasVRAMLimitBytes,
            (unsigned long) sample->residentAtlasPageCount,
            (unsigned long) sample->residentAtlasPageLimit,
            (unsigned long) sample->residentDirectVRAMBytes,
            (unsigned long) sample->residentDirectVRAMLimitBytes);
        fprintf(file,
            ",%lu,%s,%lu,%s,%lu,%lu,%lu,%lu,%lu,%s,%ld,%ld,%lu,%ld,%lu",
            (unsigned long) sample->atlasBlobReads,
            atlasBlobReadPages,
            (unsigned long) sample->atlasBlobCacheBytes,
            frameImportEvents,
            (unsigned long) sample->directBlobReads,
            (unsigned long) sample->pageImports,
            (unsigned long) sample->pageEvictions,
            (unsigned long) sample->importFailures,
            (unsigned long) sample->directAssetLoads,
            N3DSPerf_directLoadKindName(sample->directLastLoadKind),
            (long) sample->directLastLoadIndex,
            (long) sample->directLastLoadFrame,
            (unsigned long) sample->directLastLoadVRAMBytes,
            (long) sample->directLastLoadUsedCachedBlob,
            (unsigned long) sample->textureSwitches);
        fprintf(file,
            ",%ld,%s,%lu,%ld,%s,%ld,%ld,%ld,%ld,%s,%ld,%lu,%lu,%s,%s",
            (long) sample->atlasImportObjectIndex, atlasObjectName,
            (unsigned long) sample->atlasImportInstanceId,
            (long) sample->atlasImportInstanceSpriteIndex, atlasInstanceSpriteName,
            (long) sample->atlasImportInstanceSubimg,
            (long) sample->atlasImportInstanceTPAGIndex,
            (long) sample->atlasImportTriggerTPAGIndex,
            (long) sample->atlasImportTriggerSpriteIndex, atlasTriggerSpriteName,
            (long) sample->atlasImportTriggerFrameIndex,
            (unsigned long) sample->atlasImportCount,
            (unsigned long) sample->atlasImportEvictions,
            atlasPassName, atlasPages);
        fprintf(file, ",%s", atlasPageInfo);
        fprintf(file,
            ",%ld,%s,%lu,%ld,%ld,%ld,%.3f,%.3f,%ld,%ld,%ld,%ld",
            (long) sample->targetChoiceObjectIndex, targetName,
            (unsigned long) sample->targetChoiceInstanceId,
            (long) sample->targetChoiceSpriteIndex, (long) sample->targetChoiceSubimg,
            (long) sample->targetChoiceTPAGIndex, (double) sample->targetChoiceImageIndex,
            (double) sample->targetChoiceImageSpeed, (long) sample->battleActive,
            (long) sample->myFight, (long) sample->mnFight,
            (long) sample->battleTransitionPrewarmActive);
        fprintf(file, ",%s,%ld,%ld,%ld,%s,%lu,%lu,%lu,%lu",
            N3DSPerf_battlePrewarmTargetName(sample->battlePrewarmTargetKind),
            (long) sample->battlePrewarmTargetRoomIndex,
            (long) sample->battlePrewarmStageStart,
            (long) sample->battlePrewarmStageEnd,
            N3DSPerf_battlePrewarmAssetName(sample->battlePrewarmAssetStage),
            (unsigned long) sample->battlePrewarmJobBytesCompleted,
            (unsigned long) sample->battlePrewarmJobBytesTotal,
            (unsigned long) sample->battlePrewarmTickChunks,
            (unsigned long) sample->battlePrewarmTickBytes);
        fprintf(file, ",%ld,%s,%.3f,%lu",
            (long) sample->topStepObjectIndex, topName,
            sample->topStepObjectMs, (unsigned long) sample->topStepObjectCalls);
        fprintf(file,
            ",%ld,%ld,%ld,%.3f,%.3f,%.3f,%.3f,%.3f,%ld,%.3f,%.3f,%.3f,"
            "%.3f,%lu,%llu,%.3f,%lu,%llu,%.3f,%lu,%llu,%s,%ld,%.3f,%lu,%llu,%s,%ld,"
            "%ld,%ld,%ld,%lu,%lu,%llu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%ld,%lu,%lu,%lu,%lu\n",
            (long) sample->roomTransitioned,
            (long) sample->roomFromIndex,
            (long) sample->roomToIndex,
            sample->roomTransitionMs,
            sample->roomInitMs,
            sample->roomStartMs,
            sample->rendererPrewarmMs,
            sample->audioPrewarmMs,
            (long) sample->prewarmBattleRoom,
            sample->prewarmTotalMs,
            sample->prewarmClearPinsMs,
            sample->prewarmFlushDirectMs,
            sample->prewarmBattleDirectMs,
            (unsigned long) sample->prewarmBattleDirectReads,
            (unsigned long long) sample->prewarmBattleDirectReadBytes,
            sample->prewarmJapaneseFontsMs,
            (unsigned long) sample->prewarmJapaneseFontReads,
            (unsigned long long) sample->prewarmJapaneseFontReadBytes,
            sample->prewarmRoomAtlasMs,
            (unsigned long) sample->prewarmRoomAtlasReads,
            (unsigned long long) sample->prewarmRoomAtlasReadBytes,
            prewarmRoomAtlasPages,
            (long) sample->prewarmRoomAtlasReadPagesTruncated,
            sample->prewarmBattleCriticalMs,
            (unsigned long) sample->prewarmBattleCriticalReads,
            (unsigned long long) sample->prewarmBattleCriticalReadBytes,
            prewarmBattleCriticalPages,
            (long) sample->prewarmBattleCriticalReadPagesTruncated,
            (long) sample->battleManifestLoaded,
            (long) sample->battleManifestAttempted,
            (long) sample->battleManifestFound,
            (unsigned long) sample->battleManifestPageCount,
            (unsigned long) sample->battleManifestColdReads,
            (unsigned long long) sample->battleManifestReadBytes,
            (unsigned long) sample->prewarmAtlasCacheBytesBefore,
            (unsigned long) sample->prewarmAtlasCacheBytesAfter,
            (unsigned long) sample->prewarmDirectCacheBytesBefore,
            (unsigned long) sample->prewarmDirectCacheBytesAfter,
            (unsigned long) sample->prewarmDirectVRAMBytesBefore,
            (unsigned long) sample->prewarmDirectVRAMBytesAfterFlush,
            (unsigned long) sample->prewarmDirectVRAMBytesAfter,
            (long) sample->audioPrewarmWorkerEnabled,
            (unsigned long) sample->audioPrewarmCacheBytesBefore,
            (unsigned long) sample->audioPrewarmCacheBytesAfter,
            (unsigned long) sample->audioPrewarmPendingSounds,
            (unsigned long) sample->audioPrewarmPendingBytes);
    }
    fprintf(file, "# end %s-event\n", eventName != NULL ? eventName : "perf");
}

static void N3DSPerf_flushQueuedEvents(Runner* runner) {
    if (gN3DSPerfPendingEventCount == 0) return;

    FILE* file = fopen(N3DS_DUST_PERF_LOG_PATH, "a");
    if (file == NULL) {
        fprintf(stderr, "N3DS dust perf: failed to open %s\n", N3DS_DUST_PERF_LOG_PATH);
        return;
    }

    repeat(gN3DSPerfPendingEventCount, i) {
        N3DSPerfPendingEvent* pending = &gN3DSPerfPendingEvents[i];
        N3DSPerf_writeEventToFile(file, &pending->sampler, runner, pending->eventName, pending->activePhaseName);
    }
    fclose(file);
    gN3DSPerfPendingEventCount = 0;
}

static void N3DSDustPerf_update(
    N3DSDustPerfSampler* sampler,
    Runner* runner,
    const N3DSDustPerfFrame* captured
) {
    if (sampler == NULL || runner == NULL || captured == NULL) return;
    N3DSDustPerfFrame sample = *captured;

    if (!sampler->active) {
        if (sample.dustCount == 0) {
            N3DSDustPerf_pushPreFrame(sampler, &sample);
            return;
        }
        sampler->active = true;
        sampler->truncated = false;
        sampler->sampleCount = 0;
        sampler->postFramesRemaining = N3DS_DUST_PERF_POST_FRAMES;
        sampler->eventId++;
        N3DSDustPerf_copyPreFrames(sampler);
        sample.phase = 1;
        N3DSDustPerf_appendSample(sampler, &sample);
        return;
    }

    sample.phase = sample.dustCount > 0 ? 1 : 0;
    N3DSDustPerf_appendSample(sampler, &sample);
    if (sample.dustCount > 0) {
        sampler->postFramesRemaining = N3DS_DUST_PERF_POST_FRAMES;
        return;
    }
    if (sampler->postFramesRemaining > 0) sampler->postFramesRemaining--;
    if (sampler->postFramesRemaining > 0) return;

    N3DSPerf_queueEvent(sampler, "dust", "dust");
    sampler->active = false;
    sampler->truncated = false;
    sampler->sampleCount = 0;
    sampler->preCount = 0;
    sampler->preWrite = 0;
}

static void N3DSDustPerf_flushPending(N3DSDustPerfSampler* sampler) {
    if (sampler == NULL || !sampler->active || sampler->sampleCount == 0) return;
    N3DSPerf_queueEvent(sampler, "dust", "dust");
    sampler->active = false;
    sampler->sampleCount = 0;
}

static void N3DSAttackPerf_update(
    N3DSDustPerfSampler* sampler,
    Runner* runner,
    const N3DSDustPerfFrame* captured
) {
    if (sampler == NULL || runner == NULL || captured == NULL) return;
    N3DSDustPerfFrame sample = *captured;
    bool targetChoiceAnimating =
        sample.targetChoiceObjectIndex >= 0 && fabsf(sample.targetChoiceImageSpeed) > 0.001f;
    bool attackActive =
        (sample.battleActive != 0 && sample.myFight == 1) || targetChoiceAnimating;

    if (!sampler->active) {
        if (!attackActive) {
            N3DSDustPerf_pushPreFrame(sampler, &sample);
            return;
        }
        sampler->active = true;
        sampler->truncated = false;
        sampler->sampleCount = 0;
        sampler->postFramesRemaining = N3DS_DUST_PERF_POST_FRAMES;
        sampler->eventId++;
        N3DSDustPerf_copyPreFrames(sampler);
        sample.phase = 1;
        N3DSDustPerf_appendSample(sampler, &sample);
        return;
    }

    sample.phase = attackActive ? 1 : 0;
    N3DSDustPerf_appendSample(sampler, &sample);
    if (attackActive) {
        sampler->postFramesRemaining = N3DS_DUST_PERF_POST_FRAMES;
        return;
    }
    if (sampler->postFramesRemaining > 0) sampler->postFramesRemaining--;
    if (sampler->postFramesRemaining > 0) return;

    N3DSPerf_queueEvent(sampler, "attack", "attack");
    sampler->active = false;
    sampler->truncated = false;
    sampler->sampleCount = 0;
    sampler->preCount = 0;
    sampler->preWrite = 0;
}

static void N3DSAttackPerf_flushPending(N3DSDustPerfSampler* sampler) {
    if (sampler == NULL || !sampler->active || sampler->sampleCount == 0) return;
    N3DSPerf_queueEvent(sampler, "attack", "attack");
    sampler->active = false;
    sampler->sampleCount = 0;
}

static void N3DSBattleEntryPerf_reset(N3DSDustPerfSampler* sampler) {
    if (sampler == NULL) return;
    sampler->active = false;
    sampler->truncated = false;
    sampler->postFramesRemaining = 0;
    sampler->sampleCount = 0;
    sampler->preCount = 0;
    sampler->preWrite = 0;
}

static void N3DSBattleEntryPerf_update(
    N3DSDustPerfSampler* sampler,
    Runner* runner,
    const N3DSDustPerfFrame* captured
) {
    if (sampler == NULL || runner == NULL || captured == NULL) return;
    N3DSDustPerfFrame sample = *captured;
    bool transitionActive = sample.battleTransitionPrewarmActive != 0;
    bool enteredBattle = runner->frameRoomTransitioned &&
        (sample.prewarmBattleRoom != 0 || sample.battleActive != 0);

    if (!sampler->active) {
        if (!transitionActive && !enteredBattle) return;
        sampler->active = true;
        sampler->truncated = false;
        sampler->sampleCount = 0;
        sampler->postFramesRemaining = enteredBattle ? N3DS_BATTLE_ENTRY_POST_FRAMES - 1u : 0u;
        sampler->eventId++;
        sample.phase = 1;
        N3DSDustPerf_appendSample(sampler, &sample);
        if (!enteredBattle || sampler->postFramesRemaining > 0) return;
        N3DSPerf_queueEvent(sampler, "battle_entry", "entry");
        N3DSBattleEntryPerf_reset(sampler);
        return;
    }

    sample.phase = (transitionActive || enteredBattle) ? 1 : 0;
    N3DSDustPerf_appendSample(sampler, &sample);
    if (sampler->truncated) {
        N3DSPerf_queueEvent(sampler, "battle_entry", "entry");
        N3DSBattleEntryPerf_reset(sampler);
        return;
    }

    if (enteredBattle) {
        sampler->postFramesRemaining = N3DS_BATTLE_ENTRY_POST_FRAMES - 1u;
        if (sampler->postFramesRemaining > 0) return;
        N3DSPerf_queueEvent(sampler, "battle_entry", "entry");
        N3DSBattleEntryPerf_reset(sampler);
        return;
    }

    if (sampler->postFramesRemaining > 0) {
        sampler->postFramesRemaining--;
        if (sampler->postFramesRemaining > 0) return;
        N3DSPerf_queueEvent(sampler, "battle_entry", "entry");
        N3DSBattleEntryPerf_reset(sampler);
        return;
    }

    if (runner->frameRoomTransitioned && !enteredBattle) {
        N3DSPerf_queueEvent(sampler, "battle_entry", "entry");
        N3DSBattleEntryPerf_reset(sampler);
        return;
    }

    // Continue through the transition until the room-change frame arrives.
}

static void N3DSBattleEntryPerf_flushPending(N3DSDustPerfSampler* sampler) {
    if (sampler == NULL || !sampler->active || sampler->sampleCount == 0) return;
    N3DSPerf_queueEvent(sampler, "battle_entry", "entry");
    N3DSBattleEntryPerf_reset(sampler);
}

static void N3DSDebugMonitor_init(N3DSDebugMonitor* monitor) {
    if (monitor == NULL) return;
    memset(monitor, 0, sizeof(*monitor));
    monitor->textBuf = C2D_TextBufNew(1024);
    monitor->sampleStartMs = osGetTime();
}

static void N3DSDebugMonitor_free(N3DSDebugMonitor* monitor) {
    if (monitor == NULL) return;
    if (monitor->textBuf != NULL) {
        C2D_TextBufDelete(monitor->textBuf);
        monitor->textBuf = NULL;
    }
}

static void N3DSDebugMonitor_tickFrame(N3DSDebugMonitor* monitor, double renderMs) {
    if (monitor == NULL) return;

    monitor->sampledFrames++;
    monitor->sampledRenderMs += renderMs;
    u64 nowMs = osGetTime();
    u64 elapsedMs = nowMs - monitor->sampleStartMs;
    if (elapsedMs < 250) return;

    monitor->displayedFps = ((double) monitor->sampledFrames * 1000.0) / (double) elapsedMs;
    monitor->displayedRenderMs = monitor->sampledFrames > 0
        ? monitor->sampledRenderMs / (double) monitor->sampledFrames
        : 0.0;
    monitor->sampledFrames = 0;
    monitor->sampledRenderMs = 0.0;
    monitor->sampleStartMs = nowMs;
}

//font used in godmode9 that's easy to read. got lazy and just embedded the pbm bytes directly here 

static uint8_t N3DSDebugTinyFont_getRow(char c, uint32_t row) {
    static const uint8_t font[95][10] = {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00 },
        { 0x00, 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A, 0x00, 0x00 },
        { 0x00, 0x04, 0x0E, 0x15, 0x0C, 0x06, 0x15, 0x0E, 0x04, 0x00 },
        { 0x00, 0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13, 0x00, 0x00 },
        { 0x00, 0x04, 0x0A, 0x04, 0x09, 0x15, 0x12, 0x0D, 0x00, 0x00 },
        { 0x00, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x02, 0x04, 0x04, 0x04, 0x04, 0x04, 0x02, 0x00, 0x00 },
        { 0x00, 0x08, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x00, 0x00 },
        { 0x00, 0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x18, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00 },
        { 0x00, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x00 },
        { 0x00, 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x1F, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02, 0x00, 0x00 },
        { 0x00, 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x1F, 0x01, 0x02, 0x02, 0x04, 0x04, 0x04, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x18, 0x00 },
        { 0x00, 0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x13, 0x15, 0x17, 0x10, 0x0F, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00, 0x00 },
        { 0x00, 0x1E, 0x09, 0x09, 0x0E, 0x09, 0x09, 0x1E, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x1E, 0x09, 0x09, 0x09, 0x09, 0x09, 0x1E, 0x00, 0x00 },
        { 0x00, 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F, 0x00, 0x00 },
        { 0x00, 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x00, 0x00 },
        { 0x00, 0x0F, 0x10, 0x10, 0x13, 0x11, 0x11, 0x0F, 0x00, 0x00 },
        { 0x00, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00, 0x00 },
        { 0x00, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F, 0x00, 0x00 },
        { 0x00, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x18, 0x00, 0x00 },
        { 0x00, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00, 0x00 },
        { 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x00, 0x00 },
        { 0x00, 0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00 },
        { 0x00, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D, 0x00, 0x00 },
        { 0x00, 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x11, 0x00, 0x00 },
        { 0x00, 0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00 },
        { 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04, 0x00, 0x00 },
        { 0x00, 0x11, 0x11, 0x11, 0x11, 0x15, 0x15, 0x0A, 0x00, 0x00 },
        { 0x00, 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11, 0x00, 0x00 },
        { 0x00, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00 },
        { 0x00, 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F, 0x00, 0x00 },
        { 0x00, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x06, 0x00, 0x00 },
        { 0x00, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x02, 0x02, 0x00 },
        { 0x00, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0C, 0x00, 0x00 },
        { 0x00, 0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00 },
        { 0x00, 0x08, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F, 0x00, 0x00 },
        { 0x00, 0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x1E, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x0E, 0x11, 0x10, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x01, 0x01, 0x0F, 0x11, 0x11, 0x13, 0x0D, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0F, 0x00, 0x00 },
        { 0x00, 0x06, 0x09, 0x08, 0x1E, 0x08, 0x08, 0x08, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x0D, 0x13, 0x11, 0x11, 0x0F, 0x01, 0x1E },
        { 0x00, 0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00, 0x00 },
        { 0x00, 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x1F, 0x00, 0x00 },
        { 0x00, 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x18 },
        { 0x00, 0x10, 0x10, 0x12, 0x14, 0x1C, 0x12, 0x11, 0x00, 0x00 },
        { 0x00, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x06, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x1E, 0x10, 0x10 },
        { 0x00, 0x00, 0x00, 0x0F, 0x11, 0x11, 0x13, 0x0D, 0x01, 0x01 },
        { 0x00, 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E, 0x00, 0x00 },
        { 0x00, 0x04, 0x04, 0x0E, 0x04, 0x04, 0x04, 0x02, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x1E },
        { 0x00, 0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F, 0x00, 0x00 },
        { 0x00, 0x02, 0x04, 0x04, 0x08, 0x04, 0x04, 0x02, 0x00, 0x00 },
        { 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },
        { 0x00, 0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00, 0x00, 0x00 },
    };

    if (row >= 10u || c < 32 || c > 126) return 0;
    return font[(uint8_t) c - 32u][row];
}

static void N3DSDebugTinyFont_drawText(const char* text, float x, float y, uint32_t color) {
    if (text == NULL) return;

    const float pixel = 1.0f;
    const float charAdvance = 6.0f;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == ' ') {
            x += charAdvance;
            continue;
        }

        repeat(10u, row) {
            uint8_t bits = N3DSDebugTinyFont_getRow(*cursor, (uint32_t) row);
            repeat(6u, col) {
                if ((bits & (uint8_t) (1u << (5u - col))) == 0u) continue;
                C2D_DrawRectSolid(x + (float) col * pixel, y + (float) row * pixel, 0.0f, pixel, pixel, color);
            }
        }
        x += charAdvance;
    }
}

static void N3DSDebugMonitor_draw(N3DSDebugMonitor* monitor, Runner* runner, Renderer* renderer) {
#ifdef N3DS_DISABLE_BOTTOM_SCREEN
    (void) monitor;
    (void) runner;
    (void) renderer;
    return;
#endif
    if (monitor == NULL || runner == NULL || renderer == NULL || monitor->textBuf == NULL) return;

    const char* roomName = "(none)";
    if (runner->currentRoom != NULL && runner->currentRoom->name != NULL && runner->currentRoom->name[0] != '\0') {
        roomName = runner->currentRoom->name;
    }

    uint32_t atlasVRAMBytes = N3DSRenderer_getResidentAtlasVRAMBytes(renderer);
    uint32_t atlasVRAMLimitBytes = N3DSRenderer_getResidentAtlasVRAMLimitBytes(renderer);
    uint32_t atlasPageCount = N3DSRenderer_getResidentAtlasPageCount(renderer);
    uint32_t atlasPageLimit = N3DSRenderer_getResidentAtlasPageLimit(renderer);
    uint32_t directVRAMBytes = N3DSRenderer_getResidentDirectAssetVRAMBytes(renderer);
    uint32_t trackedVRAMBytes = atlasVRAMBytes + directVRAMBytes;
    uint32_t ramFreeBytes = osGetMemRegionFree(MEMREGION_APPLICATION);
    uint32_t ramTotalBytes = osGetMemRegionSize(MEMREGION_APPLICATION);
    uint32_t linearFreeBytes = linearSpaceFree();

    char fpsLine[96];
    char vramLine[64];
    char atlasLine[96];
    char ramLine[96];
    char audioLine[96];
    char roomLine[96];
    char touchLine[96];
    char vramUsed[24];
    char vramTotal[24];
    char atlasUsed[24];
    char atlasLimit[24];
    char directUsed[24];
    char ramFree[24];
    char ramTotal[24];
    char linearFree[24];
    char audioCached[24];
    char audioLimit[24];
    uint32_t cachedSounds = 0;
    uint32_t totalSounds = 0;
    uint32_t cachedSoundBytes = 0;
    uint32_t cacheLimitBytes = 0;

    N3DSAudioSystem_getCacheStats(runner->audioSystem, &cachedSounds, &totalSounds, &cachedSoundBytes, &cacheLimitBytes);

    snprintf(
        fpsLine,
        sizeof(fpsLine),
        "FPS %.1f  R %.1fms",
        monitor->displayedFps > 0.0 ? monitor->displayedFps : 0.0,
        monitor->displayedRenderMs > 0.0 ? monitor->displayedRenderMs : 0.0
    );
    N3DS_formatDebugSize(vramUsed, sizeof(vramUsed), trackedVRAMBytes);
    N3DS_formatDebugSize(vramTotal, sizeof(vramTotal), N3DS_TOTAL_VRAM_BYTES);
    N3DS_formatDebugSize(atlasUsed, sizeof(atlasUsed), atlasVRAMBytes);
    N3DS_formatDebugSize(atlasLimit, sizeof(atlasLimit), atlasVRAMLimitBytes);
    N3DS_formatDebugSize(directUsed, sizeof(directUsed), directVRAMBytes);
    N3DS_formatDebugSize(ramFree, sizeof(ramFree), ramFreeBytes);
    N3DS_formatDebugSize(ramTotal, sizeof(ramTotal), ramTotalBytes);
    N3DS_formatDebugSize(linearFree, sizeof(linearFree), linearFreeBytes);
    N3DS_formatDebugSize(audioCached, sizeof(audioCached), cachedSoundBytes);
    N3DS_formatDebugSize(audioLimit, sizeof(audioLimit), cacheLimitBytes);
    snprintf(vramLine, sizeof(vramLine), "VRAM %s/%s",
        vramUsed,
        vramTotal);
    snprintf(atlasLine, sizeof(atlasLine), "AT %lu/%lu %s  DR %s",
        (unsigned long) atlasPageCount,
        (unsigned long) atlasPageLimit,
        atlasUsed,
        directUsed);
    snprintf(ramLine, sizeof(ramLine), "RAM %s/%s  L %s",
        ramFree,
        ramTotal,
        linearFree);
    snprintf(audioLine, sizeof(audioLine), "AUD %lu/%lu %s/%s",
        (unsigned long) cachedSounds,
        (unsigned long) totalSounds,
        audioCached,
        audioLimit);
    snprintf(roomLine, sizeof(roomLine), "R %.28s", roomName);
    snprintf(
        touchLine,
        sizeof(touchLine),
        "TF%d MV%.0f C%.0f,%.0f T%.0f,%.0f",
        (int) monitor->touchFollowStatus,
        monitor->touchFollowActivity,
        monitor->touchFollowCurrentX,
        monitor->touchFollowCurrentY,
        monitor->touchFollowTargetX,
        monitor->touchFollowTargetY
    );

    N3DSRenderer_beginBottomScreenGUI(renderer, 320, 240);
    const float boxX = 92.0f;
    const float boxY = 8.0f;
    const float boxW = 220.0f;
    const float boxH = 127.0f;
    const float textX = boxX + 7.0f;
    C2D_DrawRectSolid(boxX, boxY, 0.0f, boxW, boxH, C2D_Color32(8, 10, 16, 218));
    C2D_DrawRectSolid(boxX, boxY, 0.0f, boxW, 2.0f, C2D_Color32(77, 118, 255, 245));
    C2D_DrawRectSolid(boxX, boxY + boxH - 1.0f, 0.0f, boxW, 1.0f, C2D_Color32(32, 46, 78, 230));
    N3DSDebugTinyFont_drawText("DBG", textX, boxY + 7.0f, C2D_Color32(255, 255, 255, 255));
    N3DSDebugTinyFont_drawText(fpsLine, boxX + 42.0f, boxY + 7.0f, C2D_Color32(196, 230, 255, 255));
    N3DSDebugTinyFont_drawText(vramLine, textX, boxY + 25.0f, C2D_Color32(196, 230, 255, 255));
    N3DSDebugTinyFont_drawText(atlasLine, textX, boxY + 42.0f, C2D_Color32(146, 188, 236, 255));
    N3DSDebugTinyFont_drawText(ramLine, textX, boxY + 59.0f, C2D_Color32(196, 230, 255, 255));
    N3DSDebugTinyFont_drawText(audioLine, textX, boxY + 76.0f, C2D_Color32(196, 255, 196, 255));
    N3DSDebugTinyFont_drawText(roomLine, textX, boxY + 93.0f, C2D_Color32(255, 230, 163, 255));
    N3DSDebugTinyFont_drawText(touchLine, textX, boxY + 110.0f, C2D_Color32(255, 190, 224, 255));
    N3DSRenderer_endBottomScreenGUI(renderer);
}

static bool N3DS_stringStartsWithIgnoreCase(const char* value, const char* prefix) {
    if (value == NULL || prefix == NULL) return false;
    while (*prefix != '\0') {
        if (*value == '\0') return false;
        if (tolower((unsigned char) *value) != tolower((unsigned char) *prefix)) return false;
        ++value;
        ++prefix;
    }
    return true;
}

static const char* N3DS_getRoomBorderAssetName(const Room* room) {
    const char* roomName = room != NULL ? room->name : NULL;
    if (roomName == NULL || roomName[0] == '\0') return "border_none";

    if (N3DS_stringStartsWithIgnoreCase(roomName, "room_gaster") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_mysteryman")) return "room_gaster";

    if (N3DS_stringStartsWithIgnoreCase(roomName, "room_truelab")) return "room_truelab";

    if (N3DS_stringStartsWithIgnoreCase(roomName, "room_castle") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_asghouse") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_asgoreroom") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_lastruins_corridor") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_sanscorridor") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_undertale_end")) return "room_castle";

    if (N3DS_stringStartsWithIgnoreCase(roomName, "room_fire")) return "room_fire";

    if (N3DS_stringStartsWithIgnoreCase(roomName, "room_water")) return "room_water";

    if (N3DS_stringStartsWithIgnoreCase(roomName, "room_tundra") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_ice") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_fogroom")) return "room_tundra";

    if (N3DS_stringStartsWithIgnoreCase(roomName, "room_ruins") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_area1") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_torhouse") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_torielroom") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_asrielroom") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_kitchen") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_basement") ||
        N3DS_stringStartsWithIgnoreCase(roomName, "room_ruinsexit")) return "room_ruins";

    return "border_none";
}

static bool N3DS_tryLoadRoomBorderSprite(const char* assetName, C2D_Sprite* outSprite, C2D_SpriteSheet* outSheet) {
    if (assetName == NULL || outSprite == NULL || outSheet == NULL) return false;

    char candidatePaths[2][256];
    snprintf(candidatePaths[0], sizeof(candidatePaths[0]), "sdmc:/3ds/cinnamon/gfx/borders/%s.t3x", assetName);
    snprintf(candidatePaths[1], sizeof(candidatePaths[1]), "romfs:/gfx/borders/%s.t3x", assetName);

    repeat(2, i) {
        const char* path = candidatePaths[i];
        C2D_SpriteSheet sheet = C2D_SpriteSheetLoad(path);
        if (sheet == NULL) continue;
        if (C2D_SpriteSheetCount(sheet) <= 0) {
            C2D_SpriteSheetFree(sheet);
            continue;
        }

        *outSheet = sheet;
        C2D_SpriteFromSheet(outSprite, sheet, 0);
        C2D_SpriteSetPos(outSprite, 0.0f, 0.0f);
        C2D_SpriteSetDepth(outSprite, 1.0f);
        return true;
    }

    return false;
}

static bool N3DS_refreshRoomBorderSprite(
    const Room* room,
    C2D_Sprite* borderSprite,
    C2D_SpriteSheet* borderSheet,
    bool* haveRoomBorder,
    char* loadedBorderAssetName,
    size_t loadedBorderAssetNameSize
) {
    if (borderSprite == NULL || borderSheet == NULL || haveRoomBorder == NULL ||
        loadedBorderAssetName == NULL || loadedBorderAssetNameSize == 0) {
        return false;
    }

    const char* desiredAssetName = N3DS_getRoomBorderAssetName(room);
    if (strcmp(loadedBorderAssetName, desiredAssetName) == 0) return *haveRoomBorder;

    if (*borderSheet != NULL) {
        C2D_SpriteSheetFree(*borderSheet);
        *borderSheet = NULL;
    }

    memset(borderSprite, 0, sizeof(*borderSprite));
    *haveRoomBorder = N3DS_tryLoadRoomBorderSprite(desiredAssetName, borderSprite, borderSheet);
    if (!*haveRoomBorder && strcmp(desiredAssetName, "border_none") != 0) {
        *haveRoomBorder = N3DS_tryLoadRoomBorderSprite("border_none", borderSprite, borderSheet);
        snprintf(loadedBorderAssetName, loadedBorderAssetNameSize, "%s", *haveRoomBorder ? "border_none" : desiredAssetName);
        return *haveRoomBorder;
    }

    snprintf(loadedBorderAssetName, loadedBorderAssetNameSize, "%s", desiredAssetName);
    return *haveRoomBorder;
}

static void N3DS_drawFallbackRoomBorder(void) {
    const float screenW = 400.0f;
    const float screenH = 240.0f;
    const float outer = 6.0f;
    const float inner = 3.0f;
    const uint32_t outerColor = C2D_Color32(24, 18, 8, 255);
    const uint32_t trimColor = C2D_Color32(208, 170, 84, 255);
    const uint32_t highlightColor = C2D_Color32(255, 232, 170, 255);

    const float overlayDepth = 1.0f;

    C2D_DrawRectSolid(0.0f, 0.0f, overlayDepth, screenW, outer, outerColor);
    C2D_DrawRectSolid(0.0f, screenH - outer, overlayDepth, screenW, outer, outerColor);
    C2D_DrawRectSolid(0.0f, outer, overlayDepth, outer, screenH - outer * 2.0f, outerColor);
    C2D_DrawRectSolid(screenW - outer, outer, overlayDepth, outer, screenH - outer * 2.0f, outerColor);

    C2D_DrawRectSolid(outer, outer, overlayDepth, screenW - outer * 2.0f, inner, trimColor);
    C2D_DrawRectSolid(outer, screenH - outer - inner, overlayDepth, screenW - outer * 2.0f, inner, trimColor);
    C2D_DrawRectSolid(outer, outer + inner, overlayDepth, inner, screenH - (outer + inner) * 2.0f, trimColor);
    C2D_DrawRectSolid(screenW - outer - inner, outer + inner, overlayDepth, inner, screenH - (outer + inner) * 2.0f, trimColor);

    C2D_DrawRectSolid(outer + inner, outer + inner, overlayDepth, screenW - (outer + inner) * 2.0f, 1.0f, highlightColor);
    C2D_DrawRectSolid(outer + inner, screenH - outer - inner - 1.0f, overlayDepth, screenW - (outer + inner) * 2.0f, 1.0f, highlightColor);
}

static char* chooseDataWinPath(void) {
    if (fileExists("romfs:/data.win")) return safeStrdup("romfs:/data.win");
    return safeStrdup("sdmc:/3ds/cinnamon/data.win");
}

static void syncKey(RunnerKeyboardState* keyboard, bool* state, int32_t key, bool held) {
    if (held && !*state) RunnerKeyboard_onKeyDown(keyboard, key);
    if (!held && *state) RunnerKeyboard_onKeyUp(keyboard, key);
    *state = held;
}

static void N3DSLoadingScreen_free(N3DSLoadingScreen* screen) {
    if (screen == NULL) return;
    if (screen->textBuf != NULL) {
        C2D_TextBufDelete(screen->textBuf);
        screen->textBuf = NULL;
    }
    if (screen->target != NULL) {
        C3D_RenderTargetDelete(screen->target);
        screen->target = NULL;
    }
}

static void N3DS_appendBootLog(const char* message) {
    if (message == NULL || message[0] == '\0') return;

    if (gN3DSBootLogLineCount < N3DS_BOOT_LOG_MAX_LINES) {
        snprintf(
            gN3DSBootLogLines[gN3DSBootLogLineCount],
            sizeof(gN3DSBootLogLines[gN3DSBootLogLineCount]),
            "%s",
            message
        );
        gN3DSBootLogLineCount++;
        return;
    }

    repeat(N3DS_BOOT_LOG_MAX_LINES - 1, i) {
        snprintf(
            gN3DSBootLogLines[i],
            sizeof(gN3DSBootLogLines[i]),
            "%s",
            gN3DSBootLogLines[i + 1]
        );
    }
    snprintf(
        gN3DSBootLogLines[N3DS_BOOT_LOG_MAX_LINES - 1],
        sizeof(gN3DSBootLogLines[N3DS_BOOT_LOG_MAX_LINES - 1]),
        "%s",
        message
    );
}

void Runner_platformBootLog(const char* message) {
    N3DS_appendBootLog(message);
}

static void N3DSLoadingScreen_draw(N3DSLoadingScreen* screen) {
    if (screen == NULL) return;

    float progress = 0.0f;
    if (screen->totalChunks > 0) {
        progress = (float) screen->chunkIndex / (float) screen->totalChunks;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
    }

    if (screen->useCitro2D && screen->target != NULL && screen->textBuf != NULL) {
        const float barX = 36.0f;
        const float barY = 146.0f;
        const float barW = 328.0f;
        const float barH = 18.0f;
        const float fillW = (barW - 4.0f) * progress;

        C2D_Text title;
        C2D_Text status;
        C2D_Text detail;
        char detailLine[64];
        snprintf(detailLine, sizeof(detailLine), "%d / %d chunks", screen->chunkIndex, screen->totalChunks);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(screen->target, C2D_Color32(8, 10, 14, 255));
        C2D_SceneBegin(screen->target);

        C2D_DrawRectSolid(0.0f, 0.0f, 0.5f, 400.0f, 240.0f, C2D_Color32(8, 10, 14, 255));
        C2D_DrawRectSolid(barX, barY, 0.5f, barW, barH, C2D_Color32(42, 48, 60, 255));
        C2D_DrawRectSolid(barX + 2.0f, barY + 2.0f, 0.5f, fillW, barH - 4.0f, C2D_Color32(110, 224, 160, 255));

        C2D_TextBufClear(screen->textBuf);
        C2D_TextParse(&title, screen->textBuf, "Loading Game Data");
        C2D_TextParse(&status, screen->textBuf, screen->statusLine);
        C2D_TextParse(&detail, screen->textBuf, detailLine);
        C2D_TextOptimize(&title);
        C2D_TextOptimize(&status);
        C2D_TextOptimize(&detail);

        C2D_DrawText(&title, C2D_WithColor, 36.0f, 74.0f, 0.5f, 0.60f, 0.60f, C2D_Color32(255, 255, 255, 255));
        C2D_DrawText(&status, C2D_WithColor, 36.0f, 104.0f, 0.5f, N3DS_LOADING_TEXT_SCALE, N3DS_LOADING_TEXT_SCALE, C2D_Color32(210, 214, 224, 255));
        C2D_DrawText(&detail, C2D_WithColor, 36.0f, 172.0f, 0.5f, 0.34f, 0.34f, C2D_Color32(146, 153, 168, 255));

        int firstLogLine = gN3DSBootLogLineCount > 3 ? gN3DSBootLogLineCount - 3 : 0;
        float logY = 188.0f;
        for (int i = firstLogLine; i < gN3DSBootLogLineCount; ++i) {
            C2D_Text logLine;
            C2D_TextParse(&logLine, screen->textBuf, gN3DSBootLogLines[i]);
            C2D_TextOptimize(&logLine);
            C2D_DrawText(&logLine, C2D_WithColor, 36.0f, logY, 0.5f, 0.25f, 0.25f, C2D_Color32(164, 176, 192, 255));
            logY += 12.0f;
        }

        C3D_FrameEnd(0);
        gspWaitForVBlank();
        return;
    }

    int filled = (int) lroundf(progress * 24.0f);
    if (filled < 0) filled = 0;
    if (filled > 24) filled = 24;

    char bar[25];
    repeat(24, i) {
        bar[i] = (int) i < filled ? '#' : '-';
    }
    bar[24] = '\0';

    consoleSelect(&screen->console);
    printf("\x1b[2J");
    printf("\x1b[4;8HLoading Game Data");
    printf("\x1b[7;8H%s", screen->statusLine);
    printf("\x1b[10;8H[%s]", bar);
    printf("\x1b[12;8H%d / %d chunks", screen->chunkIndex, screen->totalChunks);
    printf("\x1b[15;8HPlease wait...");
    int firstLogLine = gN3DSBootLogLineCount > 5 ? gN3DSBootLogLineCount - 5 : 0;
    for (int i = firstLogLine; i < gN3DSBootLogLineCount; ++i) {
        printf("\x1b[%d;4H%s", 17 + (i - firstLogLine), gN3DSBootLogLines[i]);
    }

    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

static void N3DSLoadingScreen_set(N3DSLoadingScreen* screen, const char* statusLine, int chunkIndex, int totalChunks) {
    if (screen == NULL) return;
    snprintf(screen->statusLine, sizeof(screen->statusLine), "%s", statusLine != NULL ? statusLine : "");
    screen->chunkIndex = chunkIndex;
    screen->totalChunks = totalChunks;
    N3DSLoadingScreen_draw(screen);
}

static void N3DS_waitForStartExitScreen(N3DSLoadingScreen* screen, const char* statusLine) {
    N3DSLoadingScreen_set(screen, statusLine, 1, 1);
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
}

static s64 N3DS_ticksToNs(u64 ticks) {
    return (s64) ((ticks * 1000000000ULL) / SYSCLOCK_ARM11);
}

static void N3DS_sleepUntilTick(u64 targetTick) {
    const u64 coarseGuardTicks = SYSCLOCK_ARM11 / 2000u; // ~0.5 ms

    while (true) {
        u64 now = svcGetSystemTick();
        if (now >= targetTick) return;

        u64 remaining = targetTick - now;
        if (remaining <= coarseGuardTicks) break;

        svcSleepThread(N3DS_ticksToNs(remaining - coarseGuardTicks));
    }

    while (svcGetSystemTick() < targetTick) {
    }
}

static void N3DS_beginPacedFrame(u64* nextFrameTick, u64 frameTicks) {
    if (nextFrameTick == NULL || frameTicks == 0) return;

    u64 now = svcGetSystemTick();
    if (*nextFrameTick == 0) {
        *nextFrameTick = now;
    } else if (now > *nextFrameTick + frameTicks * 4u) {
        *nextFrameTick = now;
    } else {
        while (now > *nextFrameTick) {
            *nextFrameTick += frameTicks;
        }
    }

    N3DS_sleepUntilTick(*nextFrameTick);
    *nextFrameTick += frameTicks;
}

static void N3DSDataWinProgressCallback(const char* chunkName, int chunkIndex, int totalChunks, MAYBE_UNUSED DataWin* dataWin, void* userData) {
    N3DSLoadingScreen* screen = (N3DSLoadingScreen*) userData;
    if (screen == NULL) return;

    char status[128];
    snprintf(status, sizeof(status), "Parsing chunk %.4s", chunkName != NULL ? chunkName : "----");
    N3DSLoadingScreen_set(screen, status, chunkIndex + 1, totalChunks);
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    gfxInitDefault();
    romfsInit();
    APT_SetAppCpuTimeLimit(30);
    osSetSpeedupEnable(true);

    bool citroReady = false;

    if (C3D_Init(0x80000) && C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C2D_Prepare();
        citroReady = true;
    } else {
        C2D_Fini();
        C3D_Fini();
    }

    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/cinnamon", 0777);

    N3DSInputProfile inputProfile;
    N3DSInputProfile_load(&inputProfile, "romfs:/cinnamon_input_profile.cfg");
    N3DSTouchState touchState = {0};
    N3DSBattleFollowController battleFollowController;
    N3DSBattleFollowController_reset(&battleFollowController, inputProfile.battleFollowStepHint);
    N3DSBattleMenuTouchController battleMenuTouchController = {0};

    char* dataWinPath = chooseDataWinPath();

    N3DSLoadingScreen loadingScreen = {0};
    if (citroReady) {
        loadingScreen.useCitro2D = true;
        loadingScreen.target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        loadingScreen.textBuf = C2D_TextBufNew(512);
        if (loadingScreen.target == NULL || loadingScreen.textBuf == NULL) {
            N3DSLoadingScreen_free(&loadingScreen);
            loadingScreen.useCitro2D = false;
        }
    } else {
        consoleInit(GFX_TOP, &loadingScreen.console);
    }

    N3DSLoadingScreen_set(&loadingScreen, "Scanning data.win", 0, 1);

    DataWin* dataWin = DataWin_parse(
        dataWinPath,
        (DataWinParserOptions) {
            .parseGen8 = true,
            .parseOptn = true,
            .parseLang = true,
            .parseExtn = false,
            .parseSond = true,
            .parseAgrp = true,
            .parseSprt = true,
            .parseBgnd = true,
            .parsePath = true,
            .parseScpt = true,
            .parseGlob = true,
            .parseShdr = true,
            .parseFont = true,
            .parseTmln = true,
            .parseObjt = true,
            .parseRoom = true,
            .parseTpag = true,
            .parseCode = true,
            .parseVari = true,
            .parseFunc = true,
            .parseStrg = true,
            .parseTxtr = false,
            .parseAudo = false,
            .skipLoadingPreciseMasksForNonPreciseSprites = true,
            .progressCallback = N3DSDataWinProgressCallback,
            .progressCallbackUserData = &loadingScreen,
        }
    );

    free(dataWinPath);

    if (dataWin == NULL) {
        N3DS_waitForStartExitScreen(&loadingScreen, "Failed to load data.win. Press START.");
        N3DSLoadingScreen_free(&loadingScreen);
        return 1;
    }

    FileSystem* fileSystem = (FileSystem*) N3DSFileSystem_create("romfs:/", "sdmc:/3ds/cinnamon/");
    AudioSystem* audioSystem = (AudioSystem*) N3DSAudioSystem_create();
    N3DSLoadingScreen_set(&loadingScreen, "Loading sound bank", 1, 2);
    audioSystem->vtable->init(audioSystem, dataWin, fileSystem);
    if (!N3DSAudioSystem_isReady(audioSystem)) {
        const char* audioError = N3DSAudioSystem_getStartupError(audioSystem);
        N3DS_waitForStartExitScreen(
            &loadingScreen,
            audioError != NULL ? audioError : "Audio init failed. Press START."
        );
        audioSystem->vtable->destroy(audioSystem);
        N3DSFileSystem_destroy((N3DSFileSystem*) fileSystem);
        DataWin_free(dataWin);
        N3DSLoadingScreen_free(&loadingScreen);
        if (citroReady) {
            C2D_Fini();
            C3D_Fini();
        }
        romfsExit();
        gfxExit();
        return 1;
    }

    N3DSLoadingScreen_set(&loadingScreen, "Initializing renderer", 2, 2);
    VMContext* vm = VM_create(dataWin);
    Renderer* renderer = N3DSRenderer_create();
    Runner* runner = Runner_create(dataWin, vm, renderer, fileSystem, audioSystem);

    if (!N3DSRenderer_isReady(renderer)) {
        const char* error = N3DSRenderer_getStartupError(renderer);
        N3DS_waitForStartExitScreen(&loadingScreen, error != NULL ? error : "Renderer init failed. Press START.");
        renderer->vtable->destroy(renderer);
        audioSystem->vtable->destroy(audioSystem);
        Runner_free(runner);
        N3DSFileSystem_destroy((N3DSFileSystem*) fileSystem);
        VM_free(vm);
        DataWin_free(dataWin);
        N3DSLoadingScreen_free(&loadingScreen);
        if (citroReady) {
            C2D_Fini();
            C3D_Fini();
        }
        romfsExit();
        gfxExit();
        return 1;
    }

    N3DSLoadingScreen_free(&loadingScreen);
    N3DSDebugMonitor debugMonitor;
    N3DSDebugMonitor_init(&debugMonitor);

    runner->osType = OS_3DS;
    Runner_initFirstRoom(runner);

    bool haveRoomBorder = false;
    C2D_Sprite roomBorder;
    memset(&roomBorder, 0, sizeof(roomBorder));
    C2D_SpriteSheet roomBorderSheet = NULL;
    char loadedRoomBorderAsset[32] = "";
    bool debugMonitorVisible = false;
    N3DS_refreshRoomBorderSprite(runner->currentRoom, &roomBorder, &roomBorderSheet, &haveRoomBorder, loadedRoomBorderAsset, sizeof(loadedRoomBorderAsset));

    bool leftHeld = false, rightHeld = false, upHeld = false, downHeld = false;
    bool aHeld = false, bHeld = false, yHeld = false;
    bool lHeld = false, rHeld = false;
    bool touchActionHeld = false;

    Gen8* gen8 = &dataWin->gen8;
    int32_t gameW = (int32_t) gen8->defaultWindowWidth;
    int32_t gameH = (int32_t) gen8->defaultWindowHeight;

    const u64 frameTicks = (SYSCLOCK_ARM11 + 15u) / 30u;
    u64 nextFrameTick = svcGetSystemTick();
    while (aptMainLoop() && !runner->shouldExit) {
        N3DS_beginPacedFrame(&nextFrameTick, frameTicks);
        hidScanInput();
        u32 held = hidKeysHeld();
        u32 down = hidKeysDown();
        u32 up = hidKeysUp();

        if (down & KEY_SELECT) debugMonitorVisible = !debugMonitorVisible;
        bool debugWarpToAsriel = debugMonitorVisible && (down & KEY_L) &&
            (uint32_t) N3DS_DEBUG_ASRIEL_ROOM < runner->dataWin->room.count;
        if (debugWarpToAsriel) {
            runner->pendingRoom = N3DS_DEBUG_ASRIEL_ROOM;
        }

        N3DSTouchState_update(&touchState, &inputProfile, held, down, up);
        if (down & KEY_TOUCH) {
            N3DSBattleFollowController_reset(&battleFollowController, inputProfile.battleFollowStepHint);
            N3DSBattleMenuTouchController_reset(&battleMenuTouchController);
        }

        bool touchLeft = false;
        bool touchRight = false;
        bool touchUp = false;
        bool touchDown = false;
        bool usedBattleFollow = false;
        if (inputProfile.touchEnabled && touchState.active && inputProfile.battleFollowEnabled) {
            float activity = 0.0f;
            float currentX = 0.0f;
            float currentY = 0.0f;
            float targetX = 0.0f;
            float targetY = 0.0f;
            N3DSBattleTouchStatus followStatus = Runner_get3DSBattleTouchTarget(
                runner,
                inputProfile.battleFollowTargetToken,
                inputProfile.battleFollowTargetObjectIndex,
                inputProfile.battleFollowActiveSelfVar,
                (float) inputProfile.battleFollowActiveGreaterThan,
                (float) touchState.current.px,
                (float) touchState.current.py,
                &activity,
                &currentX,
                &currentY,
                &targetX,
                &targetY
            );
            debugMonitor.touchFollowStatus = followStatus;
            debugMonitor.touchFollowActivity = activity;
            debugMonitor.touchFollowCurrentX = currentX;
            debugMonitor.touchFollowCurrentY = currentY;
            debugMonitor.touchFollowTargetX = targetX;
            debugMonitor.touchFollowTargetY = targetY;

            if (followStatus == N3DS_BATTLE_TOUCH_READY) {
                float errorX = targetX - currentX;
                float errorY = targetY - currentY;
                float distance = sqrtf(errorX * errorX + errorY * errorY);
                float activationDistance = fmaxf(
                    (float) inputProfile.battleFollowTolerance * 2.0f,
                    inputProfile.battleFollowStepHint * 2.0f
                );
                u64 touchAgeMs = osGetTime() - touchState.startMs;

                if (touchState.tapCancelled || battleFollowController.engaged ||
                    (distance > activationDistance && touchAgeMs >= inputProfile.battleFollowActivationMs)) {
                    battleFollowController.engaged = true;
                    touchState.tapCancelled = true;
                    N3DSBattleFollowController_compute(
                        &battleFollowController,
                        activity,
                        currentX,
                        currentY,
                        targetX,
                        targetY,
                        (float) inputProfile.battleFollowTolerance,
                        &touchLeft,
                        &touchRight,
                        &touchUp,
                        &touchDown
                    );
                    usedBattleFollow = true;
                }
            }
        }

        if (inputProfile.touchEnabled && touchState.active && touchState.tapCancelled && !usedBattleFollow) {
            int32_t dx = (int32_t) touchState.current.px - (int32_t) touchState.start.px;
            int32_t dy = (int32_t) touchState.current.py - (int32_t) touchState.start.py;
            touchLeft = dx < -inputProfile.dragDeadzonePx;
            touchRight = dx > inputProfile.dragDeadzonePx;
            touchUp = dy < -inputProfile.dragDeadzonePx;
            touchDown = dy > inputProfile.dragDeadzonePx;
        }
        if (!touchState.active && (up & KEY_TOUCH)) {
            N3DSBattleFollowController_reset(&battleFollowController, inputProfile.battleFollowStepHint);
        }
        bool touchTap = inputProfile.touchEnabled && touchState.tapPulseFrames > 0u;

        bool menuLeft = false;
        bool menuRight = false;
        bool menuUp = false;
        bool menuDown = false;
        bool menuConfirm = false;
        bool menuCancel = false;

        if (inputProfile.battleMenuTouchEnabled && touchTap) {
            N3DSBattleMenuState menuState;
            float menuTouchX = 0.0f;
            float menuTouchY = 0.0f;
            if (Runner_get3DSBattleMenuState(runner, &menuState) &&
                Runner_map3DSBottomBattleUITouch(
                    runner,
                    (float) touchState.current.px,
                    (float) touchState.current.py,
                    &menuTouchX,
                    &menuTouchY
                )) {
                N3DSBattleMenuTouchResult menuTouchResult =
                    N3DSBattleMenuTouchController_capture(
                        &battleMenuTouchController,
                        &menuState,
                        menuTouchX,
                        menuTouchY
                    );
                if (menuTouchResult != N3DS_BATTLE_MENU_TOUCH_PASS_THROUGH) {
                    touchTap = false;
                    touchState.tapPulseFrames = 0u;
                }
                if (menuTouchResult == N3DS_BATTLE_MENU_TOUCH_CANCEL) {
                    menuCancel = true;
                }
            }
        }

        u32 physicalMenuKeys = KEY_A | KEY_B | KEY_X | KEY_Y | KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN;
        if (down & physicalMenuKeys) {
            N3DSBattleMenuTouchController_reset(&battleMenuTouchController);
        }
        if (inputProfile.battleMenuTouchEnabled) {
            N3DSBattleMenuTouchController_step(
                &battleMenuTouchController,
                runner,
                &menuLeft,
                &menuRight,
                &menuUp,
                &menuDown,
                &menuConfirm
            );
        }

        touchLeft = touchLeft || menuLeft;
        touchRight = touchRight || menuRight;
        touchUp = touchUp || menuUp;
        touchDown = touchDown || menuDown;

        circlePosition circle;
        hidCircleRead(&circle);

        bool leftInput = (held & KEY_LEFT) || circle.dx < -80 || touchLeft;
        bool rightInput = (held & KEY_RIGHT) || circle.dx > 80 || touchRight;
        bool upInput = (held & KEY_UP) || circle.dy > 80 || touchUp;
        bool downInput = (held & KEY_DOWN) || circle.dy < -80 || touchDown;
        bool aInput = (held & KEY_A) || menuConfirm || (touchTap && inputProfile.tapKeyCode == 'Z');
        bool bInput = (held & KEY_B) || menuCancel || (touchTap && inputProfile.tapKeyCode == 'X');
        bool cInput = (held & KEY_Y) || (held & KEY_X) || (touchTap && inputProfile.tapKeyCode == 'C');

        if (touchTap && inputProfile.tapKeyCode == VK_LEFT) leftInput = true;
        if (touchTap && inputProfile.tapKeyCode == VK_RIGHT) rightInput = true;
        if (touchTap && inputProfile.tapKeyCode == VK_UP) upInput = true;
        if (touchTap && inputProfile.tapKeyCode == VK_DOWN) downInput = true;

        syncKey(runner->keyboard, &leftHeld, VK_LEFT, leftInput);
        syncKey(runner->keyboard, &rightHeld, VK_RIGHT, rightInput);
        syncKey(runner->keyboard, &upHeld, VK_UP, upInput);
        syncKey(runner->keyboard, &downHeld, VK_DOWN, downInput);

        syncKey(runner->keyboard, &aHeld, 'Z', aInput);
        syncKey(runner->keyboard, &bHeld, 'X', bInput);
        syncKey(runner->keyboard, &yHeld, 'C', cInput);
        if (inputProfile.tapKeyCode != 'Z' && inputProfile.tapKeyCode != 'X' && inputProfile.tapKeyCode != 'C' &&
            inputProfile.tapKeyCode != VK_LEFT && inputProfile.tapKeyCode != VK_RIGHT &&
            inputProfile.tapKeyCode != VK_UP && inputProfile.tapKeyCode != VK_DOWN) {
            syncKey(runner->keyboard, &touchActionHeld, inputProfile.tapKeyCode, touchTap);
        }
        syncKey(runner->keyboard, &lHeld, VK_PAGEDOWN, (held & KEY_L) && !debugWarpToAsriel);
        syncKey(runner->keyboard, &rHeld, VK_PAGEUP, (held & KEY_R));

        u64 stepStartTick = svcGetSystemTick();
        Runner_step(runner);
        double frameStepMs = N3DSPerf_ticksToMs(svcGetSystemTick() - stepStartTick);
        N3DS_tryTriggerAsrielLed(runner);
        runner->audioSystem->vtable->update(runner->audioSystem, 1.0f / 30.0f);
        u64 battlePrewarmStartTick = svcGetSystemTick();
        bool battleTransitionPrewarmActive = N3DSRenderer_tickBattlePrewarm(renderer, runner);
        double battlePrewarmTickMs = N3DSPerf_ticksToMs(svcGetSystemTick() - battlePrewarmStartTick);
        N3DS_refreshRoomBorderSprite(runner->currentRoom, &roomBorder, &roomBorderSheet, &haveRoomBorder, loadedRoomBorderAsset, sizeof(loadedRoomBorderAsset));

        float displayScaleX = 1.0f;
        float displayScaleY = 1.0f;
        Runner_computeViewDisplayScale(runner, gameW, gameH, &displayScaleX, &displayScaleY);
        N3DSRenderPerfSample renderPerf;
        memset(&renderPerf, 0, sizeof(renderPerf));
        u64 renderStartTick = svcGetSystemTick();

        u64 phaseTick = svcGetSystemTick();
        C3D_FrameBegin(0);
        renderPerf.frameBeginMs = N3DSPerf_ticksToMs(svcGetSystemTick() - phaseTick);
        // C3D_FrameBegin waits for the previous GPU queue. Once it returns,
        // C3D_GetDrawingTime() is the completed drawing time for that previous frame.
        renderPerf.previousGpuDrawingMs = (double) C3D_GetDrawingTime();

        Runner_resetFrameDrawStats(runner);

        phaseTick = svcGetSystemTick();
        renderer->vtable->beginFrame(renderer, gameW, gameH, 400, 240);
        renderPerf.rendererBeginMs = N3DSPerf_ticksToMs(svcGetSystemTick() - phaseTick);

        phaseTick = svcGetSystemTick();
        Runner_drawViews(runner, gameW, gameH, displayScaleX, displayScaleY, false);
        renderPerf.drawViewsMs = N3DSPerf_ticksToMs(svcGetSystemTick() - phaseTick);

        phaseTick = svcGetSystemTick();
        N3DSRenderer_beginOverlay(renderer);
        if (haveRoomBorder) C2D_DrawSprite(&roomBorder);
        else N3DS_drawFallbackRoomBorder();
        if (debugMonitorVisible) {
            N3DSDebugMonitor_draw(&debugMonitor, runner, renderer);
        }
        renderPerf.overlayMs = N3DSPerf_ticksToMs(svcGetSystemTick() - phaseTick);

        phaseTick = svcGetSystemTick();
        renderer->vtable->endFrame(renderer);
        renderPerf.rendererEndMs = N3DSPerf_ticksToMs(svcGetSystemTick() - phaseTick);
        // Cmd-buffer usage is meaningful while the current frame's command buffer is still bound.
        renderPerf.cmdBufUsagePercent = (double) C3D_GetCmdBufUsage() * 100.0;

        phaseTick = svcGetSystemTick();
        C3D_FrameEnd(0);
        renderPerf.frameEndMs = N3DSPerf_ticksToMs(svcGetSystemTick() - phaseTick);
        renderPerf.c3dProcessingMs = (double) C3D_GetProcessingTime();
        renderPerf.totalMs = N3DSPerf_ticksToMs(svcGetSystemTick() - renderStartTick);

        N3DSDebugMonitor_tickFrame(&debugMonitor, renderPerf.totalMs);
        N3DSDustPerfFrame perfSample = N3DSDustPerf_captureFrame(
            runner,
            renderer,
            frameStepMs,
            battlePrewarmTickMs,
            battleTransitionPrewarmActive,
            &renderPerf
        );
        if (perfSample.roomTransitioned != 0) {
            gN3DSPerfBattleRoomActive = perfSample.prewarmBattleRoom != 0;
        } else if (perfSample.battleActive != 0) {
            gN3DSPerfBattleRoomActive = true;
        }
        N3DSDustPerf_update(&gN3DSDustPerfSampler, runner, &perfSample);
        N3DSAttackPerf_update(&gN3DSAttackPerfSampler, runner, &perfSample);
        N3DSBattleEntryPerf_update(&gN3DSBattleEntryPerfSampler, runner, &perfSample);
        if (!gN3DSPerfBattleRoomActive &&
            perfSample.battleActive == 0 &&
            !gN3DSDustPerfSampler.active &&
            !gN3DSAttackPerfSampler.active &&
            !gN3DSBattleEntryPerfSampler.active) {
            N3DSPerf_flushQueuedEvents(runner);
        }

        RunnerKeyboard_beginFrame(runner->keyboard);
        if (touchState.tapPulseFrames > 0u) touchState.tapPulseFrames--;
    }

    N3DSDustPerf_flushPending(&gN3DSDustPerfSampler);
    N3DSAttackPerf_flushPending(&gN3DSAttackPerfSampler);
    N3DSBattleEntryPerf_flushPending(&gN3DSBattleEntryPerfSampler);
    N3DSPerf_flushQueuedEvents(runner);

    renderer->vtable->destroy(renderer);
    audioSystem->vtable->destroy(audioSystem);
    Runner_free(runner);
    N3DSFileSystem_destroy((N3DSFileSystem*) fileSystem);
    VM_free(vm);
    DataWin_free(dataWin);
    N3DSDebugMonitor_free(&debugMonitor);

    if (roomBorderSheet != NULL) {
        C2D_SpriteSheetFree(roomBorderSheet);
    }

    if (citroReady) {
        C2D_Fini();
        C3D_Fini();
    }

    romfsExit();
    gfxExit();
    return 0;
}
