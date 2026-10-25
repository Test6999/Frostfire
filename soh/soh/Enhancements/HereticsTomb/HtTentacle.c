// Heretics Tomb: a lone Morpha water tentacle (see HtTentacle.h). Motion and drawing are Morpha's own (z_boss_mo.c:
// BossMo_Tentacle, BossMo_UpdateTent, BossMo_DrawTentacle), cut down to one tentacle without the nucleus.

#include "HtTentacle.h"
#include "HtBoat.h"
#include "HtArmorpha.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/object_mo/object_mo.h"
#include "soh/frame_interpolation.h"
#include <math.h>

enum {
    TENT_READY,
    TENT_SWING,
    TENT_ATTACK,
    TENT_CURL,
    TENT_GRAB,
    TENT_SHAKE,
    TENT_TOSS,   // shaken loose: it throws Link back onto the boat
    TENT_WAIT = 10,
    TENT_SPAWN,
    TENT_RETREAT = 101,
    TENT_DESPAWN,
    TENT_FROZEN = 150,
};

enum { SW_LAG_X, SW_SIZE_X, SW_RATE_X, SW_LAG_Z, SW_SIZE_Z, SW_RATE_Z, MAX_STRETCH };

#define TENT_SCALE 0.016f        // Morpha's is 0.01 (girth)
#define TENT_HEIGHT 1.8f         // ...and it stands 1.8 times as tall: from the water it reaches the boat's deck, 140 up
                                 // (Morpha's arena floor is only 50 above its water)
#define TENT_FREEZE_FRAMES 200   // frozen 10 s, then it thaws
#define TENT_TOSS_FRAMES 22      // the throw back aboard
#define HT_TENT_ARENA_GRAB_SEG 34 // Armorpha's hold Link up by the tip (Morpha's: 24, half way)
#define TENT_REACH 340.0f        // further than this Link is out of reach: it sinks and rises again

static s16 sCurlRot[41] = {
    0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  4, 8, 8, 8, 9, 9, 9,
    9, 9, 9, 12, 15, 15, 15, 15, 15, 15, 15, 20, 20, 20, 0, 0, 0, 0, 0, 0,
};
static s16 sGrabRot[41] = {
    0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, -5, -5, -5,
    0, 5, 10, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 0, 0, 0, 0,  0,
};
static s16 sAttackRot[41] = {
    0, 5, 6, 7, 8, 8, 7, 6, 6, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static f32 sTentWidth[41] = {
    3.56f, 3.25f, 2.96f, 2.69f, 2.44f, 2.21f, 2.0f, 1.81f, 1.64f, 1.49f, 1.36f, 1.25f, 1.16f, 1.09f,
    1.04f, 1.01f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  0.98f, 0.95f, 0.9f, 0.8f,  0.6f,  1.0f,  1.0f,  1.0f,  1.0f,
};
static const s16 sSphRadius[19] = { 0, 0, 30, 24, 22, 20, 18, 16, 14, 12, 10, 10, 10, 10, 10, 10, 10, 10, 10 };

static ColliderJntSphElementInit sElementsInit[19];
static ColliderJntSphInit sJntSphInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    19,
    sElementsInit,
};

static Gfx* sTentDLists[41] = {
    gMorphaTentaclePart0DL,  gMorphaTentaclePart1DL,  gMorphaTentaclePart2DL,  gMorphaTentaclePart3DL,
    gMorphaTentaclePart4DL,  gMorphaTentaclePart5DL,  gMorphaTentaclePart6DL,  gMorphaTentaclePart7DL,
    gMorphaTentaclePart8DL,  gMorphaTentaclePart9DL,  gMorphaTentaclePart10DL, gMorphaTentaclePart11DL,
    gMorphaTentaclePart12DL, gMorphaTentaclePart13DL, gMorphaTentaclePart14DL, gMorphaTentaclePart15DL,
    gMorphaTentaclePart16DL, gMorphaTentaclePart17DL, gMorphaTentaclePart18DL, gMorphaTentaclePart19DL,
    gMorphaTentaclePart20DL, gMorphaTentaclePart21DL, gMorphaTentaclePart22DL, gMorphaTentaclePart23DL,
    gMorphaTentaclePart24DL, gMorphaTentaclePart25DL, gMorphaTentaclePart26DL, gMorphaTentaclePart27DL,
    gMorphaTentaclePart28DL, gMorphaTentaclePart29DL, gMorphaTentaclePart30DL, gMorphaTentaclePart31DL,
    gMorphaTentaclePart32DL, gMorphaTentaclePart33DL, gMorphaTentaclePart34DL, gMorphaTentaclePart35DL,
    gMorphaTentaclePart36DL, gMorphaTentaclePart37DL, gMorphaTentaclePart38DL, gMorphaTentaclePart39DL,
    gMorphaTentaclePart40DL,
};

// (Morpha's own little random generator: the bubbles inside the tentacle are drawn in the same places every frame)
static s32 sSeed1, sSeed2, sSeed3;
static void Tent_InitRand(s32 a, s32 b, s32 c) {
    sSeed1 = a;
    sSeed2 = b;
    sSeed3 = c;
}
static f32 Tent_Rand(void) {
    f32 rand;

    sSeed1 = (sSeed1 * 171) % 30269;
    sSeed2 = (sSeed2 * 172) % 30307;
    sSeed3 = (sSeed3 * 170) % 30323;
    rand = (sSeed1 / 30269.0f) + (sSeed2 / 30307.0f) + (sSeed3 / 30323.0f);
    while (rand >= 1.0f) {
        rand -= 1.0f;
    }
    return fabsf(rand);
}

// Let go of, Link can't be caught again (by any tentacle) till he's been back on his feet a moment: thrown or flung
// through the air, another one snatched him out of it - no chance to land, let alone fight back
#define TENT_NO_GRAB_FRAMES 30
static s16 sNoGrab = 0;
static u32 sNoGrabFrame = 0xFFFFFFFF;

