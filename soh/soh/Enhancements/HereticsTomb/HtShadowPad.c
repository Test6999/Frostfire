// Heretics Tomb's door in Hyrule Field: the shadow pad and the culvert's bars. See HtShadowPad.h.

#include "HtShadowPad.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "z64player.h"
#include "soh/ResourceManagerHelpers.h"
#include <math.h>

#include <string.h>

// the field's bars, across the culvert at the stream's west end (spot00 room 0): a quad on the wall's plane x -5889,
// y -380..-300 (under the lintel), z 634..1034; vertices 43..46 of its room mesh's buffer, collision triangles 182, 183
#define HT_BARS_VTX_PATH "__OTR__scenes/shared/spot00_scene/spot00_room_0Vtx_002340"
#define HT_BARS_VTX_PATH_OLD "__OTR__scenes/nonmq/spot00_scene/spot00_room_0Vtx_002340"
#define HT_BARS_VTX0 43
#define HT_BARS_PLANE_X -5889
#define HT_BARS_BACK 4       // risen, they slide up just behind the wall, which hides them
#define HT_BARS_RISE 82.0f   // all the way up behind the lintel (y -300..-220)
#define HT_BARS_POLY0 182
#define HT_BARS_POLY_IGNORE 0xC000 // (entities and projectiles go through)
#define HT_BARS_MID_X -5889.0f
#define HT_BARS_MID_Y -340.0f
#define HT_BARS_MID_Z 834.0f
#define HT_WATER_Y -315.0f

// in through the culvert: past the bars' plane, inside the opening
#define HT_CULVERT_X -5900.0f
#define HT_CULVERT_Z0 640.0f
#define HT_CULVERT_Z1 1028.0f

// out of the tomb: the flooded tunnel from the entrance hall's pool (room 0: the hole low in its south wall at z 1250,
// x -60..60, y -200..-45; the tunnel runs on to z 1450) - well into it, under water
#define HT_TOMB_EXIT_Z 1330.0f
#define HT_TOMB_EXIT_HALF_W 60.0f
#define HT_TOMB_EXIT_TOP -45.0f
// ...back into the stream, a little way out from the bars, swimming east
#define HT_ARRIVE_X -5800
#define HT_ARRIVE_Z 834

// the pad: Link must stand on its middle to play
#define HT_PAD_PLAY_R 70.0f // (the middle of the pad)
#define HT_MJIN_SHADOW 2     // Bg_Mjin params: the Shadow Medallion's pad
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId); // (z_scene.c)

enum {
    PAD_IDLE,      // closed: listening for the song
    PAD_WAIT,      // the song's right: waiting for the ocarina to go away
    PAD_CS,        // the bars rise
    PAD_OPEN,      // open: the culvert leads into the tomb
};

// cutscene timing
#define CS_GRIND 18                  // the bars start up
#define CS_RISE_END (CS_GRIND + 90)  // (and are up)
#define CS_END (CS_RISE_END + 40)

static bool sArriveFromTomb = false;
static bool sArriveByWarp = false;  // (the boss's blue warp: down onto the shadow pad)
static bool sWarpArrival = false;   // (this visit to the field began so)
static ActorEntry sArriveEntry;

// --- the bars ---

static const s16 sBarsY[4] = { -300, -380, -380, -300 };
static const s16 sBarsZ[4] = { 1034, 1034, 634, 634 };

static Vtx* HtBars_Vtx(void) {
    static Vtx* sVtx = NULL;
    static s32 sChecked = -1;
    Vtx* v = NULL;

    if (ResourceMgr_FileExists(HT_BARS_VTX_PATH)) {
        v = ResourceMgr_LoadVtxByName((char*)HT_BARS_VTX_PATH);
    } else if (ResourceMgr_FileExists(HT_BARS_VTX_PATH_OLD)) {
        v = ResourceMgr_LoadVtxByName((char*)HT_BARS_VTX_PATH_OLD);
    }
    if (v == NULL) {
        return NULL;
    }
    if ((v != sVtx) || (sChecked < 0)) {
        s32 i;

        // (only the field's own bars: anything else there and they're left alone)
        sChecked = 1;
        for (i = 0; i < 4; i++) {
            Vtx* q = &v[HT_BARS_VTX0 + i];
            if ((q->v.ob[2] != sBarsZ[i]) || (q->v.ob[1] < sBarsY[i]) || (q->v.ob[1] > sBarsY[i] + HT_BARS_RISE + 1) ||
                (q->v.ob[0] > HT_BARS_PLANE_X) || (q->v.ob[0] < HT_BARS_PLANE_X - HT_BARS_BACK)) {
                sChecked = 0;
            }
        }
        sVtx = v;
    }
    return sChecked ? v : NULL;
}

