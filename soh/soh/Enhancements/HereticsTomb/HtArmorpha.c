// Heretics Tomb: Armorpha, the B2 mini-boss. See HtArmorpha.h.

#include "HtArmorpha.h"
#include "HtTentacle.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/object_mo/object_mo.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include <math.h>

enum {
    ARM_WAIT,     // under the pool till Link walks in
    ARM_SWIRL,    // swims to the open water nearest Link and whirls round there, fast: a tentacle rises
    ARM_HIDDEN,   // riding its tentacle
    ARM_SLIP,     // slipping out of its tentacle (frozen too soon, or it's off to raise the next) into the water
    ARM_EJECT,    // thrown out of the shattered tentacle
    ARM_SLUMP,    // landed (on the stone, or in the water): a moment's heap
    ARM_RISE,     // rising, drawing the water up round itself into a great bubble
    ARM_TO_PATH,  // floating to its round
    ARM_ORBIT,    // rolling round the room in its bubble (it knocks Link down)
    ARM_HOP,      // back into the pool
    ARM_SINK,     // under the water: then the hunt goes on
    ARM_DYING,
    ARM_DEAD,     // gone (the actor stays: the water still burns)
};

#define ARM_SCALE 0.014f
#define ARM_RADIUS 35.0f              // the core's size at that scale
#define ARM_TOP_SEGMENT 38            // the top of its slide: just under the tentacle's tip (of 40)
#define ARM_TRAP_SEGMENT 30           // frozen only near the top of its slide is it trapped (lower, it slips out)
#define ARM_EJECT_FRAMES 22
#define ARM_EJECT_DIST 150.0f         // thrown the way the hammer swung
#define ARM_SLUMP_FRAMES 20
#define ARM_RISE_FRAMES 30
#define ARM_BUBBLE_R 150.0f           // its bubble's radius (it towers over Link)
#define ARM_BUBBLE_SCALE 4.3f         // ...as the core's membrane, scaled up (35 -> 150)
#define ARM_ORBIT_Y (HT_ARENA_FLOOR + ARM_BUBBLE_R - 60.0f) // its centre on the round: the bubble low, sunk into the stone
#define ARM_PATH_HALF 525.0f          // its round: a square round the walkway (the bubble clear of walls and platforms)
#define ARM_PATH_LEN (8.0f * ARM_PATH_HALF)
#define ARM_LAPS 3
#define ARM_FLASH_FRAMES 50           // cut: flickering red, untouchable
#define ARM_HOP_FRAMES 34
#define ARM_WATER_DAMAGE 8            // half a heart...
#define ARM_WATER_PERIOD 20           // ...a second

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x10 },                  // the bubble rolling into Link: a heart, and down he goes
        { 0xFFCFFFFF & ~DMG_HOOKSHOT, 0x00, 0x00 }, // the hookshot and longshot never take hold
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 35, 70, -35, { 0, 0, 0 } },
};

// --- the arena ---

static f32 Arm_LocalX(f32 x) {
    return x - HT_ARENA_X;
}
static f32 Arm_LocalZ(f32 z) {
    return z - HT_ARENA_Z;
}

// stone (the walkway or a platform) within `margin` of the point? (Morpha's BossMo_NearLand)
static s32 Arm_NearLand(f32 x, f32 z, f32 margin) {
    f32 lx = Arm_LocalX(x), lz = Arm_LocalZ(z);

    if ((fabsf(lx) >= HT_ARENA_POOL - margin) || (fabsf(lz) >= HT_ARENA_POOL - margin)) {
        return true;
    }
    if ((fabsf(fabsf(lx) - HT_ARENA_PLAT) < HT_ARENA_PLAT_HALF + margin) &&
        (fabsf(fabsf(lz) - HT_ARENA_PLAT) < HT_ARENA_PLAT_HALF + margin)) {
        return true;
    }
    return false;
}

static s32 Arm_InPool(Vec3f* p) {
    return (fabsf(Arm_LocalX(p->x)) < HT_ARENA_POOL) && (fabsf(Arm_LocalZ(p->z)) < HT_ARENA_POOL);
}

static s32 Arm_InArena(Vec3f* p) {
    return (fabsf(Arm_LocalX(p->x)) < HT_ARENA_HALF - 40.0f) && (fabsf(Arm_LocalZ(p->z)) < HT_ARENA_HALF - 40.0f);
}

// the open water nearest a point (a 30-unit grid over the pool, 60 clear of the stone)
static void Arm_NearestWater(Vec3f* p, Vec3f* out) {
    f32 best = 1e9f;
    f32 gx, gz;

    out->x = HT_ARENA_X;
    out->z = HT_ARENA_Z;
    for (gx = -420.0f; gx <= 420.0f; gx += 30.0f) {
        for (gz = -420.0f; gz <= 420.0f; gz += 30.0f) {
            f32 x = HT_ARENA_X + gx, z = HT_ARENA_Z + gz;
            f32 d = SQ(x - p->x) + SQ(z - p->z);

            if ((d < best) && !Arm_NearLand(x, z, 60.0f)) {
                best = d;
                out->x = x;
                out->z = z;
            }
        }
    }
    out->y = HT_ARENA_WATER;
}