static void Tent_TickNoGrab(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (sNoGrabFrame == play->gameplayFrames) {
        return; // (once a frame, however many tentacles)
    }
    sNoGrabFrame = play->gameplayFrames;
    if ((sNoGrab > 0) && (player->actor.parent == NULL) &&
        ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER))) {
        sNoGrab--;
    }
}

static void Tent_ReleaseLink(HtTentacle* this, PlayState* play, s32 fling) {
    Player* player = GET_PLAYER(play);

    if (&this->actor == player->actor.parent) {
        sNoGrab = TENT_NO_GRAB_FRAMES;
        player->av2.actionVar2 = 0x65;
        player->actor.parent = NULL;
        player->csAction = 0;
        // (upright again: wrung by it, he was left leaning 45 degrees for good)
        player->actor.shape.rot.x = player->actor.shape.rot.z = 0;
        player->actor.world.rot.x = player->actor.world.rot.z = 0;
        if (fling) {
            Actor_SetPlayerKnockbackLarge(play, &this->actor, 20.0f, this->actor.shape.rot.y + 0x8000, 14.0f, 0);
        }
    }
}

// Link held up in its grip: never through a wall (it swung him through the ring's bank). Kept on the near side of
// any wall between the tentacle's stem and him, 30 clear of it.
static void Tent_KeepOutOfWalls(HtTentacle* this, PlayState* play, Vec3f* pos) {
    Vec3f from = { this->actor.world.pos.x, pos->y, this->actor.world.pos.z };

    if (this->actor.params & HT_TENTACLE_ARENA) {
        // (Armorpha's hold him up by the tip: tested from up the tentacle by him, not its stem in the water - the
        // walkway's edge between them dragged him down to the stem)
        from.x = this->tentPos[HT_TENT_ARENA_GRAB_SEG - 8].x;
        from.z = this->tentPos[HT_TENT_ARENA_GRAB_SEG - 8].z;
    }
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;

    if (BgCheck_EntityLineTest1(&play->colCtx, &from, pos, &hit, &poly, true, false, false, true, &bgId)) {
        f32 dx = hit.x - from.x, dz = hit.z - from.z;
        f32 d = sqrtf(SQ(dx) + SQ(dz));

        if (d > 30.0f) {
            pos->x = from.x + dx * (d - 30.0f) / d;
            pos->z = from.z + dz * (d - 30.0f) / d;
        } else {
            pos->x = from.x;
            pos->z = from.z;
        }
    }
}

static void Tent_ResetSwing(HtTentacle* this) {
    this->tentMaxAngle = 0.001f;
    this->tentSpeed = 0.0f;
    this->fwork[SW_RATE_X] = this->fwork[SW_RATE_Z] = this->fwork[SW_SIZE_X] = this->fwork[SW_SIZE_Z] = 0.0f;
}

// after a grab (thrown back aboard, or flung off): Armorpha's sink away as Morpha's do; the boat's just stand back up
// and sway, ready again - no going under and rising between goes
static void Tent_AfterMove(HtTentacle* this, s16 retreatTimer) {
    if (this->actor.params & HT_TENTACLE_ARENA) {
        this->state = TENT_RETREAT;
        this->timer = retreatTimer;
    } else {
        this->state = TENT_READY;
        this->timer = 45;
    }
}

s32 HtTentacle_IsLunging(Actor* thisx) {
    s16 st = ((HtTentacle*)thisx)->state;

    return (st == TENT_ATTACK) || (st == TENT_CURL) || (st == TENT_GRAB) || (st == TENT_SHAKE);
}

s32 HtTentacle_IsFrozen(Actor* thisx) {
    return ((HtTentacle*)thisx)->state == TENT_FROZEN;
}

s32 HtTentacle_IsUp(Actor* thisx) {
    return ((HtTentacle*)thisx)->baseAlpha > 100.0f;
}

s32 HtTentacle_IsLeaving(Actor* thisx) {
    s16 st = ((HtTentacle*)thisx)->state;

    return (st == TENT_RETREAT) || (st == TENT_DESPAWN);
}

// Armorpha's next tentacle: from under the water straight up (it whirled there first)
void HtTentacle_RiseNow(Actor* thisx) {
    HtTentacle* this = (HtTentacle*)thisx;

    if (this->state == TENT_WAIT) {
        this->timer = 0;
    }
}

void HtTentacle_Init(Actor* thisx, PlayState* play) {
    HtTentacle* this = (HtTentacle*)thisx;
    s32 i;

    for (i = 0; i < 19; i++) {
        ColliderJntSphElementInit* e = &sElementsInit[i];

        e->info.elemType = ELEMTYPE_UNK4;
        e->info.toucher.dmgFlags = 0xFFCFFFFF;
        e->info.toucher.effect = 0;
        e->info.toucher.damage = 0x10;
        e->info.bumper.dmgFlags = 0xFFCFFFFF;
        e->info.bumper.effect = 0;
        e->info.bumper.defense = 0;
        e->info.toucherFlags = TOUCH_ON | TOUCH_SFX_NORMAL;
        e->info.bumperFlags = BUMP_ON;
        e->info.ocElemFlags = OCELEM_ON;
        e->dim.limb = i;
        e->dim.modelSphere.center.x = e->dim.modelSphere.center.y = e->dim.modelSphere.center.z = 0;
        e->dim.modelSphere.radius = sSphRadius[i];
        e->dim.scale = 100;
    }
    // (Armorpha's: thinner and lower than the boat's - its upper half reaches out further as it lunges; its hit spheres
    // keep the boat's size, so the ice still finds it)
    Actor_SetScale(thisx, (thisx->params & HT_TENTACLE_ARENA) ? 0.012f : TENT_SCALE);
    this->heightMul = (thisx->params & HT_TENTACLE_ARENA) ? 0.8f : TENT_HEIGHT;
    this->coreIndex = -1;
    ActorShape_Init(&thisx->shape, 0.0f, NULL, 0.0f);
    thisx->targetMode = 5;
    thisx->naviEnemyId = HT_TENTACLE_NAVI_TEXT - 0x600; // Navi's C-up: text 0x0600 + id (Katana.cpp)
    thisx->gravity = 0.0f;
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    thisx->colChkInfo.health = 1;
    thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, thisx, &sJntSphInit, this->elements);
    this->tentMaxAngle = 1.0f;
    this->state = TENT_WAIT;
    this->timer = 10 + (s16)Rand_ZeroFloat(20.0f);
    thisx->shape.rot.y = thisx->yawTowardsPlayer;
}

