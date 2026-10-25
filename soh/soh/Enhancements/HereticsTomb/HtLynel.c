// Heretics Tomb: the Lynel (model, standing idle). See HtLynel.h.

#include "HtLynel.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_horse_ganon/object_horse_ganon.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/ResourceManagerHelpers.h"
#include "HtLynelBuild.h"     // the model, animations and textures, built from the player's game data
#include "HtLynelAnimDefs.h"
#include "HtFlameJet.h" // HtIce_DrawSleeve
#define LYNEL_CHAIN_FREEZE 30 // frames of the ice running along the chain before it shatters (as the tomb's chains)
#include "HtWaterSpout.h"
#include "HtPedestal.h"
#include "HtFireRing.h"
#include "HtHorse.h"
#include "HtShadowPad.h"
#include "soh/frame_interpolation.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include <math.h>

static void HtLynel_SetWaterDrop(HtLynel* this, PlayState* play, f32 drop);
void Player_Action_Roll(Player* this, PlayState* play); // (z_player.c: rolling, under the whirl)

extern MtxF gSkinLimbMatrices[]; // (z_skin.c)
void Skin_ApplyLimbModifications(GraphicsContext* gfxCtx, Skin* skin, s32 limbIndex, s32 arg3);
void Player_InitItemAction(PlayState* play, Player* this, s8 itemAction); // (z_player.c)

// the horse skeleton's limbs: 8 the chest (where the upper body sits), 9-12 the neck and head (folded away), 13-14
// the head's own parts (not drawn)
#define LYNEL_LIMB_CHEST 8
#define LYNEL_LIMB_NECK_FIRST 9
#define LYNEL_LIMB_NECK_LAST 12
#define LYNEL_LIMB_HEAD_A 13
#define LYNEL_LIMB_HEAD_B 14

// the horse's body skin: its tail's texture (shared with the mane) swapped for a purple copy while the Lynel is
// here (patched in at Init, out at Destroy - the vanilla horse elsewhere keeps its own)
#define LYNEL_SKIN_DL "__OTR__objects/object_horse_ganon/gHorseGanonSkelLimbsLimb_008294SkinLimbDL_009D20"
#define LYNEL_SKIN_HAIR_TEXIMG 4 // the 4th texture the skin's display list sets is the mane and tail's

static s32 sHairPatchIdx = -1;

// the head's list: its eye texture (the 3rd it sets) swapped for a shut eye while the horse sleeps chained; it opens
// its eyes as the intro starts (the chain cut)
#define LYNEL_HEAD_DL "__OTR__objects/object_horse_ganon/gHorseGanonSkelLimbsLimb_008314DL_0090F0"
#define LYNEL_HEAD_EYE_TEXIMG 3

static s32 sEyePatchIdx = -1;

// the instruction index of the skin list's n-th SetTextureImage (a hashed one: two entries), counted the way the
// patcher counts (expanded commands are two). (Step 8.3 patched a fixed index, one short - the list opens with a
// marker - and the game crashed on the half-patched command.)
static s32 HtLynel_FindTexImg(Gfx* dl, s32 n) {
    s32 i = 0;
    s32 found = 0;

    if (dl == NULL) {
        return -1;
    }
    while (i < 4096) {
        u8 op = (dl[i].words.w0 >> 24) & 0xFF;

        if (op == G_ENDDL) {
            return -1;
        }
        if (op == G_SETTIMG_OTR_HASH) {
            if (++found == n) {
                return i;
            }
        }
        if ((op == G_SETTIMG_OTR_HASH) || (op == G_DL_OTR_HASH) || (op == G_VTX_OTR_HASH) || (op == G_BRANCH_Z_OTR) ||
            (op == G_MARKER) || (op == G_MTX_OTR) || (op == G_MOVEMEM_OTR)) {
            i += 2;
        } else {
            i++;
        }
    }
    return -1;
}

// the chain: hit by player swords (only the Iai cuts it - anything else rings off the iron)
static ColliderCylinderInit sChainCylinderInit = {
    {
        COLTYPE_METAL,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON | BUMP_NO_DAMAGE | BUMP_NO_HITMARK,
        OCELEM_NONE,
    },
    { 40, 40, 0, { 0, 0, 0 } },
};

// the body: four spheres pushing Link out (placed each frame in the actor's frame, by form)
static ColliderJntSphElementInit sBodyElemInit[4] = {
    { { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0, 0, 0 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON }, { 0, { { 0, 0, 0 }, 40 }, 100 } },
    { { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0, 0, 0 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON }, { 0, { { 0, 0, 0 }, 40 }, 100 } },
    { { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0, 0, 0 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON }, { 0, { { 0, 0, 0 }, 40 }, 100 } },
    { { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0, 0, 0 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON }, { 0, { { 0, 0, 0 }, 40 }, 100 } },
};
static ColliderJntSphInit sBodyInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_JNTSPH },
    ARRAY_COUNT(sBodyElemInit),
    sBodyElemInit,
};
// (x, y, z, radius) in the actor's frame, world units: the horse lying (rump, belly, chest, neck), the Lynel standing
// (rump, barrel, the upper body, the legs between)
static const s16 sBodyLying[4][4] = { { 0, 25, -55, 45 }, { 0, 28, 10, 45 }, { 0, 32, 60, 40 }, { 0, 55, 100, 30 } };
static const s16 sBodyStanding[4][4] = { { 0, 90, -40, 50 }, { 0, 90, 30, 50 }, { 0, 170, 70, 45 }, { 0, 35, 0, 40 } };

static void HtLynel_UpdateBody(HtLynel* this, PlayState* play) {
    const s16(*sp)[4] = (this->form == HT_LYNEL_CHAINED) ? sBodyLying : sBodyStanding;
    f32 c = Math_CosS(this->actor.shape.rot.y);
    f32 sn = Math_SinS(this->actor.shape.rot.y);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(this->bodyElems); i++) {
        Sphere16* w = &this->bodyElems[i].dim.worldSphere;

        w->center.x = (s16)(this->actor.world.pos.x + sp[i][0] * c + sp[i][2] * sn);
        w->center.y = (s16)(this->actor.world.pos.y + sp[i][1]);
        w->center.z = (s16)(this->actor.world.pos.z - sp[i][0] * sn + sp[i][2] * c);
        w->radius = sp[i][3];
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->body.base);
}

// the charge's horns
static ColliderCylinderInit sHornsInit = {
    { COLTYPE_HIT0, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x10 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 55, 150, 0, { 0, 0, 0 } },
};

// the horse's mane and tail turn purple: only once it has become the Lynel (Ganondorf's horse keeps its own)
static void HtLynel_PatchHair(void) {
    if (sHairPatchIdx >= 0) {
        return;
    }
    sHairPatchIdx = HtLynel_FindTexImg(ResourceMgr_LoadGfxByName(LYNEL_SKIN_DL), LYNEL_SKIN_HAIR_TEXIMG);
    if (sHairPatchIdx >= 0) {
        Gfx hairImg = gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, gHtLynelHairTex);
        Gfx noOp = gsDPNoOp();

        ResourceMgr_PatchGfxByName(LYNEL_SKIN_DL, "htLynelHair0", sHairPatchIdx, hairImg);
        ResourceMgr_PatchGfxByName(LYNEL_SKIN_DL, "htLynelHair1", sHairPatchIdx + 1, noOp);
    }
}

// the horse's own mane and tail back (the curse lifted)
static void HtLynel_UnpatchHair(void) {
    if (sHairPatchIdx >= 0) {
        ResourceMgr_UnpatchGfxByName(LYNEL_SKIN_DL, "htLynelHair0");
        ResourceMgr_UnpatchGfxByName(LYNEL_SKIN_DL, "htLynelHair1");
        sHairPatchIdx = -1;
    }
}

static s32 sEyeShown = 2; // the eye texture patched in (0 shut, 1 half, 2 the horse's own)

// the horse's eyes: 0 shut, 1 half open, 2 open (its own texture)
static void HtLynel_SetEyes(s32 which) {
    if (which == sEyeShown) {
        return;
    }
    if (sEyePatchIdx >= 0) {
        ResourceMgr_UnpatchGfxByName(LYNEL_HEAD_DL, "htLynelEye0");
        ResourceMgr_UnpatchGfxByName(LYNEL_HEAD_DL, "htLynelEye1");
        sEyePatchIdx = -1;
    }
    sEyeShown = which;
    if (which == 2) {
        return;
    }
    sEyePatchIdx = HtLynel_FindTexImg(ResourceMgr_LoadGfxByName(LYNEL_HEAD_DL), LYNEL_HEAD_EYE_TEXIMG);
    if (sEyePatchIdx >= 0) {
        Gfx eyeImg = gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
                                         (which == 0) ? gHtLynelEyeClosedTex : gHtLynelEyeHalfTex);
        Gfx noOp = gsDPNoOp();

        ResourceMgr_PatchGfxByName(LYNEL_HEAD_DL, "htLynelEye0", sEyePatchIdx, eyeImg);
        ResourceMgr_PatchGfxByName(LYNEL_HEAD_DL, "htLynelEye1", sEyePatchIdx + 1, noOp);
    }
}
#define HtLynel_CloseEyes() HtLynel_SetEyes(0)
#define HtLynel_OpenEyes() HtLynel_SetEyes(2)

// the ground's rumble, for so many frames (EARTHQUAKE_LAST would never stop: a one-shot of a looping sound)
static void HtLynel_Rumble(HtLynel* this, s16 frames) {
    HtSfx_PlayFor(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, NA_SE_EV_EARTHQUAKE,
                  frames);
}

// the phase 1 spouts, in a ring round the pedestal (erupting: thrown up by the transformation's blast)
static void HtLynel_SpawnSpouts(HtLynel* this, PlayState* play) {
    s32 i;

    for (i = 0; i < HT_LYNEL_SPOUTS; i++) {
        s16 a = this->actor.world.rot.y + 0x10000 / (2 * HT_LYNEL_SPOUTS) + i * (0x10000 / HT_LYNEL_SPOUTS);

        this->spouts[i] = Actor_Spawn(&play->actorCtx, play, HT_ACTOR_WATER_SPOUT,
                    this->actor.world.pos.x + Math_SinS(a) * HT_LYNEL_SPOUT_RING,
                    this->actor.home.pos.y - HT_LYNEL_SPOUT_DROP,
                    this->actor.world.pos.z + Math_CosS(a) * HT_LYNEL_SPOUT_RING, 0, 0, 0,
                    HT_WATER_SPOUT_PARAMS(0, 9, 1) | HT_WATER_SPOUT_ERUPT);
    }
}

static void HtLynel_BecomeLynel(HtLynel* this) {
    HtLynel_PatchHair();
    this->tform = 1.0f;
    this->form = HT_LYNEL_FORM;
    this->actor.naviEnemyId = HT_LYNEL_HINT_TEXT - 0x600; // Navi's C-up (Katana.cpp)
    this->collider.base.colType = COLTYPE_HIT0; // (the chain's collider, now its body: flesh, not iron)
    this->actor.targetMode = 5;
    this->reelStep = 0;
    this->reelTimer = 0;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED; // (Z-targetable, as bosses are)
}

void HtLynel_Init(Actor* thisx, PlayState* play) {
    HtLynel* this = (HtLynel*)thisx;
    f32 c = Math_CosS(thisx->world.rot.y);
    f32 sn = Math_SinS(thisx->world.rot.y);

    // its model, animations and textures: built once from the player's own game data (HtLynelBuild.cpp)
    if (!HtLynelBuild_Init()) {
        osSyncPrintf("HtLynel: the game data isn't what the Lynel is built from\n");
        Actor_Kill(thisx);
        return;
    }
    this->actor.scale.x = HT_LYNEL_SCALE * HT_LYNEL_HORSE_K * HT_LYNEL_HORSE_KX;
    this->actor.scale.y = HT_LYNEL_SCALE * HT_LYNEL_HORSE_K;
    this->actor.scale.z = HT_LYNEL_SCALE * HT_LYNEL_HORSE_K;
    this->actor.uncullZoneForward = 3000.0f;
    this->actor.uncullZoneScale = 600.0f;
    this->actor.uncullZoneDownward = 600.0f;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 90.0f);
    Skin_Init(play, &this->skin, (SkeletonHeader*)gHorseGanonSkel, (AnimationHeader*)gHorseGanonIdleAnim);
    Animation_PlayLoop(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonIdleAnim);
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 160.0f;

    // the anchor, in the pedestal beside the horse
    this->anchor.x = thisx->world.pos.x + HT_LYNEL_ANCHOR_X * c + HT_LYNEL_ANCHOR_Z * sn;
    this->anchor.y = thisx->world.pos.y;
    this->anchor.z = thisx->world.pos.z - HT_LYNEL_ANCHOR_X * sn + HT_LYNEL_ANCHOR_Z * c;
    this->chainEnd = this->anchor;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sChainCylinderInit);
    Collider_InitCylinder(play, &this->horns);
    Collider_SetCylinder(play, &this->horns, thisx, &sHornsInit);
    this->action = 0; // P1_WAIT
    Collider_InitJntSph(play, &this->body);
    Collider_SetJntSph(play, &this->body, thisx, &sBodyInit, this->bodyElems);
    {
        // the swords' trails: pale violet, fading (the Gerudo fighters' EffectBlure)
        EffectBlureInit1 b;
        s32 k;

        b.p1StartColor[0] = 255; b.p1StartColor[1] = 225; b.p1StartColor[2] = 255; b.p1StartColor[3] = 255;
        b.p2StartColor[0] = 190; b.p2StartColor[1] = 110; b.p2StartColor[2] = 255; b.p2StartColor[3] = 80;
        b.p1EndColor[0] = 200; b.p1EndColor[1] = 120; b.p1EndColor[2] = 255; b.p1EndColor[3] = 0;
        b.p2EndColor[0] = 120; b.p2EndColor[1] = 40; b.p2EndColor[2] = 200; b.p2EndColor[3] = 0;
        b.elemDuration = 8;
        b.unkFlag = 0;
        b.calcMode = 2;
        for (k = 0; k < 2; k++) {
            Effect_Add(play, &this->blure[k], EFFECT_BLURE1, 0, 0, &b);
        }
    }
    thisx->colChkInfo.mass = MASS_IMMOVABLE;

    if (Flags_GetSwitch(play, HT_LYNEL_DEAD_FLAG)) {
        // beaten: nothing here but the warp home (where its disc was)
        this->form = HT_LYNEL_DEAD;
        this->dead = true;
        thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        HtLynel_SetWaterDrop(this, play, 30.0f);
        Actor_Spawn(&play->actorCtx, play, ACTOR_DOOR_WARP1, thisx->home.pos.x,
                    thisx->home.pos.y - HT_PEDESTAL_TOP_DROP, thisx->home.pos.z, 0, 0, 0, -1);
        Flags_SetSwitch(play, HT_PEDESTAL_SINK_FLAG);
        return;
    }
    if (Flags_GetSwitch(play, HT_LYNEL_CHAIN_FLAG)) {
        HtLynel_BecomeLynel(this); // freed before: the fight starts with the Lynel, the spouts already up, the
        Flags_SetSwitch(play, HT_PEDESTAL_SINK_FLAG); // pedestal down
        this->sinkTimer = HT_PEDESTAL_SINK_FRAMES;
        thisx->world.pos.y = thisx->home.pos.y - HT_PEDESTAL_TOP_DROP;
        this->anchor.y = thisx->world.pos.y; // (sunk with the pedestal)
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_BOSS);
        Animation_PlayLoop(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonIdleAnim);
        HtLynel_SpawnSpouts(this, play);
    } else {
        // the chain: targetable (Navi's C-up says how to cut it)
        this->form = HT_LYNEL_CHAINED;
        this->sinkTimer = HT_PEDESTAL_SINK_FRAMES; // (no sinking till the intro is over)
        HtLynel_CloseEyes(); // asleep
        thisx->flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        thisx->targetMode = 4;
        thisx->naviEnemyId = 0xF1; // the chains' full hint: "use an Iai strike" (Katana.cpp)
        thisx->focus.pos = this->anchor;
        thisx->focus.pos.y += 20.0f;
    }
}

// the boss room: no ocarina (as a vanilla boss room's scene restrictions) - put back as it was once we're gone
static s16 sOcarinaRestriction = -1;
static void HtLynel_RestoreRoom(PlayState* play);

static s32 sChainShown = false;
static s32 sEpitaphShown = false;
static s32 sFireSfxOn = false; // the phase-3 flames' roar playing: cut dead when the breath ends (or is cut short)

// a looping sound, both ways it can have been asked for: stopped now (not left to ring on)
static void HtLynel_StopLoop(u16 sfxId) {
    Audio_StopSfxById(sfxId);
    Audio_StopSfxById((u16)(sfxId - SFX_FLAG));
}

s32 HtLynel_EpitaphShown(void) {
    return sEpitaphShown;
}

s32 HtLynel_ChainShown(void) {
    return sChainShown;
}

void HtLynel_Destroy(Actor* thisx, PlayState* play) {
    HtLynel* this = (HtLynel*)thisx;

    if (!HtLynelBuild_Init()) {
        return; // (never set up: killed in its Init)
    }

    sChainShown = false;
    sEpitaphShown = false;
    if (sFireSfxOn) {
        sFireSfxOn = false;
        HtLynel_StopLoop(NA_SE_EN_DODO_J_FIRE);
    }
    if (sOcarinaRestriction >= 0) {
        play->interfaceCtx.restrictions.ocarina = sOcarinaRestriction;
        sOcarinaRestriction = -1;
    }
    HtLynel_RestoreRoom(play);
    Katana_SetLocked(false);
    Effect_Delete(play, this->blure[0]);
    Effect_Delete(play, this->blure[1]);

    Skin_Free(play, &this->skin);
    Collider_DestroyCylinder(play, &this->collider);
    Collider_DestroyJntSph(play, &this->body);
    Collider_DestroyCylinder(play, &this->horns);
    if (this->subCamId != 0) {
        Play_ClearCamera(play, this->subCamId);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        this->subCamId = 0;
    }
    HtLynel_OpenEyes();
    if (sHairPatchIdx >= 0) {
        ResourceMgr_UnpatchGfxByName(LYNEL_SKIN_DL, "htLynelHair0");
        ResourceMgr_UnpatchGfxByName(LYNEL_SKIN_DL, "htLynelHair1");
        sHairPatchIdx = -1;
    }
}


static void HtLynel_SetHorseAnim(HtLynel* this, AnimationHeader* anim, u8 loop) {
    if (loop) {
        Animation_PlayLoop(&this->skin.skelAnime, anim);
    } else {
        Animation_PlayOnce(&this->skin.skelAnime, anim);
    }
}

static s32 HtLynel_IsIaiCut(Actor* attacker) {
    if (attacker == NULL) {
        return false;
    }
    if (attacker->id == HT_ACTOR_ICE_SLASH) {
        return attacker->params == 1; // the Iai wave
    }
    if (attacker->id == ACTOR_PLAYER) {
        return Katana_IsIaiBladeHit(attacker);
    }
    return false;
}

// The Iai: the chain frosts and shatters along its length; the horse is free
static void HtLynel_CutChain(HtLynel* this, PlayState* play) {
    static Color_RGBA8 prim = { 200, 240, 255, 255 };
    static Color_RGBA8 env = { 90, 170, 255, 255 };
    Vec3f accel = { 0.0f, -1.0f, 0.0f };
    s32 i;

    Flags_SetSwitch(play, HT_LYNEL_CHAIN_FLAG); // (its eyes open in the cutscene)
    Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP); // silence: the music cuts out
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_STRIKE_HARD);
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_CHAIN_KEY_UNLOCK);
    for (i = 0; i < 24; i++) {
        f32 t = Rand_ZeroOne();
        Vec3f pos = { this->anchor.x + (this->chainEnd.x - this->anchor.x) * t,
                      this->anchor.y + 6.0f + (this->chainEnd.y - this->anchor.y) * t,
                      this->anchor.z + (this->chainEnd.z - this->anchor.z) * t };
        Vec3f vel = { Rand_CenteredFloat(6.0f), Rand_ZeroFloat(5.0f) + 2.0f, Rand_CenteredFloat(6.0f) };

        EffectSsEnIce_Spawn(play, &pos, Rand_ZeroFloat(0.3f) + 0.35f, &vel, &accel, &prim, &env, 25);
    }
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->form = HT_LYNEL_CUTSCENE;
    this->csTimer = 0;
    this->tform = 0.0f;
    this->subCamId = 0;
}

// ---- the intro cutscene ------------------------------------------------------------------------------------
// (frames)  0 the chain shatters: close on the shut eye - it opens (half, a blink, half, wide); 45 pulling back
//          55 it gets up, forelegs first (an authored stand-up, not a blend)
//          75 a low shot: 100 it rears and neighs
//          95 circling it: dark smoke welling up off it, thicker and thicker, the ground shaking harder
//         132 the light swells till the screen is white...
//         150 ...and behind it the horse is the Lynel (swapped whole, unseen: no half-way shrinking or growing);
//             the blast: a shock wave, the spouts burst up round the pedestal; the white fades off the Lynel
//         240 its title card; 275 back to Link (the pedestal sinks once the card has gone: HT_LYNEL_SINK_DELAY)
//         150-215 pulling back high over the arena; it roars
//         215 back to Link: the fight
#define CS_SHOT_EYE 0
#define CS_SHOT_PULL 45
#define CS_STAND 55
#define CS_SHOT_REAR 95                  // (cut to once it's up)
#define CS_REAR 105
#define CS_REAR_HOLD 20                   // (the rearing's height: its head highest - held there for the change)
#define CS_SHOT_ORBIT (CS_REAR + CS_REAR_HOLD)
#define CS_TF_START (CS_SHOT_ORBIT + 2)
#define CS_WHITE_IN (CS_TF_START + 32)
#define CS_BLAST (CS_WHITE_IN + 18)
#define CS_TITLE (CS_BLAST + 35)
#define CS_FACEOFF (CS_TITLE + 52)    // it has come down from rearing: glaring down at Link
#define CS_ZOOM_OUT (CS_TITLE + 40)   // (2 s after its name: the camera draws back to Link...)
#define CS_ZOOM_END (CS_TITLE + 85)   // (...there, the fight's view)
#define CS_END (CS_TITLE + 100)
#define CS_ZOOM_IN 0.45f              // (the close framing: this far from the Lynel along the final shot's line)

typedef struct {
    s16 start, end;
    Vec3f at0, eye0, at1, eye1; // in the horse's frame (it faces +z), world units
} HtLynelShot;

static const HtLynelShot sShots[] = {
    { CS_SHOT_EYE, CS_SHOT_PULL, { 7, 81, 98 }, { 34, 83, 102 }, { 7, 81, 98 }, { 37, 84, 103 } },
    // easing back to a low front shot as it gets up: head and forelegs - its hind legs stay hidden behind it
    { CS_SHOT_PULL, CS_SHOT_REAR, { 7, 81, 98 }, { 37, 84, 103 }, { 0, 130, 85 }, { 50, 140, 250 } },
    // up on its hind legs, neighing: the camera closing in on it...
    { CS_SHOT_REAR, CS_SHOT_ORBIT, { 0, 120, 20 }, { -215, 45, 205 }, { 0, 175, 30 }, { -120, 75, 125 } },
    // ...and staying close, circling, as it changes - still reared up
    { CS_SHOT_ORBIT, CS_BLAST, { 0, 185, 0 }, { -150, 125, 120 }, { 0, 195, 0 }, { -180, 140, -50 } },
    // (from the blast on: the face-off, from behind Link - HtLynel_CsCamera)
};

static void HtLynel_LocalToWorld(HtLynel* this, const Vec3f* l, Vec3f* w) {
    f32 c = Math_CosS(this->actor.shape.rot.y);
    f32 sn = Math_SinS(this->actor.shape.rot.y);

    w->x = this->actor.world.pos.x + l->x * c + l->z * sn;
    w->y = this->actor.world.pos.y + l->y;
    w->z = this->actor.world.pos.z - l->x * sn + l->z * c;
}

static void HtLynel_CsCamera(HtLynel* this, PlayState* play) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sShots); i++) {
        const HtLynelShot* sh = &sShots[i];

        if ((this->csTimer >= sh->start) && (this->csTimer < sh->end)) {
            f32 t = (f32)(this->csTimer - sh->start) / (sh->end - sh->start);
            Vec3f at, eye;

            t = 0.5f - 0.5f * Math_CosF(t * M_PI); // eased
            at.x = sh->at0.x + (sh->at1.x - sh->at0.x) * t;
            at.y = sh->at0.y + (sh->at1.y - sh->at0.y) * t;
            at.z = sh->at0.z + (sh->at1.z - sh->at0.z) * t;
            eye.x = sh->eye0.x + (sh->eye1.x - sh->eye0.x) * t;
            eye.y = sh->eye0.y + (sh->eye1.y - sh->eye0.y) * t;
            eye.z = sh->eye0.z + (sh->eye1.z - sh->eye0.z) * t;
            HtLynel_LocalToWorld(this, &at, &this->camAt);
            HtLynel_LocalToWorld(this, &eye, &this->camEye);
            break;
        }
    }
    if (this->csTimer >= CS_BLAST) {
        // the face-off, from the blast to the end - the title card and its rearing in it: low behind Link's right
        // shoulder, Link at the left of the frame, wary, looking up at the Lynel towering over him (and the camera's
        // then where the fight's camera picks up)
        Player* player = GET_PLAYER(play);
        f32 dx = this->actor.world.pos.x - player->actor.world.pos.x;
        f32 dz = this->actor.world.pos.z - player->actor.world.pos.z;
        f32 d = sqrtf(SQ(dx) + SQ(dz));
        f32 k = CLAMP((f32)(this->csTimer - CS_ZOOM_OUT) / (CS_ZOOM_END - CS_ZOOM_OUT), 0.0f, 1.0f);
        f32 rx, rz;

        if (d > 1.0f) {
            dx /= d;
            dz /= d;
        }
        rx = -dz; // (his right)
        rz = dx;
        // one line of sight throughout: close on the Lynel (Link behind the camera) through its name and its rearing,
        // then drawn straight back along it till Link is in the frame - the fight's about to begin
        k = 0.5f - 0.5f * Math_CosF(k * M_PI);
        k = CS_ZOOM_IN + (1.0f - CS_ZOOM_IN) * k;
        this->camAt.x = this->actor.world.pos.x - rx * 55.0f;
        this->camAt.y = this->actor.world.pos.y + 140.0f;
        this->camAt.z = this->actor.world.pos.z - rz * 55.0f;
        this->camEye.x = this->camAt.x + (player->actor.world.pos.x - dx * 80.0f + rx * 62.0f - this->camAt.x) * k;
        this->camEye.y = this->camAt.y + (player->actor.world.pos.y + 38.0f - this->camAt.y) * k;
        this->camEye.z = this->camAt.z + (player->actor.world.pos.z - dz * 80.0f + rz * 62.0f - this->camAt.z) * k;
    }
    Play_CameraSetFov(play, this->subCamId, 60.0f);
    Play_CameraSetAtEye(play, this->subCamId, &this->camAt, &this->camEye);
}

static void HtLynel_Quake(HtLynel* this, PlayState* play, s16 y, s16 frames) {
    s16 q = Quake_Add(Play_GetCamera(play, this->subCamId), 3);

    Quake_SetSpeed(q, 22000);
    Quake_SetQuakeValues(q, y, 0, 0, 0);
    Quake_SetCountdown(q, frames);
}