static void HtBars_Set(PlayState* play, f32 lift) {
    Vtx* v = HtBars_Vtx();
    s32 i;

    if (v != NULL) {
        for (i = 0; i < 4; i++) {
            Vtx* q = &v[HT_BARS_VTX0 + i];
            q->v.ob[0] = HT_BARS_PLANE_X - ((lift > 0.0f) ? HT_BARS_BACK : 0);
            q->v.ob[1] = sBarsY[i] + (s16)lift;
        }
    }
    if ((play->colCtx.colHeader != NULL) && (play->colCtx.colHeader->numPolygons > HT_BARS_POLY0 + 1)) {
        for (i = 0; i < 2; i++) {
            CollisionPoly* poly = &play->colCtx.colHeader->polyList[HT_BARS_POLY0 + i];
            Vec3s* a = &play->colCtx.colHeader->vtxList[COLPOLY_VTX_INDEX(poly->flags_vIA)];

            if ((a->x != HT_BARS_PLANE_X) || (poly->normal.x < 0x7000)) {
                continue; // (not the bars)
            }
            if (lift > 0.0f) {
                poly->flags_vIA |= HT_BARS_POLY_IGNORE;
            } else {
                poly->flags_vIA &= ~HT_BARS_POLY_IGNORE;
            }
        }
    }
}

// --- the pad ---

static HtShadowPad* sPad = NULL;
static bool sNocturneHeard = false;