void HtTentacle_Destroy(Actor* thisx, PlayState* play) {
    HtTentacle* this = (HtTentacle*)thisx;

    // (only while it holds him: at a void-out the room is torn down and Link may already be gone - touching him then
    // crashed the game)
    if ((this->state == TENT_GRAB) || (this->state == TENT_SHAKE) || (this->state == TENT_TOSS)) {
        Tent_ReleaseLink(this, play, false);
    }
    if ((this->actor.parent != NULL) && (this->actor.parent->id == HT_ACTOR_ARMORPHA) &&
        (this->actor.parent->update != NULL)) {
        HtArmorpha_OnTentacleGone(this->actor.parent, thisx, play);
    }
    Collider_DestroyJntSph(play, &this->collider);
}

// one step of the writhing (every pose but the attack and the grab): the segments sway about their swing
static void Tent_Sway(HtTentacle* this, f32 stretchTarget, f32 stretchStep, s32 count, f32 mul) {
    s16 i;

    for (i = 0; i < count; i++) {
        f32 sn = Math_SinS(((s16)this->fwork[SW_LAG_X] * i) + this->xSwing);
        f32 a = this->fwork[SW_SIZE_X] * (i * 0.025f * sn) * mul;
        f32 cs = Math_SinS(((s16)this->fwork[SW_LAG_Z] * i) + this->zSwing);
        f32 b = this->fwork[SW_SIZE_Z] * (i * 0.025f * cs) * mul;

        Math_ApproachF(&this->tentStretch[i].y, stretchTarget, 0.1f, stretchStep);
        Math_ApproachS(&this->tentRot[i].x, a, 1.0f / this->tentMaxAngle, this->tentSpeed);
        Math_ApproachS(&this->tentRot[i].z, b, 1.0f / this->tentMaxAngle, this->tentSpeed);
    }
}