// --- its tentacles ---

static s32 Arm_IsMine(HtArmorpha* this, Actor* a) {
    return (a->id == HT_ACTOR_TENTACLE) && (a->parent == &this->actor) && (a->update != NULL);
}

static s32 Arm_CountTentacles(HtArmorpha* this, PlayState* play) {
    Actor* a = play->actorCtx.actorLists[ACTORCAT_BOSS].head;
    s32 n = 0;

    for (; a != NULL; a = a->next) {
        if (Arm_IsMine(this, a)) {
            n++;
        }
    }
    return n;
}

// they sink (as the core is thrown out of the water)
static void Arm_KillTentacles(HtArmorpha* this, PlayState* play) {
    Actor* a = play->actorCtx.actorLists[ACTORCAT_BOSS].head;

    for (; a != NULL; a = a->next) {
        if (Arm_IsMine(this, a)) {
            EffectSsGRipple_Spawn(play, &a->world.pos, 300, 900, 0);
            EffectSsGSplash_Spawn(play, &a->world.pos, NULL, NULL, 1, 600);
            Actor_Kill(a);
        }
    }
    this->host = NULL;
}

static s32 Arm_MaxTentacles(HtArmorpha* this) {
    return 1 + (HT_ARMORPHA_HP - this->hp); // 1, 2, 3, 4
}

static s16 Arm_Stage(HtArmorpha* this) {
    return CLAMP(this->hp, 1, HT_ARMORPHA_HP) - 1; // 3 at full health .. 0 at the end
}

// its cadence: how soon after raising one it may raise the next (each stage sooner)...
static s16 Arm_Cadence(HtArmorpha* this) {
    static const s16 sFrames[HT_ARMORPHA_HP] = { 40, 65, 85, 100 };

    return sFrames[Arm_Stage(this)];
}

// ...and how fast it whirls round the spot of its own next tentacle - twice round (at the end it's rapid fire)
static s16 Arm_SwirlSpeed(HtArmorpha* this) {
    static const s16 sSpeed[HT_ARMORPHA_HP] = { 0x2000, 0x1A00, 0x1400, 0x1000 };

    return sSpeed[Arm_Stage(this)];
}

// the open water nearest where Link is going (where he'll be in half a second), clear of its other tentacles (and of
// `avoid`, if given)
static void Arm_FindSpot(HtArmorpha* this, PlayState* play, Vec3f* out, Vec3f* avoid) {
    Player* player = GET_PLAYER(play);
    f32 px = player->actor.world.pos.x + player->actor.velocity.x * 10.0f;
    f32 pz = player->actor.world.pos.z + player->actor.velocity.z * 10.0f;
    f32 best = 1e9f;
    f32 gx, gz;

    out->x = HT_ARENA_X;
    out->z = HT_ARENA_Z;
    for (gx = -420.0f; gx <= 420.0f; gx += 30.0f) {
        for (gz = -420.0f; gz <= 420.0f; gz += 30.0f) {
            f32 x = HT_ARENA_X + gx, z = HT_ARENA_Z + gz;
            f32 d = sqrtf(SQ(x - px) + SQ(z - pz));
            s32 ok = (d >= 110.0f) && !Arm_NearLand(x, z, 50.0f);

            if (ok && (avoid != NULL) && (SQ(x - avoid->x) + SQ(z - avoid->z) < SQ(140.0f))) {
                ok = false;
            }
            if (ok) {
                Actor* a = play->actorCtx.actorLists[ACTORCAT_BOSS].head;

                for (; a != NULL; a = a->next) {
                    if (Arm_IsMine(this, a) && (SQ(x - a->world.pos.x) + SQ(z - a->world.pos.z) < SQ(140.0f))) {
                        ok = false;
                        break;
                    }
                }
            }
            if (ok && (d < best)) {
                best = d;
                out->x = x;
                out->z = z;
            }
        }
    }
    out->y = HT_ARENA_WATER;
}

static void Arm_PickSpot(HtArmorpha* this, PlayState* play) {
    Arm_FindSpot(this, play, &this->spot, NULL);
}

// Its other tentacles - the ones it doesn't ride - rise on their own as the cadence comes round, round where Link is
// heading (no whirl: they're there to keep him moving), as long as there's room under the cap (one place is always
// kept for its own).
static void Arm_RaiseOthers(HtArmorpha* this, PlayState* play) {
    s32 others;
    Vec3f p;
    Actor* t;

    if (this->spawnTimer != 0) {
        return;
    }
    others = Arm_CountTentacles(this, play) - ((this->host != NULL) ? 1 : 0);
    if (others >= Arm_MaxTentacles(this) - 1) {
        return;
    }
    Arm_FindSpot(this, play, &p, (this->state == ARM_SWIRL) ? &this->spot : NULL);
    t = Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, HT_ACTOR_TENTACLE, p.x, HT_ARENA_WATER, p.z, 0, 0, 0,
                           HT_TENTACLE_ARENA);
    this->actor.child = NULL;
    if (t != NULL) {
        HtTentacle_RiseNow(t);
        EffectSsGRipple_Spawn(play, &p, 300, 900, 0);
        this->spawnTimer = Arm_Cadence(this);
    }
}