void HtShadowPad_Init(Actor* thisx, PlayState* play) {
    HtShadowPad* this = (HtShadowPad*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_SetScale(thisx, 1.0f);
    thisx->uncullZoneForward = 6000.0f;
    thisx->uncullZoneScale = 400.0f;
    thisx->uncullZoneDownward = 400.0f;
    this->subCamId = 0;
    this->arriveTimer = 0;
    this->arriveDone = false;
    if (HtTomb_IsOpen()) {
        this->state = PAD_OPEN;
        this->lift = HT_BARS_RISE;
    } else {
        this->state = PAD_IDLE;
        this->lift = 0.0f;
    }
    HtBars_Set(play, this->lift);
    (void)colHeader; // (the pad itself is the vanilla one, Bg_Mjin: this actor only listens and runs the cutscene)
    sPad = this;
    sNocturneHeard = false;
}

void HtShadowPad_Destroy(Actor* thisx, PlayState* play) {
    HtShadowPad* this = (HtShadowPad*)thisx;

    if (this->subCamId != 0) {
        Play_ClearCamera(play, this->subCamId);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        this->subCamId = 0;
    }
    if (this->state == PAD_CS) {
        // (cut short: up already, as the flag says)
        this->lift = HT_BARS_RISE;
    }
    HtBars_Set(play, HtTomb_IsOpen() ? HT_BARS_RISE : 0.0f);
    if (sPad == this) {
        sPad = NULL;
    }
}

// The song is played as anywhere else (free play: the pad doesn't ask for one song, which would cross out every other
// note); the message code asks here when it hears the Nocturne, before it offers the warp.
bool HtShadowPad_TakeNocturne(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((play->sceneNum != SCENE_HYRULE_FIELD) || (sPad == NULL) || (sPad->state != PAD_IDLE) || (player == NULL)) {
        return false;
    }
    if ((Actor_WorldDistXZToActor(&sPad->dyna.actor, &player->actor) > HT_PAD_PLAY_R) ||
        (fabsf(player->actor.world.pos.y - sPad->dyna.actor.world.pos.y) > 60.0f)) {
        return false;
    }
    sNocturneHeard = true;
    return true;
}

static void HtShadowPad_Listen(HtShadowPad* this, PlayState* play) {
    if (sNocturneHeard) {
        // the Nocturne on the pad: it answers
        sNocturneHeard = false;
        HtTomb_SetOpen(true); // (the chime once the bars are up: CS end)
        this->glow = 1.0f;
        this->timer = 0;
        this->state = PAD_WAIT;
    }
}

static void HtShadowPad_Quake(PlayState* play, s16 camId, s16 y, s16 frames) {
    s16 q = Quake_Add(Play_GetCamera(play, camId), 3);

    Quake_SetSpeed(q, 22000);
    Quake_SetQuakeValues(q, y, 0, 0, 0);
    Quake_SetCountdown(q, frames);
}

static void HtShadowPad_Cutscene(HtShadowPad* this, PlayState* play) {
    s16 t = this->timer;

    if (t == 0) {
        func_80064520(play, &play->csCtx);
        Player_SetCsActionWithHaltedActors(play, &this->dyna.actor, 1);
        this->subCamId = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, this->subCamId, CAM_STAT_ACTIVE);
        Play_CameraSetFov(play, this->subCamId, 60.0f);
        // out on the stream's bank, looking at the culvert's mouth
        this->camAt.x = HT_BARS_MID_X;
        this->camAt.y = HT_BARS_MID_Y + 10.0f;
        this->camAt.z = HT_BARS_MID_Z;
        this->camEye.x = HT_BARS_MID_X + 360.0f;
        this->camEye.y = HT_BARS_MID_Y + 150.0f;
        this->camEye.z = HT_BARS_MID_Z + 170.0f;
    }
    if (this->subCamId != 0) {
        // (a slow push in)
        this->camEye.x -= 0.45f;
        this->camEye.z -= 0.2f;
        Play_CameraSetAtEye(play, this->subCamId, &this->camAt, &this->camEye);
    }

    if (t == CS_GRIND) {
        HtSfx_PlayFor(HT_BARS_MID_X, HT_BARS_MID_Y, HT_BARS_MID_Z, NA_SE_EV_IRON_DOOR_OPEN, 1);
        HtSfx_PlayFor(HT_BARS_MID_X, HT_BARS_MID_Y, HT_BARS_MID_Z, NA_SE_EV_METALDOOR_SLIDE, CS_RISE_END - CS_GRIND);
        HtShadowPad_Quake(play, this->subCamId, 1, CS_RISE_END - CS_GRIND);
    }
    if ((t > CS_GRIND) && (t <= CS_RISE_END)) {
        // a grind loose, then steady
        Math_StepToF(&this->lift, HT_BARS_RISE, (t < CS_GRIND + 20) ? 0.5f : 1.0f);
        HtBars_Set(play, this->lift);
        if ((t % 3) == 0) {
            // the water stirred where the bars come up through it
            Vec3f pos;

            pos.x = HT_BARS_MID_X + 6.0f;
            pos.y = HT_WATER_Y;
            pos.z = HT_CULVERT_Z0 + Rand_ZeroOne() * (HT_CULVERT_Z1 - HT_CULVERT_Z0);
            EffectSsGRipple_Spawn(play, &pos, 80, 380, 0);
        }
        if ((t % 6) == 0) {
            Vec3f pos;

            pos.x = HT_BARS_MID_X + 4.0f;
            pos.y = HT_WATER_Y - 30.0f;
            pos.z = HT_CULVERT_Z0 + Rand_ZeroOne() * (HT_CULVERT_Z1 - HT_CULVERT_Z0);
            EffectSsBubble_Spawn(play, &pos, 0.0f, 20.0f, 20.0f, 0.2f);
        }
    }
    if (t == CS_RISE_END) {
        HtBars_Set(play, HT_BARS_RISE);
        this->lift = HT_BARS_RISE;
        HtSfx_PlayFor(HT_BARS_MID_X, HT_BARS_MID_Y, HT_BARS_MID_Z, NA_SE_EV_METALDOOR_STOP, 1);
    }
    if (t == CS_RISE_END + 12) {
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME); // (the way's open)
    }
    if (t >= CS_END) {
        Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);

        func_800C08AC(play, this->subCamId, 0);
        this->subCamId = 0;
        (void)cam;
        func_80064534(play, &play->csCtx);
        Player_SetCsActionWithHaltedActors(play, &this->dyna.actor, 7);
        this->state = PAD_OPEN;
        return;
    }
    this->timer++;
}