static void Tent_Act(HtTentacle* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 st = this->state;
    s16 prev28 = 0, prev15;
    s16 i;

    if ((st == TENT_READY) || (st == TENT_RETREAT) || (st == TENT_SWING) || (st == TENT_SHAKE)) {
        f32 rx, lx, sx, rz, lz, sz, ra, sa;

        if (st == TENT_READY) {
            rx = 2000.0f; lx = 3000.0f; sx = 1000.0f; rz = 1500.0f; lz = 2500.0f; sz = 1000.0f; ra = 20.0f; sa = 30.0f;
        } else if (st == TENT_SWING) {
            rx = 2500.0f; lx = -1000.0f; sx = 3000.0f; rz = 1500.0f; lz = 2500.0f; sz = 0.0f; ra = 30.0f; sa = 60.0f;
            if (((this->sfxTimer % 16) == 0) && (this->timer < 30)) {
                Sfx_PlaySfxAtPos(&this->tentTipPos, NA_SE_EN_MOFER_WAVE);
            }
        } else if (st == TENT_SHAKE) {
            if (this->timer > 40) {
                rx = 1300.0f; lx = -3200.0f; sx = 7000.0f; rz = 800.0f; lz = 2500.0f; sz = 5000.0f; ra = 30.0f; sa = 60.0f;
                if ((this->sfxTimer % 32) == 0) {
                    Sfx_PlaySfxAtPos(&this->tentTipPos, NA_SE_EN_MOFER_WAVE);
                    Rumble_Request(0, 100, 5, 2);
                    Player_PlaySfx(&player->actor, NA_SE_VO_LI_FREEZE + player->ageProperties->unk_92);
                }
            } else {
                rx = 2000.0f; lx = -1000.0f; sx = 5000.0f; rz = 1500.0f; lz = 2500.0f; sz = 100.0f; ra = 70.0f; sa = 70.0f;
                if ((this->sfxTimer % 16) == 0) {
                    Sfx_PlaySfxAtPos(&this->tentTipPos, NA_SE_EN_MOFER_WAVE);
                    Rumble_Request(0, 160, 5, 4);
                    Player_PlaySfx(&player->actor, NA_SE_VO_LI_FREEZE + player->ageProperties->unk_92);
                }
            }
        } else {
            rx = 1300.0f; lx = 3200.0f; sx = 7000.0f; rz = 800.0f; lz = 2500.0f; sz = 5000.0f; ra = 30.0f; sa = 30.0f;
        }
        Math_ApproachF(&this->fwork[SW_RATE_X], rx, 1.0f, ra);
        Math_ApproachF(&this->fwork[SW_LAG_X], lx, 1.0f, 30.0f);
        Math_ApproachF(&this->fwork[SW_SIZE_X], sx, 1.0f, sa);
        Math_ApproachF(&this->fwork[SW_RATE_Z], rz, 1.0f, ra);
        Math_ApproachF(&this->fwork[SW_LAG_Z], lz, 1.0f, 30.0f);
        Math_ApproachF(&this->fwork[SW_SIZE_Z], sz, 1.0f, sa);
        this->xSwing += (s16)this->fwork[SW_RATE_X];
        this->zSwing += (s16)this->fwork[SW_RATE_Z];
    }

    switch (st) {
        case TENT_WAIT:
            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            if (this->timer == 0) {
                this->state = TENT_SPAWN;
                this->timer = (this->actor.params & HT_TENTACLE_ARENA) ? 24 : 70; // (Armorpha's rise at once)
                this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
            }
            break;

        case TENT_SPAWN:
            if (this->timer < 20) {
                Math_ApproachF(&this->tentRippleSize, 0.15f, 0.5f, 0.01f);
                Math_ApproachF(&this->baseAlpha, 150.0f, 1.0f, 5.0f);
                if (this->baseAlpha >= 150.0f) {
                    this->state = TENT_READY;
                    this->timer = 60;
                    if (!(this->actor.params & HT_TENTACLE_ARENA) && !Flags_GetSwitch(play, HT_TENTACLE_HINT_FLAG)) {
                        // the first one: Navi says what to do
                        Flags_SetSwitch(play, HT_TENTACLE_HINT_FLAG);
                        Message_StartTextbox(play, HT_TENTACLE_NAVI_TEXT, NULL);
                    }
                }
            }
            if ((this->timer % 6) == 0) {
                EffectSsGRipple_Spawn(play, &this->actor.world.pos, 200, 600, 0);
            }
            break;

        case TENT_READY:
        case TENT_SWING:
            Sfx_PlaySfxAtPos(&this->tentTipPos, NA_SE_EN_MOFER_APPEAR - SFX_FLAG);
            for (i = 0; i < 41; i++) {
                f32 sn = Math_SinS(((s16)this->fwork[SW_LAG_X] * i) + this->xSwing);
                f32 a = this->fwork[SW_SIZE_X] * (i * 0.025f * sn);
                f32 cs = Math_SinS(((s16)this->fwork[SW_LAG_Z] * i) + this->zSwing);
                f32 b = this->fwork[SW_SIZE_Z] * (i * 0.025f * cs);

                Math_ApproachF(&this->tentStretch[i].y, this->fwork[MAX_STRETCH] * 5.0f, 0.1f, 0.4f);
                if (i == 28) {
                    prev28 = this->tentRot[i].x;
                }
                Math_ApproachS(&this->tentRot[i].x, a, 1.0f / this->tentMaxAngle, this->tentSpeed);
                Math_ApproachS(&this->tentRot[i].z, b, 1.0f / this->tentMaxAngle, this->tentSpeed);
            }
            if (st == TENT_SWING) {
                Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer + this->attackAngleMod, 0xA, 0x1F4);
            }
            Math_ApproachF(&this->fwork[MAX_STRETCH], 1.0f, 0.5f, 0.04f);
            Math_ApproachF(&this->tentMaxAngle, 1.0f, 1.0f, 0.002f);
            Math_ApproachF(&this->tentSpeed, 400.0f, 1.0f, 6.0f);
            if (st == TENT_READY) {
                if ((this->timer == 0) && !(this->actor.params & HT_TENTACLE_ARENA) &&
                    (this->actor.xzDistToPlayer > TENT_REACH)) {
                    this->timer = 10; // (the boat's: out of reach, it just stands swaying till Link comes near)
                } else if (this->timer == 0) {
                    this->state = TENT_SWING;
                    this->timer = 50;
                    this->attackAngleMod = Rand_CenteredFloat(0x1000);
                }
            } else if ((this->timer == 0) && (this->tentRot[28].x >= 0) && (prev28 < 0)) {
                this->state = TENT_ATTACK;
                this->timer = 55;
            }
            break;

        case TENT_ATTACK:
            Sfx_PlaySfxAtPos(&this->tentTipPos, NA_SE_EN_MOFER_ATTACK - SFX_FLAG);
            for (i = 0; i < 41; i++) {
                // (Armorpha's lunge further: their upper half stretches half as long again as it bends - the
                // lower half stays low, where the ice can reach it)
                Math_ApproachF(&this->tentStretch[i].y,
                               this->fwork[MAX_STRETCH] * ((((40 - i) * 25.0f) / 100.0f) + 5.0f) *
                                   (((this->actor.params & HT_TENTACLE_ARENA) && (i >= 14)) ? 1.5f : 1.0f),
                               0.5f, (this->actor.params & HT_TENTACLE_ARENA) ? 1.0f : 0.7f);
                Math_ApproachS(&this->tentRot[i].x, sAttackRot[i] * 0x100, 1.0f / this->tentMaxAngle, this->tentSpeed);
                Math_ApproachS(&this->tentRot[i].z, 0, 1.0f / this->tentMaxAngle, this->tentSpeed);
            }
            Math_ApproachF(&this->tentMaxAngle, 0.5f, 1.0f, 0.01f);
            Math_ApproachF(&this->tentSpeed, 160.0f, 1.0f, 50.0f);
            if ((this->timer == 0) || (this->linkHitTimer != 0)) {
                f32 dx = this->tentPos[22].x - player->actor.world.pos.x;
                f32 dy = this->tentPos[22].y - player->actor.world.pos.y;
                f32 dz = this->tentPos[22].z - player->actor.world.pos.z;

                Tent_ResetSwing(this);
                f32 grabR = (this->actor.params & HT_TENTACLE_ARENA) ? 170.0f : 150.0f;
                f32 grabY = (this->actor.params & HT_TENTACLE_ARENA) ? 90.0f : 70.0f;

                if ((fabsf(dy) < grabY) && (player->actor.parent == NULL) && (sNoGrab == 0) &&
                    (sqrtf(SQ(dx) + SQ(dy) + SQ(dz)) < grabR)) {
                    this->state = TENT_CURL;
                    this->timer = 40;
                    this->linkToLeft = ((s16)(this->actor.shape.rot.y - this->actor.yawTowardsPlayer) < 0);
                } else {
                    this->state = TENT_READY;
                    this->timer = 30;
                    // (Armorpha's sink away when Link's out of reach; the boat's stay up, standing)
                    if ((this->actor.params & HT_TENTACLE_ARENA) && (this->actor.xzDistToPlayer > TENT_REACH)) {
                        this->state = TENT_RETREAT;
                        this->timer = 75;
                    }
                }
            }
            break;

        case TENT_CURL:
        case TENT_GRAB:
            if (this->timer == 125) {
                this->tentMaxAngle = 0.001f;
                this->tentSpeed = 0.0f;
            }
            for (i = 0; i < 41; i++) {
                s16 t = (this->timer > 25) ? sCurlRot[i] : sGrabRot[i];

                Math_ApproachS(&this->tentRot[i].z, t * (this->linkToLeft ? -0x100 : 0x100), 1.0f / this->tentMaxAngle,
                               this->tentSpeed);
            }
            Math_ApproachF(&this->tentMaxAngle, 0.1f, 1.0f, 0.01f);
            Math_ApproachF(&this->tentSpeed, 960.0f, 1.0f, 30.0f);
            if (this->timer >= 30) {
                Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 0xC8);
            }
            if (this->state == TENT_CURL) {
                if ((this->timer >= 5) && (this->linkHitTimer != 0) && (player->actor.parent == NULL) &&
                    (sNoGrab == 0)) {
                    if (play->grabPlayer(play, player)) {
                        player->actor.parent = &this->actor;
                        this->state = TENT_GRAB;
                        Sfx_PlaySfxAtPos(&this->tentTipPos, NA_SE_EN_MOFER_CATCH);
                        Audio_PlaySfxGeneral(NA_SE_VO_LI_DAMAGE_S, &player->actor.projectedPos, 4,
                                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                             &gSfxDefaultReverb);
                    } else {
                        this->state = TENT_READY;
                        Tent_ResetSwing(this);
                        this->timer = 30;
                    }
                }
                if ((this->state == TENT_CURL) && (this->timer == 4)) {
                    this->state = TENT_READY;
                    Tent_ResetSwing(this);
                    this->timer = 30;
                }
            }
            if (this->state == TENT_GRAB) {
                player->av2.actionVar2 = 0xA;
                player->actor.speedXZ = player->actor.velocity.y = 0;
                Math_ApproachF(&player->actor.world.pos.x, this->grabPosRot.pos.x, 0.5f, 20.0f);
                Math_ApproachF(&player->actor.world.pos.y, this->grabPosRot.pos.y, 0.5f, 20.0f);
                Math_ApproachF(&player->actor.world.pos.z, this->grabPosRot.pos.z, 0.5f, 20.0f);
                Tent_KeepOutOfWalls(this, play, &player->actor.world.pos);
                Math_ApproachS(&player->actor.shape.rot.x, this->grabPosRot.rot.x, 2, 0x7D0);
                Math_ApproachS(&player->actor.shape.rot.y, this->grabPosRot.rot.y, 2, 0x7D0);
                Math_ApproachS(&player->actor.shape.rot.z, this->grabPosRot.rot.z, 2, 0x7D0);
                if (this->timer == 0) {
                    this->state = TENT_SHAKE;
                    Tent_ResetSwing(this);
                    this->timer = 150;
                    this->mashCounter = 0;
                    this->sfxTimer = 30;
                }
            }
            break;

        case TENT_SHAKE: {
            u32 buttons = play->state.input[0].press.button;

            if ((this->timer % 8) == 0) {
                play->damagePlayer(play, -1);
            }
            prev15 = this->tentRot[15].x;
            if (CHECK_BTN_ALL(buttons, BTN_A) || CHECK_BTN_ALL(buttons, BTN_B)) {
                this->mashCounter++;
            }
            for (i = 0; i < 20; i++) {
                f32 sn = Math_SinS(((s16)this->fwork[SW_LAG_X] * i) + this->xSwing);
                f32 a = this->fwork[SW_SIZE_X] * (i * 0.025f * sn);
                f32 cs = Math_SinS(((s16)this->fwork[SW_LAG_Z] * i) + this->zSwing);
                f32 b = this->fwork[SW_SIZE_Z] * (i * 0.025f * cs);

                Math_ApproachF(&this->tentStretch[i].y, this->fwork[MAX_STRETCH] * ((((40 - i) * 25.0f) / 100.0f) + 5.0f),
                               0.1f, 0.1f);
                Math_ApproachS(&this->tentRot[i].x, a, 1.0f / this->tentMaxAngle, this->tentSpeed);
                Math_ApproachS(&this->tentRot[i].z, b, 1.0f / this->tentMaxAngle, this->tentSpeed);
            }
            player->av2.actionVar2 = 0xA;
            player->actor.world.pos = this->grabPosRot.pos;
            Tent_KeepOutOfWalls(this, play, &player->actor.world.pos);
            player->actor.world.rot = player->actor.shape.rot = this->grabPosRot.rot;
            player->actor.velocity.y = 0;
            player->actor.speedXZ = 0;
            Math_ApproachF(&this->fwork[MAX_STRETCH], 1.0f, 0.5f, 0.01f);
            Math_ApproachF(&this->tentMaxAngle, 0.5f, 1.0f, 0.005f);
            Math_ApproachF(&this->tentSpeed, 480.0f, 1.0f, 10.0f);
            Math_ApproachF(&this->tentPulse, 0.3f, 0.5f, 0.03f);
            if (this->mashCounter >= 40) {
                // shaken loose before the end: it hurls him back aboard (a throw it steers itself, so he lands on
                // the deck whatever the swing)
                Vec3f deck;
                Actor* boat = this->actor.parent;

                if ((boat != NULL) && (boat->id == HT_ACTOR_BOAT)) {
                    deck.x = boat->world.pos.x + (this->actor.world.pos.x - boat->world.pos.x) * 0.3f;
                    deck.z = boat->world.pos.z + (this->actor.world.pos.z - boat->world.pos.z) * 0.3f;
                    deck.y = boat->world.pos.y + HT_BOAT_DECK + 5.0f;
                    this->tossFrom = player->actor.world.pos;
                    this->tossTo = deck;
                    this->state = TENT_TOSS;
                    this->timer = TENT_TOSS_FRAMES;
                    this->invincTimer = 50;
                    Sfx_PlaySfxAtPos(&this->tentTipPos, NA_SE_EN_MOFER_WAVE);
                } else {
                    Tent_AfterMove(this, 75);
                    this->invincTimer = 50;
                    Tent_ReleaseLink(this, play, false);
                }
            } else if ((this->timer == 0) && (this->tentRot[15].x < 0) && (prev15 >= 0)) {
                // held to the end: let go at the top of a swing, flung away
                Tent_AfterMove(this, 75);
                this->invincTimer = 50;
                Tent_ReleaseLink(this, play, true);
            }
            break;
        }

        case TENT_TOSS: {
            // an arc from where he was held to the deck, 120 high at its top
            f32 t = 1.0f - (f32)this->timer / TENT_TOSS_FRAMES;

            player->av2.actionVar2 = 0xA;
            player->actor.velocity.y = 0;
            player->actor.speedXZ = 0;
            player->actor.world.pos.x = this->tossFrom.x + (this->tossTo.x - this->tossFrom.x) * t;
            player->actor.world.pos.z = this->tossFrom.z + (this->tossTo.z - this->tossFrom.z) * t;
            player->actor.world.pos.y = this->tossFrom.y + (this->tossTo.y - this->tossFrom.y) * t + 480.0f * t * (1.0f - t);
            player->actor.shape.rot.x = player->actor.shape.rot.z = 0;
            for (i = 0; i < 41; i++) {
                Math_ApproachS(&this->tentRot[i].x, 0, 4, 0x400);
                Math_ApproachS(&this->tentRot[i].z, 0, 4, 0x400);
            }
            if (this->timer == 0) {
                Tent_ReleaseLink(this, play, false);
                Tent_ResetSwing(this);
                Tent_AfterMove(this, 75);
            }
            break;
        }

        case TENT_RETREAT:
            for (i = 0; i < 41; i++) {
                f32 sn = Math_SinS(((s16)this->fwork[SW_LAG_X] * i) + this->xSwing);
                f32 a = (i * 0.025f * sn * this->fwork[SW_SIZE_X]) * this->fwork[MAX_STRETCH];
                f32 cs = Math_SinS(((s16)this->fwork[SW_LAG_Z] * i) + this->zSwing);
                f32 b = (i * 0.025f * cs * this->fwork[SW_SIZE_Z]) * this->fwork[MAX_STRETCH];

                Math_ApproachF(&this->tentStretch[i].y, this->fwork[MAX_STRETCH] * 5.0f, 0.5f, 0.2f);
                Math_ApproachS(&this->tentRot[i].x, a, 1.0f / this->tentMaxAngle, this->tentSpeed);
                Math_ApproachS(&this->tentRot[i].z, b, 1.0f / this->tentMaxAngle, this->tentSpeed);
            }
            Math_ApproachF(&this->fwork[MAX_STRETCH], 0.0f, 0.5f, 0.02f);
            Math_ApproachF(&this->tentMaxAngle, 0.5f, 1.0f, 0.01f);
            Math_ApproachF(&this->tentSpeed, 320.0f, 1.0f, 50.0f);
            if (this->timer == 0) {
                this->state = TENT_DESPAWN;
                this->timer = 30 + (s16)Rand_ZeroFloat(20.0f);
            }
            break;

        case TENT_DESPAWN:
            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            Math_ApproachF(&this->baseAlpha, 0.0f, 1.0f, 5.0f);
            if ((this->baseAlpha <= 0.5f) && (this->timer == 0) && (this->actor.params & HT_TENTACLE_ARENA)) {
                // Armorpha's: gone (it raises its next one where Link is now)
                Actor_Kill(&this->actor);
                break;
            }
            if ((this->baseAlpha <= 0.5f) && (this->timer == 0)) {
                // sunk: it rises again where it stood
                this->state = TENT_WAIT;
                this->timer = 20 + (s16)Rand_ZeroFloat(20.0f);
                Tent_ResetSwing(this);
            }
            break;
    }
}