static void Arm_StartSwirl(HtArmorpha* this, PlayState* play) {
    this->state = ARM_SWIRL;
    this->timer = 0;
    this->host = NULL;
    this->posInTent = 0;
    this->climbing = false;
    Arm_PickSpot(this, play);
}

static void Arm_LeaveHost(HtArmorpha* this) {
    if (this->host != NULL) {
        ((HtTentacle*)this->host)->coreIndex = -1;
    }
    this->host = NULL;
    this->climbing = false;
    this->state = ARM_SLIP;
    this->timer = 10;
}

void HtArmorpha_OnTentacleShattered(Actor* thisx, Actor* tentacle, PlayState* play) {
    HtArmorpha* this = (HtArmorpha*)thisx;
    Player* player = GET_PLAYER(play);
    s16 yaw = player->actor.shape.rot.y;

    if ((this->state != ARM_HIDDEN) || (tentacle != this->host)) {
        return; // empty
    }
    // thrown out the way the hammer swung; its other tentacles sink
    this->host = NULL;
    Arm_KillTentacles(this, play);
    this->from = this->actor.world.pos;
    this->to.x = this->from.x + Math_SinS(yaw) * ARM_EJECT_DIST;
    this->to.z = this->from.z + Math_CosS(yaw) * ARM_EJECT_DIST;
    this->to.y = Arm_NearLand(this->to.x, this->to.z, 0.0f) ? HT_ARENA_FLOOR + ARM_RADIUS : HT_ARENA_WATER;
    this->state = ARM_EJECT;
    this->timer = 0;
    this->flightFrames = ARM_EJECT_FRAMES;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MOFER_CORE_JUMP);
}

void HtArmorpha_OnTentacleGone(Actor* thisx, Actor* tentacle, PlayState* play) {
    HtArmorpha* this = (HtArmorpha*)thisx;

    if (tentacle == this->host) {
        this->host = NULL;
    }
}

// --- its round ---

// faster each stage (and a little faster each lap)
static f32 Arm_RollSpeed(HtArmorpha* this) {
    static const f32 sSpeed[HT_ARMORPHA_HP] = { 36.0f, 30.0f, 25.0f, 20.0f }; // (hp 1 .. 4)

    return sSpeed[Arm_Stage(this)] * (1.0f + 0.1f * (s32)(this->lapDone / ARM_PATH_LEN));
}

// a point on its round: a square midway along the walkway, walked by distance u
static void Arm_PathPos(f32 u, Vec3f* out) {
    f32 s = ARM_PATH_HALF;
    s32 k;
    f32 t;

    while (u < 0.0f) {
        u += ARM_PATH_LEN;
    }
    while (u >= ARM_PATH_LEN) {
        u -= ARM_PATH_LEN;
    }
    k = (s32)(u / (2.0f * s));
    t = u - k * 2.0f * s - s;
    switch (k) {
        case 0: out->x = t; out->z = -s; break;
        case 1: out->x = s; out->z = t; break;
        case 2: out->x = -t; out->z = s; break;
        default: out->x = -s; out->z = -t; break;
    }
    out->x += HT_ARENA_X;
    out->z += HT_ARENA_Z;
    out->y = ARM_ORBIT_Y;
}

static f32 Arm_NearestPathU(Vec3f* p) {
    f32 best = 1e9f, bestU = 0.0f, u;
    Vec3f q;

    for (u = 0.0f; u < ARM_PATH_LEN; u += 40.0f) {
        Arm_PathPos(u, &q);
        if (SQ(q.x - p->x) + SQ(q.z - p->z) < best) {
            best = SQ(q.x - p->x) + SQ(q.z - p->z);
            bestU = u;
        }
    }
    return bestU;
}

static void Arm_BurstBubble(HtArmorpha* this, PlayState* play) {
    Vec3f p;
    s32 i;

    for (i = 0; i < 8; i++) {
        p.x = this->actor.world.pos.x + Math_SinS(i * 0x2000) * ARM_BUBBLE_R * 0.8f;
        p.z = this->actor.world.pos.z + Math_CosS(i * 0x2000) * ARM_BUBBLE_R * 0.8f;
        p.y = this->actor.world.pos.y - ARM_BUBBLE_R * 0.5f;
        EffectSsGSplash_Spawn(play, &p, NULL, NULL, 1, 700);
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_BOMB_DROP_WATER);
    this->bubble = 0.0f;
}

// --- the actor ---

void HtArmorpha_Init(Actor* thisx, PlayState* play) {
    HtArmorpha* this = (HtArmorpha*)thisx;

    Actor_SetScale(thisx, ARM_SCALE);
    ActorShape_Init(&thisx->shape, 0.0f, NULL, 0.0f);
    thisx->gravity = 0.0f;
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    thisx->colChkInfo.health = HT_ARMORPHA_HP;
    thisx->targetMode = 5;
    thisx->naviEnemyId = HT_ARMORPHA_NAVI_TEXT - 0x600; // Navi's C-up (Katana.cpp)
    thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    this->hp = HT_ARMORPHA_HP;
    this->state = Flags_GetSwitch(play, HT_ARMORPHA_FLAG) ? ARM_DEAD : ARM_WAIT;
    thisx->world.pos.x = HT_ARENA_X;
    thisx->world.pos.z = HT_ARENA_Z;
    thisx->world.pos.y = HT_ARENA_BED + 40.0f;
}