// The blue warp's arrival: Link comes down out of the sky onto the pad. The field's own camera watched it from high
// up - and over the walls, onto the nothing beyond them. Instead: from low on the stream's bank, looking up at him.
#define ARRIVE_MAX 320
static void HtShadowPad_ArriveCam(HtShadowPad* this, PlayState* play, Player* player) {
    s32 landed;

    if (this->arriveTimer == 0) {
        CollisionPoly* poly;
        s32 bgId;
        Vec3f probe;
        f32 groundY;

        this->subCamId = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, this->subCamId, CAM_STAT_ACTIVE);
        Play_CameraSetFov(play, this->subCamId, 60.0f);
        // in front of him (he faces south, +z), a little west of him: looking up and on into the field
        this->camEye.x = HT_SHADOW_PAD_X - 60.0f;
        this->camEye.z = HT_SHADOW_PAD_Z + 300.0f;
        probe.x = this->camEye.x;
        probe.y = HT_SHADOW_PAD_Y + 400.0f;
        probe.z = this->camEye.z;
        groundY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &probe);
        this->camEye.y = MAX(HT_SHADOW_PAD_Y + 30.0f, MAX(groundY, HT_WATER_Y) + 30.0f);
        this->camAt = player->actor.world.pos;
    }
    this->arriveTimer++;
    if (this->subCamId == 0) {
        this->arriveDone = true;
        return;
    }
    // (on him as he comes down; at the end, on him standing)
    this->camAt.x = player->actor.world.pos.x;
    this->camAt.z = player->actor.world.pos.z;
    Math_ApproachF(&this->camAt.y, MAX(player->actor.world.pos.y + 45.0f, HT_SHADOW_PAD_Y + 50.0f), 0.5f, 60.0f);
    Play_CameraSetAtEye(play, this->subCamId, &this->camAt, &this->camEye);

    landed = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE);
    if ((landed && (this->arriveTimer > 20)) || (this->arriveTimer > ARRIVE_MAX)) {
        Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);

        cam->eye = this->camEye;
        cam->eyeNext = this->camEye;
        cam->at = this->camAt;
        func_800C08AC(play, this->subCamId, 0);
        this->subCamId = 0;
        this->arriveDone = true;
    }
}

void HtShadowPad_Update(Actor* thisx, PlayState* play) {
    HtShadowPad* this = (HtShadowPad*)thisx;
    Player* player = GET_PLAYER(play);

    Math_StepToF(&this->glow, 0.0f, 0.012f);

    if (sWarpArrival && !this->arriveDone && (this->state != PAD_CS)) {
        HtShadowPad_ArriveCam(this, play, player);
    }

    switch (this->state) {
        case PAD_IDLE:
            HtShadowPad_Listen(this, play);
            break;
        case PAD_WAIT:
            // (the ocarina away first)
            this->timer++;
            if (((play->msgCtx.msgMode == MSGMODE_NONE) && !Player_InCsMode(play)) || (this->timer > 90)) {
                this->timer = 0;
                this->state = PAD_CS;
            }
            break;
        case PAD_CS:
            HtShadowPad_Cutscene(this, play);
            break;
        case PAD_OPEN:
            // in through the culvert
            if ((player->actor.world.pos.x < HT_CULVERT_X) && (player->actor.world.pos.x > HT_CULVERT_X - 300.0f) &&
                (player->actor.world.pos.z > HT_CULVERT_Z0 - 20.0f) &&
                (player->actor.world.pos.z < HT_CULVERT_Z1 + 20.0f) && (player->actor.world.pos.y < -290.0f) &&
                (play->transitionTrigger == TRANS_TRIGGER_OFF)) {
                play->nextEntranceIndex = ENTR_HERETICS_TOMB_0;
                play->transitionTrigger = TRANS_TRIGGER_START;
                play->transitionType = TRANS_TYPE_FADE_BLACK;
            }
            break;
    }
}

void HtShadowPad_Draw(Actor* thisx, PlayState* play) {
    // (nothing: the pad is Bg_Mjin's)
}

// --- hooks (Katana.cpp) ---

void HtShadowPad_ArriveByWarp(void) {
    sArriveByWarp = true;
}

bool HtShadowPad_ArrivedByWarp(void) {
    return sWarpArrival;
}