static void HtLynel_UpdateCutscene(HtLynel* this, PlayState* play) {
    s16 t = this->csTimer;

    if (t == 0) {
        // take the camera; Link (and everything else) holds still
        Player* player = GET_PLAYER(play);
        s16 yaw = Actor_WorldYawTowardActor(&player->actor, &this->actor);

        func_80064520(play, &play->csCtx);
        // Link: the katana back in its sheath, turned on the horse - wary, a foot drawn back, ready to run (the stance
        // he takes in the vanilla boss cutscenes)
        if (player->heldItemAction != PLAYER_IA_NONE) {
            // (at once: the cutscene stance doesn't play the putting away)
            player->heldItemId = ITEM_NONE;
            player->nextModelGroup = Player_ActionToModelGroup(player, PLAYER_IA_NONE);
            Player_InitItemAction(play, player, PLAYER_IA_NONE);
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_PUTAWAY);
        }
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = yaw;
        Player_SetCsActionWithHaltedActors(play, &this->actor, 1); // (his alert stance, a foot back: link_normal_wait)
        this->subCamId = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, this->subCamId, CAM_STAT_ACTIVE);
        Play_CameraSetFov(play, this->subCamId, 60.0f);
    }
    // the eye opens: half, shut again (a blink), half, wide
    HtLynel_SetEyes((t < 14) ? 0 : (t < 20) ? 1 : (t < 23) ? 0 : (t < 27) ? 1 : 2);
    if (t == 27) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_GROAN);
    }
    if (t == CS_STAND + 4) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_GROAN);
    }
    if (t == CS_STAND + LYNEL_STAND_FRAMES) {
        // up: on into its idle (the stand-up ends on the idle's first frame)
        Animation_PlayLoop(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonIdleAnim);
    }
    if (t == CS_REAR) {
        Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonRearingAnim, 1.0f, 0.0f,
                         Animation_GetLastFrame((AnimationHeader*)gHorseGanonRearingAnim), ANIMMODE_ONCE, -6.0f);
    }
    if (t == CS_REAR + 7) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
    }
    if (t == CS_SHOT_ORBIT) {
        // up on its hind legs, neighing: it stays there, and the change takes it there
        this->skin.skelAnime.playSpeed = 0.0f;
    }
    if ((t >= CS_SHOT_ORBIT) && (t < CS_BLAST + 25)) {
        // dark smoke welling up off it, thicker as the change takes hold (and thinning off the Lynel after)
        f32 dens = (t < CS_BLAST) ? (f32)(t - CS_SHOT_ORBIT) / (CS_BLAST - CS_SHOT_ORBIT)
                                  : 1.0f - (f32)(t - CS_BLAST) / 25.0f;
        s32 n = 1 + (s32)(dens * 3.0f);
        s32 i;

        for (i = 0; i < n; i++) {
            Vec3f pos;
            Vec3f vel = { Rand_CenteredFloat(1.5f), 2.0f + Rand_ZeroFloat(1.5f), Rand_CenteredFloat(1.5f) };
            Vec3f accel = { 0.0f, 0.1f, 0.0f };
            Vec3f l = { Rand_CenteredFloat(120.0f), 30.0f + Rand_ZeroFloat(150.0f), Rand_CenteredFloat(220.0f) };

            HtLynel_LocalToWorld(this, &l, &pos);
            if (i & 1) {
                EffectSsDeadDb_Spawn(play, &pos, &vel, &accel, 150 + (s16)(dens * 80.0f), 14, 60, 10, 90, 255, 10,
                                     0, 20, 1, 9, false);
            } else {
                EffectSsDeadDb_Spawn(play, &pos, &vel, &accel, 110 + (s16)(dens * 60.0f), 12, 120, 40, 170, 255, 30,
                                     0, 50, 1, 8, false);
            }
        }
    }
    if (t == CS_TF_START) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_TWINROBA_TRANSFORM);
        HtLynel_Quake(this, play, 2, CS_WHITE_IN - CS_TF_START);
    }
    if ((t >= CS_TF_START) && (t < CS_BLAST) && ((t % 12) == 0)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
    }
    if (t == CS_WHITE_IN) {
        HtLynel_Quake(this, play, 5, CS_BLAST - CS_WHITE_IN);
        play->envCtx.fillScreen = true;
        play->envCtx.screenFillColor[0] = 0; // (a fade to black: easier on the eyes than the white it was)
        play->envCtx.screenFillColor[1] = 0;
        play->envCtx.screenFillColor[2] = 0;
        play->envCtx.screenFillColor[3] = 0;
    }
    if ((t > CS_WHITE_IN) && (t < CS_BLAST)) {
        // the light swells to a full white-out
        f32 k = (f32)(t - CS_WHITE_IN) / (CS_BLAST - 2 - CS_WHITE_IN);

        play->envCtx.screenFillColor[3] = (u8)(255.0f * CLAMP(k * k, 0.0f, 1.0f));
    }
    if (t == CS_BLAST) {
        // behind the white: the horse is the Lynel, whole. The blast: a shock wave over the water, the spouts
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        Vec3f pos = this->actor.world.pos;

        this->tform = 1.0f;
        HtLynel_PatchHair();
        Animation_PlayLoop(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonIdleAnim); // (down on its hooves)
        play->envCtx.fillScreen = true;
        play->envCtx.screenFillColor[3] = 255;
        // (behind the white, too: it has turned to face Link - it means to kill him - and Link stands a few paces off
        // its forehooves, facing it, for the face-off)
        this->actor.world.rot.y = this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
        {
            Player* player = GET_PLAYER(play);
            s16 yaw = this->actor.yawTowardsPlayer;

            player->actor.world.pos.x = this->actor.world.pos.x + Math_SinS(yaw) * 330.0f;
            player->actor.world.pos.z = this->actor.world.pos.z + Math_CosS(yaw) * 330.0f;
            player->actor.world.pos.y = this->actor.world.pos.y;
            player->actor.prevPos = player->actor.world.pos;
            player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = yaw + 0x8000;
        }
        EffectSsBlast_SpawnWhiteShockwave(play, &pos, &zero, &zero);
        pos.y -= HT_LYNEL_SPOUT_DROP - 15.0f; // (on the water)
        EffectSsGRipple_Spawn(play, &pos, 400, 3000, 0);
        EffectSsGRipple_Spawn(play, &pos, 300, 2400, 6);
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOMB_EXPLOSION);
        HtLynel_Rumble(this, 25);
        HtLynel_Quake(this, play, 8, 25);
        Rumble_Request(0.0f, 200, 20, 10);
        HtLynel_SpawnSpouts(this, play);
        this->upperAnim = LYNEL_ANIM_GUARD;
        this->upperFrame = 0;
    }
    if ((t > CS_BLAST + 3) && play->envCtx.fillScreen) {
        // (held white a moment, then fading off the Lynel over ~20 frames)
        if (play->envCtx.screenFillColor[3] > 13) {
            play->envCtx.screenFillColor[3] -= 13;
        } else {
            play->envCtx.fillScreen = false;
        }
    }
    if (t == CS_TITLE - 6) {
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_BOSS); // the boss music
    }
    if (t == CS_TITLE) {
        // its name - and it rears up on its hind legs, neighing, then roars: a show of strength
        TitleCard_InitBossName(play, &play->actorCtx.titleCtx, (void*)HT_LYNEL_TITLE_TEX, 160, 180, 128, 40, false);
        Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonRearingAnim, 0.7f, 0.0f,
                         Animation_GetLastFrame((AnimationHeader*)gHorseGanonRearingAnim), ANIMMODE_ONCE, -6.0f);
        this->upperAnim = LYNEL_ANIM_GUARD;
        this->upperFrame = 0;
    }
    if (t == CS_TITLE + 6) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
    }
    if (t == CS_TITLE + 22) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
    }
    if (t == CS_TITLE + 48) {
        Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonIdleAnim, 1.0f, 0.0f,
                         Animation_GetLastFrame((AnimationHeader*)gHorseGanonIdleAnim), ANIMMODE_LOOP, -8.0f);
        this->upperAnim = LYNEL_ANIM_GLARE; // down on its hooves, squared up to Link, glaring down at him
        this->upperFrame = 0;
    }
    if (t == CS_FACEOFF + 6) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_BREATH);
    }

    if (t < CS_STAND + LYNEL_STAND_FRAMES) {
        // lying (breathing), then getting up: the authored poses, frame by frame
        Vec3s* jt = this->skin.skelAnime.jointTable;
        const Vec3s* src = (t < CS_STAND) ? gHtLynelCrouchPose : gHtLynelStandUp[t - CS_STAND];
        s32 i;

        for (i = 0; i < LYNEL_CROUCH_JOINTS; i++) {
            jt[i] = src[i];
        }
    } else {
        SkelAnime_Update(&this->skin.skelAnime);
    }
    if (this->tform > 0.0f) {
        if (++this->upperFrame >= sLynelAnimFrames[this->upperAnim]) {
            this->upperAnim = LYNEL_ANIM_IDLE;
            this->upperFrame = 0;
        }
    }
    HtLynel_CsCamera(this, play);

    if (++this->csTimer >= CS_END) {
        // back to Link, where the camera now stands: the fight
        Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);

        cam->eye = this->camEye;
        cam->eyeNext = this->camEye;
        cam->at = this->camAt;
        func_800C08AC(play, this->subCamId, 0);
        this->subCamId = 0;
        func_80064534(play, &play->csCtx);
        Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
        play->envCtx.fillScreen = false;
        HtLynel_BecomeLynel(this);
        this->sinkTimer = -HT_LYNEL_SINK_DELAY; // (counting up to the sinking)
    }
}

// after the intro: once the title card has gone, the pedestal sinks flush with the floor, the Lynel on it going down
// with it
static void HtLynel_UpdateSink(HtLynel* this, PlayState* play) {
    if (this->sinkTimer >= HT_PEDESTAL_SINK_FRAMES) {
        return;
    }
    if (++this->sinkTimer == 0) {
        Flags_SetSwitch(play, HT_PEDESTAL_SINK_FLAG);
    }
    if (this->sinkTimer >= 0) {
        this->anchor.y = this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP * HtPedestal_SinkCurve(this->sinkTimer);
    }
}

// what it stands on: the pedestal's top (sinking, or sunk) inside its rim, else the arena floor
static f32 HtLynel_FloorY(HtLynel* this) {
    f32 r2 = SQ(this->actor.world.pos.x - this->actor.home.pos.x) + SQ(this->actor.world.pos.z - this->actor.home.pos.z);
    s32 t = CLAMP(this->sinkTimer, 0, HT_PEDESTAL_SINK_FRAMES);

    if (r2 < SQ(420.0f)) {
        return this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP * HtPedestal_SinkCurve(t);
    }
    return this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP;
}

static void HtLynel_UpdateChained(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3s* jt = this->skin.skelAnime.jointTable;
    s32 i;

    // lying still, breathing slowly
    this->breath++;
    for (i = 0; i < LYNEL_CROUCH_JOINTS; i++) {
        jt[i] = gHtLynelCrouchPose[i];
    }
    jt[0].y += (s16)(60.0f * Math_SinS(this->breath * 0x180));

    // Navi, as Link comes near the first time
    if (!Flags_GetSwitch(play, HT_LYNEL_NAVI_FLAG) &&
        (Actor_WorldDistXZToActor(&this->actor, &player->actor) < HT_LYNEL_NAVI_RANGE)) {
        Flags_SetSwitch(play, HT_LYNEL_NAVI_FLAG);
        Message_StartTextbox(play, HT_LYNEL_NAVI_TEXT, NULL);
    }

    // the chain: from the anchor to the fetlock, its collider halfway
    this->collider.dim.radius = 30;
    this->collider.dim.height = 40;
    this->collider.dim.yShift = -10;
    this->collider.dim.pos.x = (s16)((this->anchor.x + this->chainEnd.x) * 0.5f);
    this->collider.dim.pos.y = (s16)this->anchor.y;
    this->collider.dim.pos.z = (s16)((this->anchor.z + this->chainEnd.z) * 0.5f);
    // (Z-targeting's point: fixed on the chain's middle as it first lies - not following the leg's breathing)
    if ((this->leapFrom.x == 0.0f) && (this->leapFrom.z == 0.0f) &&
        ((this->chainEnd.x != this->anchor.x) || (this->chainEnd.z != this->anchor.z))) {
        this->leapFrom.x = (this->anchor.x + this->chainEnd.x) * 0.5f;
        this->leapFrom.z = (this->anchor.z + this->chainEnd.z) * 0.5f;
    }
    if ((this->leapFrom.x != 0.0f) || (this->leapFrom.z != 0.0f)) {
        this->actor.focus.pos.x = this->leapFrom.x;
        this->actor.focus.pos.z = this->leapFrom.z;
    } else {
        this->actor.focus.pos.x = this->anchor.x;
        this->actor.focus.pos.z = this->anchor.z;
    }
    this->actor.focus.pos.y = this->anchor.y + 15.0f;

    if (this->chainFreeze > 0) {
        // the Iai's frost running out along the chain from where it struck: then it shatters
        if (--this->chainFreeze == 0) {
            HtLynel_CutChain(this, play);
        }
        return;
    }
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (HtLynel_IsIaiCut(this->collider.base.ac)) {
            Actor* ac = this->collider.base.ac;
            f32 dx = this->chainEnd.x - this->anchor.x, dz = this->chainEnd.z - this->anchor.z;
            f32 l2 = SQ(dx) + SQ(dz);

            this->chainHitT = 0.5f;
            if ((ac != NULL) && (l2 > 1.0f)) {
                this->chainHitT = CLAMP(((ac->world.pos.x - this->anchor.x) * dx + (ac->world.pos.z - this->anchor.z) * dz) / l2,
                                        0.0f, 1.0f);
            }
            this->chainFreeze = LYNEL_CHAIN_FREEZE;
            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_STRIKE_HARD);
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_FREEZE);
            return;
        }
        if (this->timer == 0) {
            Vec3f spark = this->actor.focus.pos;

            CollisionCheck_SpawnShieldParticlesMetal(play, &spark);
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
            this->timer = 10;
        }
    }
    if (this->timer > 0) {
        this->timer--;
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

// ---- phase 1 --------------------------------------------------------------------------------------------------
// It gallops out to the arena's rim and round it; then it pulls up facing Link, rears on its hind legs neighing (the
// tell), and charges him head down, horns first. The spouts are the counter - but the timing is everything: a pillar
// that was already ice when the charge began, or frozen while the Lynel was still far off, it sees: it swerves round
// it (or breaks off and gallops away). Only a pillar frozen at the last moment, in its path, catches its horns.
typedef enum {
    P1_WAIT,     // the pedestal still sinking
    P1_TO_RIM,   // galloping out to the circle
    P1_CIRCLE,   // round the arena
    P1_TURN,     // pulling up, turning on Link
    P1_REAR,     // the tell: up on its hind legs, neighing
    P1_CHARGE,
    P1_RECOVER,  // slowing out of a charge
    P1_STUCK,    // horns in a pillar (placeholder: the stuck crouch and the openings come next)
    P1_SLASH,    // Link stood in its way: a sword cut ahead (blocked: it tramples him)
    P1_KICK,     // Link too close behind: both hind legs lashed out (unblockable)
    P1_LEAP,     // the opening: wheeling away from Link and leaping off the pedestal, out toward the rim
    P2_POUND,    // phase 2: the ground pound (a jump in place: boulders, fire slugs)
    P2_TELL,     // phase 2: the big charge's tell - a long neigh, reared, arms crossed
    P2_RUSH,     // ...straight at Link (an Iai stops it)...
    P2_CUT,      // ...the cross slash, point blank
    P2_HURT,     // struck by the Iai
    P2_XCHARGE,  // phase 2-3: winding up the ranged X (both swords raised, gathering)
    P2_XWAIT,    // ...thrown: watching it go by Link, then the tell
    P3_WATCH,    // phase 3: turning after Link round the ring
    P3_STRIKE,   // ...a sword strike
    P3_BLADE,    // ...dodged: its blade bitten into the stone (an opening)
    P3_STAGGER,  // ...blocked: its arm thrown back - and straight on into a cut with the other
    P3_REELING,  // ...both cuts blocked: reeling, wide open (two strikes)
    P3_REPOS,    // ...after the X: wheeling round square onto Link
    P3_XOUT,     // ...the X's second half: both blades flung outward
    P3_FIRE,     // ...its fire breath
    P3_SPIN,     // ...the whirl (both blades right round: roll under it)
    P1_THROW,    // every now and then from the circle: a cut that flings a slanted wave of dark force at Link
} HtLynelAction;

#define P1_WAVE_SPEED 30.0f       // the sword wave: unblockable - Link has to get out of its way
#define P1_WAVE_HALF 105.0f       // half its width (it hits within ~85 of its middle)
#define P1_WAVE_SLANT 0.7f        // its slant (rise across its width)
#define P1_WAVE_LIFE 70
#define P2_DMG_X 0x20             // the X: two hearts
#define P2_X_SPEED 16.0f          // ...and slow (a dodge across the cage, not a reflex)
#define P2_X_LIFE 130
#define P2_X_HALF 320.0f          // the X's half width: Link has to run for the far side of the cage
#define P2_X_RISE 0.42f           // its arms' slope (a broad X, ~23 degrees off level)
#define P2_X_Y 120.0f             // its crossing's height
#define P1_DMG_WAVE 0x18          // a heart and a half

#define P1_SLASH_HIT0 13          // the slash's cutting frames
#define P1_SLASH_HIT1 19
#define P1_DMG_SLASH 0x10         // a heart
#define P1_DMG_TRAMPLE 0x20       // two
#define P1_DMG_KICK 0x20
#define P1_DMG_CHARGE 0x20

#define P1_RIM_R 1150.0f          // the circle it gallops on (the spouts stand at 800, the wall at 1500)
#define P1_GALLOP_SPEED 24.0f
#define P1_CHARGE_SPEED 34.0f
#define P1_NOTICE_DIST 520.0f     // a pillar turning to ice farther off than this, it sees in time (~0.75 s)
#define P1_REAR_FRAMES 33          // the tell: rearing, neighing, then the charge
#define P1_LEAP_TURN 12           // the opening leap: wheeling away first...
#define P1_LEAP_DIST 700.0f       // ...then this far through the air
#define P1_HORN_REACH 105.0f      // from its middle to the horns' tips (the head down), at the charge
#define P1_STUCK_FRAMES 170       // horns in the ice: the Iai's window (8.5 s)
#define P1_ARENA_R 1360.0f        // it never runs farther out than this


static void HtLynel_SetAction(HtLynel* this, s16 action, s16 timer) {
    this->action = action;
    this->actTimer = timer;
}

static void HtLynel_HorseAnim(HtLynel* this, AnimationHeader* anim, f32 speed, u8 mode, f32 morph) {
    Animation_Change(&this->skin.skelAnime, anim, speed, 0.0f, Animation_GetLastFrame(anim), mode, morph);
}

#define HT_LYNEL_UPPER_BLEND 5
static void HtLynel_UpperAnim(HtLynel* this, s16 anim) {
    if (this->upperAnim != anim) {
        // (blended over from where it was)
        this->upperFromAnim = this->upperAnim;
        this->upperFromFrame = CLAMP(this->upperFrame, 0, sLynelAnimFrames[this->upperAnim] - 1);
        this->upperBlend = HT_LYNEL_UPPER_BLEND;
        this->upperAnim = anim;
        this->upperFrame = 0;
    }
}

static void HtLynel_StartGallop(HtLynel* this) {
    HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonGallopingAnim, 1.0f, ANIMMODE_LOOP, -6.0f);
    HtLynel_UpperAnim(this, LYNEL_ANIM_IDLE);
}

// steer toward a point, turning at most maxTurn a frame
static void HtLynel_SteerTo(HtLynel* this, f32 x, f32 z, s16 maxTurn) {
    s16 yaw = Math_FAtan2F(x - this->actor.world.pos.x, z - this->actor.world.pos.z) * (0x8000 / M_PI);

    Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 3, maxTurn, 0x40);
}

static void HtLynel_Move(HtLynel* this, PlayState* play_) {
    f32 dx, dz, r;

    this->actor.world.pos.x += Math_SinS(this->actor.world.rot.y) * this->speed;
    this->actor.world.pos.z += Math_CosS(this->actor.world.rot.y) * this->speed;
    dx = this->actor.world.pos.x - this->actor.home.pos.x;
    dz = this->actor.world.pos.z - this->actor.home.pos.z;
    r = sqrtf(SQ(dx) + SQ(dz));
    if (r > P1_ARENA_R) { // (kept off the wall)
        this->actor.world.pos.x = this->actor.home.pos.x + dx * P1_ARENA_R / r;
        this->actor.world.pos.z = this->actor.home.pos.z + dz * P1_ARENA_R / r;
    }
    this->actor.world.pos.y = HtLynel_FloorY(this);
    // the gallop's beat keeps time with its pace: hoofbeats, and the water kicked up
    if (this->skin.skelAnime.animation == (void*)gHorseGanonGallopingAnim) {
        // (the horse's gallop sound is a whole stride of hoofbeats: once a stride, the legs turning over to match)
        this->skin.skelAnime.playSpeed = CLAMP(this->speed / 11.0f, 0.6f, 3.2f);
        if (Animation_OnFrame(&this->skin.skelAnime, 0.0f)) {
            Vec3f pos = this->actor.world.pos;

            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN);
            if (this->actor.world.pos.y < this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP + 10.0f) { // (in the water)
                Audio_PlayActorSound2(&this->actor, NA_SE_PL_WALK_WATER1);
                pos.y += 15.0f;
                pos.x -= Math_SinS(this->actor.world.rot.y) * 60.0f;
                pos.z -= Math_CosS(this->actor.world.rot.y) * 60.0f;
                EffectSsGSplash_Spawn(play_, &pos, NULL, NULL, 0, 300);
                EffectSsGRipple_Spawn(play_, &pos, 100, 500, 0);
            }
        }
    }
}

// the charge's look ahead, pillar by pillar. Returns the index of a pillar it has run into (stuck), else -1; may
// swerve round a pillar it has seen, or break off (P1_RECOVER)
static s32 HtLynel_ChargeCheckPillars(HtLynel* this, PlayState* play) {
    f32 fx = Math_SinS(this->actor.world.rot.y);
    f32 fz = Math_CosS(this->actor.world.rot.y);
    s32 i;

    for (i = 0; i < HT_LYNEL_SPOUTS; i++) {
        Actor* sp = this->spouts[i];
        f32 dx, dz, ahead, side, dist, half;

        if ((sp == NULL) || !HtWaterSpout_IsSolid(sp)) {
            this->spoutKnown[i] = false; // (melted: if it freezes again, that's news)
            continue;
        }
        dx = sp->world.pos.x - this->actor.world.pos.x;
        dz = sp->world.pos.z - this->actor.world.pos.z;
        ahead = dx * fx + dz * fz;
        side = dx * fz - dz * fx;
        dist = sqrtf(SQ(dx) + SQ(dz));
        half = HtWaterSpout_Width(sp) * 0.5f;
        if (!this->spoutKnown[i] && (dist > P1_NOTICE_DIST)) {
            // it sees this one in time
            this->spoutKnown[i] = true;
            if ((ahead > 0.0f) && (fabsf(side) < half + 90.0f) && (Rand_ZeroOne() < 0.4f)) {
                // ...and breaks off the charge, wheeling away
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
                return -2;
            }
        }
        if (ahead <= 0.0f) {
            continue;
        }
        if (this->spoutKnown[i]) {
            // a pillar it knows: swerve round it (a sidestep, then back on Link)
            if ((fabsf(side) < half + 110.0f) && (ahead < 520.0f)) {
                this->actor.world.rot.y += (side > 0.0f) ? -0x300 : 0x300;
            }
        } else if ((fabsf(side) < half + 40.0f) && (ahead < P1_HORN_REACH + half)) {
            return i; // frozen too late to see: the horns go into it
        }
    }
    return -1;
}

// Link, in its frame: how far ahead of it (- behind), how far to its left (- right)
static void HtLynel_LinkLocal(HtLynel* this, Player* player, f32* ahead, f32* side) {
    f32 dx = player->actor.world.pos.x - this->actor.world.pos.x;
    f32 dz = player->actor.world.pos.z - this->actor.world.pos.z;
    f32 fx = Math_SinS(this->actor.world.rot.y);
    f32 fz = Math_CosS(this->actor.world.rot.y);

    *ahead = dx * fx + dz * fz;
    *side = dx * fz - dz * fx;
}

// Link holding his shield up against it
static s32 HtLynel_LinkShields(HtLynel* this, Player* player) {
    s16 toLynel = Actor_WorldYawTowardActor(&player->actor, &this->actor);

    return (player->stateFlags1 & PLAYER_STATE1_SHIELDING) && (ABS((s16)(player->actor.shape.rot.y - toLynel)) < 0x3000);
}

// an authored horse pose (stuck, kick) about to be written over the joint table: blended onto from the pose as it is
#define LYNEL_POSE_BLEND 6
static void HtLynel_PoseBlendFrom(HtLynel* this) {
    s32 i;

    for (i = 0; i < LYNEL_CROUCH_JOINTS; i++) {
        this->poseFrom[i] = this->skin.skelAnime.jointTable[i];
    }
    this->poseBlend = 0;
}

static void HtLynel_WritePose(HtLynel* this, const Vec3s* pose) {
    Vec3s* jt = this->skin.skelAnime.jointTable;
    s32 i;

    if (this->poseBlend >= LYNEL_POSE_BLEND) {
        for (i = 0; i < LYNEL_CROUCH_JOINTS; i++) {
            jt[i] = pose[i];
        }
        return;
    }
    {
        f32 t = (f32)(++this->poseBlend) / LYNEL_POSE_BLEND;

        t = t * t * (3.0f - 2.0f * t);
        // (the root's a translation; the rest are angles, taken the short way round)
        jt[0].x = this->poseFrom[0].x + (s16)((pose[0].x - this->poseFrom[0].x) * t);
        jt[0].y = this->poseFrom[0].y + (s16)((pose[0].y - this->poseFrom[0].y) * t);
        jt[0].z = this->poseFrom[0].z + (s16)((pose[0].z - this->poseFrom[0].z) * t);
        for (i = 1; i < LYNEL_CROUCH_JOINTS; i++) {
            jt[i].x = this->poseFrom[i].x + (s16)((s16)(pose[i].x - this->poseFrom[i].x) * t);
            jt[i].y = this->poseFrom[i].y + (s16)((s16)(pose[i].y - this->poseFrom[i].y) * t);
            jt[i].z = this->poseFrom[i].z + (s16)((s16)(pose[i].z - this->poseFrom[i].z) * t);
        }
    }
}

// horns caught: rooted, both hands on the ice, heaving back against it
static void HtLynel_StartPush(HtLynel* this) {
    HtLynel_PoseBlendFrom(this);
    this->poseFrame = 0;
    HtLynel_UpperAnim(this, LYNEL_ANIM_PUSH);
    this->upperFrame = 0;
}

static void HtLynel_Interrupt(HtLynel* this, s16 action, s16 timer) {
    this->prevAction = this->action;
    this->prevTimer = this->actTimer;
    this->hitDone = false;
    HtLynel_SetAction(this, action, timer);
}

// the horns slam into the ice: the ground shakes (as under the Megaton Hammer), a heavy thud, ice flying
static void HtLynel_StuckImpact(HtLynel* this, PlayState* play, Actor* sp, s32 big) {
    static Color_RGBA8 prim = { 200, 240, 255, 255 };
    static Color_RGBA8 env = { 90, 170, 255, 255 };
    Vec3f accel = { 0.0f, -1.0f, 0.0f };
    s16 q = Quake_Add(GET_ACTIVE_CAM(play), 3);
    s32 i;

    Quake_SetSpeed(q, 27767);
    Quake_SetQuakeValues(q, big ? 7 : 3, 0, 0, 0);
    Quake_SetCountdown(q, big ? 20 : 8);
    Rumble_Request(0.0f, big ? 255 : 140, big ? 20 : 8, big ? 150 : 100);
    if (big) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_IRONNACK_BREAK_PILLAR);
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_BLOCK_BOUND);
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
    } else {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_BLOCK_BOUND);
    }
    if (sp != NULL) {
        for (i = 0; i < (big ? 18 : 5); i++) {
            Vec3f pos = { sp->world.pos.x + Rand_CenteredFloat(100.0f), sp->world.pos.y + 60.0f + Rand_ZeroFloat(110.0f),
                          sp->world.pos.z + Rand_CenteredFloat(100.0f) };
            Vec3f vel = { Rand_CenteredFloat(8.0f), Rand_ZeroFloat(6.0f) + 2.0f, Rand_CenteredFloat(8.0f) };

            EffectSsEnIce_Spawn(play, &pos, Rand_ZeroFloat(0.3f) + 0.3f, &vel, &accel, &prim, &env, 25);
        }
    }
    if (big) {
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        Vec3f pos = this->actor.world.pos;

        pos.y += 15.0f;
        EffectSsBlast_SpawnWhiteShockwave(play, &pos, &zero, &zero);
        EffectSsGRipple_Spawn(play, &pos, 200, 1600, 0);
    }
}

// stuck, it can be cut: an Iai at its flank (from the side - behind, it kicks). Returns true if it was hurt
static s32 HtLynel_CheckIai(HtLynel* this, PlayState* play, f32 ahead, f32 side) {
    this->collider.dim.radius = 95;
    this->collider.dim.height = 210;
    this->collider.dim.yShift = 0;
    this->collider.dim.pos.x = (s16)this->actor.world.pos.x;
    this->collider.dim.pos.y = (s16)this->actor.world.pos.y;
    this->collider.dim.pos.z = (s16)this->actor.world.pos.z;
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (HtLynel_IsIaiCut(this->collider.base.ac)) {
            return true;
        }
        if (this->timer == 0) {
            Vec3f spark = this->actor.world.pos;

            spark.y += 100.0f;
            CollisionCheck_SpawnShieldParticlesMetal(play, &spark);
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
            this->timer = 10;
        }
    }
    if (this->timer > 0) {
        this->timer--;
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    return false;
}

