// Heretics Tomb: chain barrier only the Ice Katana can cut. See HtChainBarrier.h for params.
//
// Why ice cuts iron: the Iai draw flash-freezes the chains and cold makes iron brittle, so the strike shatters
// them (they frost over for a moment, then burst). Ordinary blows only ring off.
// Visual placeholder: crossed locked-door chains (no padlock), thickened, repeated across the barrier, both faces.
// Registered at runtime through ActorDB with a fixed id (see Katana.h).

#include "HtChainBarrier.h"
#include "Katana.h"
#include "HtFlameJet.h" // HtIce_DrawCrystals
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include <math.h>

#define CHAIN_FREEZE_FRAMES 30 // frost runs out through the chains from the strike, then they shatter
#define CHAIN_FROST_ALPHA 190
#define CHAIN_SHARDS 18
#define CHAIN_THICKNESS 1.6f   // the door chains are thin; thicken them across their length
#define NAVI_CHAIN_TEXT_ID 0xF0 // Navi's C-up text is naviEnemyId + 0x600 (replaced by Katana.cpp)
#define NAVI_CHAIN_IAI_TEXT_ID 0xF1 // the full hint: "use an Iai strike"
#define NO_SWITCH_FLAG 0x3F

#define DMG_ANY_SWORD                                                                                     \
    (DMG_SLASH_KOKIRI | DMG_SLASH_MASTER | DMG_SLASH_GIANT | DMG_SPIN_KOKIRI | DMG_SPIN_MASTER | \
     DMG_SPIN_GIANT | DMG_JUMP_KOKIRI | DMG_JUMP_MASTER | DMG_JUMP_GIANT)

// Uncut barriers in the room, so doors behind them can refuse to open (see Katana.cpp).
#define MAX_ACTIVE_BARRIERS 16
static HtChainBarrier* sActiveBarriers[MAX_ACTIVE_BARRIERS];

static void HtChainBarrier_SetActive(HtChainBarrier* this, s32 active) {
    for (s32 i = 0; i < MAX_ACTIVE_BARRIERS; i++) {
        if (active ? (sActiveBarriers[i] == NULL) : (sActiveBarriers[i] == this)) {
            sActiveBarriers[i] = active ? this : NULL;
            return;
        }
    }
}

s32 HtChainBarrier_Separates(Vec3f* a, Vec3f* b) {
    for (s32 i = 0; i < MAX_ACTIVE_BARRIERS; i++) {
        HtChainBarrier* bar = sActiveBarriers[i];

        if (bar != NULL) {
            f32 nx = Math_SinS(bar->dyna.actor.shape.rot.y);
            f32 nz = Math_CosS(bar->dyna.actor.shape.rot.y);
            Vec3f* c = &bar->dyna.actor.world.pos;
            f32 da = (a->x - c->x) * nx + (a->z - c->z) * nz;
            f32 db = (b->x - c->x) * nx + (b->z - c->z) * nz;

            if ((da * db < 0.0f) && (Math_Vec3f_DistXZ(b, c) < 700.0f)) {
                return true;
            }
        }
    }
    return false;
}

s32 HtChainBarrier_BlocksPosition(Vec3f* pos) {
    for (s32 i = 0; i < MAX_ACTIVE_BARRIERS; i++) {
        HtChainBarrier* b = sActiveBarriers[i];
        if (b != NULL) {
            f32 dx = pos->x - b->dyna.actor.world.pos.x;
            f32 dz = pos->z - b->dyna.actor.world.pos.z;
            f32 reach = (b->width > 80.0f) ? b->width : 80.0f;

            if (((dx * dx + dz * dz) < (reach * reach)) && (fabsf(pos->y - b->dyna.actor.world.pos.y) < b->height)) {
                return true;
            }
        }
    }
    return false;
}

static ColliderCylinderInit sCylinderInit = {
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
        { DMG_ANY_SWORD | DMG_BOOMERANG, 0x00, 0x00 }, // DMG_BOOMERANG: the plain katana slash wave
        TOUCH_NONE,
        BUMP_ON | BUMP_NO_DAMAGE | BUMP_NO_HITMARK,
        OCELEM_NONE,
    },
    { 60, 120, 0, { 0, 0, 0 } },
};

