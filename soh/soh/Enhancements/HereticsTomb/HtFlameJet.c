// Heretics Tomb: wall flamethrower the Ice Katana can freeze.
//
// While burning it shoots a jet of Dodongo-style fire that burns Link. A katana slash wave, an Iai wave or an
// ice arrow that touches the flame freezes it: the fire dies, the nozzle ices over and a timer ticks (the
// vanilla timed-switch tick) until it thaws and reignites. That gives fire corridors and timed runs.
// Needs object_dodongo in the room's object list (for the fire sprites).
//
// Registered at runtime through ActorDB with a fixed id (see Katana.h), so rooms can place it.

#include "HtFlameJet.h"
#include "Katana.h"
#include "HtTorch.h"
#include "HtWaterSpout.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include <math.h>

enum {
    FLAMEJET_BURNING,
    FLAMEJET_RESTING, // off part of the on/off cycle
    FLAMEJET_FROZEN,
};

#define FLAMEJET_CYCLE_FRAMES 40       // 2 s on, 2 s off in cycling mode
#define FLAMEJET_RELAY_OFF_FRAMES 120  // relay mode: 2 s on, 6 s off
#define FLAMEJET_MODE_RELAY 3
#define FLAMEJET_DEFAULT_FREEZE_SEC 8
#define FLAMEJET_PERMANENT 63
#define FLAMEJET_THAW_WARNING 40       // the ice flickers for the last 2 s
#define FLAMEJET_GROW_SPEED 40.0f      // units per frame when igniting
#define FLAMEJET_FIRE_LIFE 10          // frames each fire puff lives
#define FLAMEJET_DAMAGE 4              // same as the Fire Temple flame walls
#define FLAMEJET_PUFFS 3               // fire puffs per frame, staggered so the jet starts at the nozzle

// The katana slash wave hits as DMG_BOOMERANG, the Iai wave as DMG_SPIN_GIANT.

static ColliderJntSphElementInit sElementInit = {
    {
        ELEMTYPE_UNK0,
        { 0x20000000, 0x01, 0x04 }, // fire: burns Link
        { DMG_BOOMERANG | DMG_ARROW_ICE | DMG_SPIN_GIANT, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_ON | BUMP_NO_DAMAGE | BUMP_NO_SWORD_SFX | BUMP_NO_HITMARK,
        OCELEM_NONE,
    },
    { 0, { { 0, 0, 0 }, 20 }, 100 },
};

static ColliderJntSphElementInit sElementsInit[HT_FLAME_JET_SPHERES];

static ColliderJntSphInit sJntSphInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_JNTSPH,
    },
    HT_FLAME_JET_SPHERES,
    sElementsInit,
};

void HtFlameJet_Init(Actor* thisx, PlayState* play) {
    HtFlameJet* this = (HtFlameJet*)thisx;
    s32 params = this->actor.params;
    s32 freezeSec = params & 0x3F;
    s32 steps = (params >> 6) & 0xF;
    f32 cosX = Math_CosS(this->actor.shape.rot.x);

    for (s32 i = 0; i < HT_FLAME_JET_SPHERES; i++) {
        sElementsInit[i] = sElementInit;
    }
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sJntSphInit, this->elements);

    this->dir.x = Math_SinS(this->actor.shape.rot.y) * cosX;
    this->dir.y = -Math_SinS(this->actor.shape.rot.x);
    this->dir.z = Math_CosS(this->actor.shape.rot.y) * cosX;

    this->length = (steps == 0) ? 200.0f : steps * 40.0f;
    if (freezeSec == FLAMEJET_PERMANENT) {
        this->freezeFrames = -1;
    } else {
        this->freezeFrames = ((freezeSec == 0) ? FLAMEJET_DEFAULT_FREEZE_SEC : freezeSec) * 20;
    }
    this->mode = (params >> 10) & 3;

    this->state = (this->mode == 2) ? FLAMEJET_RESTING : FLAMEJET_BURNING;
    this->timer = FLAMEJET_CYCLE_FRAMES;
    if (this->mode == FLAMEJET_MODE_RELAY) {
        // (into its 6 s cycle by the phase: the burn is its first 2 s)
        s32 t = ((params >> 12) & 7) * 20 % (FLAMEJET_CYCLE_FRAMES + FLAMEJET_RELAY_OFF_FRAMES);

        if (t < FLAMEJET_CYCLE_FRAMES) {
            this->timer = FLAMEJET_CYCLE_FRAMES - t;
        } else {
            this->state = FLAMEJET_RESTING;
            this->timer = FLAMEJET_CYCLE_FRAMES + FLAMEJET_RELAY_OFF_FRAMES - t;
        }
    }
    this->reach = (this->state == FLAMEJET_BURNING) ? this->length : 0.0f;
    this->iceAlpha = 0.0f;
    this->actor.uncullZoneForward = 2000.0f;
    this->actor.uncullZoneScale = this->length;
    this->actor.uncullZoneDownward = this->length;
}