// its sword wave: a slanted crescent of dark force skimming the water at Link (no shield stops it)
static void HtLynel_UpdateOneWave(HtLynel* this, PlayState* play, HtLynelWave* w) {
    static Color_RGBA8 prim = { 230, 140, 255, 255 };
    static Color_RGBA8 env = { 90, 0, 140, 0 };
    Player* player = GET_PLAYER(play);
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f drift = { 0.0f, 0.3f, 0.0f };
    f32 fx, fz, dx, dz, along, lat, prevAlong;
    s32 i;

    if (!w->on) {
        return;
    }
    fx = Math_SinS(w->yaw);
    fz = Math_CosS(w->yaw);
    dx = player->actor.world.pos.x - w->pos.x;
    dz = player->actor.world.pos.z - w->pos.z;
    prevAlong = dx * fx + dz * fz;
    {
        f32 sp = (w->slant == 2) ? P2_X_SPEED : P1_WAVE_SPEED;

        w->pos.x += fx * sp;
        w->pos.z += fz * sp;
        along = prevAlong - sp;
    }
    lat = dx * fz - dz * fx;
    // swept past Link this frame, near enough its middle: he's thrown (unblockable)
    if ((prevAlong >= -10.0f) && (along <= 10.0f) &&
        (fabsf(lat) < ((w->slant == 2) ? P2_X_HALF : P1_WAVE_HALF) - 20.0f) &&
        (fabsf(player->actor.world.pos.y - w->pos.y) < 90.0f) && !Player_InCsMode(play)) {
        Actor_SetPlayerKnockbackLarge(play, &this->actor, 12.0f, w->yaw, 8.0f,
                                      (w->slant == 2) ? P2_DMG_X : P1_DMG_WAVE);
        Audio_PlayActorSound2(&player->actor, NA_SE_EN_MGANON_SWDIMP);
        w->on = false;
        return;
    }
    // the crescent itself is drawn (HtLynel_DrawWave): here, the violet sparks and dark smoke it trails
    for (i = 0; i <= 4; i++) {
        f32 t = i / 4.0f * 2.0f - 1.0f;
        f32 half = (w->slant == 2) ? P2_X_HALF : P1_WAVE_HALF;
        f32 sl = (w->slant == 2) ? ((t > 0.0f) ? -1.0f : 1.0f) : w->slant; // (the triangle's sides)
        f32 rise = (w->slant == 2) ? P2_X_RISE : P1_WAVE_SLANT * 0.5f;
        Vec3f pos = { w->pos.x + fz * t * half, w->pos.y + ((w->slant == 2) ? P2_X_Y : 55.0f) + sl * t * half * rise,
                      w->pos.z - fx * t * half };

        EffectSsKiraKira_SpawnDispersed(play, &pos, &zero, &drift, &prim, &env, 3000, 10);
        if ((i & 1) == 0) {
            Vec3f vel = { -fx * 2.0f, 0.5f, -fz * 2.0f };

            EffectSsDeadDb_Spawn(play, &pos, &vel, &zero, 45, 6, 120, 40, 170, 200, 40, 0, 60, 1, 6, false);
        }
    }
    if (((w->life % 4) == 0) && (this->waterDrop < 20.0f)) {
        EffectSsGSplash_Spawn(play, &w->pos, NULL, NULL, 0, 250);
    }
    HtSfx_PlayFor(w->pos.x, w->pos.y, w->pos.z, NA_SE_EN_FANTOM_FIRE, 2);
    if ((--w->life <= 0) || (SQ(w->pos.x - this->actor.home.pos.x) +
                                     SQ(w->pos.z - this->actor.home.pos.z) > SQ(1480.0f))) {
        w->on = false; // (spent against the wall)
    }
}

static void HtLynel_UpdateWave(HtLynel* this, PlayState* play) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(this->waves); i++) {
        HtLynel_UpdateOneWave(this, play, &this->waves[i]);
    }
}

// ---- phase 2: Link caged in the fire ------------------------------------------------------------------------------
// The Lynel round the rim outside, throwing its sword waves (and now the X, both swords at once: wider, harder);
// every so often a ground pound - a jump in place that brings boulders down from the vault on Link (and some at random)
// and fire slugs into the cage (each one killed leaves a magic jar: the katana runs on magic). Once Link has cut
// himself room enough in the rings, the big charge: a long neigh, up on its hind legs, arms crossed and both swords
// swept back behind it; then straight at him through the fire - torso open: an Iai strike while it comes stops it.
// Missed, it's on him: the cross slash, point blank (four hearts). Three strikes end phase 2.
#define P2_CUT_NEED_0 4           // segments Link must cut before the big charge...
#define P2_CHARGE_EVERY 300       // ...or this long since the last one, whatever he's cut (15 s)
#define P2_XCHARGE_FRAMES 36      // the ranged X's wind-up (its warning)
#define P2_TELL_FRAMES 80          // reared and neighing: Link's time to charge his Iai (tight)
#define P2_TELL_APEX 11           // (the rearing's top: held there)
#define P2_RUSH_SPEED 30.0f
#define P2_CUT_REACH 150.0f       // how near it starts the cross slash
#define P2_DMG_CROSS 0x40         // four hearts
#define P2_DMG_ROCK 0x10
#define P2_RAIN_FRAMES 170        // a ground pound's rain of boulders (Volvagia's): this long...
#define P2_RAIN_EVERY 14          // ...one every so often (every third on Link)
#define P2_HURT_FRAMES 34         // reeling from the Iai: reared and wheeling about
#define P2_ROCK_DROP 650.0f

static s32 HtLynel_P2Tier(HtLynel* this) {
    return CLAMP(this->p2Hits, 0, 2);
}

// the big charge's tell: up on its hind legs, arms crossed, neighing (P2_TELL)
static void HtLynel_StartTell(HtLynel* this) {
    this->reelTimer = 0;
    this->speed = 0.0f;
    this->hitDone = false;
    Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonRearingAnim, 0.5f, 0.0f,
                     Animation_GetLastFrame((AnimationHeader*)gHorseGanonRearingAnim), ANIMMODE_ONCE, -4.0f);
    HtLynel_UpperAnim(this, LYNEL_ANIM_CROSSWIND);
    HtLynel_SetAction(this, P2_TELL, P2_TELL_FRAMES);
    this->rainTimer = 0; // (no more boulders: Link's to watch the charge)
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
}

// ---- Volvagia's falling rocks (En_Vb_Ball, ported: that actor needs Volvagia as its parent) -----------------------
// A boulder drops from the vault, its shadow on the floor under it; where it lands it bursts in two halves thrown
// off a little way, each bursting in two again, those into chips - the floor shaking with the boulder, Link hurt by
// any of them that falls on him (it shatters on him).
#define HT_VOLVAGIA_ROCK_DL "__OTR__objects/object_fd/gVolvagiaRockDL"
#define HT_VOLVAGIA_DEBRIS_MAT_DL "__OTR__objects/object_fd/gVolvagiaDebrisMaterialDL"
#define HT_VOLVAGIA_DEBRIS_DL "__OTR__objects/object_fd/gVolvagiaDebrisModelDL"

static void HtLynel_SpawnChip(HtLynel* this, Vec3f* pos, Vec3f* vel, f32 scale, f32 floorY) {
    s32 i;

    for (i = 0; i < HT_LYNEL_CHIPS; i++) {
        HtLynelChip* c = &this->chips[i];

        if (!c->on) {
            c->on = true;
            c->pos = *pos;
            c->vel = *vel;
            c->scale = scale / 1000.0f;
            c->rx = Rand_ZeroFloat(100.0f);
            c->ry = Rand_ZeroFloat(100.0f);
            c->floorY = floorY;
            c->gen = ++this->rockGen;
            return;
        }
    }
}

static HtLynelRock* HtLynel_SpawnRockAt(HtLynel* this, Vec3f* pos, f32 scale, s32 size, f32 floorY) {
    s32 i;

    for (i = 0; i < HT_LYNEL_ROCKS; i++) {
        HtLynelRock* r = &this->rocks[i];

        if (!r->on) {
            r->on = true;
            r->size = size;
            r->pos = *pos;
            r->vel.x = r->vel.y = r->vel.z = 0.0f;
            r->scale = scale;
            r->floorY = floorY;
            r->rotX = r->rotY = 0.0f;
            r->rvX = BINANG_TO_RAD((s16)Rand_CenteredFloat(0x2000));
            r->rvY = BINANG_TO_RAD((s16)Rand_CenteredFloat(0x2000));
            r->shadowA = 0.0f;
            r->gen = ++this->rockGen;
            return r;
        }
    }
    return NULL;
}

// a boulder off the vault over (x, z), down onto floorY (as Volvagia's: scale 0.013 - 0.018)
static void HtLynel_SpawnRock(HtLynel* this, PlayState* play, f32 x, f32 z, f32 floorY) {
    Vec3f p = { x, floorY + P2_ROCK_DROP, z };
    s32 i;

    if (HtLynel_SpawnRockAt(this, &p, ((s16)Rand_ZeroFloat(50.0f) + 130) / 10000.0f, 0, floorY) != NULL) {
        for (i = 0; i < 6; i++) {
            Vec3f dp = { x + Rand_CenteredFloat(300.0f), floorY + P2_ROCK_DROP - Rand_ZeroFloat(150.0f),
                         z + Rand_CenteredFloat(300.0f) };
            Vec3f dv = { 0.0f, 0.0f, 0.0f };

            HtLynel_SpawnChip(this, &dp, &dv, (s16)Rand_ZeroFloat(15.0f) + 20, floorY);
        }
    }
}

static void HtLynel_UpdateRocks(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 cx = this->actor.home.pos.x;
    f32 cz = this->actor.home.pos.z;
    s32 i, k;

    for (i = 0; i < HT_LYNEL_CHIPS; i++) {
        HtLynelChip* c = &this->chips[i];

        if (!c->on) {
            continue;
        }
        c->vel.y -= 1.0f;
        c->pos.x += c->vel.x;
        c->pos.y += c->vel.y;
        c->pos.z += c->vel.z;
        c->rx += 0.55f;
        c->ry += 0.1f;
        if (c->pos.y <= c->floorY) {
            c->on = false;
        }
    }
    for (i = 0; i < HT_LYNEL_ROCKS; i++) {
        HtLynelRock* r = &this->rocks[i];
        f32 rad, dx, dz, dist, d2;
        s32 burst = false, onLink = false;

        if (!r->on) {
            continue;
        }
        r->rotX += r->rvX;
        r->rotY += r->rvY;
        r->vel.y -= 2.0f; // (Volvagia's: -1 a frame twice over)
        r->pos.x += r->vel.x;
        r->pos.y += r->vel.y;
        r->pos.z += r->vel.z;
        Math_ApproachF(&r->shadowA, 175.0f, 1.0f, 40.0f);
        if ((this->rainOuter > this->rainInner) && (r->size != 0)) {
            // (the pieces thrown off keep to the floor the rain falls on)
            dx = r->pos.x - cx;
            dz = r->pos.z - cz;
            dist = sqrtf(SQ(dx) + SQ(dz));
            if ((dist > 1.0f) && ((dist < this->rainInner) || (dist > this->rainOuter))) {
                f32 to = CLAMP(dist, this->rainInner, this->rainOuter);

                r->pos.x = cx + dx / dist * to;
                r->pos.z = cz + dz / dist * to;
            }
        }
        rad = r->scale * 1700.0f;
        // on Link: it shatters on him
        dx = player->actor.world.pos.x - r->pos.x;
        dz = player->actor.world.pos.z - r->pos.z;
        d2 = SQ(dx) + SQ(dz);
        if ((d2 < SQ(r->scale * 3000.0f + 15.0f)) && (r->pos.y - rad < player->actor.world.pos.y + 60.0f) &&
            (r->pos.y + rad > player->actor.world.pos.y) && !Player_InCsMode(play)) {
            burst = onLink = true;
        }
        if ((r->pos.y - rad <= r->floorY) && (r->vel.y <= 0.0f)) {
            r->pos.y = r->floorY + rad;
            burst = true;
        }
        if (!burst) {
            continue;
        }
        r->on = false;
        if (onLink) {
            Actor_SetPlayerKnockbackSmall(play, &this->actor, 8.0f, Math_FAtan2F(dx, dz) * (0x8000 / M_PI), 5.0f,
                                          (r->size == 0) ? P2_DMG_ROCK : P2_DMG_ROCK / 2);
            Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_HIT);
        }
        if (r->size < 2) {
            // in two, thrown off - the boulder's with a shake of the floor and a crash
            if (r->size == 0) {
                Actor_RequestQuakeAndRumble(&this->actor, play, 5, 10);
            }
            for (k = 0; k < 2; k++) {
                Vec3f off;
                HtLynelRock* n;

                if (r->size == 0) {
                    off.x = Rand_CenteredFloat(13.0f);
                    off.y = Rand_ZeroFloat(5.0f) + 6.0f;
                    off.z = Rand_CenteredFloat(13.0f);
                } else {
                    off.x = Rand_CenteredFloat(10.0f);
                    off.y = Rand_ZeroFloat(3.0f) + 4.0f;
                    off.z = Rand_CenteredFloat(10.0f);
                }
                {
                    Vec3f np = { r->pos.x + off.x, r->pos.y + off.y, r->pos.z + off.z };

                    n = HtLynel_SpawnRockAt(this, &np, r->scale * 0.5f, r->size + 1, r->floorY);
                }
                if (n != NULL) {
                    if ((k == 0) && (r->size == 0)) {
                        HtSfx_PlayFor(n->pos.x, n->pos.y, n->pos.z, NA_SE_EN_VALVAISA_ROCK, 1);
                    }
                    n->vel = off;
                    n->rvY = 0.0f;
                    n->rvX = BINANG_TO_RAD((s16)(0x1000 / 10.0f * sqrtf(SQ(off.x) + SQ(off.z))));
                    n->rotY = Math_FAtan2F(off.x, off.z);
                    n->shadowA = 200.0f;
                }
            }
            for (k = 0; k < 15; k++) {
                Vec3f cp = { r->pos.x + Rand_CenteredFloat(10.0f), r->pos.y + Rand_CenteredFloat(10.0f),
                             r->pos.z + Rand_CenteredFloat(10.0f) };
                Vec3f cv = { Rand_CenteredFloat(25.0f), Rand_ZeroFloat(5.0f) + 8.0f, Rand_CenteredFloat(25.0f) };

                HtLynel_SpawnChip(this, &cp, &cv, (s16)Rand_ZeroFloat(12.0f) + 15, r->floorY);
            }
            {
                Vec3f fp = { r->pos.x, r->floorY, r->pos.z };

                Actor_SpawnFloorDustRing(play, &this->actor, &fp, (r->size == 0) ? 45.0f : 25.0f, 10, 7.0f,
                                         (r->size == 0) ? 350 : 220, 12, true);
            }
        } else {
            // the last of it: chips
            for (k = 0; k < 5; k++) {
                Vec3f cp = { r->pos.x + Rand_CenteredFloat(5.0f), r->pos.y + Rand_CenteredFloat(5.0f),
                             r->pos.z + Rand_CenteredFloat(5.0f) };
                Vec3f cv = { Rand_CenteredFloat(10.0f), Rand_ZeroFloat(3.0f) + 3.0f, Rand_CenteredFloat(10.0f) };

                HtLynel_SpawnChip(this, &cp, &cv, (s16)Rand_ZeroFloat(12.0f) + 15, r->floorY);
            }
            HtSfx_PlayFor(r->pos.x, r->pos.y, r->pos.z, NA_SE_EV_ROCK_BROKEN, 1);
        }
    }
}

// Volvagia's rain: a boulder every so often over the ring inner..outer (every third straight over Link)
static void HtLynel_UpdateRain(HtLynel* this, PlayState* play, f32 inner, f32 outer, f32 floorY) {
    Player* player = GET_PLAYER(play);

    this->rainInner = inner;
    this->rainOuter = outer;
    if (this->rainTimer <= 0) {
        return;
    }
    this->rainTimer--;
    if (--this->rainNext > 0) {
        return;
    }
    this->rainNext = P2_RAIN_EVERY;
    if ((this->rainTimer / P2_RAIN_EVERY) % 3 == 0) {
        HtLynel_SpawnRock(this, play, player->actor.world.pos.x, player->actor.world.pos.z, floorY);
    } else {
        f32 a = Rand_ZeroFloat(2.0f * M_PI);
        f32 r = inner + Rand_ZeroFloat(outer - inner);

        HtLynel_SpawnRock(this, play, this->actor.home.pos.x + sinf(a) * r, this->actor.home.pos.z + cosf(a) * r,
                          floorY);
    }
}

static void HtLynel_DrawRocks(HtLynel* this, PlayState* play) {
    s32 i, mat = false;

    OPEN_DISPS(play->state.gfxCtx);
    for (i = 0; i < HT_LYNEL_ROCKS; i++) {
        HtLynelRock* r = &this->rocks[i];
        f32 sh = r->scale * 68.0f;

        if (!r->on) {
            continue;
        }
        FrameInterpolation_RecordOpenChild(r, r->gen);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        Matrix_Translate(r->pos.x, r->pos.y, r->pos.z, MTXMODE_NEW);
        Matrix_RotateY(r->rotY, MTXMODE_APPLY);
        Matrix_RotateX(r->rotX, MTXMODE_APPLY);
        Matrix_Scale(r->scale, r->scale, r->scale, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_VOLVAGIA_ROCK_DL);
        // its shadow on the floor under it
        Gfx_SetupDL_44Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 0, 0, 0, (u8)r->shadowA);
        Matrix_Translate(r->pos.x, r->floorY + 1.0f, r->pos.z, MTXMODE_NEW);
        Matrix_Scale(sh, 1.0f, sh, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gCircleShadowDL);
        FrameInterpolation_RecordCloseChild();
    }
    for (i = 0; i < HT_LYNEL_CHIPS; i++) {
        HtLynelChip* c = &this->chips[i];

        if (!c->on) {
            continue;
        }
        FrameInterpolation_RecordOpenChild(c, c->gen);
        if (!mat) {
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_VOLVAGIA_DEBRIS_MAT_DL);
            mat = true;
        }
        Matrix_Translate(c->pos.x, c->pos.y, c->pos.z, MTXMODE_NEW);
        Matrix_RotateY(c->ry, MTXMODE_APPLY);
        Matrix_RotateX(c->rx, MTXMODE_APPLY);
        Matrix_Scale(c->scale, c->scale, 1.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_VOLVAGIA_DEBRIS_DL);
        FrameInterpolation_RecordCloseChild();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// is this actor still alive (in the enemy list)?
static s32 HtLynel_StillThere(PlayState* play, Actor* a) {
    Actor* e = play->actorCtx.actorLists[ACTORCAT_ENEMY].head;

    for (; e != NULL; e = e->next) {
        if ((e == a) && (e->update != NULL)) {
            return true;
        }
    }
    return false;
}

// the ground pound's landing: boulders down from the vault (one on Link), fire slugs into the cage
static void HtLynel_PoundLands(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 cx = this->actor.home.pos.x;
    f32 cz = this->actor.home.pos.z;
    f32 floorY = this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP;
    s32 i, n = (this->p2Hits >= 1) ? 5 : 0; // (the boulders: from phase 2-2 on)
    s16 q = Quake_Add(GET_ACTIVE_CAM(play), 3);

    Quake_SetSpeed(q, 27767);
    Quake_SetQuakeValues(q, 8, 0, 0, 0);
    Quake_SetCountdown(q, 22);
    Rumble_Request(0.0f, 220, 20, 140);
    HtLynel_Rumble(this, 22);
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_HAMMER_HIT);
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 120.0f, 14, 10.0f, 600, 20, true);
    if (n > 0) {
        // (the vault shaken loose: a rain of boulders, a while)
        this->rainTimer = P2_RAIN_FRAMES;
        this->rainNext = 0;
    }
    for (i = 0; i < 2; i++) {
        if ((this->slugs[i] == NULL) && (Rand_ZeroOne() < 0.85f)) {
            f32 a = Rand_ZeroFloat(2.0f * M_PI);
            f32 r = 150.0f + Rand_ZeroFloat(220.0f);
            f32 x = cx + sinf(a) * r;
            f32 z = cz + cosf(a) * r;

            if (SQ(x - player->actor.world.pos.x) + SQ(z - player->actor.world.pos.z) < SQ(150.0f)) {
                x = cx - sinf(a) * r; // (not on top of Link)
                z = cz - cosf(a) * r;
            }
            this->slugs[i] = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BW, x, floorY, z, 0, (s16)(a * 0x8000 / M_PI),
                                         0, 0);
            if (this->slugs[i] != NULL) {
                this->slugPos[i] = this->slugs[i]->world.pos;
            }
        }
    }
}

// phase 2's own business each frame: the cage's pace, the slugs' jars, the boulders, and when to charge or pound
static void HtLynel_UpdatePhase2(HtLynel* this, PlayState* play) {
    HtFireRing* ring;
    s32 tier = HtLynel_P2Tier(this);
    s32 free = (this->action == P1_CIRCLE) || (this->action == P1_TO_RIM);
    s32 i;

    if ((this->fireRing != NULL) && (this->fireRing->update == NULL)) {
        this->fireRing = NULL;
    }
    ring = (HtFireRing*)this->fireRing;
    if ((ring != NULL) && !ring->dying) {
        // (the same pace all through phase 2; it holds still while the big charge comes - Link's Iai)
        ring->emitInterval = 140;
        ring->maxRings = 2;
        ring->speed = 2.6f;
        ring->paused = (this->action == P2_TELL) || (this->action == P2_RUSH) || (this->action == P2_CUT) ||
                       (this->action == P2_XCHARGE) || (this->action == P2_XWAIT) ||
                       ((this->action == P1_THROW) && this->throwX);
    }
    for (i = 0; i < 2; i++) {
        if (this->slugs[i] == NULL) {
            continue;
        }
        if (HtLynel_StillThere(play, this->slugs[i])) {
            // (kept inside the cage)
            Actor* s = this->slugs[i];
            f32 dx = s->world.pos.x - this->actor.home.pos.x;
            f32 dz = s->world.pos.z - this->actor.home.pos.z;
            f32 d = sqrtf(SQ(dx) + SQ(dz));

            if (d > HT_FIRE_WALL_R - 45.0f) {
                s->world.pos.x = this->actor.home.pos.x + dx * (HT_FIRE_WALL_R - 45.0f) / d;
                s->world.pos.z = this->actor.home.pos.z + dz * (HT_FIRE_WALL_R - 45.0f) / d;
                s->world.rot.y += 0x8000; // (turned back in)
                s->shape.rot.y = s->world.rot.y;
            }
            this->slugPos[i] = s->world.pos;
        } else {
            // a slug killed: its magic jar
            Vec3f p = this->slugPos[i];

            p.y += 10.0f;
            {
                // (thrown in toward the cage's middle, not off to its edge: about half way in)
                EnItem00* jar = Item_DropCollectible(play, &p, ITEM00_MAGIC_LARGE);
                f32 jx = this->actor.home.pos.x - p.x;
                f32 jz = this->actor.home.pos.z - p.z;
                f32 jd = sqrtf(SQ(jx) + SQ(jz));

                if ((jar != NULL) && (jd > 60.0f)) {
                    jar->actor.world.rot.y = Math_FAtan2F(jx, jz) * (0x8000 / M_PI);
                    jar->actor.speedXZ = CLAMP(jd * 0.5f, 0.0f, 220.0f) / 20.0f; // (~20 frames up and down)
                }
            }
            this->slugs[i] = NULL;
        }
    }
    HtLynel_UpdateRain(this, play, 40.0f, HT_FIRE_WALL_R - 40.0f, this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP);
    HtLynel_UpdateRocks(this, play);
    if (this->calmTimer > 0) {
        this->calmTimer--;
    }
    if (this->p2Done || !free || Player_InCsMode(play)) {
        return;
    }
    // the big charge, once Link has cut himself room
    if (this->reelTimer < 1000) {
        this->reelTimer++; // (frames since its last big charge)
    }
    if ((ring != NULL) && ((ring->cutCount >= P2_CUT_NEED_0) || (this->reelTimer >= P2_CHARGE_EVERY)) &&
        (this->calmTimer == 0) && !this->waves[0].on && !this->waves[1].on) {
        if (this->p2Hits < 2) {
            // (phases 2-1 and 2-2: straight into the tell)
            HtLynel_StartTell(this);
            return;
        }
        // phase 2-3: first the X from range - wound up where Link can see it, then thrown where he stands (huge:
        // he runs for the far side of the cage)
        this->speed = MIN(this->speed, 10.0f);
        HtLynel_UpperAnim(this, LYNEL_ANIM_SLASHX);
        HtLynel_SetAction(this, P2_XCHARGE, P2_XCHARGE_FRAMES);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
        return;
    }
    // the ground pound: every 9-13 s, sooner when Link runs low on magic
    if (--this->poundTimer <= 0) {
        this->poundTimer = 180 + (s16)Rand_ZeroFloat(80.0f);
        if (gSaveContext.magic < gSaveContext.magicCapacity / 3) {
            this->poundTimer -= 70;
        }
        this->poseFrame = -1;
        HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
        HtLynel_SetAction(this, P2_POUND, 0);
    }
}

// struck while it comes (the Iai): it reels, then gallops off - three, and phase 2 is over
static void HtLynel_P2Struck(HtLynel* this, PlayState* play) {
    HtFireRing* ring = (HtFireRing*)this->fireRing;

    this->p2Hits++;
    Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 15);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_DAMAGE);
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
    // (the hit: up on its hind legs at once, head thrown back and arms flung out, wheeling right round on them; down
    // facing away, it gallops off)
    this->speed = 0.0f;
    this->circleAng = this->actor.world.rot.y + 0x8000; // (turned about)
    this->circleDir = (Rand_ZeroOne() < 0.5f) ? 1 : -1;  // (which way it wheels)
    Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonRearingAnim, 1.4f, 0.0f,
                     Animation_GetLastFrame((AnimationHeader*)gHorseGanonRearingAnim), ANIMMODE_ONCE, -2.0f);
    HtLynel_UpperAnim(this, LYNEL_ANIM_HURT);
    HtLynel_SetAction(this, P2_HURT, P2_HURT_FRAMES);
    this->calmTimer = 140;
    if (ring != NULL) {
        ring->cutCount = 0;
    }
    if (this->p2Hits >= 3) {
        // phase 2 over: the dig (HtLynel_UpdateTransition3)
        this->p2Done = true;
        this->form = HT_LYNEL_TRANS3;
        this->csTimer = 0;
    }
}

