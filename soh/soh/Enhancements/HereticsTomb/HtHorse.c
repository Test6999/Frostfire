// Heretics Tomb: the black horse (HtHorse.h) - its animations, the darkness and flames while it's ridden, and the
// stable where Link swaps horses.
#include "HtHorse.h"
#include <math.h>
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "objects/object_horse_ganon/object_horse_ganon.h"
#include "HtLynelBuild.h" // Epona's refusal and jumps on it: retargeted from the player's game data
#include <stdlib.h>
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/frame_interpolation.h"

#define HT_HORSE_JOINTS 54

// ---- its animations: Ganon's horse's own, and Epona's refusal and jumps on it (built here into animations) ----
static AnimationHeader sHtRefuse, sHtJumpLow, sHtJumpHigh;
static s16* sHtRefuseData;
static s16* sHtJumpLowData;
static s16* sHtJumpHighData;
static JointIndex sHtRefuseIdx[HT_HORSE_JOINTS], sHtJumpLowIdx[HT_HORSE_JOINTS], sHtJumpHighIdx[HT_HORSE_JOINTS];

// every component a run of its frames: index c * n (none static)
static void HtHorse_BuildAnim(AnimationHeader* h, s16* data, JointIndex* idx, const Vec3s* frames, s32 n) {
    s32 j, f;

    for (j = 0; j < HT_HORSE_JOINTS; j++) {
        idx[j].x = (3 * j + 0) * n;
        idx[j].y = (3 * j + 1) * n;
        idx[j].z = (3 * j + 2) * n;
        for (f = 0; f < n; f++) {
            const Vec3s* v = &frames[f * HT_HORSE_JOINTS + j];

            data[(3 * j + 0) * n + f] = v->x;
            data[(3 * j + 1) * n + f] = v->y;
            data[(3 * j + 2) * n + f] = v->z;
        }
    }
    h->common.frameCount = n;
    h->frameData = data;
    h->jointIndices = idx;
    h->staticIndexMax = 0;
}

void* HtHorse_AnimTable(void) {
    static AnimationHeader* sTable[9];
    static s32 sBuilt = false;

    if (!sBuilt) {
        s32 have = HtLynelBuild_HorseAnims();

        if (have) {
            sHtRefuseData = malloc(sizeof(s16) * 3 * HT_HORSE_JOINTS * gHtHorseRefuseFrames);
            sHtJumpLowData = malloc(sizeof(s16) * 3 * HT_HORSE_JOINTS * gHtHorseJumpLowFrames);
            sHtJumpHighData = malloc(sizeof(s16) * 3 * HT_HORSE_JOINTS * gHtHorseJumpHighFrames);
            HtHorse_BuildAnim(&sHtRefuse, sHtRefuseData, sHtRefuseIdx, gHtHorseRefuse, gHtHorseRefuseFrames);
            HtHorse_BuildAnim(&sHtJumpLow, sHtJumpLowData, sHtJumpLowIdx, gHtHorseJumpLow, gHtHorseJumpLowFrames);
            HtHorse_BuildAnim(&sHtJumpHigh, sHtJumpHighData, sHtJumpHighIdx, gHtHorseJumpHigh, gHtHorseJumpHighFrames);
        }
        sTable[ENHORSE_ANIM_IDLE] = (AnimationHeader*)gHorseGanonIdleAnim;
        sTable[ENHORSE_ANIM_WHINNEY] = (AnimationHeader*)gHorseGanonWhinnyAnim;
        sTable[ENHORSE_ANIM_STOPPING] = have ? &sHtRefuse : (AnimationHeader*)gHorseGanonRearingAnim;
        sTable[ENHORSE_ANIM_REARING] = (AnimationHeader*)gHorseGanonRearingAnim;
        sTable[ENHORSE_ANIM_WALK] = (AnimationHeader*)gHorseGanonWalkingAnim;
        sTable[ENHORSE_ANIM_TROT] = (AnimationHeader*)gHorseGanonTrottingAnim;
        sTable[ENHORSE_ANIM_GALLOP] = (AnimationHeader*)gHorseGanonGallopingAnim;
        sTable[ENHORSE_ANIM_LOW_JUMP] = have ? &sHtJumpLow : (AnimationHeader*)gHorseGanonGallopingAnim;
        sTable[ENHORSE_ANIM_HIGH_JUMP] = have ? &sHtJumpHigh : (AnimationHeader*)gHorseGanonGallopingAnim;
        sBuilt = true;
    }
    return sTable;
}

// ---- ridden: the sky darkening (Ganondorf's own darkness), flames off its hooves at a gallop ----
static f32 sHtDark = 0.0f;
static s32 sHtDarkApplied = false;
static s32 sHtDarkFrame = -1;