static void Tent_Shatter(HtTentacle* this, PlayState* play) {
    s16 j;

    // (the burst's scale is Link's 0.01 for his own ice: pieces a little bigger than his)
    for (j = 4; j < 41; j += 6) {
        EffectSsIcePiece_SpawnBurst(play, &this->tentPos[j], 0.015f);
    }
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_ICE_BROKEN);
    if ((this->actor.parent != NULL) && (this->actor.parent->id == HT_ACTOR_ARMORPHA) &&
        (this->actor.parent->update != NULL)) {
        HtArmorpha_OnTentacleShattered(this->actor.parent, &this->actor, play);
    }
    Enemy_StartFinishingBlow(play, &this->actor);
    Actor_Kill(&this->actor);
}

// frozen, it stands just off the boat's side, past the rails: a Megaton Hammer swing whose head touches it breaks it
// too, in case the hammer's own collider misses it over the rail
#define TENT_HAMMER_REACH 70.0f
static s32 Tent_HammerNear(HtTentacle* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f* head;
    s16 j;

    if ((player->heldItemAction != PLAYER_IA_HAMMER) || (player->meleeWeaponState == 0)) {
        return false;
    }
    head = &player->meleeWeaponInfo[0].tip;
    for (j = 0; j < 41; j += 2) {
        f32 dx = head->x - this->tentPos[j].x;
        f32 dy = head->y - this->tentPos[j].y;
        f32 dz = head->z - this->tentPos[j].z;

        if (SQ(dx) + SQ(dy) + SQ(dz) < SQ(TENT_HAMMER_REACH)) {
            return true;
        }
    }
    return false;
}