static void HtLynel_UpdatePhase1(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 cx = this->actor.home.pos.x;
    f32 cz = this->actor.home.pos.z;
    f32 ahead, side;
    s32 i;

    HtLynel_LinkLocal(this, player, &ahead, &side);
    if (this->slashCool > 0) {
        this->slashCool--;
    }
    // Link too close behind it (not in a charge): the double kick
    if (((this->action == P1_TURN) || (this->action == P1_STUCK) || (this->action == P1_RECOVER) ||
         ((this->action == P1_CIRCLE) && (this->speed < 10.0f))) &&
        (ahead < -70.0f) && (ahead > -230.0f) && (fabsf(side) < 55.0f) && !Player_InCsMode(play)) {
        HtLynel_Interrupt(this, P1_KICK, 0);
        HtLynel_PoseBlendFrom(this);
        HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_GROAN);
    }
    // Link standing in its path as it gallops: it cuts at him
    if (((this->action == P1_TO_RIM) || (this->action == P1_CIRCLE) || (this->action == P1_RECOVER)) &&
        (this->slashCool == 0) && (ahead > 60.0f) && (ahead < 330.0f) && (fabsf(side) < 140.0f)) {
        HtLynel_Interrupt(this, P1_SLASH, 0);
        HtLynel_UpperAnim(this, (side > 0.0f) ? LYNEL_ANIM_SLASHL : LYNEL_ANIM_SLASHR);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_SWORD);
    }

    switch (this->action) {
        case P1_WAIT:
            {
                // it opens with a leap: wheeling away from Link, off the pedestal, out toward the rim - then round it
                // (Link has his bearings before the first charge)
                f32 a = Math_FAtan2F(this->actor.world.pos.x - player->actor.world.pos.x,
                                     this->actor.world.pos.z - player->actor.world.pos.z);

                this->circleAng = (s16)(a * (0x8000 / M_PI));
                this->circleDir = (Rand_ZeroOne() < 0.5f) ? 1 : -1;
                this->throwCool = 60 + (s16)Rand_ZeroFloat(40.0f);
                this->speed = 0.0f;
                this->poseFrame = -1;
                HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
                HtLynel_SetAction(this, P1_LEAP, 0);
            }
            break;

        case P1_LEAP: {
            s32 f = this->actTimer - P1_LEAP_TURN;

            if (f < 0) {
                // wheeling round, away
                Math_SmoothStepToS(&this->actor.world.rot.y, this->circleAng, 2, 0x1800, 0x100);
                this->speed = 0.0f;
                this->poseFrame = -1;
            } else {
                if (f == 0) {
                    this->actor.world.rot.y = this->circleAng;
                    HtLynel_PoseBlendFrom(this);
                    HtLynel_UpperAnim(this, LYNEL_ANIM_CHARGE);
                }
                this->poseFrame = f;
                if (f == 2) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_JUMP);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
                }
                this->speed = (f < LYNEL_JUMP_UP)     ? 6.0f
                              : (f < LYNEL_JUMP_DOWN) ? (P1_LEAP_DIST / (LYNEL_JUMP_DOWN - LYNEL_JUMP_UP))
                                                      : P1_GALLOP_SPEED * 0.7f;
                if (f == LYNEL_JUMP_DOWN) {
                    // down, in a burst of water
                    Vec3f pos = this->actor.world.pos;
                    s16 q = Quake_Add(GET_ACTIVE_CAM(play), 3);

                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_LAND);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_WALK);
                    pos.y += 15.0f;
                    EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 600);
                    EffectSsGRipple_Spawn(play, &pos, 300, 1500, 0);
                    Quake_SetSpeed(q, 27767);
                    Quake_SetQuakeValues(q, 4, 0, 0, 0);
                    Quake_SetCountdown(q, 10);
                    Rumble_Request(0.0f, 180, 10, 120);
                }
                if (f >= LYNEL_JUMP_FRAMES - 1) {
                    // galloping on, out to the rim
                    this->speed = P1_GALLOP_SPEED * 0.8f;
                    HtLynel_StartGallop(this);
                    HtLynel_SetAction(this, P1_TO_RIM, 0);
                    break;
                }
                HtLynel_Move(this, play);
            }
            this->actTimer++;
            break;
        }

        case P1_TO_RIM: {
            f32 tx = cx + Math_SinS(this->circleAng) * P1_RIM_R;
            f32 tz = cz + Math_CosS(this->circleAng) * P1_RIM_R;

            Math_StepToF(&this->speed, P1_GALLOP_SPEED, 2.0f);
            HtLynel_SteerTo(this, tx, tz, 0xA00);
            HtLynel_Move(this, play);
            if (SQ(this->actor.world.pos.x - tx) + SQ(this->actor.world.pos.z - tz) < SQ(150.0f)) {
                HtLynel_SetAction(this, P1_CIRCLE, 40 + (s16)Rand_ZeroFloat(30.0f));
            }
            break;
        }

        case P1_CIRCLE: {
            // round the rim, aiming a little ahead of where it is
            f32 a, tx, tz;

            a = Math_FAtan2F(this->actor.world.pos.x - cx, this->actor.world.pos.z - cz);
            this->circleAng = (s16)(a * (0x8000 / M_PI));
            tx = cx + Math_SinS(this->circleAng + this->circleDir * 0x1400) * P1_RIM_R;
            tz = cz + Math_CosS(this->circleAng + this->circleDir * 0x1400) * P1_RIM_R;
            Math_StepToF(&this->speed, P1_GALLOP_SPEED, 2.0f);
            HtLynel_SteerTo(this, tx, tz, 0x800);
            HtLynel_Move(this, play);
            if ((this->throwCool > 0) && (--this->throwCool == 0) && !this->waves[0].on && !this->waves[1].on &&
                (Actor_WorldDistXZToActor(&this->actor, &player->actor) > 450.0f)) {
                // every now and then: the sword wave (after two Iai strikes, two of them, one from each sword)
                this->throwsLeft = (this->iaiHits >= 2) ? 2 : 1;
                this->throwX = false;
                if (this->form == HT_LYNEL_PHASE2) {
                    // phase 2: often the X instead (both swords: wider, harder to dodge), more so as it's hurt
                    this->throwsLeft = 1;
                }
                HtLynel_Interrupt(this, P1_THROW, 0);
                HtLynel_UpperAnim(this, this->throwX ? LYNEL_ANIM_SLASHX
                                                     : ((side > 0.0f) ? LYNEL_ANIM_SLASHL : LYNEL_ANIM_SLASHR));
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_SWORD);
                break;
            }
            if (this->calmTimer > 0) {
                this->calmTimer--;
            }
            if ((--this->actTimer <= 0) && (this->calmTimer == 0)) {
                if (this->form == HT_LYNEL_PHASE2) {
                    this->actTimer = 60; // (phase 2's big charge: next step)
                } else {
                    HtLynel_SetAction(this, P1_TURN, 20);
                }
            }
            break;
        }

        case P1_TURN:
            // pulling up, wheeling round on Link
            Math_StepToF(&this->speed, 0.0f, 3.0f);
            Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 2, 0xE00, 0x100);
            HtLynel_Move(this, play);
            if (this->speed < 3.0f) {
                if (this->skin.skelAnime.animation != (void*)gHorseGanonIdleAnim) {
                    HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonIdleAnim, 1.0f, ANIMMODE_LOOP, -6.0f);
                }
            }
            if (((--this->actTimer <= 0) || (this->speed <= 0.0f)) &&
                (ABS((s16)(this->actor.world.rot.y - this->actor.yawTowardsPlayer)) < 0x800)) {
                // the tell: it rears up, neighing
                this->speed = 0.0f;
                HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonRearingAnim, 1.0f, ANIMMODE_ONCE, -4.0f);
                HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
                HtLynel_SetAction(this, P1_REAR, 33);
            }
            break;

        case P1_REAR:
            Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 4, 0x200, 0x40);
            if ((this->actTimer == P1_REAR_FRAMES) &&
                (this->skin.skelAnime.animation != (void*)gHorseGanonRearingAnim)) {
                // (the opening: up it goes now)
                HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonRearingAnim, 1.0f, ANIMMODE_ONCE, -4.0f);
            }
            if (this->actTimer == 28) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
            }
            if (--this->actTimer <= 0) {
                // the charge: head down, horns first, straight at Link
                this->actor.world.rot.y = this->actor.yawTowardsPlayer;
                this->chargeLeft = Actor_WorldDistXZToActor(&this->actor, &player->actor) + 450.0f;
                this->hitDone = false;
                for (i = 0; i < HT_LYNEL_SPOUTS; i++) {
                    // what's ice already, it has seen
                    this->spoutKnown[i] = (this->spouts[i] != NULL) && HtWaterSpout_IsSolid(this->spouts[i]);
                }
                HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonGallopingAnim, 1.4f, ANIMMODE_LOOP, -4.0f);
                HtLynel_UpperAnim(this, LYNEL_ANIM_CHARGE);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
                HtLynel_SetAction(this, P1_CHARGE, 0);
            }
            break;

        case P1_CHARGE: {
            s32 hit;

            Math_StepToF(&this->speed, P1_CHARGE_SPEED, 5.0f);
            if (this->actTimer++ < 8) { // (a little homing while it gets going, then it's committed)
                Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 4, 0x180, 0x20);
            }
            hit = HtLynel_ChargeCheckPillars(this, play);
            if (hit == -2) {
                HtLynel_SetAction(this, P1_RECOVER, 0);
                this->actor.world.rot.y += (Rand_ZeroOne() < 0.5f) ? 0x2000 : -0x2000;
                HtLynel_UpperAnim(this, LYNEL_ANIM_IDLE);
                break;
            }
            if (hit >= 0) {
                // horns deep in the ice
                Actor* sp = this->spouts[hit];

                this->stuckSpout = hit;
                this->speed = 0.0f;
                HtLynel_StuckImpact(this, play, sp, true);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_DAMAGE);
                // (Navi's C-up now says what to do: HtLynel_Update)
                HtLynel_SetAction(this, P1_STUCK, P1_STUCK_FRAMES);
                HtLynel_StartPush(this);
                break;
            }
            HtLynel_Move(this, play);
            this->chargeLeft -= this->speed;
            if ((this->chargeLeft <= 0.0f) ||
                (SQ(this->actor.world.pos.x - cx) + SQ(this->actor.world.pos.z - cz) > SQ(P1_ARENA_R - 20.0f))) {
                HtLynel_UpperAnim(this, LYNEL_ANIM_IDLE);
                HtLynel_SetAction(this, P1_RECOVER, 0);
            }
            break;
        }

        case P1_RECOVER:
            // slowing out of it, then back to the rim
            Math_StepToF(&this->speed, P1_GALLOP_SPEED * 0.6f, 2.5f);
            HtLynel_Move(this, play);
            if (++this->actTimer > 12) {
                f32 a = Math_FAtan2F(this->actor.world.pos.x - cx, this->actor.world.pos.z - cz);

                this->circleAng = (s16)(a * (0x8000 / M_PI));
                HtLynel_StartGallop(this);
                HtLynel_SetAction(this, P1_TO_RIM, 0);
            }
            break;

        case P1_THROW:
            // the cut, galloping on; at its height the wave flies off at Link (the X: pulled up, squared to him)
            if (this->throwX) {
                Math_StepToF(&this->speed, 0.0f, 3.0f);
                if (!this->hitDone) {
                    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 0x800, 0x80);
                }
            } else {
                Math_StepToF(&this->speed, P1_GALLOP_SPEED * 0.8f, 2.0f);
            }
            HtLynel_Move(this, play);
            this->actTimer += 2;
            this->upperFrame++;
            if (!this->hitDone && (this->actTimer >= P1_SLASH_HIT0 + 2)) {
                HtLynelWave* w = &this->waves[this->waves[0].on ? 1 : 0];

                this->hitDone = true;
                w->on = true;
                w->life = this->throwX ? P2_X_LIFE : P1_WAVE_LIFE;
                w->slant = this->throwX ? 2 : ((this->upperAnim == LYNEL_ANIM_SLASHL) ? 1 : -1);
                w->yaw = Actor_WorldYawTowardActor(&this->actor, &player->actor);
                w->pos = this->actor.world.pos;
                w->pos.x += Math_SinS(w->yaw) * 120.0f;
                w->pos.z += Math_CosS(w->yaw) * 120.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_THUNDER);
                Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
            }
            // (a volley of two: straight into the second cut, with the other sword - it ends early, at the wave)
            if ((this->throwsLeft > 1) && this->hitDone && (this->actTimer >= P1_SLASH_HIT0 + 8)) {
                this->throwsLeft--;
                this->hitDone = false;
                this->actTimer = 4;
                HtLynel_UpperAnim(this, (this->upperAnim == LYNEL_ANIM_SLASHL) ? LYNEL_ANIM_SLASHR : LYNEL_ANIM_SLASHL);
                this->upperFrame = 4;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_SWORD);
            } else if (this->actTimer >= sLynelAnimFrames[this->upperAnim] - 1) {
                // the next one: every 5-9 s of its circling at full strength; once hurt, about twice as often
                this->throwCool = (this->iaiHits >= 1) ? (50 + (s16)Rand_ZeroFloat(40.0f))
                                                       : (100 + (s16)Rand_ZeroFloat(80.0f));
                if (this->form == HT_LYNEL_PHASE2) {
                    this->throwCool = 90 + (s16)Rand_ZeroFloat(60.0f);
                }
                HtLynel_UpperAnim(this, LYNEL_ANIM_IDLE);
                HtLynel_SetAction(this, P1_CIRCLE, MAX(this->prevTimer, 15));
                if (this->throwX) {
                    // the X thrown: once it's gone by Link, the point-blank one - the tell, then the rush
                    HtLynel_SetAction(this, P2_XWAIT, 120);
                }
            }
            break;

        case P1_SLASH:
            // the cut, galloping on through: blocked, he's run down under the hooves
            Math_StepToF(&this->speed, P1_GALLOP_SPEED * 0.55f, 2.0f);
            HtLynel_Move(this, play);
            this->actTimer += 2;   // (the cut at double speed: two of its frames a frame)
            this->upperFrame++;
            if (!this->hitDone && (this->actTimer >= P1_SLASH_HIT0) && (this->actTimer <= P1_SLASH_HIT1) &&
                (ahead > -20.0f) && (ahead < 270.0f) && (fabsf(side) < 170.0f) && !Player_InCsMode(play)) {
                this->hitDone = true;
                if (HtLynel_LinkShields(this, player)) {
                    // blocked - the sword rings off the shield, and it tramples him
                    Vec3f spark = player->actor.world.pos;

                    spark.y += 40.0f;
                    CollisionCheck_SpawnShieldParticlesMetal(play, &spark);
                    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_REFLECT_SW);
                    this->speed = P1_GALLOP_SPEED;
                    this->prevTimer = -1; // (the trample follows)
                } else {
                    Actor_SetPlayerKnockbackSmall(play, &this->actor, 8.0f, this->actor.world.rot.y, 4.0f, P1_DMG_SLASH);
                }
            }
            if ((this->prevTimer == -1) && (this->actTimer >= P1_SLASH_HIT1 + 3)) {
                this->prevTimer = 0;
                Actor_SetPlayerKnockbackLarge(play, &this->actor, 14.0f, this->actor.world.rot.y, 10.0f, P1_DMG_TRAMPLE);
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN);
            }
            if (this->actTimer >= sLynelAnimFrames[this->upperAnim] - 1) {
                this->slashCool = 40;
                HtLynel_UpperAnim(this, LYNEL_ANIM_IDLE);
                HtLynel_SetAction(this, (this->prevAction == P1_CIRCLE) ? P1_CIRCLE : P1_TO_RIM,
                                  (this->prevAction == P1_CIRCLE) ? 20 : 0);
            }
            break;

        case P2_POUND: {
            // slowing, then a jump in place: down with a shock that brings the vault's boulders down
            s32 f = this->actTimer - 8;

            if (f < 0) {
                Math_StepToF(&this->speed, 0.0f, 4.0f);
                HtLynel_Move(this, play);
            } else {
                if (f == 0) {
                    this->speed = 0.0f;
                    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
                    HtLynel_PoseBlendFrom(this);
                }
                this->poseFrame = f;
                if (f == 2) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_JUMP);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
                }
                if (f == LYNEL_JUMP_DOWN) {
                    HtLynel_PoundLands(this, play);
                }
                if (f >= LYNEL_JUMP_FRAMES - 1) {
                    this->poseFrame = -1;
                    HtLynel_StartGallop(this);
                    HtLynel_SetAction(this, P1_TO_RIM, 0);
                    break;
                }
            }
            this->actTimer++;
            break;
        }

        case P2_XCHARGE:
            // both swords raised and held, violet light gathering on them: the warning
            Math_StepToF(&this->speed, 0.0f, 2.0f);
            HtLynel_Move(this, play);
            Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 0x800, 0x80);
            if (this->upperFrame > 10) {
                this->upperFrame = 10; // (held up: incremented to 11 below)
            }
            HtSfx_PlayFor(this->actor.world.pos.x, this->actor.world.pos.y + 150.0f, this->actor.world.pos.z,
                          NA_SE_EN_GANON_CHARGE_MASIC, 2);
            if ((this->actTimer % 2) == 0) {
                static Color_RGBA8 prim = { 240, 190, 255, 255 };
                static Color_RGBA8 env = { 130, 0, 200, 0 };
                Vec3f zero = { 0.0f, 0.0f, 0.0f };
                Vec3f up = { 0.0f, 0.4f, 0.0f };

                for (i = 0; i < 2; i++) {
                    Vec3f p = this->bladeTipW[i];

                    p.x += Rand_CenteredFloat(30.0f);
                    p.y += Rand_CenteredFloat(30.0f);
                    p.z += Rand_CenteredFloat(30.0f);
                    EffectSsKiraKira_SpawnDispersed(play, &p, &zero, &up, &prim, &env, 4000, 12);
                }
            }
            if (--this->actTimer <= 0) {
                // loosed: aimed where Link is now (it won't follow him)
                this->actor.world.rot.y = this->actor.yawTowardsPlayer;
                this->throwX = true;
                this->throwsLeft = 1;
                this->hitDone = false;
                this->prevAction = P1_CIRCLE;
                this->prevTimer = 30;
                HtLynel_SetAction(this, P1_THROW, 10);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_SWORD);
            }
            break;

        case P2_XWAIT:
            // watching it go - till it's past Link (or spent), then the tell
            Math_StepToF(&this->speed, 0.0f, 3.0f);
            HtLynel_Move(this, play);
            Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 0x600, 0x80);
            {
                s32 gone = true;

                for (i = 0; i < ARRAY_COUNT(this->waves); i++) {
                    HtLynelWave* w = &this->waves[i];

                    if (w->on && (w->slant == 2) &&
                        ((player->actor.world.pos.x - w->pos.x) * Math_SinS(w->yaw) +
                             (player->actor.world.pos.z - w->pos.z) * Math_CosS(w->yaw) >
                         -60.0f)) {
                        gone = false;
                    }
                }
                if (gone || (--this->actTimer <= 0)) {
                    this->throwX = false;
                    HtLynel_StartTell(this);
                }
            }
            break;

        case P2_TELL:
            // reared, crossed, neighing long - squared up to Link (its torso open: an Iai now stops it)
            Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 0x600, 0x80);
            if (this->actTimer > 16) {
                // up, and held there, neighing
                if (this->skin.skelAnime.curFrame >= P2_TELL_APEX) {
                    this->skin.skelAnime.playSpeed = 0.0f;
                }
            } else {
                this->skin.skelAnime.playSpeed = 1.1f; // (down onto its forehooves as it breaks into the rush)
            }
            if ((this->actTimer == P2_TELL_FRAMES - 40) || (this->actTimer == P2_TELL_FRAMES - 75)) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
            }
            if (this->actTimer == 14) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
            }
            if (HtLynel_CheckIai(this, play, ahead, side)) {
                HtLynel_P2Struck(this, play);
                break;
            }
            if (--this->actTimer <= 0) {
                this->actor.world.rot.y = this->actor.yawTowardsPlayer;
                this->chargeLeft = Actor_WorldDistXZToActor(&this->actor, &player->actor) + 150.0f;
                HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonGallopingAnim, 1.4f, ANIMMODE_LOOP, -4.0f);
                HtLynel_SetAction(this, P2_RUSH, 0);
            }
            break;

        case P2_RUSH:
            // through the fire, straight at him - and after him wherever he goes
            Math_StepToF(&this->speed, P2_RUSH_SPEED, 5.0f);
            this->actTimer++;
            Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 2, 0x900, 0x80);
            if (HtLynel_CheckIai(this, play, ahead, side)) {
                HtLynel_P2Struck(this, play);
                break;
            }
            HtLynel_Move(this, play);
            this->chargeLeft -= this->speed;
            if ((SQ(ahead) + SQ(side) < SQ(P2_CUT_REACH)) || (this->actTimer > 90)) {
                // on him: the cross slash
                this->hitDone = false;
                HtLynel_UpperAnim(this, LYNEL_ANIM_CROSSCUT);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_SWORD);
                Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
                HtLynel_SetAction(this, P2_CUT, 0);
            }
            break;

        case P2_CUT:
            Math_StepToF(&this->speed, 0.0f, 6.0f);
            if (this->actTimer < 4) {
                Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 2, 0xC00, 0x100);
            }
            HtLynel_Move(this, play);
            if (!this->hitDone && (this->actTimer >= 3) && (this->actTimer <= 7) && (ahead > -30.0f) &&
                (ahead < 240.0f) && (fabsf(side) < 220.0f) && !Player_InCsMode(play)) {
                this->hitDone = true;
                Actor_SetPlayerKnockbackLarge(play, &this->actor, 18.0f, this->actor.world.rot.y, 10.0f,
                                              P2_DMG_CROSS);
                Audio_PlayActorSound2(&player->actor, NA_SE_EN_MGANON_SWDIMP);
            }
            if (++this->actTimer >= sLynelAnimFrames[LYNEL_ANIM_CROSSCUT] - 1) {
                if (this->fireRing != NULL) {
                    ((HtFireRing*)this->fireRing)->cutCount = 0;
                }
                this->calmTimer = 100;
                HtLynel_StartGallop(this);
                HtLynel_SetAction(this, P1_TO_RIM, 0);
            }
            break;

        case P2_HURT: {
            s32 e = P2_HURT_FRAMES - this->actTimer; // (frames since the blow)

            this->speed = 0.0f;
            if ((e >= 3) && (e < 21)) {
                // wheeling round on its hind legs: a half turn over the rear's height
                this->actor.world.rot.y += this->circleDir * (0x8000 / 18);
            }
            if (e == 21) {
                this->actor.world.rot.y = this->circleAng;
            }
            this->upperFrame = MIN(e, 4); // (thrown back, arms flung out: held while it's up)
            if ((e < 40) && ((e % 8) == 0)) {
                Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 4); // (flickering red)
            }
            if (e == 4) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
            }
            if (e == 22) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_LAND); // (down again)
            }
            if (e == 24) {
                HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonIdleAnim, 1.0f, ANIMMODE_LOOP, -4.0f);
                HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
            }
            if (--this->actTimer <= 0) {
                f32 a = Math_FAtan2F(this->actor.world.pos.x - cx, this->actor.world.pos.z - cz);

                this->circleAng = (s16)(a * (0x8000 / M_PI));
                HtLynel_StartGallop(this);
                HtLynel_SetAction(this, P1_TO_RIM, 0);
            }
            break;
        }

        case P1_KICK:
            // (the joint table is written below: the authored kick)
            this->speed = 0.0f;
            if (!this->hitDone && (this->actTimer >= LYNEL_KICK_HIT0) && (this->actTimer <= LYNEL_KICK_HIT1) &&
                (ahead < -50.0f) && (ahead > -290.0f) && (fabsf(side) < 70.0f) && !Player_InCsMode(play)) {
                this->hitDone = true;
                Actor_SetPlayerKnockbackLarge(play, &this->actor, 16.0f, this->actor.world.rot.y + 0x8000, 9.0f,
                                              P1_DMG_KICK);
                Audio_PlayActorSound2(&player->actor, NA_SE_IT_HAMMER_HIT);
            }
            if (this->actTimer == LYNEL_KICK_HIT0 - 1) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
            }
            if (++this->actTimer >= LYNEL_KICK_FRAMES) {
                // back to what it was doing
                if (this->prevAction == P1_STUCK) {
                    // back to heaving at the ice
                    HtLynel_SetAction(this, P1_STUCK, MAX(this->prevTimer, 20));
                    HtLynel_StartPush(this);
                } else {
                    HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonIdleAnim, 1.0f, ANIMMODE_LOOP, 0.0f);
                    HtLynel_UpperAnim(this, LYNEL_ANIM_IDLE);
                    HtLynel_StartGallop(this);
                    HtLynel_SetAction(this, P1_RECOVER, 0);
                }
            }
            break;

        case P1_STUCK: {
            // straining against the ice (the ground shudders with each heave), till it tears free - or an Iai at its
            // flank cuts it loose
            s32 cut = HtLynel_CheckIai(this, play, ahead, side);

            if ((this->poseFrame % LYNEL_PUSH_FRAMES) == LYNEL_PUSH_HEAVE) {
                // each heave back against the ice
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
                HtLynel_StuckImpact(this, play, this->spouts[this->stuckSpout], false);
            }
            if (cut) {
                // hurt: it screams, wrenches free - and keeps its distance a while (sword waves) before charging again
                this->iaiHits++;
                this->calmTimer = 110 + (s16)Rand_ZeroFloat(50.0f);
                this->throwCool = 25;
                Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 15);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_DAMAGE);
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
                HtLynel_StuckImpact(this, play, this->spouts[this->stuckSpout], true);
                this->actTimer = 0;
            }
            if (cut && (this->iaiHits >= HT_LYNEL_P1_IAIS)) {
                // the third: phase 1 is over - enraged, it tears free (the fire cage: HtLynel_UpdateTransition)
                this->form = HT_LYNEL_TRANSITION;
                this->csTimer = 0;
                break;
            }
            if (--this->actTimer <= 0) {
                Actor* sp = this->spouts[this->stuckSpout];

                if (sp != NULL) {
                    HtWaterSpout_Thaw(sp, play);
                }
                this->actor.world.rot.y += 0x8000; // it wheels round, away
                this->speed = 4.0f;
                HtLynel_UpperAnim(this, LYNEL_ANIM_IDLE);
                HtLynel_StartGallop(this);
                HtLynel_SetAction(this, P1_RECOVER, 0);
            }
            break;
        }
    }

    // the charge's horns: Link in front of them is thrown back (and down)
    if ((this->action == P1_CHARGE) && !this->hitDone && !Player_InCsMode(play) &&
        (((ahead > 20.0f) && (ahead < 175.0f) && (fabsf(side) < 95.0f)) ||          // the horns
         ((ahead > 110.0f) && (ahead < 215.0f) && (fabsf(side) < 140.0f)))) {       // the swords, spread ahead
        this->hitDone = true;
        Actor_SetPlayerKnockbackLarge(play, &this->actor, 20.0f, this->actor.world.rot.y, 10.0f, P1_DMG_CHARGE);
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_HAMMER_HIT);
    }

    if (this->action == P1_KICK) {
        HtLynel_WritePose(this, gHtLynelKick[CLAMP(this->actTimer, 0, LYNEL_KICK_FRAMES - 1)]);
    } else if (0) {
    } else if ((this->action == P2_POUND) && (this->poseFrame >= 0)) {
        HtLynel_WritePose(this, gHtLynelJump[CLAMP(this->poseFrame, 0, LYNEL_JUMP_FRAMES - 1)]);
    } else if ((this->action == P1_LEAP) && (this->poseFrame >= 0)) {
        HtLynel_WritePose(this, gHtLynelJump[CLAMP(this->poseFrame, 0, LYNEL_JUMP_FRAMES - 1)]);
    } else if (this->action == P1_STUCK) {
        // the hooves planted, the arms in step with the heave
        HtLynel_WritePose(this, gHtLynelPush[this->poseFrame % LYNEL_PUSH_FRAMES]);
        this->upperFrame = this->poseFrame % LYNEL_PUSH_FRAMES;
        this->poseFrame++;
    } else {
        SkelAnime_Update(&this->skin.skelAnime);
    }
    if ((this->action != P1_STUCK) && (++this->upperFrame >= sLynelAnimFrames[this->upperAnim])) {
        this->upperFrame = (this->action == P1_SLASH) ? sLynelAnimFrames[this->upperAnim] - 1 : 0;
        if (this->upperAnim == LYNEL_ANIM_GUARD) {
            this->upperFrame = sLynelAnimFrames[this->upperAnim] - 1; // (held)
        }
    }
}

// the magic pots by the wall: one broken is put back a while after (the fight runs on the katana's magic)
static void HtLynel_UpdatePots(HtLynel* this, PlayState* play) {
    s32 i;

    if ((play->gameplayFrames % 10) != 0) {
        return;
    }
    for (i = 0; i < 8; i++) {
        f32 a = M_PI / 8.0f + i * (M_PI / 4.0f); // (eight round the wall: build_heretics_dungeon.py)
        f32 x = this->actor.home.pos.x + sinf(a) * HT_LYNEL_POT_R;
        f32 z = this->actor.home.pos.z + cosf(a) * HT_LYNEL_POT_R;
        Actor* p = play->actorCtx.actorLists[ACTORCAT_PROP].head;
        s32 there = false;

        for (; p != NULL; p = p->next) {
            if ((p->id == ACTOR_OBJ_TSUBO) && (SQ(p->home.pos.x - x) + SQ(p->home.pos.z - z) < SQ(40.0f))) {
                there = true;
                break;
            }
        }
        if (there) {
            this->potTimer[i] = 0;
        } else if ((this->potTimer[i] += 10) >= HT_LYNEL_POT_RESPAWN) {
            this->potTimer[i] = 0;
            Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_TSUBO, x, this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP, z, 0,
                        0, 0, 0x0E /* ITEM00_MAGIC_LARGE */);
        }
    }
}