void HtShadowPad_OnSceneInit(PlayState* play) {
    sWarpArrival = false;
    if (play->sceneNum != SCENE_HYRULE_FIELD) {
        sArriveFromTomb = sArriveByWarp = false;
        return;
    }
    if (sArriveByWarp && (play->linkActorEntry != NULL)) {
        // the boss beaten: the blue warp sets Link down on the shadow pad, facing the stream (the black horse waits on
        // its far bank - Katana.cpp)
        sArriveEntry = *play->linkActorEntry;
        sArriveEntry.pos.x = (s16)HT_SHADOW_PAD_X;
        sArriveEntry.pos.y = (s16)HT_SHADOW_PAD_Y;
        sArriveEntry.pos.z = (s16)HT_SHADOW_PAD_Z;
        sArriveEntry.rot.x = sArriveEntry.rot.z = 0;
        sArriveEntry.rot.y = 0; // (south, +z: across the stream)
        sArriveEntry.params = (sArriveEntry.params & 0xF0FF) | (PLAYER_START_MODE_BLUE_WARP << 8);
        play->linkActorEntry = &sArriveEntry;
        sArriveByWarp = false;
        sArriveFromTomb = false;
        sWarpArrival = true;
        return;
    }
    if (sArriveFromTomb && (play->linkActorEntry != NULL)) {
        // (a copy: the scene's own entry is the loaded resource)
        sArriveEntry = *play->linkActorEntry;
        sArriveEntry.pos.x = HT_ARRIVE_X;
        sArriveEntry.pos.y = (s16)HT_WATER_Y;
        sArriveEntry.pos.z = HT_ARRIVE_Z;
        sArriveEntry.rot.x = sArriveEntry.rot.z = 0;
        sArriveEntry.rot.y = 0x4000; // (east, away from the culvert)
        sArriveEntry.params = (sArriveEntry.params & 0xF0FF) | (PLAYER_START_MODE_MOVE_FORWARD_SLOW << 8);
        play->linkActorEntry = &sArriveEntry;
    }
    sArriveFromTomb = false;
}

void HtShadowPad_OnSpawnActors(PlayState* play) {
    Actor* a;

    if (play->sceneNum != SCENE_HYRULE_FIELD) {
        return;
    }
    for (a = play->actorCtx.actorLists[ACTORCAT_PROP].head; a != NULL; a = a->next) {
        if (a->id == HT_ACTOR_SHADOW_PAD) {
            return;
        }
    }
    // the pad: the vanilla shadow warp pad (Bg_Mjin, the Shadow Medallion's emblem - as by the Shadow Temple), its
    // objects loaded here (the field's own list hasn't them)
    if (Object_GetIndex(&play->objectCtx, OBJECT_MJIN) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_MJIN);
    }
    if (Object_GetIndex(&play->objectCtx, OBJECT_MJIN_DARK) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_MJIN_DARK);
    }
    Actor_Spawn(&play->actorCtx, play, ACTOR_BG_MJIN, HT_SHADOW_PAD_X, HT_SHADOW_PAD_Y, HT_SHADOW_PAD_Z, 0, 0, 0,
                HT_MJIN_SHADOW);
    Actor_Spawn(&play->actorCtx, play, HT_ACTOR_SHADOW_PAD, HT_SHADOW_PAD_X, HT_SHADOW_PAD_Y, HT_SHADOW_PAD_Z, 0, 0, 0,
                0);
}

void HtShadowPad_TombUpdate(PlayState* play) {
    Player* player;

    if ((play == NULL) || (play->sceneNum != SCENE_HERETICS_TOMB) || (play->roomCtx.curRoom.num != 0)) {
        return;
    }
    player = GET_PLAYER(play);
    if ((player == NULL) || Player_InCsMode(play) || (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        return;
    }
    if ((player->actor.world.pos.z > HT_TOMB_EXIT_Z) && (fabsf(player->actor.world.pos.x) < HT_TOMB_EXIT_HALF_W) &&
        (player->actor.world.pos.y < HT_TOMB_EXIT_TOP)) {
        // out the way he came: the culvert, into the stream
        sArriveFromTomb = true;
        play->nextEntranceIndex = ENTR_HYRULE_FIELD_ON_BRIDGE_SPAWN;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    }
}