void HtChainBarrier_Init(Actor* thisx, PlayState* play) {
    HtChainBarrier* this = (HtChainBarrier*)thisx;
    s32 params = this->dyna.actor.params;
    s32 widthSteps = (params >> 6) & 0xF;
    s32 heightSteps = (params >> 10) & 0xF;
    CollisionHeader* colHeader = NULL;

    this->switchFlag = params & 0x3F;
    this->width = (widthSteps == 0) ? 120.0f : widthSteps * 20.0f;
    this->height = (heightSteps == 0) ? 120.0f : heightSteps * 20.0f;

    if ((this->switchFlag != NO_SWITCH_FLAG) && Flags_GetSwitch(play, this->switchFlag)) {
        Actor_Kill(&this->dyna.actor); // already cut
        return;
    }

    // the collision model is 100x100 units, so the actor scale sizes it
    this->dyna.actor.scale.x = this->width / 100.0f;
    this->dyna.actor.scale.y = this->height / 100.0f;
    this->dyna.actor.scale.z = 1.0f;
    DynaPolyActor_Init(&this->dyna, 0);
    CollisionHeader_GetVirtual(HT_CHAIN_BARRIER_COL, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);

    // Z-targetable (green: an object, not an enemy) with a Navi hint on C-up
    this->dyna.actor.naviEnemyId =
        ((u16)this->dyna.actor.params & HT_CHAIN_BARRIER_FULL_HINT) ? NAVI_CHAIN_IAI_TEXT_ID : NAVI_CHAIN_TEXT_ID;
    this->dyna.actor.targetMode = 4; // targetable from 700 units (default 350)
    this->dyna.actor.focus.pos = this->dyna.actor.world.pos;
    this->dyna.actor.focus.pos.y += this->height * 0.5f;

    HtChainBarrier_SetActive(this, true);

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->dyna.actor, &sCylinderInit);
    this->collider.dim.radius = this->width / 2.0f;
    this->collider.dim.height = this->height;
    Collider_UpdateCylinder(&this->dyna.actor, &this->collider);
}

void HtChainBarrier_Destroy(Actor* thisx, PlayState* play) {
    HtChainBarrier* this = (HtChainBarrier*)thisx;

    HtChainBarrier_SetActive(this, false);
    if (this->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
    Collider_DestroyCylinder(play, &this->collider);
}

// Only an Iai draw cuts the chains: either its wave, or the katana blade during the Iai swing itself.
static s32 HtChainBarrier_IsIaiCut(Actor* attacker) {
    if (attacker == NULL) {
        return false;
    }
    if (attacker->id == HT_ACTOR_ICE_SLASH) {
        return attacker->params == 1; // Iai wave
    }
    if (attacker->id == ACTOR_PLAYER) {
        return Katana_IsIaiBladeHit(attacker);
    }
    return false;
}

// An Iai draw flash-freezes the iron: cold-brittle, the chains frost over for a moment and then shatter.
static void HtChainBarrier_StartShatter(HtChainBarrier* this, PlayState* play, Actor* attacker) {
    this->cutTimer = CHAIN_FREEZE_FRAMES;
    // where it struck, along the barrier: the frost starts there
    this->hitAcross = 0.0f;
    if (attacker != NULL) {
        f32 dx = attacker->world.pos.x - this->dyna.actor.world.pos.x;
        f32 dz = attacker->world.pos.z - this->dyna.actor.world.pos.z;

        this->hitAcross = CLAMP(dx * Math_CosS(this->dyna.actor.shape.rot.y) - dz * Math_SinS(this->dyna.actor.shape.rot.y),
                                -this->width / 2.0f, this->width / 2.0f);
    }
    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    this->dyna.bgId = BGACTOR_NEG_ONE;
    this->dyna.actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    HtChainBarrier_SetActive(this, false);
    if (this->switchFlag != NO_SWITCH_FLAG) {
        Flags_SetSwitch(play, this->switchFlag);
    }
    Audio_PlayActorSound2(&this->dyna.actor, NA_SE_IT_SWORD_STRIKE_HARD);
    HtSfx_PlayFor(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y, this->dyna.actor.world.pos.z,
                  NA_SE_EV_ICE_FREEZE, CHAIN_FREEZE_FRAMES);
}

static void HtChainBarrier_Shatter(HtChainBarrier* this, PlayState* play) {
    static Color_RGBA8 prim = { 200, 240, 255, 255 };
    static Color_RGBA8 env = { 90, 170, 255, 255 };
    f32 c = Math_CosS(this->dyna.actor.shape.rot.y);
    f32 sn = Math_SinS(this->dyna.actor.shape.rot.y);

    Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_ICE_BROKEN);
    Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_CHAIN_KEY_UNLOCK);
    for (s32 i = 0; i < CHAIN_SHARDS; i++) {
        // shards from all over the barrier face, flung out to both sides
        f32 across = Rand_CenteredFloat(this->width);
        Vec3f pos = { this->dyna.actor.world.pos.x + c * across,
                      this->dyna.actor.world.pos.y + Rand_ZeroFloat(this->height),
                      this->dyna.actor.world.pos.z - sn * across };
        f32 out = Rand_CenteredFloat(8.0f);
        Vec3f vel = { sn * out + Rand_CenteredFloat(3.0f), Rand_ZeroFloat(5.0f) + 2.0f, c * out + Rand_CenteredFloat(3.0f) };
        Vec3f accel = { 0.0f, -1.0f, 0.0f };

        EffectSsEnIce_Spawn(play, &pos, Rand_ZeroFloat(0.3f) + 0.35f, &vel, &accel, &prim, &env, 25);
    }
    Actor_Kill(&this->dyna.actor);
}