// ---- phase 1 -> 2: the fire cage ------------------------------------------------------------------------------------
// Struck a third time, it tears free and leaps away; Link (out of sight) is set down on the sunk pedestal. It lands,
// rears and stamps: a wall of fire bursts up round the pedestal, the water boils off, the room reddens. Then Link,
// close: he looks left, right, behind him - and starts; the camera draws back and up, and the ring of fire round him
// shows. The fight goes on: Link caged, the Lynel round the rim outside.
#define TR_TURN 10                                   // wheeling to its leap...
#define TR_JUMP_END (TR_TURN + LYNEL_JUMP_FRAMES)    // ...the leap...
#define TR_LAND (TR_TURN + LYNEL_JUMP_DOWN)
#define TR_PIVOT 16                                  // ...galloping round on the spot to face the cage...
#define TR_REAR (TR_JUMP_END + TR_PIVOT)             // ...up on its hind legs...
#define TR_SLAM (TR_REAR + 28)                       // ...and down: its forehooves strike (the rearing's frame 28)
#define TR_SHOT_LINK (TR_SLAM + 32)                  // Link, close, looking about him (right, left, behind) as the
#define TR_LOOK2 (TR_SHOT_LINK + 40)                 // camera draws back and up, one shot, till the ring round him
#define TR_ZOOM TR_SHOT_LINK                         // shows
#define TR_ZOOM_END (TR_ZOOM + 95)
#define TR_END (TR_ZOOM_END + 12)
#define TR_SLAM_R 760.0f                             // where it stamps, from the middle (outside the cage)
#define TR_MELT 60                                   // the water boiling away
#define TR_HEAT 80                                   // the room reddening

// steam (the frost meeting fire, the water boiling): a pale dust puff, rising
static void HtLynel_Steam(PlayState* play, Vec3f* pos, Vec3f* vel, Vec3f* acc, s16 scale) {
    static Color_RGBA8 prim = { 240, 240, 240, 150 };
    static Color_RGBA8 env = { 190, 190, 200, 0 };

    EffectSsDust_Spawn(play, 0, pos, vel, acc, &prim, &env, scale / 3, 12, 26, 0);
}

// the arena's water: its surface (drawn: HtLynel_Draw) and the scene's water box under it, down by `drop`
static s16 sWaterBoxY = -32000;
static void HtLynel_SetWaterDrop(HtLynel* this, PlayState* play, f32 drop) {
    CollisionHeader* ch = play->colCtx.colHeader;
    s32 i;

    this->waterDrop = drop;
    for (i = 0; i < ch->numWaterBoxes; i++) {
        WaterBox* wb = &ch->waterBoxes[i];

        if (((wb->properties >> 13) & 0x3F) != HT_BOSS_ROOM) {
            continue;
        }
        if (sWaterBoxY == -32000) {
            sWaterBoxY = wb->ySurface;
        }
        wb->ySurface = (drop >= 20.0f) ? -32000 : (s16)(sWaterBoxY - drop);
    }
}

// gone (left, or the room reloaded): the water box back as it was (the scene's collision outlives us), the light too
static void HtLynel_RestoreRoom(PlayState* play) {
    CollisionHeader* ch = play->colCtx.colHeader;
    s32 i;

    if (sWaterBoxY != -32000) {
        for (i = 0; i < ch->numWaterBoxes; i++) {
            if (((ch->waterBoxes[i].properties >> 13) & 0x3F) == HT_BOSS_ROOM) {
                ch->waterBoxes[i].ySurface = sWaterBoxY;
            }
        }
        sWaterBoxY = -32000;
    }
    for (i = 0; i < 3; i++) {
        play->envCtx.adjAmbientColor[i] = play->envCtx.adjLight1Color[i] = play->envCtx.adjFogColor[i] = 0;
    }
    play->envCtx.fillScreen = false;
}

// the room's red: its fog and light warmed, and a red veil over the screen - deeper as its rage grows
static void HtLynel_UpdateHeat(HtLynel* this, PlayState* play) {
    f32 h = this->heat;

    if (h <= 0.0f) {
        return;
    }
    play->envCtx.adjAmbientColor[0] = (s16)(70 * h);
    play->envCtx.adjAmbientColor[1] = (s16)(-15 * h);
    play->envCtx.adjAmbientColor[2] = (s16)(-35 * h);
    play->envCtx.adjLight1Color[0] = (s16)(60 * h);
    play->envCtx.adjLight1Color[1] = (s16)(-10 * h);
    play->envCtx.adjLight1Color[2] = (s16)(-40 * h);
    play->envCtx.adjFogColor[0] = (s16)(110 * h);
    play->envCtx.adjFogColor[1] = (s16)(-5 * h);
    play->envCtx.adjFogColor[2] = (s16)(-40 * h);
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 255;
    play->envCtx.screenFillColor[1] = 35;
    play->envCtx.screenFillColor[2] = 0;
    play->envCtx.screenFillColor[3] = (u8)(42 * h);
}

static void HtLynel_UpdateTransition(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 t = this->csTimer;
    f32 cx = this->actor.home.pos.x;
    f32 cz = this->actor.home.pos.z;
    f32 floorY = this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP;
    s32 i;

    if (t == 0) {
        f32 a = Math_FAtan2F(this->actor.world.pos.x - cx, this->actor.world.pos.z - cz);
        s16 ang = (s16)(a * (0x8000 / M_PI)) + 0x2800; // (a good leap: a way round the circle from where it is)

        // the camera; everyone holds still - Link with his katana back in its sheath (at once: the cutscene stance
        // doesn't play the putting away)
        func_80064520(play, &play->csCtx);
        if (player->heldItemAction != PLAYER_IA_NONE) {
            player->heldItemId = ITEM_NONE;
            player->nextModelGroup = Player_ActionToModelGroup(player, PLAYER_IA_NONE);
            Player_InitItemAction(play, player, PLAYER_IA_NONE);
        }
        Player_SetCsActionWithHaltedActors(play, &this->actor, 1);
        this->subCamId = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, this->subCamId, CAM_STAT_ACTIVE);
        Play_CameraSetFov(play, this->subCamId, 60.0f);
        for (i = 0; i < ARRAY_COUNT(this->waves); i++) {
            this->waves[i].on = false;
        }
        // Link, out of sight: set down in the middle of the pedestal, facing where it will stamp
        this->slamPos.x = cx + Math_SinS(ang) * TR_SLAM_R;
        this->slamPos.y = floorY;
        this->slamPos.z = cz + Math_CosS(ang) * TR_SLAM_R;
        player->actor.world.pos.x = cx;
        player->actor.world.pos.z = cz;
        player->actor.world.pos.y = floorY;
        player->actor.prevPos = player->actor.world.pos;
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = (s16)ang;
        this->leapFrom = this->actor.world.pos;
        this->speed = 0.0f;
        this->poseFrame = -1;
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
        HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonIdleAnim, 1.0f, ANIMMODE_LOOP, -4.0f);
        HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
    }
    if (t < TR_SHOT_LINK) {
        // (Link held in the middle, still - whatever he was doing when it began)
        player->actor.world.pos.x = cx;
        player->actor.world.pos.z = cz;
        player->actor.world.pos.y = floorY;
        player->actor.velocity.x = player->actor.velocity.y = player->actor.velocity.z = 0.0f;
        player->actor.speedXZ = 0.0f;
        player->actor.prevPos = player->actor.world.pos;
        if (t == 20) {
            Player_SetCsActionWithHaltedActors(play, &this->actor, 1); // (again: in case he was mid-knockback)
        }
    }
    if (t < TR_TURN) {
        // wheeling round to its leap
        s16 yaw = Math_FAtan2F(this->slamPos.x - this->actor.world.pos.x, this->slamPos.z - this->actor.world.pos.z) *
                  (0x8000 / M_PI);

        Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 2, 0x1800, 0x100);
        if (t == TR_TURN - 1) {
            this->actor.world.rot.y = yaw;
            this->leapFrom = this->actor.world.pos;
            HtLynel_PoseBlendFrom(this);
            HtLynel_UpperAnim(this, LYNEL_ANIM_CHARGE);
        }
        SkelAnime_Update(&this->skin.skelAnime);
    } else if (t < TR_JUMP_END) {
        // the leap
        s32 f = t - TR_TURN;
        f32 k = CLAMP((f32)(f - LYNEL_JUMP_UP) / (LYNEL_JUMP_DOWN - LYNEL_JUMP_UP), 0.0f, 1.0f);

        if (f == 2) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_JUMP);
        }
        this->actor.world.pos.x = this->leapFrom.x + (this->slamPos.x - this->leapFrom.x) * k;
        this->actor.world.pos.z = this->leapFrom.z + (this->slamPos.z - this->leapFrom.z) * k;
        this->actor.world.pos.y = floorY;
        if (t == TR_LAND) {
            Vec3f pos = this->actor.world.pos;

            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_LAND);
            pos.y += 15.0f;
            EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 600);
        }
        HtLynel_WritePose(this, gHtLynelJump[f]);
    } else {
        if (t == TR_JUMP_END) {
            // out of the leap still galloping, it swings round in a tight pivot to face the cage
            HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonGallopingAnim, 1.3f, ANIMMODE_LOOP, -2.0f);
            HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
            this->speed = 9.0f;
        }
        if ((t >= TR_JUMP_END) && (t < TR_REAR)) {
            s16 toCage = Math_FAtan2F(cx - this->actor.world.pos.x, cz - this->actor.world.pos.z) * (0x8000 / M_PI);

            Math_SmoothStepToS(&this->actor.world.rot.y, toCage, 2, 0x1C00, 0x200);
            Math_StepToF(&this->speed, 0.0f, 0.6f);
            this->actor.world.pos.x += Math_SinS(this->actor.world.rot.y) * this->speed;
            this->actor.world.pos.z += Math_CosS(this->actor.world.rot.y) * this->speed;
            if (t == TR_REAR - 1) {
                this->actor.world.rot.y = toCage;
            }
        }
        if (t == TR_REAR) {
            Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonRearingAnim, 1.0f, 0.0f,
                             Animation_GetLastFrame((AnimationHeader*)gHorseGanonRearingAnim), ANIMMODE_ONCE, -3.0f);
        }
        if (t == TR_REAR + 5) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
        }
        if (t == TR_REAR + 14) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
        }
        if (t == TR_SLAM) {
            // the stamp: the fire bursts up round Link, the water boils off, the spouts with it
            Vec3f pos = this->actor.world.pos;
            Vec3f zero = { 0.0f, 0.0f, 0.0f };

            this->fireRing = Actor_Spawn(&play->actorCtx, play, HT_ACTOR_FIRE_RING, cx, floorY, cz, 0, 0, 0, 0);
            HtLynel_Quake(this, play, 9, 30);
            Rumble_Request(0.0f, 255, 30, 150);
            HtLynel_Rumble(this, 30);
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_HAMMER_HIT);
            Audio_PlayActorSound2(&player->actor, NA_SE_EV_FIRE_PILLAR);
            pos.x += Math_SinS(this->actor.world.rot.y) * 90.0f;
            pos.z += Math_CosS(this->actor.world.rot.y) * 90.0f;
            EffectSsBlast_SpawnWhiteShockwave(play, &pos, &zero, &zero);
            for (i = 0; i < HT_LYNEL_SPOUTS; i++) {
                if (this->spouts[i] != NULL) {
                    Vec3f sp = this->spouts[i]->world.pos;
                    Vec3f vel = { 0.0f, 3.0f, 0.0f };
                    Vec3f acc = { 0.0f, 0.2f, 0.0f };

                    sp.y += 20.0f;
                    HtLynel_Steam(play, &sp, &vel, &acc, 600);
                    Actor_Kill(this->spouts[i]);
                    this->spouts[i] = NULL;
                }
            }
        }
        SkelAnime_Update(&this->skin.skelAnime);
        if (t == TR_SLAM + 20) {
            HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonIdleAnim, 1.0f, ANIMMODE_LOOP, -6.0f);
            HtLynel_UpperAnim(this, LYNEL_ANIM_GLARE);
        }
    }
    if (t >= TR_SLAM) {
        // the water boiling off (steam off its surface), the room reddening
        s32 m = t - TR_SLAM;

        if (m <= TR_MELT) {
            HtLynel_SetWaterDrop(this, play, 22.0f * m / TR_MELT);
            for (i = 0; i < 3; i++) {
                f32 ra = Rand_ZeroFloat(2.0f * M_PI);
                f32 rr = 200.0f + Rand_ZeroFloat(1200.0f);
                Vec3f p = { cx + sinf(ra) * rr, floorY + 15.0f, cz + cosf(ra) * rr };
                Vec3f vel = { 0.0f, 1.5f + Rand_ZeroFloat(1.5f), 0.0f };
                Vec3f acc = { 0.0f, 0.15f, 0.0f };

                HtLynel_Steam(play, &p, &vel, &acc, 350 + (s16)Rand_ZeroFloat(250.0f));
            }
            if ((m % 10) == 0) {
                HtSfx_PlayFor(cx, floorY, cz, NA_SE_EV_ICE_MELT, 10);
            }
        }
        this->heat = CLAMP((f32)m / TR_HEAT, 0.0f, 1.0f);
    }
    // Link: looking about him, then the start
    if (t == TR_SHOT_LINK) {
        Player_SetCsActionWithHaltedActors(play, &this->actor, 81);
    }
    if (t == TR_LOOK2) {
        Player_SetCsActionWithHaltedActors(play, &this->actor, 82);
    }
    if (++this->upperFrame >= sLynelAnimFrames[this->upperAnim]) {
        this->upperFrame = 0;
    }

    // the camera
    {
        Vec3f* at = &this->camAt;
        Vec3f* eye = &this->camEye;
        f32 fx = Math_SinS(this->actor.world.rot.y);
        f32 fz = Math_CosS(this->actor.world.rot.y);

        if (t < TR_JUMP_END) {
            // alongside it, through the leap (its right; Link out of the frame)
            Vec3f want;

            at->x = this->actor.world.pos.x + fx * 60.0f;
            at->y = this->actor.world.pos.y + 120.0f;
            at->z = this->actor.world.pos.z + fz * 60.0f;
            want.x = at->x + fz * 420.0f - fx * 160.0f;
            want.y = at->y + 60.0f;
            want.z = at->z - fx * 420.0f - fz * 160.0f;
            if (t == 0) {
                *eye = want;
            } else {
                Math_ApproachF(&eye->x, want.x, 0.3f, 60.0f);
                Math_ApproachF(&eye->y, want.y, 0.3f, 60.0f);
                Math_ApproachF(&eye->z, want.z, 0.3f, 60.0f);
            }
        } else if (t < TR_SHOT_LINK) {
            // behind it, high: its stamp, and the cage bursting up round Link out in the middle
            f32 dx = cx - this->actor.world.pos.x;
            f32 dz = cz - this->actor.world.pos.z;
            f32 d = sqrtf(SQ(dx) + SQ(dz));

            dx /= d;
            dz /= d;
            eye->x = this->actor.world.pos.x - dx * 280.0f + dz * 90.0f;
            eye->y = this->actor.world.pos.y + 250.0f;
            eye->z = this->actor.world.pos.z - dz * 280.0f - dx * 90.0f;
            at->x = cx - dx * 120.0f;
            at->y = floorY + 40.0f;
            at->z = cz - dz * 120.0f;
        } else {
            // Link, close in front; then drawing back and up till the ring round him shows
            f32 lx = Math_SinS(player->actor.shape.rot.y);
            f32 lz = Math_CosS(player->actor.shape.rot.y);
            f32 k = CLAMP((f32)(t - TR_ZOOM) / (TR_ZOOM_END - TR_ZOOM), 0.0f, 1.0f);

            k = 0.5f - 0.5f * Math_CosF(k * M_PI);
            at->x = player->actor.world.pos.x;
            at->y = player->actor.world.pos.y + 45.0f - 25.0f * k;
            at->z = player->actor.world.pos.z;
            eye->x = at->x + lx * (95.0f + 620.0f * k);
            eye->y = player->actor.world.pos.y + 50.0f + 520.0f * k;
            eye->z = at->z + lz * (95.0f + 620.0f * k);
        }
        Play_CameraSetFov(play, this->subCamId, 60.0f);
        Play_CameraSetAtEye(play, this->subCamId, at, eye);
    }

    if (++this->csTimer >= TR_END) {
        // the fight again: Link caged, the Lynel out round the rim
        Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);
        f32 a = Math_FAtan2F(this->actor.world.pos.x - cx, this->actor.world.pos.z - cz);

        cam->eye = this->camEye;
        cam->eyeNext = this->camEye;
        cam->at = this->camAt;
        func_800C08AC(play, this->subCamId, 0);
        this->subCamId = 0;
        func_80064534(play, &play->csCtx);
        Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
        HtLynel_SetWaterDrop(this, play, 30.0f);
        this->form = HT_LYNEL_PHASE2;
        this->p2Hits = 0;
        this->p2Done = false;
        this->poundTimer = 160;
        this->circleAng = (s16)(a * (0x8000 / M_PI));
        this->throwCool = 50;
        this->calmTimer = 0;
        HtLynel_StartGallop(this);
        HtLynel_SetAction(this, P1_TO_RIM, 0);
        if (this->fireRing != NULL) {
            ((HtFireRing*)this->fireRing)->emitInterval = 140; // (a ring every 7 s, closing on Link in ~9 s;
            ((HtFireRing*)this->fireRing)->maxRings = 2;          // two at most at once)
            ((HtFireRing*)this->fireRing)->speed = 2.6f;
            ((HtFireRing*)this->fireRing)->emitTimer = 40;
        }
    }
}

// ---- phase 2 -> 3: the dig ----------------------------------------------------------------------------------------
// Struck a third time in the cage, it roars its fury up on its hind legs, the ground shaking under the roar; then it
// leaps back onto the Triforce in the middle and turns on Link (set down before it, out of sight). It glares, goes for
// him - and the ground heaves again: the middle digs down, the Triforce's disc two levels (the Lynel with it, sunk to
// its chest), the ring round it one (Link with it): face to face with its upper body, blade to blade.
#define T3_ROAR_END 50
#define T3_LEAP (T3_ROAR_END + 8)                   // (turned to the middle: the leap)
#define T3_LAND (T3_LEAP + LYNEL_JUMP_DOWN)
#define T3_LEAP_END (T3_LEAP + LYNEL_JUMP_FRAMES)
#define T3_QUAKE T3_LAND                           // it lands before him - the impact - and the ground heaves...
#define T3_DIG (T3_QUAKE + 24)                      // ...and gives way
#define T3_DIG_FRAMES 60
#define T3_FINAL (T3_DIG + T3_DIG_FRAMES + 4)
#define T3_END (T3_FINAL + 26)
#define T3_LINK_R 168.0f                            // where Link stands, from the middle (the ring's middle)

#define P3_HITS 8                                   // Master Sword strikes to finish it
#define P3_DMG_STRIKE 0x10
#define P3_DMG_FIRE 0x10
#define P3_DMG_DUAL 0x18                            // both blades (unblockable), the whirl: a heart and a half
#define P3_SPIN_HIT0 11                             // the whirl's sweeping frames (lynel_export.py SPIN_HIT)
#define P3_SPIN_HIT1 19
#define P3_REACH 215.0f                             // its blades' reach from the middle (the ring's edge)
#define P3_STRIKE_HIT0 15                           // a strike's cutting frames (lynel_export.py SLASH3: the blade
#define P3_STRIKE_BITE 17                           // down through Link, at the floor by the last)
#define P3_STRIKE_SIDE 100.0f                       // (how far to either side of its line the cut sweeps)
#define P3_X_APEX 11                                // both blades: held up at the top of the wind-up a while...
#define P3_X_TELL 20                                // ...the warning (Link's time to get out of its line)
#define P3_SPIN_APEX 8                              // the whirl: wound back and held...
#define P3_SPIN_TELL 18                             // ...the warning (Link's time to get ready to roll)
#define P3_X_TO_OUT 20                              // the X: its inward cut done, the blades crossed - flung out
#define P3_XOUT_HIT0 1                              // (the outward cut's sweeping frames: lynel_export.py XOUT3)
#define P3_XOUT_HIT1 4
#define P3_XOUT_SIDE 165.0f                         // (it sweeps his whole front)
#define P3_FIRE_APEX 13                             // fire: head thrown back, breath held...
#define P3_FIRE_TELL 16                             // ...this long, embers at its jaws (the warning)
#define P3_SWIRL_R 245.0f                           // the whirl's swirl (Link's spin attack's): its radius
#define P3_OPEN_BLOCK 45                            // (the parry animation's length)
#define P3_PARRY_FOLLOW 9                           // one cut parried: its arm flung back this long, then the other's
#define P3_REEL_FRAMES 110                          // both parried: reeling, open, this long at most...
#define P3_REEL_HITS 2                              // ...or till Link's landed this many strikes
#define P3_STRIKE_AIM 45.0f                         // (it turns so Link stands where its cutting arm's blade comes down)
#define P3_OPEN_DODGE 45                            // its blade bitten into the floor where Link was: the opening

static void HtLynel_P3Quake(HtLynel* this, PlayState* play, s16 y, s16 frames) {
    s16 q = Quake_Add(GET_ACTIVE_CAM(play), 3);

    Quake_SetSpeed(q, 25000);
    Quake_SetQuakeValues(q, y, 0, 0, 0);
    Quake_SetCountdown(q, frames);
    Rumble_Request(0.0f, 200, frames, 100);
}

static f32 HtLynel_RingY(HtLynel* this) {
    return (this->digRing != NULL) ? this->digRing->world.pos.y : (this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP);
}