void HtArmorpha_Destroy(Actor* thisx, PlayState* play) {
    HtArmorpha* this = (HtArmorpha*)thisx;
    Actor* a = play->actorCtx.actorLists[ACTORCAT_BOSS].head;

    // (its tentacles may outlive it by a moment as the room goes: they mustn't call back into it)
    for (; a != NULL; a = a->next) {
        if ((a->id == HT_ACTOR_TENTACLE) && (a->parent == thisx)) {
            a->parent = NULL;
        }
    }
    if (this->fighting) {
        func_800F5B58(); // the dungeon's music back
    }
    Collider_DestroyCylinder(play, &this->collider);
}

// a flight's position: t in 0..1, an arc `height` high
static void Arm_Fly(HtArmorpha* this, f32 height) {
    f32 t = (f32)this->timer / this->flightFrames;

    this->actor.world.pos.x = this->from.x + (this->to.x - this->from.x) * t;
    this->actor.world.pos.z = this->from.z + (this->to.z - this->from.z) * t;
    this->actor.world.pos.y = this->from.y + (this->to.y - this->from.y) * t + height * 4.0f * t * (1.0f - t);
}

static void Arm_Splash(HtArmorpha* this, PlayState* play) {
    Vec3f p = this->actor.world.pos;

    p.y = HT_ARENA_WATER;
    EffectSsGSplash_Spawn(play, &p, NULL, NULL, 1, 800);
    EffectSsGRipple_Spawn(play, &p, 300, 1200, 0);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MOFER_CORE_MOVE_WT);
}

static void Arm_StartHop(HtArmorpha* this) {
    this->state = ARM_HOP;
    this->from = this->actor.world.pos;
    Arm_NearestWater(&this->from, &this->to);
    this->timer = 0;
    this->flightFrames = ARM_HOP_FRAMES;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MOFER_CORE_SMJUMP);
}

// struck in its bubble: the Iai draw (Link's swing, or its wave) cuts through bubble and armour; anything else just
// splashes off
static void Arm_CheckHit(HtArmorpha* this, PlayState* play) {
    Actor* attacker;
    s32 iai;

    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    if (this->flash != 0) {
        return;
    }
    attacker = this->collider.base.ac;
    iai = Katana_IsIaiBladeHit(attacker) ||
          ((attacker != NULL) && (attacker->id == HT_ACTOR_ICE_SLASH) && (attacker->params == 1));
    if (!iai) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MOFER_CORE_MOVE_WT);
        return;
    }
    this->hp--;
    this->flash = ARM_FLASH_FRAMES;
    Arm_BurstBubble(this, play);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_MOFER_CORE_DAMAGE);
    if (this->hp <= 0) {
        this->state = ARM_DYING;
        this->timer = 50;
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_MOFER_LASTVOICE);
        Enemy_StartFinishingBlow(play, &this->actor);
    } else {
        // (from the cut till its next bubble it can't be hurt: it drops, flickering red, and hops for the water)
        Arm_StartHop(this);
        // magic for the next stage's ice and Iai, at Link's feet
        Item_DropCollectible(play, &GET_PLAYER(play)->actor.world.pos, ITEM00_MAGIC_LARGE);
    }
}

static void Arm_Burst(HtArmorpha* this, PlayState* play) {
    static Vec3f accel = { 0.0f, 0.5f, 0.0f };
    Vec3f pos, vel;
    s32 i;

    for (i = 0; i < 14; i++) {
        vel.x = Rand_CenteredFloat(8.0f);
        vel.y = Rand_ZeroFloat(4.0f) + 1.0f;
        vel.z = Rand_CenteredFloat(8.0f);
        pos.x = this->actor.world.pos.x + vel.x * 4.0f;
        pos.y = this->actor.world.pos.y + vel.y * 4.0f;
        pos.z = this->actor.world.pos.z + vel.z * 4.0f;
        EffectSsDeadDb_Spawn(play, &pos, &vel, &accel, 120, 0, 200, 205, 215, 255, 60, 70, 90, 1, 9, i == 0);
    }
    // (the hearts at Link's feet: where it burst may be over the pool)
    pos = GET_PLAYER(play)->actor.world.pos;
    Item_DropCollectible(play, &pos, ITEM00_HEART);
    pos.x += 30.0f;
    Item_DropCollectible(play, &pos, ITEM00_HEART);
    // (its death cry is a held sound: played for a while, not once - once, it never stopped)
    HtSfx_PlayFor(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, NA_SE_EN_MOFER_DEAD, 40);
    Flags_SetSwitch(play, HT_ARMORPHA_FLAG);
    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    if (this->fighting) {
        func_800F5B58();
        this->fighting = false;
    }
    Arm_KillTentacles(this, play);
}