void HtChainBarrier_Update(Actor* thisx, PlayState* play) {
    HtChainBarrier* this = (HtChainBarrier*)thisx;

    if (this->cutTimer > 0) {
        if (--this->cutTimer == 0) {
            HtChainBarrier_Shatter(this, play);
        }
        return;
    }

    if (this->clangCooldown > 0) {
        this->clangCooldown--;
    }

    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (HtChainBarrier_IsIaiCut(this->collider.base.ac)) {
            HtChainBarrier_StartShatter(this, play, this->collider.base.ac);
            return;
        }
        if (this->clangCooldown == 0) {
            // anything else just clangs off the iron
            Vec3f sparkPos = this->dyna.actor.world.pos;

            sparkPos.y += this->height * 0.5f;
            CollisionCheck_SpawnShieldParticlesMetal(play, &sparkPos);
            Audio_PlayActorSound2(&this->dyna.actor, NA_SE_IT_SHIELD_REFLECT_SW);
            this->clangCooldown = 10;
        }
    }

    {
        // Targeting needs a clear line from Link to the focus point, and the barrier's own wall would block a
        // focus at its centre: keep the focus just off the face Link is looking at.
        Player* player = GET_PLAYER(play);
        f32 nx = Math_SinS(this->dyna.actor.shape.rot.y);
        f32 nz = Math_CosS(this->dyna.actor.shape.rot.y);
        f32 side = ((player->actor.world.pos.x - this->dyna.actor.world.pos.x) * nx +
                    (player->actor.world.pos.z - this->dyna.actor.world.pos.z) * nz) >= 0.0f
                       ? 1.0f
                       : -1.0f;

        this->dyna.actor.focus.pos.x = this->dyna.actor.world.pos.x + nx * side * 14.0f;
        this->dyna.actor.focus.pos.y = this->dyna.actor.world.pos.y + this->height * 0.5f;
        this->dyna.actor.focus.pos.z = this->dyna.actor.world.pos.z + nz * side * 14.0f;
    }

    Collider_UpdateCylinder(&this->dyna.actor, &this->collider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

// One crossed pair of locked-door chains (DOORLOCK_NORMAL layout, no padlock), drawn into the current matrix.
// Each of the 4 chain pieces runs from the centre to a corner of a 60 x 100 door (at scale 0.01), so the
// pieces are thickened across their own axis without stretching their length.
// frost: 0 (none) .. 1 (the arms sheathed in ice to their ends), growing out from the pair's middle
static void HtChainBarrier_DrawChainX(PlayState* play, f32 frost, f32 frostRadius) {
    static const f32 chainAngle = 0.54f; // from vertical, toward the door corners
    MtxF baseMtxF;
    f32 rotZ = 0.0f;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(0.0f, 5000.0f, 500.0f, MTXMODE_APPLY);
    Matrix_Get(&baseMtxF);

    for (s32 i = 0; i < 4; i++) {
        // The door chain is flat (drawn in the door's plane), so scaling it only makes it wider. For depth, draw
        // it a second time turned a quarter turn around its own length: seen from any side, it has body.
        for (s32 turn = 0; turn < 2; turn++) {
            Matrix_Put(&baseMtxF);
            Matrix_RotateZ(rotZ, MTXMODE_APPLY);
            Matrix_RotateZ(chainAngle, MTXMODE_APPLY); // now the chain runs along +Y
            if (turn) {
                Matrix_RotateY(M_PI / 2.0f, MTXMODE_APPLY);
            }
            Matrix_Scale(CHAIN_THICKNESS, 1.0f, CHAIN_THICKNESS, MTXMODE_APPLY);
            Matrix_RotateZ(-chainAngle, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPDisplayList(POLY_OPA_DISP++, gDoorChainDL);
        }
        if (frost > 0.0f) {
            // the arm runs from the pair's middle to a corner of the 60 x 100 door shape (5831 model units)
            static Vec3f sMid = { 0.0f, 0.0f, 0.0f };
            static Vec3f sEnd = { 0.0f, 5831.0f, 0.0f };
            Vec3f a, b;

            Matrix_Put(&baseMtxF);
            Matrix_RotateZ(rotZ + chainAngle, MTXMODE_APPLY);
            Matrix_MultVec3f(&sMid, &a);
            Matrix_MultVec3f(&sEnd, &b);
            b.x = a.x + (b.x - a.x) * frost;
            b.y = a.y + (b.y - a.y) * frost;
            b.z = a.z + (b.z - a.z) * frost;
            HtIce_DrawSleeve(play, &a, &b, frostRadius, CHAIN_FROST_ALPHA);
        }
        rotZ += (i % 2) ? (2.0f * chainAngle) : (M_PI - (2.0f * chainAngle));
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// a point on the barrier's face: `across` along it from its middle, `up` from the floor
static Vec3f HtChainBarrier_Point(HtChainBarrier* this, f32 across, f32 up) {
    Vec3f p = { this->dyna.actor.world.pos.x + Math_CosS(this->dyna.actor.shape.rot.y) * across,
                this->dyna.actor.world.pos.y + up,
                this->dyna.actor.world.pos.z - Math_SinS(this->dyna.actor.shape.rot.y) * across };
    return p;
}

// how far the frost has run along a chain at `across` (0..1): it sets off from the strike and reaches the far end of
// the barrier a little over halfway through the freeze; each chain then takes the rest to be sheathed
static f32 HtChainBarrier_FrostAt(HtChainBarrier* this, f32 across) {
    f32 p, delay;

    if (this->cutTimer <= 0) {
        return 0.0f;
    }
    p = 1.0f - (f32)this->cutTimer / CHAIN_FREEZE_FRAMES;
    delay = fabsf(across - this->hitAcross) / MAX(this->width, 1.0f) * 0.55f;
    return CLAMP((p - delay) / 0.4f, 0.0f, 1.0f);
}

#define BAR_SPACING 30.0f
#define BAR_ROWS 3 // horizontal chains across the bars

void HtChainBarrier_Draw(Actor* thisx, PlayState* play) {
    HtChainBarrier* this = (HtChainBarrier*)thisx;
    // A crossed pair is a 60 x 100 shape; size it to the barrier height (uniform scale, so links keep their
    // shape) and repeat it across the width.
    f32 k = this->height / 100.0f;
    s32 count = (s32)(this->width / (60.0f * k) + 0.5f);
    f32 spacing;

    if (count < 1) {
        count = 1;
    }
    spacing = this->width / count;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);

    if (this->dyna.actor.params & HT_CHAIN_BARRIER_BARS) {
        // prison bars: straight hanging chains every BAR_SPACING units across the width
        s32 bars = (s32)(this->width / BAR_SPACING);

        if (bars < 1) {
            bars = 1;
        }
        for (s32 n = 0; n < bars; n++) {
            Matrix_Translate(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y,
                             this->dyna.actor.world.pos.z, MTXMODE_NEW);
            Matrix_RotateY(BINANG_TO_RAD(this->dyna.actor.shape.rot.y), MTXMODE_APPLY);
            Matrix_Translate(-this->width / 2.0f + (this->width / bars) * (n + 0.5f), 0.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(1.0f, this->height / 100.0f, 1.0f, MTXMODE_APPLY);
            OPEN_DISPS(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_CHAIN_BAR_DL); // a continuous chain, floor to top
            CLOSE_DISPS(play->state.gfxCtx);
        }
        // and a few chains strung straight across, tying the bars together
        for (s32 n = 0; n < BAR_ROWS; n++) {
            Matrix_Translate(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y,
                             this->dyna.actor.world.pos.z, MTXMODE_NEW);
            Matrix_RotateY(BINANG_TO_RAD(this->dyna.actor.shape.rot.y), MTXMODE_APPLY);
            Matrix_Translate(-this->width / 2.0f, this->height * (n + 1) / (BAR_ROWS + 1), 0.0f, MTXMODE_APPLY);
            Matrix_RotateZ(-M_PI / 2.0f, MTXMODE_APPLY); // the chain model runs up +Y: lay it along +X
            Matrix_Scale(1.0f, this->width / 100.0f, 1.0f, MTXMODE_APPLY);
            OPEN_DISPS(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_CHAIN_BAR_DL);
            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
    for (s32 side = 0; side < 2 && !(this->dyna.actor.params & HT_CHAIN_BARRIER_BARS); side++) {
        for (s32 n = 0; n < count; n++) {
            Matrix_Translate(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y,
                             this->dyna.actor.world.pos.z, MTXMODE_NEW);
            Matrix_RotateY(BINANG_TO_RAD(this->dyna.actor.shape.rot.y + (side ? 0x8000 : 0)), MTXMODE_APPLY);
            Matrix_Translate(-this->width / 2.0f + spacing * (n + 0.5f), 0.0f, 0.0f, MTXMODE_APPLY);
            // a quarter turn about the vertical: the crossed pair stood edge-on to the passage (seen sideways)
            Matrix_RotateY(M_PI / 2.0f, MTXMODE_APPLY);
            Matrix_Scale(0.01f * k, 0.01f * k, 0.01f * k, MTXMODE_APPLY);
            // (the far face's pairs are drawn turned round: their place along the barrier is mirrored)
            f32 across = -this->width / 2.0f + spacing * (n + 0.5f);

            HtChainBarrier_DrawChainX(play, HtChainBarrier_FrostAt(this, side ? -across : across), 5.0f * k);
        }
    }

    if ((this->cutTimer > 0) && (this->dyna.actor.params & HT_CHAIN_BARRIER_BARS)) {
        // the bars: frost runs up and down each bar from its middle, the bars nearest the strike first, and along the
        // chains strung across from the strike out to both ends
        s32 bars = MAX((s32)(this->width / BAR_SPACING), 1);
        f32 p = 1.0f - (f32)this->cutTimer / CHAIN_FREEZE_FRAMES;

        for (s32 n = 0; n < bars; n++) {
            f32 across = -this->width / 2.0f + (this->width / bars) * (n + 0.5f);
            f32 q = HtChainBarrier_FrostAt(this, across);
            Vec3f mid = HtChainBarrier_Point(this, across, this->height / 2.0f);
            Vec3f lo = HtChainBarrier_Point(this, across, this->height / 2.0f * (1.0f - q));
            Vec3f hi = HtChainBarrier_Point(this, across, this->height / 2.0f * (1.0f + q));

            if (q > 0.0f) {
                HtIce_DrawSleeve(play, &mid, &lo, 6.0f, CHAIN_FROST_ALPHA);
                HtIce_DrawSleeve(play, &mid, &hi, 6.0f, CHAIN_FROST_ALPHA);
            }
        }
        for (s32 n = 0; n < BAR_ROWS; n++) {
            f32 y = this->height * (n + 1) / (BAR_ROWS + 1);
            f32 q = CLAMP(p / 0.7f, 0.0f, 1.0f);
            Vec3f from = HtChainBarrier_Point(this, this->hitAcross, y);

            for (s32 dir = -1; dir <= 1; dir += 2) {
                f32 edge = dir * this->width / 2.0f;
                Vec3f to = HtChainBarrier_Point(this, this->hitAcross + (edge - this->hitAcross) * q, y);

                HtIce_DrawSleeve(play, &from, &to, 5.0f, CHAIN_FROST_ALPHA);
            }
        }
    }
}