static void HtLynel_UpdateTransition3(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 t = this->csTimer;
    f32 cx = this->actor.home.pos.x;
    f32 cz = this->actor.home.pos.z;
    f32 floorY = this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP;
    s32 i;

    if (t == 0) {
        Actor* a;

        func_80064520(play, &play->csCtx);
        Player_SetCsActionWithHaltedActors(play, &this->actor, 1);
        this->subCamId = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, this->subCamId, CAM_STAT_ACTIVE);
        Play_CameraSetFov(play, this->subCamId, 60.0f);
        for (i = 0; i < ARRAY_COUNT(this->waves); i++) {
            this->waves[i].on = false;
        }
        for (i = 0; i < HT_LYNEL_ROCKS; i++) {
            this->rocks[i].on = false;
        }
        for (i = 0; i < HT_LYNEL_CHIPS; i++) {
            this->chips[i].on = false;
        }
        this->rainTimer = 0;
        if (this->fireRing != NULL) {
            ((HtFireRing*)this->fireRing)->dying = true;
        }
        // the fire slugs gone with the fire (they'd be left walking on the air over the new shaft)
        for (a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
            if (a->id == ACTOR_EN_BW) {
                Vec3f p = a->world.pos;
                Vec3f vel = { 0.0f, 2.0f, 0.0f };
                Vec3f acc = { 0.0f, 0.2f, 0.0f };

                p.y += 15.0f;
                HtLynel_Steam(play, &p, &vel, &acc, 300);
                Actor_Kill(a);
            }
        }
        for (i = 0; i < 2; i++) {
            this->slugs[i] = NULL;
        }
        this->heatFade = true; // (no fire phase any more: the red ebbs away)
        // the katana put away for the rest of the fight: its curse laid bare, the Lynel swallows the katana's frost -
        // only the Master Sword, evil's bane, cuts it now (the katana itself is fine: back in hand once it's dead)
        this->katanaPut = Katana_SetLocked(true);
        // (and whatever's in Link's hand put away: the katana's two-handed grip would linger, unseen, till then)
        player->heldItemId = ITEM_NONE;
        player->nextModelGroup = Player_ActionToModelGroup(player, PLAYER_IA_NONE);
        Player_InitItemAction(play, player, PLAYER_IA_NONE);
        // the dig's pieces in place of the pedestal's (both flush with the floor now: no change seen)
        for (a = play->actorCtx.actorLists[ACTORCAT_BG].head; a != NULL; a = a->next) {
            if ((a->id == HT_ACTOR_PEDESTAL) && (a->params < HT_PEDESTAL_DIG_RING)) {
                Actor_Kill(a);
            }
        }
        this->digRing = Actor_Spawn(&play->actorCtx, play, HT_ACTOR_PEDESTAL, cx, floorY, cz, 0, 0, 0,
                                    HT_PEDESTAL_DIG_RING);
        this->digPit = Actor_Spawn(&play->actorCtx, play, HT_ACTOR_PEDESTAL, cx, floorY, cz, 0, 0, 0,
                                   HT_PEDESTAL_DIG_PIT);
        Actor_Spawn(&play->actorCtx, play, HT_ACTOR_PEDESTAL, cx, floorY, cz, 0, 0, 0, HT_PEDESTAL_DIG_BAND);
        // the roar, up on its hind legs, the ground shaking with it
        this->speed = 0.0f;
        this->poseFrame = -1;
        Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonRearingAnim, 0.8f, 0.0f,
                         Animation_GetLastFrame((AnimationHeader*)gHorseGanonRearingAnim), ANIMMODE_ONCE, -4.0f);
        HtLynel_UpperAnim(this, LYNEL_ANIM_HURT);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
        HtLynel_P3Quake(this, play, 5, T3_ROAR_END);
    }
    if (t < T3_ROAR_END) {
        if (this->skin.skelAnime.curFrame >= 11.0f) {
            this->skin.skelAnime.playSpeed = (t < T3_ROAR_END - 14) ? 0.0f : 0.9f; // (held up through the roar)
        }
        if (t == 18) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
        }
        this->upperFrame = MIN(t, 4); // (head thrown back, arms flung wide)
        SkelAnime_Update(&this->skin.skelAnime);
    } else if (t < T3_LEAP) {
        // round to the middle
        s16 yaw = Math_FAtan2F(cx - this->actor.world.pos.x, cz - this->actor.world.pos.z) * (0x8000 / M_PI);

        Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 2, 0x2000, 0x200);
        if (t == T3_LEAP - 1) {
            this->actor.world.rot.y = yaw;
            this->leapFrom = this->actor.world.pos;
            HtLynel_PoseBlendFrom(this);
            HtLynel_UpperAnim(this, LYNEL_ANIM_CHARGE);
        }
        if (t == T3_ROAR_END) {
            HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonIdleAnim, 1.0f, ANIMMODE_LOOP, -4.0f);
        }
        SkelAnime_Update(&this->skin.skelAnime);
    } else if (t < T3_LEAP_END) {
        // the leap back onto the Triforce (Link, out of sight, set down before it)
        s32 f = t - T3_LEAP;
        f32 k = CLAMP((f32)(f - LYNEL_JUMP_UP) / (LYNEL_JUMP_DOWN - LYNEL_JUMP_UP), 0.0f, 1.0f);

        if (t == T3_LEAP) {
            f32 dx = player->actor.world.pos.x - cx;
            f32 dz = player->actor.world.pos.z - cz;
            f32 d = sqrtf(SQ(dx) + SQ(dz));

            if (d < 10.0f) {
                dx = -Math_SinS(this->actor.world.rot.y);
                dz = -Math_CosS(this->actor.world.rot.y);
                d = 1.0f;
            }
            this->linkSpot.x = cx + dx / d * T3_LINK_R;
            this->linkSpot.z = cz + dz / d * T3_LINK_R;
            this->linkSpot.y = floorY;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_JUMP);
        }
        this->actor.world.pos.x = this->leapFrom.x + (cx - this->leapFrom.x) * k;
        this->actor.world.pos.z = this->leapFrom.z + (cz - this->leapFrom.z) * k;
        this->actor.world.pos.y = floorY;
        if (t == T3_LAND) {
            // the impact: the Megaton Hammer's shock wave (a horse's worth: a big one round the small one), the floor
            // jolting - and then heaving on, under them both (not its doing: it reels, looking about it)
            Vec3f p = this->actor.world.pos;
            Vec3f zero = { 0.0f, 0.0f, 0.0f };

            p.y += 2.0f;
            EffectSsBlast_SpawnWhiteShockwave(play, &p, &zero, &zero);
            EffectSsBlast_SpawnWhiteCustomScale(play, &p, &zero, &zero, 300, 900, 14);
            Actor_SpawnFloorDustRing(play, &this->actor, &p, 140.0f, 16, 10.0f, 600, 18, true);
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_LAND);
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_HAMMER_HIT);
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOMB_EXPLOSION);
            HtLynel_P3Quake(this, play, 16, 12);
            Rumble_Request(0.0f, 255, 20, 150);
            HtLynel_UpperAnim(this, LYNEL_ANIM_SHAKEN);
            HtLynel_P3Quake(this, play, 8, T3_DIG - T3_QUAKE + T3_DIG_FRAMES + 10);
            HtLynel_Rumble(this, T3_DIG - T3_QUAKE + T3_DIG_FRAMES + 10);
        }
        if (t >= T3_LAND) {
            // (turning onto Link as it comes down out of the leap)
            Math_SmoothStepToS(&this->actor.world.rot.y,
                               Math_FAtan2F(this->linkSpot.x - cx, this->linkSpot.z - cz) * (0x8000 / M_PI), 2,
                               0x1800, 0x100);
        }
        if (t == T3_LAND + 4) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
        }
        HtLynel_WritePose(this, gHtLynelJump[f]);
    } else {
        if (t == T3_LEAP_END) {
            // down, squared up to him (reeling)
            this->actor.world.rot.y =
                Math_FAtan2F(this->linkSpot.x - cx, this->linkSpot.z - cz) * (0x8000 / M_PI); // (on Link)
            HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonIdleAnim, 1.0f, ANIMMODE_LOOP, -4.0f);
        }
        if (t == T3_QUAKE + 10) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
        }
        if ((t > T3_QUAKE) && (t < T3_DIG) && ((t % 5) == 0)) {
            // grit off the cracks opening round the middle
            f32 a = Rand_ZeroFloat(2.0f * M_PI);
            f32 r = (Rand_ZeroOne() < 0.5f) ? HT_DIG_PIT_R : HT_DIG_RING_R;
            Vec3f p = { cx + sinf(a) * r, floorY, cz + cosf(a) * r };

            EffectSsHahen_SpawnBurst(play, &p, 4.0f, 0, 8, 6, 4, HAHEN_OBJECT_DEFAULT, 10, NULL);
        }
        if (t == T3_DIG) {
            if (this->digRing != NULL) {
                HtPedestal_MoveTo(this->digRing, HT_DIG_RING_DROP, T3_DIG_FRAMES);
            }
            if (this->digPit != NULL) {
                HtPedestal_MoveTo(this->digPit, HT_DIG_PIT_DROP, T3_DIG_FRAMES);
            }
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_STONE_BOUND);
        }
        if ((t >= T3_DIG) && (t < T3_DIG + T3_DIG_FRAMES) && ((t % 3) == 0)) {
            // grit and dust off the shaft's edges as it goes down
            f32 a = Rand_ZeroFloat(2.0f * M_PI);
            f32 r = (Rand_ZeroOne() < 0.5f) ? HT_DIG_PIT_R : HT_DIG_RING_R;
            Vec3f p = { cx + sinf(a) * r, floorY, cz + cosf(a) * r };

            EffectSsHahen_SpawnBurst(play, &p, 6.0f, 0, 10, 8, 5, HAHEN_OBJECT_DEFAULT, 10, NULL);
            Actor_SpawnFloorDustRing(play, &this->actor, &p, 20.0f, 4, 4.0f, 250, 8, true);
        }
        if (this->digPit != NULL) {
            this->actor.world.pos.y = this->digPit->world.pos.y;
        }
        SkelAnime_Update(&this->skin.skelAnime);
    }
    if (t >= T3_LEAP) {
        // Link, set down (and held) before it, facing it - going down with the ring
        player->actor.world.pos.x = this->linkSpot.x;
        player->actor.world.pos.z = this->linkSpot.z;
        player->actor.world.pos.y = HtLynel_RingY(this);
        player->actor.velocity.x = player->actor.velocity.y = player->actor.velocity.z = 0.0f;
        player->actor.speedXZ = 0.0f;
        player->actor.prevPos = player->actor.world.pos;
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw =
            Math_FAtan2F(cx - this->linkSpot.x, cz - this->linkSpot.z) * (0x8000 / M_PI);
    }
    if (t == T3_QUAKE + 2) {
        Player_SetCsActionWithHaltedActors(play, &this->actor, 81); // (staggered: looking about him)
    }
    if (t == T3_FINAL) {
        Player_SetCsActionWithHaltedActors(play, &this->actor, 1);
        HtLynel_UpperAnim(this, LYNEL_ANIM_GLARE); // (caught fast: it glowers at him)
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_BREATH);
    }
    if ((t >= T3_LEAP) && (++this->upperFrame >= sLynelAnimFrames[this->upperAnim])) {
        this->upperFrame = 0; // (the leap's charge pose, then reeling: loops)
    }

    // the camera
    {
        Vec3f* at = &this->camAt;
        Vec3f* eye = &this->camEye;
        f32 fx = Math_SinS(this->actor.world.rot.y);
        f32 fz = Math_CosS(this->actor.world.rot.y);
        f32 lx = this->linkSpot.x - cx;
        f32 lz = this->linkSpot.z - cz;
        f32 ld = sqrtf(SQ(lx) + SQ(lz));

        if (ld > 1.0f) {
            lx /= ld;
            lz /= ld;
        }
        if (t < T3_LEAP) {
            // before it, low: the roar
            at->x = this->actor.world.pos.x;
            at->y = this->actor.world.pos.y + 150.0f;
            at->z = this->actor.world.pos.z;
            eye->x = at->x + fx * 300.0f + fz * 90.0f;
            eye->y = this->actor.world.pos.y + 70.0f;
            eye->z = at->z + fz * 300.0f - fx * 90.0f;
        } else if (t < T3_LEAP_END) {
            // alongside it through the leap
            Vec3f want;

            at->x = this->actor.world.pos.x + fx * 60.0f;
            at->y = this->actor.world.pos.y + 120.0f;
            at->z = this->actor.world.pos.z + fz * 60.0f;
            want.x = at->x + fz * 420.0f - fx * 160.0f;
            want.y = at->y + 60.0f;
            want.z = at->z - fx * 420.0f - fz * 160.0f;
            if (t == T3_LEAP) {
                *eye = want;
            } else {
                Math_ApproachF(&eye->x, want.x, 0.3f, 60.0f);
                Math_ApproachF(&eye->y, want.y, 0.3f, 60.0f);
                Math_ApproachF(&eye->z, want.z, 0.3f, 60.0f);
            }
        } else if (t < T3_DIG) {
            // over Link's shoulder: landed before him, and reeling as the ground heaves
            at->x = cx;
            at->y = this->actor.world.pos.y + 140.0f;
            at->z = cz;
            eye->x = this->linkSpot.x + lx * 110.0f + lz * 55.0f;
            eye->y = player->actor.world.pos.y + 75.0f;
            eye->z = this->linkSpot.z + lz * 110.0f - lx * 55.0f;
        } else if (t < T3_FINAL) {
            // wide and high: the middle going down (still reeling)
            at->x = cx;
            at->y = floorY - 70.0f;
            at->z = cz;
            eye->x = cx + lx * 700.0f + lz * 200.0f;
            eye->y = floorY + 480.0f;
            eye->z = cz + lz * 700.0f - lx * 200.0f;
        } else {
            // down in it: over Link's shoulder, face to face with its upper body
            at->x = cx;
            at->y = this->actor.world.pos.y + 130.0f;
            at->z = cz;
            eye->x = this->linkSpot.x + lx * 38.0f + lz * 40.0f; // (inside the shaft's wall)
            eye->y = player->actor.world.pos.y + 80.0f;
            eye->z = this->linkSpot.z + lz * 38.0f - lx * 40.0f;
        }
        Play_CameraSetFov(play, this->subCamId, 60.0f);
        Play_CameraSetAtEye(play, this->subCamId, at, eye);
    }

    if (++this->csTimer >= T3_END) {
        Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);

        cam->eye = this->camEye;
        cam->eyeNext = this->camEye;
        cam->at = this->camAt;
        func_800C08AC(play, this->subCamId, 0);
        this->subCamId = 0;
        func_80064534(play, &play->csCtx);
        Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
        this->form = HT_LYNEL_PHASE3;
        this->p3Hits = 0;
        this->vulnTimer = 0;
        this->rockTimer = 260;
        this->naviCool = 0;
        HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
        HtLynel_SetAction(this, P3_WATCH, 40);
        if (this->katanaPut) {
            // Navi: its bared curse swallows the katana's frost - the Master Sword
            Message_StartTextbox(play, HT_LYNEL_KATANA_TEXT, NULL);
            this->naviCool = 300;
        }
    }
}

// ---- phase 3: blade to blade ---------------------------------------------------------------------------------------
// Sunk to its chest in the middle, it can only turn - after Link, round the ring. Its sword strikes (block one with
// the shield and its arm is thrown back; dodge one and its blade bites into the stone where he stood: either way, an
// opening for the Master Sword), its fire breath (the shield holds it), and boulders off the vault. Its bared curse
// swallows the katana's frost: it glances off. Eight strikes and it's done.
static void HtLynel_P3Clang(HtLynel* this, PlayState* play) {
    Vec3f spark = this->actor.world.pos;

    if (this->timer != 0) {
        return;
    }
    spark.y += 140.0f;
    CollisionCheck_SpawnShieldParticlesMetal(play, &spark);
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
    this->timer = 10;
}

// a sword on it: 1 a strike that tells (the Master Sword in an opening), 0 otherwise (clanged off)
static s32 HtLynel_P3Struck(HtLynel* this, PlayState* play) {
    s32 hit = false;

    this->collider.dim.radius = 125; // (out to where Link's blade reaches from the pit's rim)
    this->collider.dim.height = 260;
    this->collider.dim.yShift = 0;
    this->collider.dim.pos.x = (s16)this->actor.world.pos.x;
    this->collider.dim.pos.y = (s16)this->actor.world.pos.y;
    this->collider.dim.pos.z = (s16)this->actor.world.pos.z;
    // (out of an opening it's hard: Link's sword rebounds off it)
    if (this->vulnTimer > 0) {
        this->collider.base.colType = COLTYPE_HIT0;
        this->collider.base.acFlags &= ~AC_HARD;
    } else {
        this->collider.base.colType = COLTYPE_HARD;
        this->collider.base.acFlags |= AC_HARD;
    }
    if (this->collider.base.acFlags & AC_HIT) {
        Actor* ac = this->collider.base.ac;

        this->collider.base.acFlags &= ~AC_HIT;
        if ((ac != NULL) && (ac->id == ACTOR_PLAYER) && !Katana_IsEquipped() && (this->vulnTimer > 0) &&
            (this->hitCool == 0)) {
            hit = true;
        } else {
            if ((ac != NULL) && ((ac->id == HT_ACTOR_ICE_SLASH) || Katana_IsEquipped()) && (this->naviCool == 0)) {
                Message_StartTextbox(play, HT_LYNEL_KATANA_TEXT, NULL);
                this->naviCool = 400;
            }
        }
    }
    if (this->timer > 0) {
        this->timer--;
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    return hit;
}

static void HtLynel_UpdateBreath(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 i;

    for (i = 0; i < HT_LYNEL_BREATH; i++) {
        HtLynelPuff* p = &this->puffs[i];

        if (!p->on) {
            continue;
        }
        p->pos.x += p->vel.x;
        p->pos.y += p->vel.y;
        p->pos.z += p->vel.z;
        p->life++;
        if ((p->life > 24) || (p->pos.y < HtLynel_RingY(this) + 5.0f)) {
            p->on = false;
            continue;
        }
        if (!Player_InCsMode(play) &&
            (SQ(p->pos.x - player->actor.world.pos.x) + SQ(p->pos.y - (player->actor.world.pos.y + 35.0f)) +
                 SQ(p->pos.z - player->actor.world.pos.z) <
             SQ(30.0f + p->life * 1.2f))) {
            p->on = false;
            if (HtLynel_LinkShields(this, player)) {
                // the shield holds it (quietly: the flames just part on it)
            } else if (this->burnCool == 0) {
                s32 b;

                Actor_SetPlayerKnockbackSmall(play, &this->actor, 5.0f, this->actor.world.rot.y, 3.0f, P3_DMG_FIRE);
                for (b = 0; b < PLAYER_BODYPART_MAX; b++) {
                    player->bodyFlameTimers[b] = Rand_S16Offset(0, 200);
                }
                player->bodyIsBurning = true;
                this->burnCool = 30;
            }
        }
    }
    if (this->burnCool > 0) {
        this->burnCool--;
    }
}

static void HtLynel_DrawBreath(HtLynel* this, PlayState* play) {
    Camera* cam = GET_ACTIVE_CAM(play);
    f32 camYaw = BINANG_TO_RAD((s16)(Camera_GetCamDirYaw(cam) + 0x8000));
    u32 fr = play->gameplayFrames;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0, (fr * -20) & 0x1FF, 0x20, 0x80, 0, 0,
                                  0, -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 240, 140, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 60, 0, 0);
    for (i = 0; i < HT_LYNEL_BREATH; i++) {
        HtLynelPuff* p = &this->puffs[i];
        f32 sc;

        if (!p->on) {
            continue;
        }
        sc = 0.004f + 0.00045f * p->life;
        FrameInterpolation_RecordOpenChild(p, 0);
        Matrix_Translate(p->pos.x, p->pos.y - sc * 3000.0f, p->pos.z, MTXMODE_NEW);
        Matrix_RotateY(camYaw, MTXMODE_APPLY);
        Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
        FrameInterpolation_RecordCloseChild();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// a glint off both blades' tips (an attack's warning)
static void HtLynel_Glint(HtLynel* this, PlayState* play) {
    static Color_RGBA8 prim = { 255, 255, 230, 255 };
    static Color_RGBA8 env = { 255, 200, 120, 0 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    s32 k;

    for (k = 0; k < 2; k++) {
        EffectSsGSpk_SpawnAccel(play, &this->actor, &this->bladeTipW[k], &zero, &zero, &prim, &env, 60, 30);
        EffectSsGSpk_SpawnAccel(play, &this->actor, &this->bladeTipW[k], &zero, &zero, &prim, &env, 40, 20);
    }
}

// the whirl: has either blade, between the last two draws, swept over Link (seen from above, his body's width
// round him) at a height on him?
static s32 HtLynel_PointInTri(f32 px, f32 pz, Vec3f* a, Vec3f* b, Vec3f* c) {
    f32 d1 = (px - b->x) * (a->z - b->z) - (a->x - b->x) * (pz - b->z);
    f32 d2 = (px - c->x) * (b->z - c->z) - (b->x - c->x) * (pz - c->z);
    f32 d3 = (px - a->x) * (c->z - a->z) - (c->x - a->x) * (pz - a->z);

    return !(((d1 < 0) || (d2 < 0) || (d3 < 0)) && ((d1 > 0) || (d2 > 0) || (d3 > 0)));
}

static f32 HtLynel_SegDist2(f32 px, f32 pz, Vec3f* a, Vec3f* b) {
    f32 ex = b->x - a->x, ez = b->z - a->z;
    f32 l2 = SQ(ex) + SQ(ez);
    f32 t = (l2 > 0.01f) ? CLAMP(((px - a->x) * ex + (pz - a->z) * ez) / l2, 0.0f, 1.0f) : 0.0f;

    return SQ(px - (a->x + ex * t)) + SQ(pz - (a->z + ez * t));
}

static s32 HtLynel_BladesSwept(HtLynel* this, Player* player) {
    f32 px = player->actor.world.pos.x;
    f32 pz = player->actor.world.pos.z;
    f32 py = player->actor.world.pos.y;
    s32 k;

    for (k = 0; k < 2; k++) {
        Vec3f* b0 = &this->chestW; // (its arm too: from its chest out to the blade's tip)
        Vec3f* t0 = &this->bladeTipPrev[k];
        Vec3f* t1 = &this->bladeTipW[k];
        Vec3f* b1 = &this->chestW;
        f32 y = (t1->y + b1->y) * 0.5f;

        if ((y < py - 5.0f) || (y > py + 75.0f)) {
            continue;
        }
        if (HtLynel_PointInTri(px, pz, b0, t0, t1) || HtLynel_PointInTri(px, pz, b0, t1, b1) ||
            (HtLynel_SegDist2(px, pz, b1, t1) < SQ(22.0f)) || (HtLynel_SegDist2(px, pz, t0, t1) < SQ(22.0f)) ||
            (HtLynel_SegDist2(px, pz, b0, t0) < SQ(22.0f))) {
            return true;
        }
    }
    return false;
}

// a single cut (one blade, blockable): left (1) - Link on its left, where that blade comes down - or right (0)
static void HtLynel_StartStrike3(HtLynel* this, s16 left) {
    this->strikeSide = left;
    this->strikeKind = 0;
    this->hitDone = false;
    HtLynel_UpperAnim(this, left ? LYNEL_ANIM_SLASH3L : LYNEL_ANIM_SLASH3R);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_SWORD);
    HtLynel_SetAction(this, P3_STRIKE, 0);
}

// the yaw that puts Link where its cutting blade comes down (a single cut: off to that arm's side), or square on him
static s16 HtLynel_StrikeYaw(HtLynel* this, s16 toLink, f32 d) {
    s16 off;

    if (this->strikeKind != 0) {
        return toLink;
    }
    off = Math_FAtan2F(P3_STRIKE_AIM, MAX(d, 60.0f)) * (0x8000 / M_PI);
    return this->strikeSide ? (s16)(toLink - off) : (s16)(toLink + off);
}

static void HtLynel_UpdatePhase3(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 cx = this->actor.home.pos.x;
    f32 cz = this->actor.home.pos.z;
    f32 ahead, side, d;
    s16 toLink = Math_FAtan2F(player->actor.world.pos.x - cx, player->actor.world.pos.z - cz) * (0x8000 / M_PI);
    s32 i, atLevel;

    // fixed in the middle, sunk on its disc
    this->actor.world.pos.x = cx;
    this->actor.world.pos.z = cz;
    if (this->digPit != NULL) {
        this->actor.world.pos.y = this->digPit->world.pos.y;
    }
    HtLynel_LinkLocal(this, player, &ahead, &side);
    d = sqrtf(SQ(player->actor.world.pos.x - cx) + SQ(player->actor.world.pos.z - cz));
    if (this->swirl > 0.0f) {
        // (the swirl: spinning with the sweep, then fading)
        this->swirlScroll += 1.0f;
        if ((this->action != P3_SPIN) || (this->actTimer > 21)) {
            this->swirl -= 0.2f;
        }
    }
    // (a roll just now still counts a moment after: the whirl lets him off if he went under it early)
    if (player->actionFunc == Player_Action_Roll) {
        this->rollGrace = 10;
    } else if (this->rollGrace > 0) {
        this->rollGrace--;
    }
    // (its blades sweep at its shoulders: Link's there only on the ring, not above it)
    atLevel = fabsf(player->actor.world.pos.y - HtLynel_RingY(this)) < 40.0f;
    if (this->naviCool > 0) {
        this->naviCool--;
    }
    if (this->hitCool > 0) {
        this->hitCool--;
    }
    if (this->vulnTimer > 0) {
        this->vulnTimer--;
    }
    // the Master Sword in an opening
    if (HtLynel_P3Struck(this, play)) {
        // (the Biggoron Sword: twice the blow, as ever)
        this->p3Hits += (player->heldItemAction == PLAYER_IA_SWORD_BIGGORON) ? 2 : 1;
        this->hitCool = 12;
        Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 10);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_DAMAGE);
        if (this->action == P3_REELING) {
            // (reeling: a jolt, and on reeling - till the second strike)
            if (++this->staggerHits >= P3_REEL_HITS) {
                this->vulnTimer = 0;
                HtLynel_UpperAnim(this, LYNEL_ANIM_HURT);
                HtLynel_SetAction(this, P3_WATCH, 30);
            }
        } else {
            HtLynel_UpperAnim(this, LYNEL_ANIM_HURT);
            this->upperFrame = 0;
            // (a flinch, then on guard - still open the rest of its opening: it holds off attacking till that's by)
            HtLynel_SetAction(this, P3_WATCH, this->vulnTimer + 12);
        }
        if (this->p3Hits >= P3_HITS) {
            this->form = HT_LYNEL_DEATH;
            this->csTimer = 0;
            return;
        }
    }
    // boulders off the vault, now and then
    if ((this->action == P3_FIRE) && (this->rockTimer < 30)) {
        this->rockTimer = 30; // (no boulders while it breathes fire: he's crouched behind his shield)
    }
    if (--this->rockTimer <= 0) {
        f32 ry = HtLynel_RingY(this);

        this->rockTimer = 300 + (s16)Rand_ZeroFloat(120.0f);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
        HtLynel_P3Quake(this, play, 4, 15);
        this->rainTimer = 112;
        this->rainNext = 6;
        (void)ry;
    }
    HtLynel_UpdateRain(this, play, HT_DIG_PIT_R + 15.0f, HT_DIG_RING_R - 15.0f, HtLynel_RingY(this));
    HtLynel_UpdateRocks(this, play);
    HtLynel_UpdateBreath(this, play);

    switch (this->action) {
        case P3_WATCH:
            // turning after him round the ring
            Math_SmoothStepToS(&this->actor.world.rot.y, toLink, 3, 0x500, 0x40);
            if ((--this->actTimer <= 0) && (ABS((s16)(this->actor.world.rot.y - toLink)) < 0x1800) &&
                !Player_InCsMode(play)) {
                f32 roll = Rand_ZeroOne();

                this->hitDone = false;
                if ((d < P3_REACH) && (roll < 0.40f)) {
                    // one blade (blockable): the arm on Link's side
                    this->strikeChain = 0;
                    HtLynel_StartStrike3(this, (side > 0.0f) ? 1 : 0);
                } else if ((d < P3_REACH) && (roll < 0.62f)) {
                    // both blades at once: the shield won't hold that
                    this->strikeKind = 1;
                    this->tellTimer = P3_X_TELL;
                    HtLynel_UpperAnim(this, LYNEL_ANIM_SLASHX3);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_SWORD);
                    HtLynel_SetAction(this, P3_STRIKE, 0);
                } else if ((d < P3_REACH + 30.0f) && (roll < 0.80f)) {
                    // the whirl: both arms out, right round - roll under it
                    HtLynel_UpperAnim(this, LYNEL_ANIM_SPIN);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
                    this->tellTimer = P3_SPIN_TELL;
                    HtLynel_SetAction(this, P3_SPIN, 0);
                } else {
                    // fire: breath drawn in, head thrown back first
                    this->breathDir = (Rand_ZeroOne() < 0.5f) ? 1 : -1;
                    HtLynel_UpperAnim(this, LYNEL_ANIM_BREATH);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_DODO_J_BREATH);
                    this->tellTimer = P3_FIRE_TELL;
                    HtLynel_SetAction(this, P3_FIRE, 0);
                    this->rainTimer = 0; // (and any rain stopped)
                }
            }
            break;

        case P3_STRIKE:
            // the cut, end to end: wound up high (still turning on him), down through where he stands, the blade
            // carried on across and back to its guard. Blocked (one blade): the arm thrown back off the shield, and
            // it reels - an opening. Dodged: the blade bites into the stone - an opening. Hit: it plays on out.
            if ((this->actTimer < 12) && (this->strikeKind == 0)) {
                // (wheeling round fast through the wind-up: the cutting arm brought to bear on him - a single cut
                // only: the X keeps the line it started on, telegraph and cut, for Link to get out of)
                Math_SmoothStepToS(&this->actor.world.rot.y, HtLynel_StrikeYaw(this, toLink, d), 2, 0x1400,
                                   0x100);
            }
            if ((this->strikeKind == 1) && (this->actTimer == P3_X_APEX) && (this->tellTimer > 0)) {
                // both blades raised high and held, glinting, growling: get out of its line
                if (this->tellTimer == P3_X_TELL) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
                }
                if ((this->tellTimer % 4) == 0) {
                    HtLynel_Glint(this, play);
                }
                this->tellTimer--;
                break;
            }
            if (!this->hitDone && (this->actTimer >= P3_STRIKE_HIT0) && (this->actTimer <= P3_STRIKE_BITE) &&
                (ahead > 0.0f) && (d < P3_REACH + 25.0f) && (fabsf(side) < P3_STRIKE_SIDE) && atLevel &&
                !Player_InCsMode(play)) {
                this->hitDone = true;
                if ((this->strikeKind == 0) && HtLynel_LinkShields(this, player)) {
                    Vec3f spark = player->actor.world.pos;

                    spark.y += 45.0f;
                    spark.x += Math_SinS(player->actor.shape.rot.y) * 15.0f;
                    spark.z += Math_CosS(player->actor.shape.rot.y) * 15.0f;
                    CollisionCheck_SpawnShieldParticlesMetal(play, &spark); // (sparks; no clang)
                    // that arm (only) thrown back off the shield...
                    HtLynel_UpperAnim(this, (this->upperAnim == LYNEL_ANIM_SLASH3L) ? LYNEL_ANIM_PARRYL
                                                                                    : LYNEL_ANIM_PARRYR);
                    this->upperBlend = 2; // (a jolt: hardly blended)
                    if (this->strikeChain == 0) {
                        // ...and straight on into a cut with the other, wheeling to bring it to bear
                        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_ROAR);
                        HtLynel_SetAction(this, P3_STAGGER, P3_PARRY_FOLLOW);
                    } else {
                        // ...the second parried too: it reels, wide open
                        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_DAMAGE);
                        this->vulnTimer = P3_REEL_FRAMES;
                        this->staggerHits = 0;
                        HtLynel_SetAction(this, P3_REELING, P3_REEL_FRAMES);
                    }
                    break;
                }
                // (the X's inward cut: hardly a shove - he's still there for the outward one)
                Actor_SetPlayerKnockbackSmall(play, &this->actor, (this->strikeKind == 1) ? 2.0f : 9.0f,
                                              this->actor.world.rot.y, (this->strikeKind == 1) ? 2.0f : 5.0f,
                                              (this->strikeKind == 1) ? P3_DMG_DUAL : P3_DMG_STRIKE);
            }
            if ((this->strikeKind == 1) && (this->actTimer >= P3_X_TO_OUT)) {
                // the X's blades crossed before it - flung straight back out, wide, both ways
                HtLynel_UpperAnim(this, LYNEL_ANIM_XOUT3);
                this->upperBlend = 2;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_SWORD);
                Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
                this->hitDone = false;
                HtLynel_SetAction(this, P3_XOUT, 0);
                break;
            }
            if ((this->actTimer == P3_STRIKE_BITE) && !this->hitDone && (this->strikeKind == 0)) {
                // dodged: its blade (both, for the X) bites into the stone where he was
                s32 k;

                Audio_PlayActorSound2(&this->actor, NA_SE_EV_STONE_BOUND);
                Actor_RequestQuakeAndRumble(&this->actor, play, 3, 6);
                for (k = 0; k < 2; k++) {
                    if ((this->strikeKind == 1) || (k == (this->strikeSide ? 0 : 1))) {
                        Vec3f p = this->bladeTipW[k];

                        p.y = MAX(p.y, HtLynel_RingY(this));
                        EffectSsHahen_SpawnBurst(play, &p, 6.0f, 0, 10, 8, 6, HAHEN_OBJECT_DEFAULT, 10, NULL);
                        Actor_SpawnFloorDustRing(play, &this->actor, &p, 25.0f, 6, 5.0f, 250, 10, true);
                    }
                }
                this->hitDone = true;
                this->vulnTimer = (this->strikeKind == 1) ? P3_OPEN_DODGE + 15 : P3_OPEN_DODGE;
                HtLynel_SetAction(this, P3_BLADE, this->vulnTimer);
                break;
            }
            if (++this->actTimer >= sLynelAnimFrames[this->upperAnim] - 1) {
                HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
                if (this->strikeKind == 1) {
                    HtLynel_SetAction(this, P3_REPOS, 20); // (the X spent: round square onto him again)
                } else {
                    HtLynel_SetAction(this, P3_WATCH, 30 + (s16)Rand_ZeroFloat(30.0f));
                }
            }
            break;

        case P3_XOUT:
            // the outward double slash: the blades sweep out across his whole front
            if (!this->hitDone && (this->actTimer >= P3_XOUT_HIT0) && (this->actTimer <= P3_XOUT_HIT1) &&
                (ahead > 0.0f) && (d < P3_REACH + 25.0f) && (fabsf(side) < P3_XOUT_SIDE) && atLevel &&
                !Player_InCsMode(play)) {
                this->hitDone = true;
                Actor_SetPlayerKnockbackLarge(play, &this->actor, 12.0f, toLink, 7.0f, P3_DMG_DUAL);
            }
            if (++this->actTimer >= sLynelAnimFrames[LYNEL_ANIM_XOUT3] - 1) {
                HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
                HtLynel_SetAction(this, P3_REPOS, 20); // (the X spent: round square onto him again)
            }
            break;

        case P3_REPOS:
            // wheeling round fast, square onto Link
            Math_SmoothStepToS(&this->actor.world.rot.y, toLink, 2, 0x1400, 0x100);
            if ((--this->actTimer <= 0) || (ABS((s16)(this->actor.world.rot.y - toLink)) < 0x200)) {
                HtLynel_SetAction(this, P3_WATCH, 25 + (s16)Rand_ZeroFloat(25.0f));
            }
            break;

        case P3_SPIN:
            // the whirl: the blades sweep round at his chest, fast - a roll or a crouch behind the shield goes under
            if (this->actTimer < 8) {
                Math_SmoothStepToS(&this->actor.world.rot.y, toLink, 3, 0x400, 0x40);
            }
            if ((this->actTimer == P3_SPIN_APEX) && (this->tellTimer > 0)) {
                // wound back, arms out, held - glinting: get ready to roll
                if (this->tellTimer == P3_SPIN_TELL) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
                }
                if ((this->tellTimer % 4) == 0) {
                    HtLynel_Glint(this, play);
                }
                this->tellTimer--;
                break;
            }
            if ((this->actTimer >= 9) && (this->actTimer <= 20) && ((this->actTimer % 4) < 2)) {
                Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
            }
            if (!this->hitDone && (this->actTimer >= P3_SPIN_HIT0) && (this->actTimer <= P3_SPIN_HIT1 + 1) &&
                atLevel && !Player_InCsMode(play) && (this->rollGrace == 0) &&
                !(player->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
                ((d < 135.0f) || HtLynel_BladesSwept(this, player))) { // (or pressed in under its arms)
                this->hitDone = true;
                Actor_SetPlayerKnockbackLarge(play, &this->actor, 12.0f, toLink, 7.0f, P3_DMG_DUAL);
            }
            if ((this->actTimer == 9) || (this->actTimer == P3_SPIN_APEX + 1)) {
                if (this->swirl <= 0.0f) {
                    // Link's spin attack's swirl, in its colours, round it at the blades' height
                    this->swirl = 1.0f;
                    this->swirlScroll = 0.0f;
                    Audio_PlayActorSound2(&this->actor, NA_SE_IT_ROLLING_CUT);
                }
            }
            // (the sweep itself at double speed: over in a few frames)
            this->actTimer += ((this->actTimer >= 9) && (this->actTimer < 21)) ? 2 : 1;
            if (this->actTimer >= sLynelAnimFrames[LYNEL_ANIM_SPIN] - 1) {
                HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
                HtLynel_SetAction(this, P3_WATCH, 30 + (s16)Rand_ZeroFloat(30.0f));
            }
            break;

        case P3_BLADE:
            // its blade stuck in the stone, tugging at it; then wrenched free, the cut played on out
            if ((this->actTimer % 12) == 0) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
            }
            if (--this->actTimer <= 0) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_ROCK_BROKEN);
                HtLynel_SetAction(this, P3_STRIKE, P3_STRIKE_BITE + 1);
                this->hitDone = true;
            }
            break;

        case P3_STAGGER:
            // its arm flung back off the shield - then the other arm's cut, wheeled round to bear on him
            if (--this->actTimer <= 0) {
                this->strikeChain = 1;
                HtLynel_StartStrike3(this, this->strikeSide ^ 1);
            }
            break;

        case P3_REELING:
            // both cuts parried: reeling, both arms flung about, wide open
            if ((this->actTimer % 20) == 0) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
            }
            if (--this->actTimer <= 0) {
                this->vulnTimer = 0;
                HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
                HtLynel_SetAction(this, P3_WATCH, 25);
            }
            break;

        case P3_FIRE: {
            // breath drawn in (its head thrown back: 0-19), then a sweep of fire from its mouth across where he
            // stands (20-63)
            s32 f = this->actTimer;

            if ((f == P3_FIRE_APEX) && (this->tellTimer > 0)) {
                // head thrown back, breath held: embers spilling from its jaws (the warning)
                if ((this->tellTimer % 3) == 0) {
                    static Color_RGBA8 prim = { 255, 220, 120, 255 };
                    static Color_RGBA8 env = { 255, 60, 0, 0 };
                    Vec3f vel = { Rand_CenteredFloat(2.0f), 1.5f, Rand_CenteredFloat(2.0f) };
                    Vec3f acc = { 0.0f, 0.1f, 0.0f };

                    EffectSsGSpk_SpawnAccel(play, &this->actor, &this->headW, &vel, &acc, &prim, &env, 70, 25);
                }
                if (this->tellTimer == P3_FIRE_TELL) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_UNARI);
                }
                Math_SmoothStepToS(&this->actor.world.rot.y, toLink, 3, 0x400, 0x40);
                this->breathYaw = this->actor.world.rot.y;
                this->tellTimer--;
                break;
            }
            this->actTimer++;
            if (f < 20) {
                Math_SmoothStepToS(&this->actor.world.rot.y, toLink, 3, 0x400, 0x40);
                this->breathYaw = this->actor.world.rot.y;
            } else if (f < 64) {
                s16 yaw = this->breathYaw + this->breathDir * (s16)((f - 42) * 0x110);
                Vec3f mouth = this->headW;

                this->actor.world.rot.y = yaw;
                for (i = 0; i < HT_LYNEL_BREATH; i++) {
                    HtLynelPuff* p = &this->puffs[i];

                    if (!p->on) {
                        f32 dropY = (HtLynel_RingY(this) + 35.0f - mouth.y) / 16.0f; // (down to his level by ~250)

                        p->on = true;
                        p->life = 0;
                        p->pos = mouth;
                        p->vel.x = Math_SinS(yaw) * 15.0f;
                        p->vel.z = Math_CosS(yaw) * 15.0f;
                        p->vel.y = dropY;
                        break;
                    }
                }
                // (the roar of the flames: held only while it breathes them)
                Actor_PlaySfx_Flagged(&this->actor, NA_SE_EN_DODO_J_FIRE - SFX_FLAG);
                sFireSfxOn = true;
            } else {
                HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
                HtLynel_SetAction(this, P3_WATCH, 35 + (s16)Rand_ZeroFloat(30.0f));
            }
            break;
        }
    }

    // the horse: still, or stepping round (in its hole) as it turns
    {
        s32 turning = (this->action == P3_WATCH) && (ABS((s16)(this->actor.world.rot.y - toLink)) > 0x600);
        void* want = turning ? (void*)gHorseGanonWalkingAnim : (void*)gHorseGanonIdleAnim;

        if (this->skin.skelAnime.animation != want) {
            HtLynel_HorseAnim(this, (AnimationHeader*)want, 1.0f, ANIMMODE_LOOP, -6.0f);
        }
        SkelAnime_Update(&this->skin.skelAnime);
    }
    // its upper body's frame
    if (this->action == P3_XOUT) {
        this->upperFrame = CLAMP(this->actTimer, 0, sLynelAnimFrames[LYNEL_ANIM_XOUT3] - 1);
    } else if (this->action == P3_STRIKE) {
        this->upperFrame = CLAMP(this->actTimer, 0, sLynelAnimFrames[this->upperAnim] - 1);
    } else if (this->action == P3_SPIN) {
        this->upperFrame = CLAMP(this->actTimer, 0, sLynelAnimFrames[LYNEL_ANIM_SPIN] - 1);
    } else if (this->action == P3_BLADE) {
        // (held at the bite, a tug at it every so often)
        this->upperFrame = P3_STRIKE_BITE - (((this->actTimer % 12) < 3) ? 1 : 0);
    } else if (this->action == P3_STAGGER) {
        // (the parried arm flung back: the parry animation's start)
        this->upperFrame = CLAMP(P3_PARRY_FOLLOW - this->actTimer, 0, sLynelAnimFrames[this->upperAnim] - 1);
    } else if (this->action == P3_REELING) {
        // (the parry animation's fling into the reeling loop)
        if ((this->upperAnim == LYNEL_ANIM_PARRYL) || (this->upperAnim == LYNEL_ANIM_PARRYR)) {
            if (++this->upperFrame >= 8) {
                HtLynel_UpperAnim(this, LYNEL_ANIM_SHAKEN);
            }
        } else if (++this->upperFrame >= sLynelAnimFrames[this->upperAnim]) {
            this->upperFrame = 0;
        }
    } else if (this->action == P3_FIRE) {
        s32 f = this->actTimer;

        this->upperFrame = (f < 20) ? f : (20 + ((f - 20) % 20));
    } else if ((this->upperAnim == LYNEL_ANIM_HURT) && (this->upperFrame < 6)) {
        if (++this->upperFrame >= 6) { // (a flinch, then back to its guard)
            HtLynel_UpperAnim(this, LYNEL_ANIM_GUARD);
        }
    } else if (++this->upperFrame >= sLynelAnimFrames[this->upperAnim]) {
        this->upperFrame = 0;
    }
}