// a hit: frozen by the ice, shattered by the hammer once frozen
static void Tent_CheckHits(HtTentacle* this, PlayState* play) {
    s16 i;

    for (i = 0; i < 19; i++) {
        ColliderInfo* info = &this->collider.elements[i].info;

        if (info->bumperFlags & BUMP_HIT) {
            u32 flags = (info->acHitInfo != NULL) ? info->acHitInfo->toucher.dmgFlags : 0;
            Actor* attacker = this->collider.base.ac;
            s16 j;

            for (j = 0; j < 19; j++) {
                this->collider.elements[j].info.bumperFlags &= ~BUMP_HIT;
                this->collider.elements[j].info.toucherFlags &= ~TOUCH_HIT;
            }
            this->invincTimer = 5;
            if (this->state == TENT_FROZEN) {
                if (flags & DMG_HAMMER) {
                    Tent_Shatter(this, play);
                } else {
                    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
                }
            } else if (((flags & (DMG_ARROW_ICE | DMG_SPIN_GIANT)) ||
                        ((attacker != NULL) && (attacker->id == HT_ACTOR_ICE_SLASH))) &&
                       ((this->state < TENT_ATTACK) || (this->state > TENT_SHAKE))) {
                // only once it bends forward to strike can it be caught: rising and swaying, the ice runs off it
                Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
            } else if ((flags & (DMG_ARROW_ICE | DMG_SPIN_GIANT)) ||
                       ((attacker != NULL) && (attacker->id == HT_ACTOR_ICE_SLASH))) {
                // frozen where it stands (and it lets go of Link)
                Tent_ReleaseLink(this, play, false);
                this->state = TENT_FROZEN;
                this->frozenTimer = TENT_FREEZE_FRAMES;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_FREEZE);
            } else if (flags & 0x0D800600) {
                this->linkHitTimer = 5;
            }
            this->tentRippleSize = 0.2f;
            return;
        } else if (info->toucherFlags & TOUCH_HIT) {
            info->toucherFlags &= ~TOUCH_HIT;
            this->linkHitTimer = 5;
            return;
        }
    }
}