static void HtHorse_UpdateDark(EnHorse* horse, PlayState* play, s32 ridden) {
    EnvironmentContext* env = &play->envCtx;
    s32 i;

    if (sHtDarkFrame == (s32)play->gameplayFrames) {
        return; // (one horse a frame does it)
    }
    sHtDarkFrame = play->gameplayFrames;
    Math_StepToF(&sHtDark, ridden ? 1.0f : 0.0f, ridden ? (1.0f / 45.0f) : (1.0f / 30.0f));
    if (sHtDark > 0.0f) {
        env->customSkyboxFilter = true;
        env->skyboxFilterColor[0] = 14;
        env->skyboxFilterColor[1] = 4;
        env->skyboxFilterColor[2] = 22;
        env->skyboxFilterColor[3] = (u8)(225.0f * sHtDark);
        for (i = 0; i < 3; i++) {
            env->adjAmbientColor[i] = (s16)(-70.0f * sHtDark);
            env->adjLight1Color[i] = (s16)(-95.0f * sHtDark);
            env->adjFogColor[i] = (s16)(-80.0f * sHtDark);
        }
        env->adjAmbientColor[2] += (s16)(20.0f * sHtDark); // (a cold, violet dark)
        env->adjFogColor[2] += (s16)(15.0f * sHtDark);
        sHtDarkApplied = true;
    } else if (sHtDarkApplied) {
        env->customSkyboxFilter = false;
        env->skyboxFilterColor[3] = 0;
        for (i = 0; i < 3; i++) {
            env->adjAmbientColor[i] = env->adjLight1Color[i] = env->adjFogColor[i] = 0;
        }
        sHtDarkApplied = false;
    }
}

// the flames: those trailing Ganondorf's horse in the Deku Tree's vision (En_Viewer's fire effects) - off its hind
// hooves at a gallop, left burning on the ground behind it a moment
#define HT_HORSE_FLAMES 40
typedef struct {
    Vec3f pos;
    s16 life;
    f32 scale;
    u16 epoch;
} HtHorseFlame;
static HtHorseFlame sHtFlames[HT_HORSE_FLAMES];
#define HT_FLAME_LIFE 18

static void HtHorse_Flames(EnHorse* horse, PlayState* play) {
    s32 i, k;

    for (i = 0; i < HT_HORSE_FLAMES; i++) {
        if (sHtFlames[i].life > 0) {
            sHtFlames[i].life--;
            sHtFlames[i].pos.y += 0.15f;
        }
    }
    s32 jumping = (horse->stateFlags & ENHORSE_JUMPING) != 0; // (trailing on through a jump, in the air)

    if (!jumping && ((horse->animationIdx != ENHORSE_ANIM_GALLOP) || (horse->actor.speedXZ < 6.0f) ||
                     !(horse->actor.bgCheckFlags & BGCHECKFLAG_GROUND))) {
        return;
    }
    Audio_PlaySfxGeneral(NA_SE_EV_BURNING - SFX_FLAG, &horse->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb); // (their roar, as in the vision)
    for (k = 2; k < 4; k++) { // (the hind hooves)
        for (i = 0; i < HT_HORSE_FLAMES; i++) {
            if (sHtFlames[i].life <= 0) {
                sHtFlames[i].pos = horse->htHooves[k];
                sHtFlames[i].pos.x += Rand_CenteredFloat(6.0f);
                sHtFlames[i].pos.z += Rand_CenteredFloat(6.0f);
                sHtFlames[i].pos.y = jumping ? (horse->htHooves[k].y - 4.0f) : horse->actor.world.pos.y;
                sHtFlames[i].life = HT_FLAME_LIFE;
                sHtFlames[i].scale = (Rand_ZeroOne() * 1.0f + 2.4f) * 0.001f; // (the vision's, a fifth the size: ankle-high)
                sHtFlames[i].epoch++;
                break;
            }
        }
    }
}