// ---- its death: the curse lifted ----------------------------------------------------------------------------------
// The last blow: it screams, rears feebly, slumps over its forelegs, doubled over. Then the curse burns off it - dark
// flames off the upper body as it shrinks back into the horse's chest, the horse's own neck and head coming back, the
// horse itself back to its true size - a flash, and only the black horse is left, kneeling on the Triforce, calm. The
// disc rises to the ring, the horse gets up, rears and neighs, leaps up out of the shaft and gallops off out of the
// hall through its door (it'll be found again in Hyrule Field). The blue light home rises on the disc.
#define D_COLLAPSE 30
#define D_BURN 50                       // the curse burning off, the flames growing over it...
#define D_WHITE_IN 78                   // ...the light swelling to white...
#define D_FREED 94                      // ...and in the white, it's gone: the black horse left
#define D_WHITE_HOLD 6
#define D_RISE_FRAMES 40                // the disc rising to the ring
#define D_STAND (D_FREED + 14)          // the horse getting up (the stand-up from its kneel)
#define D_STAND_FROM 9
#define D_REAR (D_STAND + LYNEL_STAND_FRAMES - D_STAND_FROM + 4)
#define D_REAR_FRAMES 36
#define D_JUMP (D_REAR + D_REAR_FRAMES)  // up out of the shaft...
#define D_JUMP_R 340.0f                 // ...onto the hall's floor this far out
#define D_GALLOP (D_JUMP + LYNEL_JUMP_FRAMES)
#define D_GALLOP_SPEED 30.0f
#define D_DOOR_R 1560.0f                // ...galloping off through the door (the wall's 1500 out)
#define D_DOOR_OPEN (D_GALLOP + 12)      // the hall's door sliding up for it...
#define D_WARP (D_GALLOP + 76)          // (...and down behind it)
#define D_END (D_WARP + 34)
#define D_HORSE_K 0.87f                 // the black horse's true size (as ridden: HtHorse.h HT_HORSE_SCALE)

static void HtLynel_UpdateDeath(HtLynel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 t = this->csTimer;
    f32 cx = this->actor.home.pos.x;
    f32 cz = this->actor.home.pos.z;
    f32 floorY = this->actor.home.pos.y - HT_PEDESTAL_TOP_DROP;
    f32 ringY = HtLynel_RingY(this);
    f32 dx = Math_SinS(this->actor.home.rot.y); // (the way to the hall's door: the way it faced, chained)
    f32 dz = Math_CosS(this->actor.home.rot.y);
    s32 i;

    if (t == 0) {
        func_80064520(play, &play->csCtx);
        Player_SetCsActionWithHaltedActors(play, &this->actor, 1);
        this->subCamId = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, this->subCamId, CAM_STAT_ACTIVE);
        Play_CameraSetFov(play, this->subCamId, 60.0f);
        for (i = 0; i < HT_LYNEL_ROCKS; i++) {
            this->rocks[i].on = false;
        }
        for (i = 0; i < HT_LYNEL_CHIPS; i++) {
            this->chips[i].on = false;
        }
        for (i = 0; i < HT_LYNEL_BREATH; i++) {
            this->puffs[i].on = false;
        }
        this->rainTimer = 0;
        this->swirl = 0.0f;
        this->vulnTimer = 0;
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_DEAD1);
        Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonRearingAnim, 0.6f, 0.0f,
                         Animation_GetLastFrame((AnimationHeader*)gHorseGanonRearingAnim), ANIMMODE_ONCE, -4.0f);
        HtLynel_UpperAnim(this, LYNEL_ANIM_HURT);
        this->deathSink = 0.0f;
        Flags_SetSwitch(play, HT_LYNEL_DEAD_FLAG);
        Flags_SetClear(play, this->actor.room);
        Katana_SetLocked(false);
        HtHorse_Free(); // (the horse his: it'll be waiting for him in Hyrule Field)
        // (Link set down on the ring on the far side from the door, facing it: out of the horse's way)
        this->linkSpot.x = cx - dx * T3_LINK_R;
        this->linkSpot.z = cz - dz * T3_LINK_R;
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
    }
    if (t < D_FREED + D_RISE_FRAMES) {
        player->actor.world.pos.x = this->linkSpot.x;
        player->actor.world.pos.z = this->linkSpot.z;
        player->actor.world.pos.y = ringY;
        player->actor.prevPos = player->actor.world.pos;
        player->actor.speedXZ = 0.0f;
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw =
            Math_FAtan2F(cx - this->linkSpot.x, cz - this->linkSpot.z) * (0x8000 / M_PI);
    }
    if (t < D_STAND) {
        // (on Link, where he's been set down)
        Math_SmoothStepToS(&this->actor.world.rot.y, Math_FAtan2F(-dx, -dz) * (0x8000 / M_PI), 3, 0x800, 0x80);
    } else if (t < D_REAR) {
        // (turning round from Link to the door as it gets up)
        s16 from = Math_FAtan2F(-dx, -dz) * (0x8000 / M_PI);
        f32 k = CLAMP((f32)(t - D_STAND) / (D_REAR - D_STAND), 0.0f, 1.0f);

        this->actor.world.rot.y = from + (s16)(0x8000 * (0.5f - 0.5f * cosf(k * M_PI)));
    } else {
        this->actor.world.rot.y = Math_FAtan2F(dx, dz) * (0x8000 / M_PI);
    }

    if (t < D_COLLAPSE) {
        // the last blow: a scream, rearing feebly
        this->upperFrame = MIN(t, 4);
        SkelAnime_Update(&this->skin.skelAnime);
        if ((t % 6) == 0) {
            Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 4);
        }
    } else if (t < D_STAND) {
        // slumped down over its forelegs, doubled over
        if (t == D_COLLAPSE) {
            HtLynel_PoseBlendFrom(this);
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_MGANON_DEAD2);
            HtLynel_P3Quake(this, play, 5, 12);
        }
        HtLynel_WritePose(this, gHtLynelStandUp[D_STAND_FROM]);
        this->upperFrame = MIN(t - D_COLLAPSE + 4, 28);
    }
    if ((t >= D_BURN) && (t < D_FREED)) {
        // the curse burning off: dark flames pouring off its upper body, thicker and thicker, swallowing it - then
        // the light swells to white (the change itself unseen, in the white)
        f32 k = (f32)(t - D_BURN) / (D_FREED - D_BURN);
        s32 n = 2 + (s32)(k * 5.0f);

        for (i = 0; i < n; i++) {
            Vec3f p = this->chestW;
            Vec3f vel = { Rand_CenteredFloat(3.0f), 2.5f + Rand_ZeroFloat(2.5f), Rand_CenteredFloat(3.0f) };
            Vec3f acc = { 0.0f, 0.3f, 0.0f };

            p.x += Rand_CenteredFloat(110.0f);
            p.z += Rand_CenteredFloat(110.0f);
            p.y += Rand_ZeroFloat(170.0f) - 20.0f;
            EffectSsDeadDb_Spawn(play, &p, &vel, &acc, 130 + (s16)Rand_ZeroFloat(80.0f + 80.0f * k), 12, 120, 40, 170,
                                 255, 30, 0, 80, 1, 9, false);
        }
        // (the flames' crackle, held only while they burn - played once it rang on long after the outro)
        Actor_PlaySfx_Flagged(&this->actor, NA_SE_EN_FANTOM_FIRE - SFX_FLAG);
        if (t == D_BURN) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_TWINROBA_TRANSFORM);
        }
        if (t >= D_WHITE_IN) {
            play->envCtx.fillScreen = true;
            play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] = play->envCtx.screenFillColor[2] =
                255;
            play->envCtx.screenFillColor[3] = (u8)(255 * (t - D_WHITE_IN + 1) / (D_FREED - D_WHITE_IN));
        }
    }
    if (t == D_FREED) {
        // (its death cries cut off in the full white: they ran on over the calm horse)
        HtLynel_StopLoop(NA_SE_EN_MGANON_DEAD1);
        HtLynel_StopLoop(NA_SE_EN_MGANON_DEAD2);
        HtLynel_StopLoop(NA_SE_EN_FANTOM_FIRE);
        HtLynel_StopLoop(NA_SE_EN_TWINROBA_TRANSFORM);
        // in the white: the curse gone - the black horse, its true size, its own mane and tail, kneeling, calm
        this->tform = 0.0f;
        HtLynel_UnpatchHair();
        this->actor.scale.x = this->actor.scale.y = this->actor.scale.z = HT_LYNEL_SCALE * D_HORSE_K;
        play->envCtx.screenFillColor[3] = 255;
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOMB_EXPLOSION);
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_BOSS_CLEAR);
        this->heatFade = true;
        if (this->digPit != NULL) {
            HtPedestal_MoveTo(this->digPit, HT_DIG_RING_DROP, D_RISE_FRAMES); // (the disc up to the ring)
        }
        if (this->digRing != NULL) {
            // (the ring again, without its rim over the pit: Link can step out onto the disc)
            f32 ry = this->digRing->world.pos.y;

            Actor_Kill(this->digRing);
            this->digRing = Actor_Spawn(&play->actorCtx, play, HT_ACTOR_PEDESTAL, cx, ry, cz, 0, 0, 0,
                                        HT_PEDESTAL_DIG_RING2);
        }
    }
    if ((t > D_FREED + D_WHITE_HOLD) && play->envCtx.fillScreen) {
        // (the white fading off the horse)
        if (play->envCtx.screenFillColor[3] > 10) {
            play->envCtx.screenFillColor[3] -= 10;
        } else {
            play->envCtx.fillScreen = false;
        }
    }
    if ((t >= D_STAND) && (t < D_REAR)) {
        // getting up (forelegs, then the hind legs), turning round to the door
        HtLynel_WritePose(this, gHtLynelStandUp[MIN(D_STAND_FROM + (t - D_STAND), LYNEL_STAND_FRAMES - 1)]);
        if (t == D_STAND + 8) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_GROAN);
        }
    }
    if ((t >= D_REAR) && (t < D_JUMP)) {
        // rearing, neighing
        if (t == D_REAR) {
            Animation_Change(&this->skin.skelAnime, (AnimationHeader*)gHorseGanonRearingAnim, 1.0f, 0.0f,
                             Animation_GetLastFrame((AnimationHeader*)gHorseGanonRearingAnim), ANIMMODE_ONCE, -6.0f);
        }
        if (t == D_REAR + 6) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
        }
        SkelAnime_Update(&this->skin.skelAnime);
    }
    if (this->digPit != NULL) {
        this->actor.world.pos.y = this->digPit->world.pos.y;
    }
    if ((t >= D_JUMP) && (t < D_GALLOP)) {
        // up out of the shaft, onto the hall's floor
        s32 f = t - D_JUMP;
        f32 k = CLAMP((f32)(f - LYNEL_JUMP_UP) / (LYNEL_JUMP_DOWN - LYNEL_JUMP_UP), 0.0f, 1.0f);
        f32 y0 = (this->digPit != NULL) ? this->digPit->world.pos.y : ringY;

        if (t == D_JUMP) {
            HtLynel_PoseBlendFrom(this);
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_JUMP);
        }
        this->actor.world.pos.x = cx + dx * D_JUMP_R * k;
        this->actor.world.pos.z = cz + dz * D_JUMP_R * k;
        this->actor.world.pos.y = y0 + (floorY - y0) * k + 90.0f * sinf(k * M_PI);
        if (t == D_JUMP + LYNEL_JUMP_DOWN) {
            Vec3f p = this->actor.world.pos;

            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_LAND);
            Actor_SpawnFloorDustRing(play, &this->actor, &p, 60.0f, 10, 6.0f, 400, 12, true);
        }
        HtLynel_WritePose(this, gHtLynelJump[f]);
    }
    if ((t >= D_GALLOP) && !this->dead) {
        // galloping off out of the hall, through its door
        f32 r;

        if (t == D_GALLOP) {
            HtLynel_HorseAnim(this, (AnimationHeader*)gHorseGanonGallopingAnim, 1.2f, ANIMMODE_LOOP, -4.0f);
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
        }
        this->actor.world.pos.x += dx * D_GALLOP_SPEED;
        this->actor.world.pos.z += dz * D_GALLOP_SPEED;
        this->actor.world.pos.y = floorY;
        if ((t % 6) == 0) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN);
        }
        SkelAnime_Update(&this->skin.skelAnime);
        r = sqrtf(SQ(this->actor.world.pos.x - cx) + SQ(this->actor.world.pos.z - cz));
        if (r > D_DOOR_R) {
            this->dead = true; // (gone through the door)
            this->actTimer = 8;  // (the door coming down after it shortly)
        }
    }
    // the hall's door: up for it as it comes, down behind it
    if (t == D_GALLOP) {
        Actor* a;

        this->outroDoor = NULL;
        for (a = play->actorCtx.actorLists[ACTORCAT_DOOR].head; a != NULL; a = a->next) {
            if ((a->id == ACTOR_DOOR_SHUTTER) &&
                (SQ(a->world.pos.x - (cx + dx * 1500.0f)) + SQ(a->world.pos.z - (cz + dz * 1500.0f)) < SQ(250.0f))) {
                this->outroDoor = a;
            }
        }
        this->doorState = 0;
    }
    if ((this->outroDoor != NULL) && (t >= D_DOOR_OPEN)) {
        Actor* door = this->outroDoor;

        if (this->doorState == 0) {
            Audio_PlayActorSound2(door, NA_SE_EV_SLIDE_DOOR_OPEN);
            door->velocity.y = 0.0f;
            this->doorState = 1;
        }
        if (this->doorState == 1) {
            Math_StepToF(&door->velocity.y, 15.0f, 3.0f);
            Math_StepToF(&door->world.pos.y, door->home.pos.y + 200.0f, door->velocity.y);
            if (this->dead && (--this->actTimer <= 0)) {
                Audio_PlayActorSound2(door, NA_SE_EV_SLIDE_DOOR_CLOSE);
                door->velocity.y = 0.0f;
                this->doorState = 2;
            }
        } else if (this->doorState == 2) {
            Math_StepToF(&door->velocity.y, 20.0f, 8.0f);
            if (Math_StepToF(&door->world.pos.y, door->home.pos.y, door->velocity.y)) {
                Audio_PlayActorSound2(door, NA_SE_EV_STONE_BOUND);
                Actor_SpawnFloorDustRing(play, door, &door->world.pos, 45.0f, 10, 8.0f, 500, 10, false);
                HtLynel_P3Quake(this, play, 2, 10);
                this->doorState = 3;
            }
        }
    }
    if (t == D_WARP) {
        // the blue light (DoorWarp1: home - HtLynel points it at Hyrule Field)
        Actor_Spawn(&play->actorCtx, play, ACTOR_DOOR_WARP1, cx, ringY, cz, 0, 0, 0, -1 /* WARP_DUNGEON_ADULT */);
    }

    // the camera
    {
        f32 lx = this->linkSpot.x - cx;
        f32 lz = this->linkSpot.z - cz;
        f32 ld = sqrtf(SQ(lx) + SQ(lz));
        Vec3f* at = &this->camAt;
        Vec3f* eye = &this->camEye;

        if (ld > 1.0f) {
            lx /= ld;
            lz /= ld;
        }
        if (t < D_BURN) {
            // from the ring, by Link: the last blow, the slump
            at->x = cx;
            at->y = ringY + 110.0f;
            at->z = cz;
            eye->x = cx + lx * 180.0f + lz * 80.0f;
            eye->y = ringY + 100.0f;
            eye->z = cz + lz * 180.0f - lx * 80.0f;
        } else if (t < D_STAND) {
            // in close on the curse burning off its chest
            at->x = (this->tform > 0.0f) ? this->chestW.x : this->actor.world.pos.x;
            at->y = (this->tform > 0.0f) ? (this->chestW.y + 40.0f) : (this->actor.world.pos.y + 90.0f);
            at->z = (this->tform > 0.0f) ? this->chestW.z : this->actor.world.pos.z;
            eye->x = cx + lx * 150.0f - lz * 90.0f;
            eye->y = ringY + 70.0f;
            eye->z = cz + lz * 150.0f + lx * 90.0f;
        } else if (t < D_JUMP) {
            // the black horse, getting up and rearing - from low, on the ring
            at->x = this->actor.world.pos.x;
            at->y = this->actor.world.pos.y + 100.0f;
            at->z = this->actor.world.pos.z;
            eye->x = cx + lx * 190.0f + lz * 60.0f;
            eye->y = ringY + 40.0f;
            eye->z = cz + lz * 190.0f - lx * 60.0f;
        } else if (t < D_WARP) {
            // high over the middle: it leaps out and gallops off for the door
            Vec3f want = this->actor.world.pos;

            if (this->dead && (this->outroDoor != NULL)) {
                want = this->outroDoor->home.pos; // (on the door, coming down behind it)
            }
            want.y += 70.0f;
            Math_ApproachF(&at->x, want.x, 0.4f, 80.0f);
            Math_ApproachF(&at->y, want.y, 0.4f, 80.0f);
            Math_ApproachF(&at->z, want.z, 0.4f, 80.0f);
            eye->x = cx - dx * 120.0f;
            eye->y = floorY + 260.0f;
            eye->z = cz - dz * 120.0f;
        } else {
            // the disc, and on it the blue light - from high over it (from the side, the pit's rim hid it)
            at->x = cx;
            at->y = ringY + 20.0f;
            at->z = cz;
            eye->x = cx + lx * 150.0f;
            eye->y = ringY + 560.0f;
            eye->z = cz + lz * 150.0f;
        }
        Play_CameraSetFov(play, this->subCamId, 60.0f);
        Play_CameraSetAtEye(play, this->subCamId, at, eye);
    }
    if (++this->csTimer >= D_END) {
        Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);

        cam->eye = this->camEye;
        cam->eyeNext = this->camEye;
        cam->at = this->camAt;
        func_800C08AC(play, this->subCamId, 0);
        this->subCamId = 0;
        func_80064534(play, &play->csCtx);
        Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
        this->dead = true;
        this->form = HT_LYNEL_DEAD;
    }
}

// dead: the warp's way home goes to Hyrule Field (its own scene table knows nothing of the tomb); the red ebbs away
static void HtLynel_FadeHeat(HtLynel* this, PlayState* play) {
    s32 i;

    if (this->heatFade && (this->heat > 0.0f)) {
        Math_StepToF(&this->heat, 0.0f, 0.01f);
        if (this->heat <= 0.0f) {
            // (the light back as it was; the water stays gone)
            for (i = 0; i < 3; i++) {
                play->envCtx.adjAmbientColor[i] = play->envCtx.adjLight1Color[i] = play->envCtx.adjFogColor[i] = 0;
            }
            play->envCtx.fillScreen = false;
        }
    }
}

static void HtLynel_UpdateDead(HtLynel* this, PlayState* play) {
    // (the blue warp: to Hyrule Field, down onto the shadow pad by the tomb's culvert - HtShadowPad.c)
    play->nextEntranceIndex = ENTR_HYRULE_FIELD_ON_BRIDGE_SPAWN;
    HtShadowPad_ArriveByWarp();
}