void HtTentacle_Update(Actor* thisx, PlayState* play) {
    HtTentacle* this = (HtTentacle*)thisx;
    s16 i, index;
    f32 w;
    s32 frozen = (this->state == TENT_FROZEN);

    Tent_TickNoGrab(play);
    SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &this->tentPos[40], &this->tentTipPos,
                                 &thisx->projectedW);
    this->varTimer++;
    this->sfxTimer++;
    Math_ApproachF(&this->ice, frozen ? 1.0f : 0.0f, 1.0f, frozen ? 0.2f : 0.05f);
    if (!frozen) {
        // the slow pulse running up it
        this->widthIndex = (this->widthIndex + 1) % 300;
        this->pulsePhase -= 3000;
        this->tentWidth[this->widthIndex] = (Math_SinS(this->pulsePhase) * this->tentPulse) + (1.0f + this->tentPulse);
        for (i = 0; i < 41; i++) {
            index = ((this->widthIndex - (i * 2)) + 300) % 300;
            {
                // (Armorpha's core inside bulges it, as Morpha's does)
                static f32 sCoreBulge[11] = { 0.1f, 0.15f, 0.2f, 0.3f, 0.4f, 0.43f, 0.4f, 0.3f, 0.2f, 0.15f, 0.1f };
                s16 j = i + 2 - this->coreIndex;
                f32 bulge = ((this->coreIndex >= 0) && (j >= -5) && (j <= 5)) ? sCoreBulge[j + 5] * 3.0f : 0.0f;

                Math_ApproachF(&this->tentScale[i].x, this->tentWidth[index] * sTentWidth[i] + bulge, 0.5f, 0.3f);
            }
            w = Math_SinS((this->varTimer * 12000.0f) + (i * 20000.0f));
            this->tentRipple[i].x = w * this->tentRippleSize;
            this->tentScale[i].y = this->tentScale[i].z = this->tentScale[i].x;
            this->tentRipple[i].y = this->tentRipple[i].z = this->tentRipple[i].x;
        }
        Math_ApproachF(&this->tentRippleSize, 0.0f, 0.1f, 0.005f);
        Math_ApproachF(&this->tentPulse, 0.2f, 0.5f, 0.01f);
        Tent_Act(this, play);
        if (this->timer != 0) {
            this->timer--;
        }
        this->baseTex[0]++;
        this->baseTex[1]++;
        this->baseTex[2] -= 3;
        this->baseTex[3]++;
    } else if (--this->frozenTimer <= 0) {
        // thawed: it shakes itself free and comes on again
        Audio_PlayActorSound2(thisx, NA_SE_EV_ICE_MELT);
        this->state = TENT_READY;
        this->timer = 30;
        Tent_ResetSwing(this);
    } else if (this->frozenTimer < 40) {
        // about to thaw: it trembles
        thisx->shape.rot.y += (this->frozenTimer & 2) ? 0x80 : -0x80;
    }
    if (this->invincTimer != 0) {
        this->invincTimer--;
    }
    if (this->linkHitTimer != 0) {
        this->linkHitTimer--;
    }
    thisx->scale.y = thisx->scale.z = thisx->scale.x;

    if (frozen && Tent_HammerNear(this, play)) {
        Tent_Shatter(this, play);
        return;
    }
    if (this->baseAlpha > 1.0f) {
        Tent_CheckHits(this, play);
        if (thisx->update == NULL) {
            return; // shattered
        }
        if (!frozen && (this->invincTimer == 0) && (this->state != TENT_GRAB) && (this->state != TENT_SHAKE) &&
            (this->state != TENT_TOSS) &&
            (this->state != TENT_RETREAT) && (this->state != TENT_DESPAWN)) {
            CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        }
        if ((this->state != TENT_GRAB) && (this->state != TENT_SHAKE) && (this->state != TENT_TOSS)) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }
}

static void Tent_SetSphere(HtTentacle* this, s32 item, Vec3f* center) {
    ColliderJntSphElement* e = &this->collider.elements[item];

    e->dim.worldSphere.center.x = center->x;
    e->dim.worldSphere.center.y = center->y;
    e->dim.worldSphere.center.z = center->z;
    e->dim.worldSphere.radius = (this->state <= TENT_SHAKE || this->state == TENT_FROZEN)
                                    ? (e->dim.modelSphere.radius * TENT_SCALE * 100.0f)
                                    : 0;
}