void HtFlameJet_Destroy(Actor* thisx, PlayState* play) {
    HtFlameJet* this = (HtFlameJet*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

// Did something icy touch the flame this frame?
static s32 HtFlameJet_CheckFreezeHit(HtFlameJet* this) {
    s32 frozen = false;

    if (this->collider.base.acFlags & AC_HIT) {
        Actor* attacker = this->collider.base.ac;

        for (s32 i = 0; i < HT_FLAME_JET_SPHERES; i++) {
            ColliderInfo* info = &this->elements[i].info;

            if ((info->bumperFlags & BUMP_HIT) && (info->acHitInfo != NULL)) {
                u32 flags = info->acHitInfo->toucher.dmgFlags;
                // the plain boomerang shares the wave's damage type, so check who threw it
                if ((flags & (DMG_ARROW_ICE | DMG_SPIN_GIANT)) ||
                    ((attacker != NULL) && (attacker->id == HT_ACTOR_ICE_SLASH))) {
                    frozen = true;
                }
            }
            info->bumperFlags &= ~BUMP_HIT;
        }
        this->collider.base.acFlags &= ~AC_HIT;
    }
    return frozen;
}

static void HtFlameJet_Freeze(HtFlameJet* this, PlayState* play) {
    Vec3f pos = this->actor.world.pos;

    this->state = FLAMEJET_FROZEN;
    this->timer = this->freezeFrames;
    this->reach = 0.0f;
    this->iceAlpha = 255.0f;
    // continuous sound: a short burst only (played once, it would never stop)
    HtSfx_PlayFor(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, NA_SE_EN_TWINROBA_SHOOT_FREEZE, 20);
    static Color_RGBA8 prim = { 220, 245, 255, 255 };
    static Color_RGBA8 env = { 90, 170, 255, 255 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f drift = { 0.0f, 0.3f, 0.0f };
    for (s32 i = 0; i < 10; i++) {
        Vec3f p = { pos.x + this->dir.x * 12.0f * i + Rand_CenteredFloat(16.0f),
                    pos.y + this->dir.y * 12.0f * i + Rand_CenteredFloat(16.0f),
                    pos.z + this->dir.z * 12.0f * i + Rand_CenteredFloat(16.0f) };
        EffectSsKiraKira_SpawnDispersed(play, &p, &zero, &drift, &prim, &env, 3000, 20);
    }
}

static void HtFlameJet_Ignite(HtFlameJet* this) {
    this->state = FLAMEJET_BURNING;
    this->timer = FLAMEJET_CYCLE_FRAMES;
    this->reach = 0.0f;
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_IGNITION);
}

static void HtFlameJet_SpawnFire(HtFlameJet* this, PlayState* play) {
    f32 speed = this->reach / FLAMEJET_FIRE_LIFE;
    Vec3f vel = { this->dir.x * speed, this->dir.y * speed, this->dir.z * speed };
    Vec3f accel = { 0.0f, 0.3f, 0.0f }; // flames curl upward a little

    // Effects move once before they are first drawn, so a puff spawned at the nozzle would first show up a
    // whole step away from it. Start the puffs a fraction of a step back: they land at 0, 1/3 and 2/3 of a step.
    for (s32 i = 1; i <= FLAMEJET_PUFFS; i++) {
        f32 back = speed * i / FLAMEJET_PUFFS;
        Vec3f pos = { this->actor.world.pos.x - this->dir.x * back + Rand_CenteredFloat(4.0f),
                      this->actor.world.pos.y - this->dir.y * back + Rand_CenteredFloat(4.0f),
                      this->actor.world.pos.z - this->dir.z * back + Rand_CenteredFloat(4.0f) };
        EffectSsDFire_Spawn(play, &pos, &vel, &accel, 50, 14, 255, 5, FLAMEJET_FIRE_LIFE);
    }
}

// Lay the hit spheres along the burning part of the jet.
static void HtFlameJet_UpdateSpheres(HtFlameJet* this, f32 reach) {
    for (s32 i = 0; i < HT_FLAME_JET_SPHERES; i++) {
        ColliderJntSphElementDim* dim = &this->elements[i].dim;
        f32 t = reach * (i + 0.5f) / HT_FLAME_JET_SPHERES;

        dim->worldSphere.center.x = this->actor.world.pos.x + this->dir.x * t;
        dim->worldSphere.center.y = this->actor.world.pos.y + this->dir.y * t;
        dim->worldSphere.center.z = this->actor.world.pos.z + this->dir.z * t;
        // the jet widens as it goes, like the fire sprites do
        // tall enough to meet the katana wave, which flies flat at Link's waist height
        dim->worldSphere.radius = 26 + (s16)(t * 0.05f);
    }
}

// Link touching the flame is thrown back out of it (the side he came from) and takes fire damage.
// Done by hand instead of through an AT collider: a hit through AT only lands while Link isn't invincible,
// so after the first hit he could walk straight through the rest of the jet.
static void HtFlameJet_CheckPlayer(HtFlameJet* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f body = player->actor.world.pos;
    Vec3f rel;
    f32 t, dx, dy, dz, reachRadius;

    if (this->knockCooldown > 0) {
        this->knockCooldown--;
        return;
    }

    body.y += 30.0f; // middle of Link's body
    rel.x = body.x - this->actor.world.pos.x;
    rel.y = body.y - this->actor.world.pos.y;
    rel.z = body.z - this->actor.world.pos.z;
    t = rel.x * this->dir.x + rel.y * this->dir.y + rel.z * this->dir.z;
    if ((t < 0.0f) || (t > this->reach)) {
        return;
    }
    // distance from the jet's axis
    dx = rel.x - this->dir.x * t;
    dy = rel.y - this->dir.y * t;
    dz = rel.z - this->dir.z * t;
    reachRadius = 26.0f + t * 0.05f + 12.0f; // flame radius + Link's body
    if ((dx * dx + dy * dy + dz * dz) > (reachRadius * reachRadius)) {
        return;
    }

    {
        // push straight out of the jet, toward the side Link is on
        s16 yaw = ((dx * dx + dz * dz) > 1.0f) ? Math_Atan2S(dz, dx)
                                               : (s16)(this->actor.shape.rot.y + 0x4000);
        Actor_SetPlayerKnockbackLarge(play, &this->actor, 9.0f, yaw, 7.0f, FLAMEJET_DAMAGE);
        Audio_PlayActorSound2(&player->actor, NA_SE_EV_FLAME_IGNITION);
        this->knockCooldown = 30;
    }
}

// Frozen spout pillars the jet plays on melt (a pillar is 80-200 wide: test against its half width, from its foot up
// to just over its top).
static void HtFlameJet_MeltSpouts(HtFlameJet* this, PlayState* play) {
    Actor* a = play->actorCtx.actorLists[ACTORCAT_BG].head;

    for (; a != NULL; a = a->next) {
        if ((a->id == HT_ACTOR_WATER_SPOUT) && (a->update != NULL)) {
            HtWaterSpout* sp = (HtWaterSpout*)a;
            f32 rx = a->world.pos.x - this->actor.world.pos.x, rz = a->world.pos.z - this->actor.world.pos.z;
            f32 h = this->dir.x * this->dir.x + this->dir.z * this->dir.z;
            f32 t = (h > 0.0001f) ? (rx * this->dir.x + rz * this->dir.z) / h : 0.0f;
            f32 dx, dz, y;

            if ((t < 0.0f) || (t > this->reach)) {
                continue;
            }
            dx = rx - this->dir.x * t;
            dz = rz - this->dir.z * t;
            y = this->actor.world.pos.y + this->dir.y * t;
            if ((SQ(dx) + SQ(dz) < SQ(sp->width * 0.5f + 26.0f)) && (y > a->world.pos.y) &&
                (y < a->world.pos.y + sp->height * sp->progress + 60.0f)) {
                HtWaterSpout_Thaw(a, play);
            }
        }
    }
}

// Torches whose flame sits in the jet catch fire.
static void HtFlameJet_LightTorches(HtFlameJet* this, PlayState* play) {
    Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head;

    for (; a != NULL; a = a->next) {
        if ((a->id == HT_ACTOR_TORCH) && (a->update != NULL)) {
            Vec3f rel = { a->world.pos.x - this->actor.world.pos.x, a->world.pos.y + 52.0f - this->actor.world.pos.y,
                          a->world.pos.z - this->actor.world.pos.z };
            f32 t = rel.x * this->dir.x + rel.y * this->dir.y + rel.z * this->dir.z;

            if ((t >= 0.0f) && (t <= this->reach)) {
                f32 dx = rel.x - this->dir.x * t, dy = rel.y - this->dir.y * t, dz = rel.z - this->dir.z * t;
                f32 r = 26.0f + t * 0.05f + 10.0f; // flame radius + the torch's own flame

                if ((dx * dx + dy * dy + dz * dz) < (r * r)) {
                    HtTorch_TryLight(a, play);
                }
            }
        }
    }
}

void HtFlameJet_Update(Actor* thisx, PlayState* play) {
    HtFlameJet* this = (HtFlameJet*)thisx;
    s32 hit = HtFlameJet_CheckFreezeHit(this);

    switch (this->state) {
        case FLAMEJET_BURNING:
            if (hit) {
                HtFlameJet_Freeze(this, play);
                break;
            }
            Math_StepToF(&this->reach, this->length, FLAMEJET_GROW_SPEED);
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_FIRE_PILLAR_S - SFX_FLAG);
            HtFlameJet_SpawnFire(this, play);
            if ((this->mode != 0) && (--this->timer <= 0)) {
                this->state = FLAMEJET_RESTING;
                this->timer = (this->mode == FLAMEJET_MODE_RELAY) ? FLAMEJET_RELAY_OFF_FRAMES : FLAMEJET_CYCLE_FRAMES;
                this->reach = 0.0f;
            }
            break;

        case FLAMEJET_RESTING:
            if (hit) {
                HtFlameJet_Freeze(this, play);
                break;
            }
            if (--this->timer <= 0) {
                HtFlameJet_Ignite(this);
            }
            break;

        case FLAMEJET_FROZEN:
            if (this->freezeFrames < 0) {
                break; // frozen for good
            }
            if (this->timer < FLAMEJET_THAW_WARNING) {
                // the crust flickers and fades: about to reignite
                this->iceAlpha = ((this->timer & 4) ? 120.0f : 255.0f) * this->timer / FLAMEJET_THAW_WARNING + 60.0f;
            }
            if (--this->timer <= 0) {
                this->iceAlpha = 0.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_MELT);
                HtFlameJet_Ignite(this);
            }
            break;
    }

    if (this->state == FLAMEJET_BURNING) {
        if (this->reach > 10.0f) {
            HtFlameJet_UpdateSpheres(this, this->reach);
            if (this->actor.home.rot.z & 1) {
                // only the nozzle takes the ice, not the flame it throws: the spheres gather at the mouth
                for (s32 i = 0; i < HT_FLAME_JET_SPHERES; i++) {
                    ColliderJntSphElementDim* dim = &this->elements[i].dim;

                    dim->worldSphere.center.x = this->actor.world.pos.x + this->dir.x * 15.0f;
                    dim->worldSphere.center.y = this->actor.world.pos.y + this->dir.y * 15.0f;
                    dim->worldSphere.center.z = this->actor.world.pos.z + this->dir.z * 15.0f;
                    dim->worldSphere.radius = 34;
                }
            }
            HtFlameJet_CheckPlayer(this, play);
            HtFlameJet_LightTorches(this, play);
            HtFlameJet_MeltSpouts(this, play);
            if (this->mode != FLAMEJET_MODE_RELAY) {
                CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            }
        }
    } else if ((this->state == FLAMEJET_RESTING) && (this->mode != FLAMEJET_MODE_RELAY)) {
        // a resting nozzle can be iced over too (near the mouth), which skips its next burn
        HtFlameJet_UpdateSpheres(this, 150.0f);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

void HtIce_DrawCrystals(PlayState* play, Vec3f* center, f32 radius, s32 count, s32 seed, u8 alpha) {
    Vec3f hiliteLightDir = { 89.8f, 0.0f, 89.8f };
    u32 frames = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    func_8002EB44(center, &play->view.eye, &hiliteLightDir, play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, frames & 0xFF, 0x20, 0x10, 1, 0, (frames * 2) & 0xFF,
                                  0x40, 0x20, 0, 1, 0, 2));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 200, 240, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 90, 170, 255, alpha);

    for (s32 i = 0; i < count; i++) {
        // fixed pseudo-random layout so the crystals don't jitter from frame to frame
        u32 h = (u32)(seed * 7919 + i * 104729);
        f32 a = (h % 6283) * 0.001f;
        f32 b = ((h / 6283) % 3141) * 0.001f - 1.57f;
        f32 r = radius * (0.4f + ((h >> 8) % 60) * 0.01f);
        f32 s = 0.45f + ((h >> 4) % 10) * 0.04f; // the freezard shatter uses 0.45-0.75

        Matrix_Translate(center->x + cosf(a) * cosf(b) * r, center->y + sinf(b) * r, center->z + sinf(a) * cosf(b) * r,
                         MTXMODE_NEW);
        Matrix_Scale(s, s, s, MTXMODE_APPLY);
        Matrix_RotateY(a, MTXMODE_APPLY);
        Matrix_RotateX(b, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gEffIceFragment2DL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void HtIce_DrawModel(PlayState* play, const char* dl, u8 alpha) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

void HtIce_DrawSleeve(PlayState* play, Vec3f* a, Vec3f* b, f32 radius, u8 alpha) {
    f32 dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
    f32 h = sqrtf(SQ(dx) + SQ(dz));
    f32 len = sqrtf(SQ(h) + SQ(dy));

    if (len < 1.0f) {
        return;
    }
    // the model runs up +y, 100 long, radius 10: turned to point from a to b, stretched to the length
    Matrix_Translate(a->x, a->y, a->z, MTXMODE_NEW);
    Matrix_RotateY(atan2f(dx, dz), MTXMODE_APPLY);
    Matrix_RotateX(atan2f(h, dy), MTXMODE_APPLY);
    Matrix_Scale(radius / 10.0f, len / 100.0f, radius / 10.0f, MTXMODE_APPLY);
    HtIce_DrawModel(play, HT_ICE_SLEEVE_DL, alpha);
}

void HtFlameJet_Draw(Actor* thisx, PlayState* play) {
    HtFlameJet* this = (HtFlameJet*)thisx;

    // the nozzle: an iron barrel through a brick plate, its black mouth at the actor, pointing along rotY / rotX
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(this->actor.shape.rot.x), MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_FLAME_NOZZLE_DL);
    CLOSE_DISPS(play->state.gfxCtx);

    if (this->iceAlpha > 0.0f) {
        // one block of ice casing the whole nozzle, plate to mouth (in the nozzle's own space)
        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(this->actor.shape.rot.x), MTXMODE_APPLY);
        HtIce_DrawModel(play, HT_ICE_BLOCK_DL, (u8)CLAMP(this->iceAlpha * 0.8f, 0.0f, 210.0f));
    }
}