// The pool burns: half a heart a second while Link is down in it.
static void Arm_BurnWater(HtArmorpha* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (Arm_InPool(&player->actor.world.pos) && (player->actor.parent == NULL) &&
        ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) || (player->actor.yDistToWater > 10.0f))) {
        if (++this->waterTimer >= ARM_WATER_PERIOD) {
            this->waterTimer = 0;
            play->damagePlayer(play, -ARM_WATER_DAMAGE);
            Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S + player->ageProperties->unk_92);
        }
    } else {
        this->waterTimer = ARM_WATER_PERIOD - 4; // (the first tick comes almost at once)
    }
}

// riding its tentacle: up and down it, without a stop, as long as it stands. Frozen near the top of its slide it's
// trapped; any lower, it slips out.
static void Arm_RideTentacle(HtArmorpha* this, PlayState* play) {
    HtTentacle* t = (HtTentacle*)this->host;
    Actor* thisx = &this->actor;

    if (HtTentacle_IsFrozen(this->host)) {
        if (!this->frozenSeen) {
            this->frozenSeen = true;
            // (over the stone it can't drop out into the water: caught wherever it is on the tentacle)
            if ((this->posInTent < ARM_TRAP_SEGMENT) &&
                !Arm_NearLand(thisx->world.pos.x, thisx->world.pos.z, ARM_RADIUS)) {
                Audio_PlayActorSound2(thisx, NA_SE_EN_MOFER_CORE_MOVE_WT);
                Arm_LeaveHost(this);
                return;
            }
            Audio_PlayActorSound2(thisx, NA_SE_EN_MOFER_CORE_DAMAGE); // caught
        }
        return; // held fast in the ice
    }
    this->frozenSeen = false;
    // its tentacle sinking (Link went out of its reach, or it let go of him): it goes to raise its next one where he
    // is now - down the tentacle first while it's out over the stone (dropped straight down, it went through it)
    if (HtTentacle_IsLeaving(this->host)) {
        if ((this->posInTent > 0) && Arm_NearLand(thisx->world.pos.x, thisx->world.pos.z, ARM_RADIUS)) {
            this->posInTent = MAX(this->posInTent - 2, 0);
            t->coreIndex = (this->posInTent >= 1) ? this->posInTent : -1;
            Math_ApproachF(&thisx->world.pos.x, t->tentPos[this->posInTent].x, 0.5f, 30.0f);
            Math_ApproachF(&thisx->world.pos.y, t->tentPos[this->posInTent].y, 0.5f, 30.0f);
            Math_ApproachF(&thisx->world.pos.z, t->tentPos[this->posInTent].z, 0.5f, 30.0f);
        } else {
            Arm_LeaveHost(this);
        }
        return;
    }
    // whack-a-mole: as long as its tentacle stands, it slides up to the top and back down to the water, up and down
    // without a stop - freeze the tentacle as it nears the top to catch it
    if (HtTentacle_IsUp(this->host)) {
        if (this->climbing) {
            if (++this->posInTent >= ARM_TOP_SEGMENT) {
                this->climbing = false;
            }
        } else if (--this->posInTent <= 0) {
            this->climbing = true;
        }
    } else if (this->posInTent > 0) {
        this->posInTent--;
    }
    this->posInTent = CLAMP(this->posInTent, 0, 40);
    t->coreIndex = (this->posInTent >= 1) ? this->posInTent : -1;

    if ((this->posInTent >= 1) && HtTentacle_IsUp(this->host)) {
        Math_ApproachF(&thisx->world.pos.x, t->tentPos[this->posInTent].x, 0.5f, 30.0f);
        Math_ApproachF(&thisx->world.pos.y, t->tentPos[this->posInTent].y, 0.5f, 30.0f);
        Math_ApproachF(&thisx->world.pos.z, t->tentPos[this->posInTent].z, 0.5f, 30.0f);
    } else {
        // at its foot, just under the water, stirring
        Math_ApproachF(&thisx->world.pos.x, this->host->world.pos.x + Math_SinS(this->moveTimer * 0x800) * 15.0f, 0.5f,
                       15.0f);
        Math_ApproachF(&thisx->world.pos.y, HT_ARENA_WATER - 35.0f, 0.5f, 15.0f);
        Math_ApproachF(&thisx->world.pos.z, this->host->world.pos.z + Math_CosS(this->moveTimer * 0x800) * 15.0f, 0.5f,
                       15.0f);
    }
}