void HtHorse_Draw(EnHorse* horse, PlayState* play) {
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);
    for (i = 0; i < HT_HORSE_FLAMES; i++) {
        HtHorseFlame* f = &sHtFlames[i];
        f32 sc;

        if (f->life <= 0) {
            continue;
        }
        // (flaring up, then dying down)
        sc = f->scale * ((f->life > HT_FLAME_LIFE - 4) ? (0.4f + 0.15f * (HT_FLAME_LIFE - f->life))
                                                       : ((f32)f->life / (HT_FLAME_LIFE - 4)));
        FrameInterpolation_RecordOpenChild(f, f->epoch);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Matrix_Translate(f->pos.x, f->pos.y, f->pos.z, MTXMODE_NEW);
        Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0,
                                      (10 * i - 20 * play->state.frames) % 512, 32, 128, 0, 0, 0, -20));
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 170, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 50, 00, 255);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPMatrix(POLY_XLU_DISP++, SEG_ADDR(1, 0), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
        FrameInterpolation_RecordCloseChild();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- the stable: the horse waiting there - talk to it to swap ----
static void HtHorse_PlaceInStall(EnHorse* horse, PlayState* play);
static void HtHorse_UpdateStable(EnHorse* horse, PlayState* play) {
    Actor* a = &horse->actor;

    if (!horse->htPlaced) {
        HtHorse_PlaceInStall(horse, play);
    }
    // (it stays in its stall, facing the rail)
    a->world.pos.x = a->home.pos.x;
    a->world.pos.z = a->home.pos.z;
    a->speedXZ = 0.0f;
    a->shape.rot.y = a->world.rot.y = a->home.rot.y;

    if (horse->htTalking) {
        u8 state = Message_GetState(&play->msgCtx);

        if ((state == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
            if (play->msgCtx.choiceIndex == 0) {
                HtHorse_SetBlackActive(!HtHorse_BlackActive());
                Audio_PlayActorSound2(a, NA_SE_EV_HORSE_NEIGH);
                horse->htSwap = true; // (z_en_horse.c: the stall's horse changes over)
                Message_ContinueTextbox(play, HT_HORSE_SWAPPED_TEXT);
            } else {
                Message_CloseTextbox(play);
                horse->htTalking = false;
            }
        } else if (((state == TEXT_STATE_DONE) && Message_ShouldAdvance(play)) || Actor_TextboxIsClosing(a, play)) {
            horse->htTalking = false;
        }
        return;
    }
    a->textId = HT_HORSE_SWAP_TEXT;
    if (Actor_ProcessTalkRequest(a, play)) {
        horse->htTalking = true;
    } else if (a->xzDistToPlayer < 140.0f) {
        Actor_OfferTalk(a, play, 140.0f);
    }
}

// Lon Lon's stable (malon_stable) has no horse of Link's in it, so the one waiting there is spawned, and once the
// stable's animals are in their places (a cow can be moved a stall over in its init, a frame or so after the scene's
// actors spawn) put in a free stall of the row behind the rail (stalls 120 wide, their middles at x -120 .. 480): the
// middle one, between the two cows, when it's free, else the next free one. Talked to over the rail.
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId); // (z_scene.c)
#define HT_STABLE_Z -245.0f
static const f32 sStableStalls[] = { 0.0f, 120.0f, 480.0f, -120.0f };

static void HtHorse_PlaceInStall(EnHorse* horse, PlayState* play) {
    Actor* a;
    s32 i, cat;
    f32 x = sStableStalls[0];

    for (cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (a = play->actorCtx.actorLists[cat].head; a != NULL; a = a->next) {
            if (((a->id == ACTOR_EN_COW) || (a->id == ACTOR_EN_HORSE_NORMAL)) && (a->init != NULL)) {
                return; // (not in its place yet)
            }
        }
    }
    for (i = 0; i < ARRAY_COUNT(sStableStalls); i++) {
        bool taken = false;

        for (cat = 0; (cat < ACTORCAT_MAX) && !taken; cat++) {
            for (a = play->actorCtx.actorLists[cat].head; a != NULL; a = a->next) {
                if (((a->id == ACTOR_EN_COW) || (a->id == ACTOR_EN_HORSE_NORMAL)) && (a->world.pos.z < -180.0f) &&
                    (fabsf(a->world.pos.x - sStableStalls[i]) < 60.0f)) {
                    taken = true;
                    break;
                }
            }
        }
        if (!taken) {
            x = sStableStalls[i];
            break;
        }
    }
    horse->actor.home.pos.x = horse->actor.world.pos.x = horse->actor.prevPos.x = x;
    horse->actor.home.pos.z = horse->actor.world.pos.z = horse->actor.prevPos.z = HT_STABLE_Z;
    horse->htPlaced = true;
}

void HtHorse_OnSpawnActors(PlayState* play) {
    Actor* a;

    if ((play->sceneNum != SCENE_STABLE) || !LINK_IS_ADULT || !HtHorse_BlackOwned() || !HtHorse_EponaObtained()) {
        return;
    }
    for (a = play->actorCtx.actorLists[ACTORCAT_BG].head; a != NULL; a = a->next) {
        if ((a->id == ACTOR_EN_HORSE) && ((EnHorse*)a)->htStable) {
            return;
        }
    }
    if (Object_GetIndex(&play->objectCtx, OBJECT_HORSE) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_HORSE); // (Epona's object: the stable doesn't load it)
    }
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HORSE, sStableStalls[0], 0.0f, HT_STABLE_Z, 0, 0, 0, 1);
}

void HtHorse_Update(EnHorse* horse, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (horse->htStable) {
        HtHorse_UpdateStable(horse, play);
        return;
    }
    if (!horse->htBlack) {
        return;
    }
    HtHorse_UpdateDark(horse, play,
                       (player->rideActor == &horse->actor) && (player->stateFlags1 & PLAYER_STATE1_ON_HORSE));
    HtHorse_Flames(horse, play);
}