void HtLynel_Update(Actor* thisx, PlayState* play) {
    HtLynel* this = (HtLynel*)thisx;

    // (the plaque by the horse stands as long as the anchor and its chain are drawn)
    sChainShown = (this->form == HT_LYNEL_CHAINED) || (this->form == HT_LYNEL_CUTSCENE);
    // (the plaque gone in the fade to black, once the screen is fully dark: in the intro's last shot it stood in the
    // way)
    sEpitaphShown = (this->form == HT_LYNEL_CHAINED) ||
                    ((this->form == HT_LYNEL_CUTSCENE) && (this->csTimer < CS_BLAST - 2));
    if (sFireSfxOn && !((this->form == HT_LYNEL_PHASE3) && (this->action == P3_FIRE))) {
        sFireSfxOn = false;
        HtLynel_StopLoop(NA_SE_EN_DODO_J_FIRE);
        HtLynel_StopLoop(NA_SE_EN_DODO_J_BREATH);
    }

    if (sOcarinaRestriction < 0) {
        sOcarinaRestriction = play->interfaceCtx.restrictions.ocarina;
    }
    play->interfaceCtx.restrictions.ocarina = 1;

    if ((this->form == HT_LYNEL_FORM) || (this->form == HT_LYNEL_PHASE2)) {
        HtLynel_UpdatePots(this, play);
    }
    HtLynel_UpdateHeat(this, play);
    HtLynel_FadeHeat(this, play);

    if (this->form == HT_LYNEL_PHASE3) {
        // (its chest, well above the ring's rim: lower, the rim's collision cuts Link's line to it - no Z-target)
        this->actor.focus.pos = this->actor.world.pos;
        this->actor.focus.pos.y += 230.0f;
        this->actor.naviEnemyId = HT_LYNEL_SWORD_TEXT - 0x600;
    }
    if ((this->form == HT_LYNEL_DEATH) || (this->form == HT_LYNEL_DEAD)) {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    }
    if ((this->form == HT_LYNEL_FORM) || (this->form == HT_LYNEL_PHASE2)) {
        // (Z-targeting and Navi aim at its chest - not the chain's anchor any more)
        this->actor.focus.pos = this->actor.world.pos;
        this->actor.focus.pos.y += 150.0f;
        // Navi's C-up: what to do now (stuck: the Iai at its flank; caged: the fire and the charge)
        this->actor.naviEnemyId = ((this->form == HT_LYNEL_PHASE2) ? HT_LYNEL_CAGE_TEXT
                                   : (this->action == P1_STUCK)    ? HT_LYNEL_STUCK_TEXT
                                                                   : HT_LYNEL_HINT_TEXT) -
                                  0x600;
    }

    if (this->upperBlend > 0) {
        this->upperBlend--;
    }
    if (!this->dead) {
        HtLynel_UpdateBody(this, play);
    }

    if (this->form == HT_LYNEL_CHAINED) {
        HtLynel_UpdateChained(this, play);
        this->actor.shape.rot.y = this->actor.world.rot.y;
        return;
    }
    HtLynel_UpdateSink(this, play);
    if (this->form == HT_LYNEL_CUTSCENE) {
        HtLynel_UpdateCutscene(this, play);
        this->actor.shape.rot.y = this->actor.world.rot.y;
        return;
    }
    if (this->form == HT_LYNEL_TRANSITION) {
        HtLynel_UpdateTransition(this, play);
        this->actor.shape.rot.y = this->actor.world.rot.y;
        return;
    }
    if (this->form == HT_LYNEL_TRANS3) {
        HtLynel_UpdateTransition3(this, play);
        this->actor.shape.rot.y = this->actor.world.rot.y;
        return;
    }
    if (this->form == HT_LYNEL_PHASE3) {
        HtLynel_UpdatePhase3(this, play);
        this->actor.shape.rot.y = this->actor.world.rot.y;
        return;
    }
    if (this->form == HT_LYNEL_DEATH) {
        HtLynel_UpdateDeath(this, play);
        if (this->csTimer >= D_WARP) {
            HtLynel_UpdateDead(this, play);
        }
        this->actor.shape.rot.y = this->actor.world.rot.y;
        return;
    }
    if (this->form == HT_LYNEL_DEAD) {
        HtLynel_UpdateDead(this, play);
        return;
    }
    if (this->form == HT_LYNEL_PHASE2) {
        HtLynel_UpdatePhase2(this, play);
    }
    HtLynel_UpdatePhase1(this, play);
    HtLynel_UpdateWave(this, play);
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static s32 HtLynel_OverrideLimbDraw(Actor* thisx, PlayState* play, s32 limbIndex, Skin* skin) {
    if (((HtLynel*)thisx)->tform < 1.0f) {
        return true; // the horse (or still becoming the Lynel): all of it
    }
    return (limbIndex != LYNEL_LIMB_HEAD_A) && (limbIndex != LYNEL_LIMB_HEAD_B);
}

// the anchor in the pedestal, and (till the Iai) the chain from it to the horse's fetlock: the chain pieces (100
// long, along +y) laid end to end along the line
static void HtLynel_DrawChain(HtLynel* this, PlayState* play) {
    Vec3f d;
    f32 len, yaw, pitch;
    s32 n, i;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // (a hair above the pedestal's top: its plate's underside lay in the top's plane and flickered against it)
    Matrix_Translate(this->anchor.x, this->anchor.y + 1.5f, this->anchor.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->actor.home.rot.y), MTXMODE_APPLY); // (set in the pedestal: it never turns)
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_LYNEL_ANCHOR_DL);
    if (this->form == HT_LYNEL_CHAINED) {
        Vec3f a = this->anchor;

        a.y += 30.0f; // from the anchor's staple
        d.x = this->chainEnd.x - a.x;
        d.y = this->chainEnd.y - a.y;
        d.z = this->chainEnd.z - a.z;
        len = sqrtf(SQ(d.x) + SQ(d.y) + SQ(d.z));
        if (len > 1.0f) {
            yaw = Math_FAtan2F(d.x, d.z);
            pitch = acosf(CLAMP(d.y / len, -1.0f, 1.0f));
            n = (s32)(len / 90.0f) + 1;
            // the hookshot's chain (3D links: no turning to the camera), along the line, thick and heavy
            (void)n;
            Matrix_Translate(a.x, a.y, a.z, MTXMODE_NEW);
            Matrix_RotateY(yaw, MTXMODE_APPLY);
            Matrix_RotateX(Math_FAtan2F(-d.y, sqrtf(SQ(d.x) + SQ(d.z))), MTXMODE_APPLY);
            Matrix_Scale(0.045f, 0.045f, len * 0.01f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_LYNEL_HOOK_CHAIN_DL);
            if (this->chainFreeze > 0) {
                // the ice running out along it from where the Iai struck, both ways
                f32 q = CLAMP((1.0f - this->chainFreeze / (f32)LYNEL_CHAIN_FREEZE) / 0.8f, 0.0f, 1.0f);
                Vec3f hit = { a.x + d.x * this->chainHitT, a.y + d.y * this->chainHitT, a.z + d.z * this->chainHitT };
                Vec3f toA = { hit.x + (a.x - hit.x) * q, hit.y + (a.y - hit.y) * q, hit.z + (a.z - hit.z) * q };
                Vec3f toB = { hit.x + (this->chainEnd.x - hit.x) * q, hit.y + (this->chainEnd.y - hit.y) * q,
                              hit.z + (this->chainEnd.z - hit.z) * q };

                HtIce_DrawSleeve(play, &hit, &toA, 9.0f, 190);
                HtIce_DrawSleeve(play, &hit, &toB, 9.0f, 190);
            }
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// the scimitars' edges in their hands' frames (model units): the tip, and a little out from the hilt
#define LYNEL_PART_L_HAND 5
#define LYNEL_PART_R_HAND 9
static Vec3f sBladeTip[2] = { { -606.0f, -639.0f, 9982.0f }, { -385.0f, -527.0f, -9992.0f } };
static Vec3f sBladeBase[2] = { { -485.0f, -651.0f, 1883.0f }, { -439.0f, -633.0f, -1893.0f } };

// is it cutting with its swords now? (its trails)
static s32 HtLynel_SwordsCutting(HtLynel* this) {
    if (this->form == HT_LYNEL_PHASE3) {
        return ((this->action == P3_STRIKE) && (this->upperFrame >= 8) && (this->upperFrame <= 24)) ||
               ((this->action == P3_XOUT) && (this->upperFrame <= 8)) ||
               false; // (the whirl: its swirl instead of the trails)
    }
    if ((this->form != HT_LYNEL_FORM) && (this->form != HT_LYNEL_PHASE2)) {
        return false;
    }
    switch (this->action) {
        case P1_SLASH:
        case P1_THROW:
            return (this->upperFrame >= 8) && (this->upperFrame <= 26);
        case P2_CUT:
            return this->upperFrame <= 12;
        default:
            return false;
    }
}

// the chest limb's model-space rotation in the idle animation's first frame (the pose the upper body was modelled
// on: lynel_export.py); the upper body turns with the chest's change from it
static const f32 sChestRot0[3][3] = {
    { 0.0f, 0.0f, 1.0f },
    { 0.292394f, -0.956298f, 0.0f },
    { 0.956298f, 0.292394f, 0.0f },
};

// one material's triangles: its corners are kept in their parts' own frames, each batch loaded part by part under
// that part's matrix (made through the matrix stack each frame: the game's frame interpolation smooths them)
static Gfx* HtLynel_DrawGroup(Gfx* gfx, const HtLynelGroup* grp, Mtx** parts) {
    const HtLynelOp* op;

    gSPDisplayList(gfx++, (Gfx*)grp->material);
    for (op = grp->ops; op->op != 3; op++) {
        if (op->op == 0) {
            gSPMatrix(gfx++, parts[op->a], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        } else if (op->op == 1) {
            gSPVertex(gfx++, (uintptr_t)&grp->vtx[op->a], op->b, op->c);
        } else {
            gSP1Triangle(gfx++, op->a, op->b, op->c, 0);
        }
    }
    return gfx;
}

static void HtLynel_PostDraw(Actor* thisx, PlayState* play, Skin* skin) {
    static Vec3f zero = { 0.0f, 0.0f, 0.0f };
    HtLynel* this = (HtLynel*)thisx;

    if (this->form == HT_LYNEL_CHAINED) {
        Skin_GetLimbPos(skin, LYNEL_CHAIN_LIMB, &zero, &this->chainEnd); // (the chain's far end)
    }
    // (its own interpolation child: drawn among the skin's limbs, whose count can shift - paired with the wrong
    // matrix from one frame to the next, the anchor shook)
    if ((this->form == HT_LYNEL_CHAINED) || (this->form == HT_LYNEL_CUTSCENE)) {
        // (the anchor, and the chain till it's cut: gone once the fight's on - it's under the sunk pedestal)
        FrameInterpolation_RecordOpenChild(&this->anchor, 0);
        HtLynel_DrawChain(this, play);
        FrameInterpolation_RecordCloseChild();
    }
    if (this->tform <= 0.0f) {
        return; // the horse: no upper body
    }
    Vec3f chest;
    MtxF* r = &gSkinLimbMatrices[LYNEL_LIMB_CHEST];
    f32 rt[3][3];
    MtxF d;
    s32 i, j, k;
    f32 s = HT_LYNEL_SCALE * this->tform; // (growing out of the chest as it changes)
    f32 pulse;

    // the chest's rotation now, against its rotation in the modelled pose: delta = R(t) * R0^T
    rt[0][0] = r->xx; rt[0][1] = r->xy; rt[0][2] = r->xz;
    rt[1][0] = r->yx; rt[1][1] = r->yy; rt[1][2] = r->yz;
    rt[2][0] = r->zx; rt[2][1] = r->zy; rt[2][2] = r->zz;
    SkinMatrix_Clear(&d);
    {
        f32 m[3][3];
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                m[i][j] = 0.0f;
                for (k = 0; k < 3; k++) {
                    m[i][j] += rt[i][k] * sChestRot0[j][k];
                }
            }
        }
        d.xx = m[0][0]; d.xy = m[0][1]; d.xz = m[0][2];
        d.yx = m[1][0]; d.yy = m[1][1]; d.yz = m[1][2];
        d.zx = m[2][0]; d.zy = m[2][1]; d.zz = m[2][2];
    }

    // the upper body, at the horse's chest limb, turning with it
    Skin_GetLimbPos(skin, LYNEL_LIMB_CHEST, &zero, &chest);
    this->chestW = chest;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(chest.x, chest.y, chest.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(thisx->shape.rot.y), MTXMODE_APPLY);
    Matrix_Mult(&d, MTXMODE_APPLY);
    Matrix_Scale(s, s, s, MTXMODE_APPLY);
    {
        const f32(*frame)[12] = gHtLynelAnims[this->upperAnim][CLAMP(this->upperFrame, 0,
                                                                    sLynelAnimFrames[this->upperAnim] - 1)];
        f32 blended[HT_LYNEL_PARTS][12];
        Mtx* parts[HT_LYNEL_PARTS];

        if (this->upperBlend > 0) {
            // (blending over from the last animation: its pose lerped into this one's)
            const f32(*from)[12] = gHtLynelAnims[this->upperFromAnim][this->upperFromFrame];
            f32 k = 1.0f - (f32)this->upperBlend / (HT_LYNEL_UPPER_BLEND + 1);

            for (i = 0; i < HT_LYNEL_PARTS; i++) {
                for (j = 0; j < 12; j++) {
                    blended[i][j] = from[i][j] + (frame[i][j] - from[i][j]) * k;
                }
            }
            frame = (const f32(*)[12])blended;
        }
        Vec3f tips[2], bases[2];

        // each part's matrix: the upper body's frame times the part's pose
        for (i = 0; i < HT_LYNEL_PARTS; i++) {
            MtxF a;

            a.xx = frame[i][0]; a.xy = frame[i][1]; a.xz = frame[i][2]; a.xw = frame[i][3];
            a.yx = frame[i][4]; a.yy = frame[i][5]; a.yz = frame[i][6]; a.yw = frame[i][7];
            a.zx = frame[i][8]; a.zy = frame[i][9]; a.zz = frame[i][10]; a.zw = frame[i][11];
            a.wx = a.wy = a.wz = 0.0f;
            a.ww = 1.0f;
            Matrix_Push();
            Matrix_Mult(&a, MTXMODE_APPLY);
            parts[i] = MATRIX_NEWMTX(play->state.gfxCtx);
            if ((i == LYNEL_PART_L_HAND) || (i == LYNEL_PART_R_HAND)) {
                // its sword's edge, tip to near the hilt (the hand part's frame: lynel_export.py blade_tip_local)
                s32 k = (i == LYNEL_PART_L_HAND) ? 0 : 1;

                Matrix_MultVec3f(&sBladeTip[k], &tips[k]);
                Matrix_MultVec3f(&sBladeBase[k], &bases[k]);
                this->bladeTipPrev[k] = this->bladeTipW[k];
                this->bladeBasePrev[k] = this->bladeBaseW[k];
                this->bladeTipW[k] = tips[k];
                this->bladeBaseW[k] = bases[k];
            }
            if (i == 1) {
                // its mouth (the head part's frame, model units: the snout's underside, lynel_export.py)
                static Vec3f mouth = { 3723.0f, 957.0f, -145.0f };

                Matrix_MultVec3f(&mouth, &this->headW); // (the fire breath's source)
            }
            Matrix_Pop();
        }
        // the swords' trails, while it cuts with them
        if (HtLynel_SwordsCutting(this)) {
            EffectBlure_AddVertex(Effect_GetByIndex(this->blure[0]), &tips[0], &bases[0]);
            EffectBlure_AddVertex(Effect_GetByIndex(this->blure[1]), &tips[1], &bases[1]);
            this->trailOn = true;
        } else if (this->trailOn) {
            EffectBlure_AddSpace(Effect_GetByIndex(this->blure[0]));
            EffectBlure_AddSpace(Effect_GetByIndex(this->blure[1]));
            this->trailOn = false;
        }
        for (i = 0; i < gHtLynelGroupCount; i++) {
            POLY_OPA_DISP = HtLynel_DrawGroup(POLY_OPA_DISP, &gHtLynelGroups[i], parts);
        }
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_LYNEL_REVERT_DL);

        // the eyes' glow, pulsing
        this->glowTimer++;
        pulse = 0.5f + 0.5f * Math_SinS(this->glowTimer * 0x500);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 40 + (s32)(40.0f * pulse), 10, 150 + (s32)(100.0f * pulse));
        POLY_XLU_DISP = HtLynel_DrawGroup(POLY_XLU_DISP, &gHtLynelHaloGroup, parts);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)HT_LYNEL_REVERT_DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// the sword wave: Link's spin-attack swirl (gameplay_keep), purple, stood on edge facing its flight and slanted like
// the cut that threw it, spinning as it goes
static void HtLynel_DrawOneCrescent(HtLynel* this, PlayState* play, HtLynelWave* w, s32 slant, f32 scale);

// the X: two long blades of dark force crossed, square to its flight - each a band bowed forward at its middle, bright
// along its spine and fading to nothing at its edges and tips (built once: unit length, scaled when drawn)
#define X_N 11 // samples along a blade
#define X_FLAMES 23 // orbs along each arm of the X
#define X_BAND 17   // ...and along its band on the floor
#define HT_GANON_ORB_MAT_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonLightOrbMaterialDL" // Ganon's light orb (+-7 units)
#define HT_GANON_ORB_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonLightOrbModelDL"
static Vtx sXVtx[3 * X_N];
static Gfx sXDL[3 * X_N + 8];
static s32 sXBuilt = false;

static void HtLynel_BuildX(void) {
    Gfx* g = sXDL;
    s32 i, k;

    for (i = 0; i < X_N; i++) {
        f32 u = (f32)i / (X_N - 1) * 2.0f - 1.0f;          // -1 .. 1 along the blade
        f32 bow = 0.30f * (1.0f - u * u);                   // (bowed forward in the middle)
        f32 w = 0.07f * (1.0f - 0.85f * u * u);             // (tapering to its tips)
        f32 tipFade = 1.0f - u * u * u * u;

        for (k = 0; k < 3; k++) {
            Vtx* v = &sXVtx[i * 3 + k];
            f32 off = (k - 1) * w;
            u8 a = (k == 1) ? (u8)(255 * tipFade) : 0;
            u8 c = (k == 1) ? 255 : 0;

            v->v.ob[0] = (s16)(u * 1000.0f);
            v->v.ob[1] = (s16)(off * 1000.0f);
            v->v.ob[2] = (s16)(bow * 1000.0f);
            v->v.flag = 0;
            v->v.tc[0] = v->v.tc[1] = 0;
            v->v.cn[0] = v->v.cn[1] = v->v.cn[2] = c;
            v->v.cn[3] = a;
        }
    }
    gSPVertex(g++, (uintptr_t)sXVtx, 3 * X_N, 0);
    for (i = 0; i < X_N - 1; i++) {
        s32 a = i * 3, b = (i + 1) * 3;

        gSP2Triangles(g++, a, b, b + 1, 0, a, b + 1, a + 1, 0);
        gSP2Triangles(g++, a + 1, b + 1, b + 2, 0, a + 1, b + 2, a + 2, 0);
    }
    gSPEndDisplayList(g++);
    sXBuilt = true;
}

static void HtLynel_DrawXBlade(PlayState* play, Vec3f* at, s16 yaw, f32 roll, f32 len, f32 thick, u8 r, u8 g, u8 b,
                               u8 a) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, r, g, b, a);
    Matrix_Translate(at->x, at->y, at->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    Matrix_RotateZ(roll, MTXMODE_APPLY);
    Matrix_Scale(len / 1000.0f, len / 1000.0f * thick, len / 1000.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sXDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// the X (drawn as a triangle: its arms' lower halves): a wall of violet energy (Ganon's light orbs, BossGanon2's) - two
// thick sloping sides over a solid band along the floor, so there's no slipping under or between: the only way out is aside. Each orb turning on itself, flickering
static void HtLynel_DrawXOrb(PlayState* play, Vec3f* p, f32 sc, f32 spin) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(p->x, p->y, p->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
    Matrix_RotateZ(spin, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)HT_GANON_ORB_DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void HtLynel_DrawX(HtLynel* this, PlayState* play, HtLynelWave* w) {
    u32 fr = play->gameplayFrames;
    f32 fx = Math_SinS(w->yaw);
    f32 fz = Math_CosS(w->yaw);
    s32 arm, k;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 250, 215, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 150, 0, 255, 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)HT_GANON_ORB_MAT_DL);
    CLOSE_DISPS(play->state.gfxCtx);
    // the arms
    for (arm = -1; arm <= 1; arm += 2) {
        for (k = 0; k < X_FLAMES; k++) {
            f32 t = (f32)k / (X_FLAMES - 1) * 2.0f - 1.0f;   // along the arm, -1 .. 1
            f32 bow = 40.0f * (1.0f - t * t);                // (bowed forward at its middle)
            f32 flick = 0.88f + 0.12f * sinf(fr * 0.7f + k * 1.9f + arm * 2.0f);
            Vec3f p = { w->pos.x + fz * t * P2_X_HALF + fx * bow, w->pos.y + P2_X_Y + arm * t * P2_X_HALF * P2_X_RISE,
                        w->pos.z - fx * t * P2_X_HALF + fz * bow };

            if ((arm * t > 0.0f) || (p.y < w->pos.y + 10.0f)) {
                continue; // (only the lower halves: with the band, a triangle - and nothing sunk in the floor)
            }
            FrameInterpolation_RecordOpenChild(w, 100 + (arm + 1) * 40 + k);
            HtLynel_DrawXOrb(play, &p, 12.0f * flick, (fr * 0.4f + k) * arm);
            FrameInterpolation_RecordCloseChild();
        }
    }
    // the band along the floor, the X's full width
    for (k = 0; k < X_BAND; k++) {
        f32 t = (f32)k / (X_BAND - 1) * 2.0f - 1.0f;
        f32 bow = 40.0f * (1.0f - t * t);
        f32 flick = 0.88f + 0.12f * sinf(fr * 0.8f + k * 2.3f);
        Vec3f p = { w->pos.x + fz * t * P2_X_HALF + fx * bow, w->pos.y + 38.0f, w->pos.z - fx * t * P2_X_HALF + fz * bow };

        FrameInterpolation_RecordOpenChild(w, 200 + k);
        HtLynel_DrawXOrb(play, &p, 10.0f * flick, fr * 0.5f + k);
        FrameInterpolation_RecordCloseChild();
    }
    {
        // the crossing: a big orb, two layers turning against each other
        Vec3f p = { w->pos.x + fx * 40.0f, w->pos.y + P2_X_Y, w->pos.z + fz * 40.0f };
        f32 sc = 20.0f * (0.9f + 0.1f * sinf(fr * 0.9f));

        FrameInterpolation_RecordOpenChild(w, 99);
        HtLynel_DrawXOrb(play, &p, sc, fr * 0.3f);
        HtLynel_DrawXOrb(play, &p, sc * 0.8f, fr * -0.45f);
        FrameInterpolation_RecordCloseChild();
    }
}

static void HtLynel_DrawOneWave(HtLynel* this, PlayState* play, HtLynelWave* w) {
    if (!w->on) {
        return;
    }
    if (w->slant == 2) {
        FrameInterpolation_RecordOpenChild(w, 1);
        HtLynel_DrawX(this, play, w);
        FrameInterpolation_RecordCloseChild();
    } else {
        FrameInterpolation_RecordOpenChild(w, 0);
        HtLynel_DrawOneCrescent(this, play, w, w->slant, 1.0f);
        FrameInterpolation_RecordCloseChild();
    }
}

static void HtLynel_DrawOneCrescent(HtLynel* this, PlayState* play, HtLynelWave* w, s32 slant, f32 scale) {
    u32 fr = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(w->pos.x, w->pos.y + 60.0f, w->pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(w->yaw), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(slant * 0x1800), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(0x4000), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD((s16)(fr * 0x1800)), MTXMODE_APPLY); // (the swirl spinning)
    Matrix_Scale(0.075f * scale, 0.075f * scale, 0.075f * scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0xFF - ((fr * 30) & 0xFF), 0, 0x40, 0x20, 1,
                                  0xFF - ((fr * 20) & 0xFF), 0, 8, 8, -30, 0, -20, 0));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 225, 150, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 110, 0, 170, 128);
    gSPDisplayList(POLY_XLU_DISP++, gSpinAttack1DL);
    gSPDisplayList(POLY_XLU_DISP++, gSpinAttack2DL);
    // a second, larger and fainter, a little behind: the wave's wake
    Matrix_Scale(1.35f, 1.35f, 1.35f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 160, 70, 220, 150);
    gSPDisplayList(POLY_XLU_DISP++, gSpinAttack3DL);
    gSPDisplayList(POLY_XLU_DISP++, gSpinAttack4DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// the whirl's swirl: Link's spin attack's (gameplay_keep), purple, ring-wide, at the blades' height
static void HtLynel_DrawSwirl(HtLynel* this, PlayState* play) {
    f32 k = P3_SWIRL_R / 1250.0f; // (the swirl's model: ~1250 units round at scale 1)
    f32 a = CLAMP(this->swirl, 0.0f, 1.0f);
    s32 sc = (s32)(this->swirlScroll * 2.0f);
    Vec3f c = this->chestW;

    if (a <= 0.0f) {
        return;
    }
    c.x += Math_SinS(this->actor.shape.rot.y) * 55.0f; // (the blades turn about its waist, a little ahead)
    c.z += Math_CosS(this->actor.shape.rot.y) * 55.0f;
    OPEN_DISPS(play->state.gfxCtx);
    FrameInterpolation_RecordOpenChild(&this->swirl, 0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(c.x, HtLynel_RingY(this) + 45.0f, c.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD((s16)(this->actor.shape.rot.y + this->swirlScroll * 0x1800)), MTXMODE_APPLY);
    Matrix_Scale(k, k * 0.8f, k, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0xFF - ((sc * 30) & 0xFF), 0, 0x40, 0x20, 1,
                                  0xFF - ((sc * 20) & 0xFF), 0, 8, 8, -30, 0, -20, 0));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 225, 150, 255, (u8)(a * 255));
    gDPSetEnvColor(POLY_XLU_DISP++, 110, 0, 170, 128);
    gSPDisplayList(POLY_XLU_DISP++, gSpinAttack1DL);
    gSPDisplayList(POLY_XLU_DISP++, gSpinAttack2DL);
    FrameInterpolation_RecordCloseChild();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void HtLynel_DrawWave(HtLynel* this, PlayState* play) {
    s32 i;

    HtLynel_DrawRocks(this, play);
    HtLynel_DrawBreath(this, play);
    HtLynel_DrawSwirl(this, play);
    for (i = 0; i < ARRAY_COUNT(this->waves); i++) {
        HtLynel_DrawOneWave(this, play, &this->waves[i]);
    }
}

void HtLynel_Draw(Actor* thisx, PlayState* play) {
    HtLynel* this = (HtLynel*)thisx;
    SkinLimb** skeleton;
    MtxF* chest;
    s32 i;

    if (this->waterDrop < 20.0f) {
        // the arena's water (its own model, in the room's coordinates): sinking into the floor as it boils off
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        if (this->waterDrop <= 0.0f) {
            // (at rest: the plain identity, as the room's own meshes are drawn - nothing for the frame
            // interpolation to pair up and blend)
            gSPMatrix(POLY_XLU_DISP++, &gMtxClear, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        } else {
            Matrix_Translate(0.0f, -this->waterDrop, 0.0f, MTXMODE_NEW);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        }
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)HT_LYNEL_WATER_DL);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    if (this->dead) {
        return; // (gone)
    }

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Skin_ApplyAnimTransformations(&this->skin, gSkinLimbMatrices, &this->actor, true);
    // the neck and head fold away into the chest: their limbs shrunk to nothing at the chest limb, so the skin's
    // neck and head vertices all gather there (inside the upper body). (The horse keeps them.)
    chest = &gSkinLimbMatrices[LYNEL_LIMB_CHEST];
    // (while it changes, part way: shrinking toward the chest; the head's own parts with them)
    for (i = LYNEL_LIMB_NECK_FIRST; (this->tform > 0.0f) && (i <= LYNEL_LIMB_HEAD_B); i++) {
        MtxF* m = &gSkinLimbMatrices[i];
        f32 k = 1.0f - this->tform;

        m->xx *= k; m->yx *= k; m->zx *= k;
        m->xy *= k; m->yy *= k; m->zy *= k;
        m->xz *= k; m->yz *= k; m->zz *= k;
        m->xw = chest->xw + (m->xw - chest->xw) * k;
        m->yw = chest->yw + (m->yw - chest->yw) * k;
        m->zw = chest->zw + (m->zw - chest->zw) * k;
    }
    // the skinned limbs' vertices, from those matrices (the custom-transforms draw below would skip this - it left
    // the horse's body unbuilt, its rump missing)
    skeleton = SEGMENTED_TO_VIRTUAL(this->skin.skeletonHeader->segment);
    for (i = 0; i < this->skin.skeletonHeader->limbCount; i++) {
        SkinLimb* limb = SEGMENTED_TO_VIRTUAL(skeleton[i]);

        if (limb->segmentType == SKIN_LIMB_TYPE_ANIMATED) {
            Skin_ApplyLimbModifications(play->state.gfxCtx, &this->skin, i, false);
        }
    }
    func_800A63CC(&this->actor, play, &this->skin, HtLynel_PostDraw, HtLynel_OverrideLimbDraw, true, false,
                  SKIN_DRAW_FLAG_CUSTOM_TRANSFORMS);
    HtLynel_DrawWave(this, play);
}