void HtArmorpha_Update(Actor* thisx, PlayState* play) {
    HtArmorpha* this = (HtArmorpha*)thisx;
    Player* player = GET_PLAYER(play);
    f32 flatTarget = 0.0f;
    s32 inBubble = false;

    Arm_BurnWater(this, play);
    if (this->flash != 0) {
        this->flash--;
    }
    if (this->spawnTimer != 0) {
        this->spawnTimer--;
    }
    this->spin += 0x300;
    this->moveTimer++;

    switch (this->state) {
        case ARM_WAIT:
            if (Arm_InArena(&player->actor.world.pos)) {
                func_800F5ACC(NA_BGM_MINI_BOSS);
                this->fighting = true;
                this->spawnTimer = Arm_Cadence(this);
                Arm_StartSwirl(this, play);
            }
            break;

        case ARM_SWIRL: {
            // swims to the open water nearest Link (a moving mark: it follows him), then whirls round it, fast
            Math_ApproachF(&thisx->world.pos.y, HT_ARENA_WATER - 35.0f, 0.3f, 10.0f);
            if (this->timer <= 0) {
                f32 d2 = SQ(thisx->world.pos.x - this->spot.x) + SQ(thisx->world.pos.z - this->spot.z);

                // (it follows Link's mark till it's close, then commits; and it never chases for long - it stuck in
                // the third stage chasing a mark that kept jumping as Link ran)
                if (((this->moveTimer % 8) == 0) && (d2 > SQ(150.0f)) && (this->timer > -60)) {
                    Arm_PickSpot(this, play);
                }
                Math_ApproachF(&thisx->world.pos.x, this->spot.x, 0.3f, 28.0f);
                Math_ApproachF(&thisx->world.pos.z, this->spot.z, 0.3f, 28.0f);
                if ((d2 < SQ(40.0f)) || (--this->timer < -90)) {
                    if (d2 >= SQ(40.0f) && !Arm_NearLand(thisx->world.pos.x, thisx->world.pos.z, 50.0f)) {
                        this->spot.x = thisx->world.pos.x; // (out of time: it whirls where it is)
                        this->spot.z = thisx->world.pos.z;
                    }
                    this->timer = 1;
                    this->circle = Math_Atan2S(thisx->world.pos.z - this->spot.z, thisx->world.pos.x - this->spot.x);
                }
            } else {
                Vec3f rp;

                this->circle += Arm_SwirlSpeed(this);
                Math_ApproachF(&thisx->world.pos.x, this->spot.x + Math_SinS(this->circle) * 60.0f, 0.6f, 40.0f);
                Math_ApproachF(&thisx->world.pos.z, this->spot.z + Math_CosS(this->circle) * 60.0f, 0.6f, 40.0f);
                if ((this->timer % 3) == 0) {
                    rp = thisx->world.pos;
                    rp.y = HT_ARENA_WATER;
                    EffectSsGRipple_Spawn(play, &rp, 150, 500, 0);
                }
                Audio_PlayActorSound2(thisx, NA_SE_EN_MOFER_CORE_ROLL - SFX_FLAG);
                // its tentacle forms as it whirls (risen by the time it's done)
                if (this->host == NULL) {
                    Actor* t = Actor_SpawnAsChild(&play->actorCtx, thisx, play, HT_ACTOR_TENTACLE, this->spot.x,
                                                  HT_ARENA_WATER, this->spot.z, 0, 0, 0, HT_TENTACLE_ARENA);

                    thisx->child = NULL;
                    if (t != NULL) {
                        HtTentacle_RiseNow(t);
                        this->host = t;
                    }
                }
                if ((++this->timer > (0x30000 / Arm_SwirlSpeed(this))) && (this->host != NULL)) { // three times round
                    this->state = ARM_HIDDEN;
                    this->posInTent = 0;
                    this->frozenSeen = false;
                }
            }
            break;
        }

        case ARM_HIDDEN:
            if ((this->host != NULL) && (this->host->update != NULL)) {
                Arm_RideTentacle(this, play);
            } else {
                this->host = NULL;
                Arm_StartSwirl(this, play);
            }
            break;

        case ARM_SLIP:
            // down out of it, into the water
            Math_ApproachF(&thisx->world.pos.y, HT_ARENA_WATER - 40.0f, 0.4f, 20.0f);
            if (--this->timer <= 0) {
                Arm_StartSwirl(this, play);
            }
            break;

        case ARM_EJECT:
            this->timer++;
            Arm_Fly(this, 140.0f);
            if (this->timer >= this->flightFrames) {
                Vec3f p = thisx->world.pos;

                this->state = ARM_SLUMP;
                this->timer = ARM_SLUMP_FRAMES;
                if (this->to.y > HT_ARENA_WATER) {
                    // on the stone: a sodden heap, splat
                    this->flat = 1.4f;
                    p.y = HT_ARENA_FLOOR;
                    EffectSsGSplash_Spawn(play, &p, NULL, NULL, 1, 500);
                    Audio_PlayActorSound2(thisx, NA_SE_EN_MOFER_CORE_LAND);
                } else {
                    Arm_Splash(this, play);
                }
            }
            break;

        case ARM_SLUMP:
            flatTarget = (this->to.y > HT_ARENA_WATER) ? 1.0f : 0.0f;
            if (--this->timer <= 0) {
                this->state = ARM_RISE;
                this->timer = 0;
                this->from = thisx->world.pos;
                Audio_PlayActorSound2(thisx, NA_SE_EV_WATER_WALL_BIG);
            }
            break;

        case ARM_RISE: {
            // up it comes, the pool's water climbing round it into a great bubble
            f32 t = (f32)++this->timer / ARM_RISE_FRAMES;

            inBubble = true;
            thisx->world.pos.y = this->from.y + (ARM_ORBIT_Y + 30.0f - this->from.y) * t;
            this->bubble = t;
            if ((this->timer % 3) == 0) {
                Vec3f p = thisx->world.pos;

                p.x += Rand_CenteredFloat(ARM_BUBBLE_R);
                p.z += Rand_CenteredFloat(ARM_BUBBLE_R);
                p.y -= ARM_BUBBLE_R * this->bubble;
                EffectSsGSplash_Spawn(play, &p, NULL, NULL, 1, 300);
            }
            if (this->timer >= ARM_RISE_FRAMES) {
                this->state = ARM_TO_PATH;
                this->lapPos = Arm_NearestPathU(&thisx->world.pos);
                this->rollSpeed = 0.0f;
            }
            break;
        }

        case ARM_TO_PATH: {
            Vec3f q;

            inBubble = true;
            Arm_PathPos(this->lapPos, &q);
            Math_ApproachF(&thisx->world.pos.x, q.x, 0.3f, 14.0f);
            Math_ApproachF(&thisx->world.pos.y, q.y, 0.3f, 8.0f);
            Math_ApproachF(&thisx->world.pos.z, q.z, 0.3f, 14.0f);
            Arm_CheckHit(this, play);
            if ((this->state == ARM_TO_PATH) &&
                ((SQ(thisx->world.pos.x - q.x) + SQ(thisx->world.pos.z - q.z)) < SQ(20.0f))) {
                this->state = ARM_ORBIT;
                this->lapDone = 0.0f;
                {
                    // round toward Link: whichever way to the round's point by him is shorter
                    f32 d = Arm_NearestPathU(&GET_PLAYER(play)->actor.world.pos) - this->lapPos;

                    while (d > ARM_PATH_LEN / 2.0f) {
                        d -= ARM_PATH_LEN;
                    }
                    while (d < -ARM_PATH_LEN / 2.0f) {
                        d += ARM_PATH_LEN;
                    }
                    this->lapDir = (d < 0.0f) ? -1.0f : 1.0f;
                }
            }
            break;
        }

        case ARM_ORBIT: {
            Vec3f q;

            // round the room, three times, like King Dodongo
            inBubble = true;
            Math_ApproachF(&this->rollSpeed, Arm_RollSpeed(this), 1.0f, 1.0f);
            if ((this->moveTimer % 2) == 0) {
                // it churns: water thrown off its foot as it goes
                Vec3f p = thisx->world.pos;
                s16 a = (s16)Rand_ZeroFloat(0x10000);

                p.x += Math_SinS(a) * ARM_BUBBLE_R * 0.9f;
                p.z += Math_CosS(a) * ARM_BUBBLE_R * 0.9f;
                p.y = HT_ARENA_FLOOR;
                EffectSsGSplash_Spawn(play, &p, NULL, NULL, 1, 350);
            }
            this->lapPos += this->rollSpeed * this->lapDir;
            this->lapDone += this->rollSpeed;
            Arm_PathPos(this->lapPos, &q);
            thisx->world.pos = q;
            Audio_PlayActorSound2(thisx, NA_SE_EN_MOFER_CORE_ROLL - SFX_FLAG);
            Arm_CheckHit(this, play);
            if ((this->state == ARM_ORBIT) && (this->lapDone >= ARM_LAPS * ARM_PATH_LEN)) {
                // uncut: its bubble bursts, and back it goes
                Arm_BurstBubble(this, play);
                Arm_StartHop(this);
            }
            break;
        }

        case ARM_DYING:
            thisx->world.pos.x += Rand_CenteredFloat(4.0f);
            thisx->world.pos.z += Rand_CenteredFloat(4.0f);
            this->flash = 2;
            if (--this->timer <= 0) {
                Arm_Burst(this, play);
                this->state = ARM_DEAD;
            }
            break;

        case ARM_HOP:
            this->timer++;
            Arm_Fly(this, 90.0f);
            if (this->timer >= this->flightFrames) {
                Arm_Splash(this, play);
                this->state = ARM_SINK;
                this->timer = 30;
            }
            break;

        case ARM_SINK:
            Math_ApproachF(&thisx->world.pos.y, HT_ARENA_WATER - 60.0f, 0.2f, 6.0f);
            if (--this->timer <= 0) {
                this->spawnTimer = Arm_Cadence(this) / 2;
                Arm_StartSwirl(this, play);
            }
            break;

        case ARM_DEAD:
        default:
            if (!this->categorySet) {
                this->categorySet = true;
                thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
                Actor_ChangeCategory(play, &play->actorCtx, thisx, ACTORCAT_MISC);
            }
            break;
    }

    if ((this->state == ARM_SWIRL) || (this->state == ARM_HIDDEN) || (this->state == ARM_SLIP)) {
        Arm_RaiseOthers(this, play);
    }

    // its shape: slumped flat on the stone a moment after it lands, quivering
    Math_ApproachF(&this->flat, flatTarget, 0.3f, (flatTarget > this->flat) ? 0.25f : 0.05f);
    {
        f32 qx = this->flat * 0.08f * Math_SinS(this->moveTimer * 0x700);
        f32 qz = this->flat * 0.08f * Math_SinS(this->moveTimer * 0x530 + 0x2000);
        f32 h = CLAMP_MIN(1.0f - 0.7f * this->flat - (qx + qz) * 0.5f, 0.15f);

        thisx->scale.x = ARM_SCALE * (1.0f + 1.3f * this->flat + qx);
        thisx->scale.z = ARM_SCALE * (1.0f + 1.3f * this->flat + qz);
        thisx->scale.y = ARM_SCALE * h;
        if ((this->state == ARM_SLUMP) && (this->to.y > HT_ARENA_WATER)) {
            thisx->world.pos.y = HT_ARENA_FLOOR + ARM_RADIUS * h;
        }
    }

    // Z-targetable in its bubble, and up in its tentacle (the tentacles never are)
    if (inBubble || ((this->state == ARM_HIDDEN) && (this->host != NULL) && (this->posInTent >= 1))) {
        thisx->flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        if (inBubble && (player->focusActor == NULL)) {
            play->actorCtx.targetCtx.unk_8C = thisx; // Navi flies to it: Z locks on to it at once
        }
    } else {
        thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    }
    if (inBubble && (this->bubble > 0.5f)) {
        this->collider.dim.radius = ARM_BUBBLE_R * this->bubble;
        this->collider.dim.height = ARM_BUBBLE_R * 2.0f * this->bubble;
        this->collider.dim.yShift = -ARM_BUBBLE_R * this->bubble;
        Collider_UpdateCylinder(thisx, &this->collider);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        if (this->state == ARM_ORBIT) {
            // rolling into him: down he goes
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        }
    } else {
        this->collider.base.acFlags &= ~AC_HIT;
        this->collider.base.atFlags &= ~AT_HIT;
    }
    thisx->focus.pos = thisx->world.pos;
}