void HtTentacle_Draw(Actor* thisx, PlayState* play) {
    static Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    HtTentacle* this = (HtTentacle*)thisx;
    Mtx* matrix;
    Vec3f lightDir;
    u16 scroll;
    s16 i, index;
    u8 a = (u8)(this->baseAlpha * 12.0f / 10.0f);
    f32 ice = this->ice;

    if (this->baseAlpha <= 0.5f) {
        return;
    }
    matrix = Graph_Alloc(play->state.gfxCtx, 41 * sizeof(Mtx));
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, (s8)(this->baseAlpha * 1.5f));
    gDPSetEnvColor(POLY_OPA_DISP++, 150, 150, 150, 0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, this->baseTex[0], this->baseTex[1], 32, 32, 1, this->baseTex[2],
                                  this->baseTex[3], 32, 32, 1, 1, 1, 1));
    // the tomb's green water (Morpha's is blue), or (frozen) pale solid ice
    gDPSetPrimColor(POLY_XLU_DISP++, 0xFF, 0xFF, (u8)(200 + 40 * ice), 255, (u8)(210 + 45 * ice),
                    (u8)(a + (255 - a) * ice * 0.7f));
    gDPSetEnvColor(POLY_XLU_DISP++, (u8)(20 + 150 * ice), (u8)(140 + 80 * ice), (u8)(60 + 195 * ice),
                   (u8)(this->baseAlpha + 90 * ice));
    scroll = (s16)(Math_SinS(this->varTimer * 0xB00) * 30.0f) + 350;
    gSPTexture(POLY_XLU_DISP++, scroll, scroll, 0, G_TX_RENDERTILE, G_ON);

    lightDir.x = play->envCtx.dirLight1.params.dir.x;
    lightDir.y = play->envCtx.dirLight1.params.dir.y;
    lightDir.z = play->envCtx.dirLight1.params.dir.z;
    Matrix_Push();
    gDPPipeSync(POLY_XLU_DISP++);
    gSPSegment(POLY_XLU_DISP++, 0x0C, matrix);
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY((thisx->shape.rot.y / (f32)0x8000) * M_PI, MTXMODE_APPLY);
    Matrix_RotateX((thisx->shape.rot.x / (f32)0x8000) * M_PI, MTXMODE_APPLY);
    Tent_InitRand(1, 29100, 9786);

    for (i = 0; i < 41; i++, matrix++) {
        FrameInterpolation_RecordOpenChild("HT Tentacle", i);
        if (i < 2) {
            Matrix_Push();
            Matrix_Scale(0.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        } else {
            if (i >= 3) {
                Matrix_Translate(0.0f, this->tentStretch[i - 2].y * this->heightMul, 0.0f, MTXMODE_APPLY);
                Matrix_RotateX((this->tentRot[i - 2].x / (f32)0x8000) * M_PI, MTXMODE_APPLY);
                Matrix_RotateZ((this->tentRot[i - 2].z / (f32)0x8000) * M_PI, MTXMODE_APPLY);
            }
            Matrix_Push();
            Matrix_Scale((this->tentScale[i - 2].x + this->tentRipple[i - 2].x) * thisx->scale.x,
                         (this->tentScale[i - 2].y + this->tentRipple[i - 2].y) * thisx->scale.y,
                         (this->tentScale[i - 2].z + this->tentRipple[i - 2].z) * thisx->scale.z, MTXMODE_APPLY);
        }
        index = ((this->widthIndex - (i * 2)) + 300) % 300;
        Matrix_RotateY((((this->tentWidth[index] - 1.0f - this->tentPulse) * 1000) / 1000.0f) * this->fwork[MAX_STRETCH],
                       MTXMODE_APPLY);
        Matrix_RotateX(M_PI / 2.0f, MTXMODE_APPLY);
        MATRIX_TOMTX(matrix);
        gSPMatrix(POLY_XLU_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        if (i == 0) {
            func_8002EB44(&this->tentPos[i], &play->view.eye, &lightDir, play->state.gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, gMorphaTentacleBaseDL);
        } else {
            gSPDisplayList(POLY_XLU_DISP++, sTentDLists[i]);
        }
        Matrix_Pop();

        if ((i >= 2) && (i < 38) && (ice < 0.5f)) {
            // the bubbles inside it
            f32 bs = (((Tent_Rand() * 0.08f) + 0.08f) * thisx->scale.x) * 100.0f;

            Matrix_Push();
            Matrix_Translate(((Tent_Rand() - 0.5f) * 10.0f) * this->tentScale[i - 2].x, (Tent_Rand() - 0.5f) * 3.0f,
                             ((Tent_Rand() - 0.5f) * 10.0f) * this->tentScale[i - 2].z, MTXMODE_APPLY);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(bs, bs, 1.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, gMorphaBubbleDL);
            Matrix_Pop();
        }
        Matrix_MultVec3f(&zeroVec, &this->tentPos[i]);
        if (i == 36) {
            Matrix_MultVec3f(&zeroVec, &thisx->focus.pos);
        }
        if (i == ((thisx->params & HT_TENTACLE_ARENA) ? HT_TENT_ARENA_GRAB_SEG : 24)) {
            // (where it holds Link: Morpha's, part way up; Armorpha's, up by the tip)
            MtxF mf;
            Vec3f grab = { -16.0f, 13.0f, 30.0f };
            Vec3s rot;

            Matrix_Push();
            if (this->linkToLeft) {
                grab.x *= -1.0f;
            }
            Matrix_MultVec3f(&grab, &this->grabPosRot.pos);
            Matrix_RotateX(-35 * M_PI / 64, MTXMODE_APPLY);
            Matrix_Get(&mf);
            Matrix_MtxFToYXZRotS(&mf, &rot, 0);
            this->grabPosRot.rot = rot;
            Matrix_Pop();
        }
        if ((i < 38) && ((i & 1) == 1)) {
            Tent_SetSphere(this, i / 2, &this->tentPos[i]);
        }
        FrameInterpolation_RecordCloseChild();
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