void HtArmorpha_Draw(Actor* thisx, PlayState* play) {
    HtArmorpha* this = (HtArmorpha*)thisx;
    u32 frames = play->gameplayFrames;
    f32 spinMul = CLAMP(1.0f - this->flat, 0.0f, 1.0f); // (slumped, it lies still)
    s32 red = (this->flash != 0) && ((this->flash & 4) != 0);
    f32 lapSpin = this->lapDone * this->lapDir / ARM_BUBBLE_R; // (rolling: its turn follows the ground it covers)

    if ((this->state == ARM_DEAD) || (this->state == ARM_WAIT)) {
        return;
    }
    if ((this->flash != 0) && (this->state != ARM_DYING) && ((this->flash & 2) != 0)) {
        return; // cut: the whole of it flickers
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, frames * 3, frames * 3, 32, 32, 1, frames * -3, frames * -3,
                                32, 32));
    gSPSegment(POLY_XLU_DISP++, 0x09,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, frames * 5, 0, 32, 32, 1, 0, frames * -10, 32, 32));

    Matrix_Push();
    Matrix_RotateX(this->spin * (M_PI / 0x8000) * 0.5f * spinMul, MTXMODE_APPLY);
    Matrix_RotateZ(this->spin * (M_PI / 0x8000) * 0.8f * spinMul, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // the casing: Morpha's membrane, dark iron and all but opaque (all red, cut)
    if (red) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 30, 20, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 255);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 150, 155, 170, 235);
        gDPSetEnvColor(POLY_XLU_DISP++, 70, 75, 90, 235);
    }
    func_8002ED80(&this->actor, play, 0);
    gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gMorphaCoreMembraneDL));

    // the nucleus: steel (all red, cut)
    gDPPipeSync(POLY_XLU_DISP++);
    if (red || ((this->state == ARM_DYING) && ((this->flash % 2) != 0))) {
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 255);
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 30, 20, 255);
    } else {
        gDPSetEnvColor(POLY_XLU_DISP++, 110, 120, 140, 128);
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 210, 215, 225, 255);
    }
    gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gMorphaCoreNucleusDL));
    Matrix_Pop();

    // its bubble: the membrane again, many times its size, in the pool's green water, rolling as it goes
    if (this->bubble > 0.01f) {
        f32 sc = ARM_SCALE * ARM_BUBBLE_SCALE * this->bubble;

        Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(this->spin * (M_PI / 0x8000) * 0.3f, MTXMODE_APPLY);
        Matrix_RotateX(lapSpin, MTXMODE_APPLY);
        Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 150, 240, 170, 150);
        gDPSetEnvColor(POLY_XLU_DISP++, 20, 120, 55, 150);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gMorphaCoreMembraneDL));
        // ...and a second skin inside, churning the other way
        Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(-this->spin * (M_PI / 0x8000) * 0.5f, MTXMODE_APPLY);
        Matrix_RotateZ(-lapSpin * 0.7f, MTXMODE_APPLY);
        Matrix_Scale(sc * 0.85f, sc * 0.85f, sc * 0.85f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 120, 220, 150, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 10, 90, 40, 110);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gMorphaCoreMembraneDL));
    }

    // on the stone: the wet patch it leaves, wider than itself
    if (this->flat > 0.05f) {
        f32 sc = 0.3f + 0.3f * CLAMP_MAX(this->flat, 1.0f);

        Gfx_SetupDL_44Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 10, 20, 15, (u8)(200 * CLAMP_MAX(this->flat, 1.0f)));
        Matrix_Translate(thisx->world.pos.x, HT_ARENA_FLOOR + 1.0f, thisx->world.pos.z, MTXMODE_NEW);
        Matrix_Scale(sc, 1.0f, sc, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gCircleShadowDL));
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
