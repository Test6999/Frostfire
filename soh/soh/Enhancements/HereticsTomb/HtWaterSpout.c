// Heretics Tomb: water spout the Ice Katana freezes into a pillar to stand on. See HtWaterSpout.h for params.
//
// Looks: the spout is a gushing column of the vanilla diving splash, crowned with a bigger burst and ripples on
// the surface; the frozen pillar is the Ice Cavern push block, stacked and fitted. (HtWaterSpout_DrawColumn, a
// textured water cylinder, is kept for reference but unused.)
// Registered at runtime through ActorDB with a fixed id (see Katana.h).

#include "HtWaterSpout.h"
#include "HtFlameJet.h" // HtIce_DrawCrystals
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/object_mizu_objects/object_mizu_objects.h"   // Water Temple water texture
#include "objects/object_hakach_objects/object_hakach_objects.h" // Bottom of the Well waterfall streaks
#include "objects/object_ice_objects/object_ice_objects.h"
#include <math.h>

enum {
    SPOUT_FLOWING,
    SPOUT_FREEZING, // ice climbs the column
    SPOUT_FROZEN,
    SPOUT_MELTING,  // the pillar sinks back into the water
};

#define SPOUT_FREEZE_FRAMES 10
#define SPOUT_MELT_FRAMES 16

#define SPOUT_DEFAULT_FREEZE_SEC 10
#define SPOUT_PERMANENT 63
#define SPOUT_THAW_WARNING 40   // the ice flickers for the last 2 s
#define SPOUT_OVERSHOOT 60.0f   // the flowing spout shoots this far above the pillar's top
#define SPOUT_RADIUS 28.0f
#define COLUMN_SEGMENTS 12      // sides of the water column
// The Ice Cavern push block (object_ice_objects_DL_000190) is 1200 x 600 x 1200 model units, base at y 0.
#define ICE_BLOCK_WIDTH 1200.0f
#define ICE_BLOCK_HEIGHT 600.0f

// Unit water column: radius 100, height 100, open at both ends. The texture wraps twice around and is stretched
// once over the whole height (long streaks rather than a repeated tile).
static Vtx sColumnVtx[2 * (COLUMN_SEGMENTS + 1)];
static s32 sColumnVtxReady = false;

static void HtWaterSpout_BuildColumn(void) {
    for (s32 i = 0; i <= COLUMN_SEGMENTS; i++) {
        f32 a = (2.0f * M_PI * i) / COLUMN_SEGMENTS;
        s16 x = (s16)(cosf(a) * 100.0f);
        s16 z = (s16)(sinf(a) * 100.0f);
        s16 s = (s16)((i * 2 * 1024) / COLUMN_SEGMENTS);
        Vtx bottom = VTX(x, 0, z, s, 1024, 255, 255, 255, 255);
        Vtx top = VTX(x, 100, z, s, 0, 255, 255, 255, 255);

        sColumnVtx[2 * i] = bottom;
        sColumnVtx[2 * i + 1] = top;
    }
    sColumnVtxReady = true;
}

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        // the katana slash wave (boomerang type), the Iai wave (Biggoron spin type) and ice arrows
        { DMG_BOOMERANG | DMG_SPIN_GIANT | DMG_ARROW_ICE, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON | BUMP_NO_DAMAGE | BUMP_NO_HITMARK | BUMP_NO_SWORD_SFX,
        OCELEM_NONE,
    },
    { 35, 100, 0, { 0, 0, 0 } },
};

void HtWaterSpout_Init(Actor* thisx, PlayState* play) {
    HtWaterSpout* this = (HtWaterSpout*)thisx;
    s32 params = this->dyna.actor.params;
    s32 freezeSec = params & 0x3F;
    s32 steps = (params >> 6) & 0xF;

    this->height = (steps == 0) ? 160.0f : steps * ((params & 0x1000) ? 40.0f : 20.0f);
    this->width = 80.0f + ((params >> 10) & 3) * 40.0f;
    if (freezeSec == SPOUT_PERMANENT) {
        this->freezeFrames = -1;
    } else {
        this->freezeFrames = ((freezeSec == 0) ? SPOUT_DEFAULT_FREEZE_SEC : freezeSec) * 20;
    }

    // the pillar's collision is a 100-unit cube: the actor scale sizes it
    this->dyna.actor.scale.x = this->width / 100.0f;
    this->dyna.actor.scale.y = this->height / 100.0f;
    this->dyna.actor.scale.z = this->width / 100.0f;
    DynaPolyActor_Init(&this->dyna, 0);
    CollisionHeader_GetVirtual(HT_ICE_PILLAR_COL, &this->colHeader);
    this->dyna.bgId = BGACTOR_NEG_ONE; // no pillar until frozen

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->dyna.actor, &sCylinderInit);
    // reaches well above the spray, so a cut from a higher pillar or ledge still catches it (no getting stranded)
    this->collider.dim.height = this->height + SPOUT_OVERSHOOT + 80.0f;
    Collider_UpdateCylinder(&this->dyna.actor, &this->collider);

    if (!sColumnVtxReady) {
        HtWaterSpout_BuildColumn();
    }
    this->state = SPOUT_FLOWING;
    this->grow = (params & HT_WATER_SPOUT_ERUPT) ? 0.0f : 1.0f;
    this->dyna.actor.uncullZoneForward = 2000.0f;
    this->dyna.actor.uncullZoneScale = this->height + SPOUT_OVERSHOOT;
    this->dyna.actor.uncullZoneDownward = this->height + SPOUT_OVERSHOOT;
}

void HtWaterSpout_Destroy(Actor* thisx, PlayState* play) {
    HtWaterSpout* this = (HtWaterSpout*)thisx;

    if (this->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
    Collider_DestroyCylinder(play, &this->collider);
}

static s32 HtWaterSpout_CheckFreezeHit(HtWaterSpout* this) {
    s32 frozen = false;

    if (this->collider.base.acFlags & AC_HIT) {
        Actor* attacker = this->collider.base.ac;
        ColliderInfo* info = &this->collider.info;

        if (info->acHitInfo != NULL) {
            u32 flags = info->acHitInfo->toucher.dmgFlags;
            // the plain boomerang shares the wave's damage type, so check who threw it
            if ((flags & (DMG_ARROW_ICE | DMG_SPIN_GIANT)) ||
                ((attacker != NULL) && (attacker->id == HT_ACTOR_ICE_SLASH))) {
                frozen = true;
            }
        }
        this->collider.base.acFlags &= ~AC_HIT;
    }
    return frozen;
}

// (defined below)
static void HtWaterSpout_StartMelt(HtWaterSpout* this, PlayState* play);

void HtWaterSpout_Thaw(Actor* thisx, PlayState* play) {
    HtWaterSpout* this = (HtWaterSpout*)thisx;

    if ((this->state == SPOUT_FREEZING) || ((this->state == SPOUT_FROZEN) && (this->freezeFrames >= 0))) {
        HtWaterSpout_StartMelt(this, play);
    }
}

// The pillar's collision follows its current height (the actor's y scale), so it rises and sinks too.
s32 HtWaterSpout_IsSolid(Actor* thisx) {
    HtWaterSpout* this = (HtWaterSpout*)thisx;

    return (this->state == SPOUT_FREEZING) || (this->state == SPOUT_FROZEN);
}

f32 HtWaterSpout_Width(Actor* thisx) {
    return ((HtWaterSpout*)thisx)->width;
}

static void HtWaterSpout_SetProgress(HtWaterSpout* this, f32 progress) {
    this->progress = CLAMP(progress, 0.0f, 1.0f);
    this->dyna.actor.scale.y = (this->height / 100.0f) * ((this->progress > 0.02f) ? this->progress : 0.02f);
}

static void HtWaterSpout_Freeze(HtWaterSpout* this, PlayState* play) {
    this->state = SPOUT_FREEZING;
    this->timer = this->freezeFrames;
    this->iceAlpha = 255.0f;
    HtWaterSpout_SetProgress(this, 0.0f);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, this->colHeader);
    HtSfx_PlayFor(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y + this->height,
                  this->dyna.actor.world.pos.z, NA_SE_EN_TWINROBA_SHOOT_FREEZE, 20);
}

static void HtWaterSpout_StartMelt(HtWaterSpout* this, PlayState* play) {
    this->state = SPOUT_MELTING;
    this->iceAlpha = 255.0f;
    Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_ICE_MELT);
}

static void HtWaterSpout_FinishMelt(HtWaterSpout* this, PlayState* play) {
    this->state = SPOUT_FLOWING;
    this->iceAlpha = 0.0f;
    HtWaterSpout_SetProgress(this, 0.0f);
    if (this->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
        this->dyna.bgId = BGACTOR_NEG_ONE;
    }
}

// The flowing spout: the column itself is drawn; add spray at the top and ripples on the water around it.
static void HtWaterSpout_Flow(HtWaterSpout* this, PlayState* play) {
    Vec3f pos = this->dyna.actor.world.pos;
    WaterBox* waterBox;
    f32 surface;

    f32 bottom = this->dyna.actor.world.pos.y;
    f32 top;

    Math_StepToF(&this->grow, 1.0f, 1.0f / 15.0f); // (an erupting spout shoots up in 15 frames)
    top = bottom + (this->height + SPOUT_OVERSHOOT) * this->grow;

    Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_FOUNTAIN - SFX_FLAG);
    // The column is built from the splash you get diving into water: two bursts a frame at random heights
    // between the water surface and the top, and a bigger one crowning it every few frames.
    if (WaterBox_GetSurface1(play, &play->colCtx, pos.x, pos.z, &surface, &waterBox) && (surface > bottom)) {
        bottom = surface;
    }
    for (s32 i = 0; i < 2; i++) {
        pos.x = this->dyna.actor.world.pos.x + Rand_CenteredFloat(12.0f);
        pos.z = this->dyna.actor.world.pos.z + Rand_CenteredFloat(12.0f);
        pos.y = bottom + Rand_ZeroFloat(MAX(top - bottom - 30.0f, 1.0f));
        EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 450 + (s16)Rand_ZeroFloat(200.0f));
    }
    if (--this->splashTimer > 0) {
        return;
    }
    this->splashTimer = 3;
    pos.x = this->dyna.actor.world.pos.x;
    pos.z = this->dyna.actor.world.pos.z;
    pos.y = top - 25.0f;
    EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 800);
    if (bottom > this->dyna.actor.world.pos.y) {
        pos.y = bottom;
        EffectSsGRipple_Spawn(play, &pos, 300, 900, 0);
    }
}

void HtWaterSpout_Update(Actor* thisx, PlayState* play) {
    HtWaterSpout* this = (HtWaterSpout*)thisx;
    s32 hit = HtWaterSpout_CheckFreezeHit(this);

    if (this->state == SPOUT_FLOWING) {
        if (hit) {
            HtWaterSpout_Freeze(this, play);
        } else {
            HtWaterSpout_Flow(this, play);
            Collider_UpdateCylinder(&this->dyna.actor, &this->collider);
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
        return;
    }

    if (this->state == SPOUT_FREEZING) {
        HtWaterSpout_SetProgress(this, this->progress + 1.0f / SPOUT_FREEZE_FRAMES);
        if (this->progress >= 1.0f) {
            this->state = SPOUT_FROZEN;
        }
        return;
    }
    if (this->state == SPOUT_MELTING) {
        HtWaterSpout_SetProgress(this, this->progress - 1.0f / SPOUT_MELT_FRAMES);
        if (this->progress <= 0.0f) {
            HtWaterSpout_FinishMelt(this, play);
        }
        return;
    }

    // frozen
    if (this->freezeFrames < 0) {
        return; // frozen for good
    }
    if (this->timer < SPOUT_THAW_WARNING) {
        this->iceAlpha = ((this->timer & 4) ? 120.0f : 255.0f) * this->timer / SPOUT_THAW_WARNING + 60.0f;
    }
    if (--this->timer <= 0) {
        HtWaterSpout_StartMelt(this, play);
    }
}

static void HtWaterSpout_DrawColumn(HtWaterSpout* this, PlayState* play, f32 fade) {
    u32 frames = play->gameplayFrames;
    f32 h = this->height + SPOUT_OVERSHOOT;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BACK | G_LIGHTING);
    // The setup is two-cycle: the second cycle must pass the first one's result through. (Using the texture again
    // there reads the next tile, which was never loaded: that was the black / grey-blue flickering cylinder.)
    // Colour and alpha blend from the environment colour (deep, see-through blue) where the texture is dark to the
    // primitive colour (bright foam) where it's light.
    gDPSetCombineLERP(POLY_XLU_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, PRIMITIVE, ENVIRONMENT, TEXEL0,
                      ENVIRONMENT, 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);

    // Inner core: the Water Temple's water (grey caustics). Outer skin: the Bottom of the Well waterfall streaks.
    // Both tinted blue by the primitive colour and scrolled upward at different speeds.
    for (s32 layer = 0; layer < 2; layer++) {
        f32 r = SPOUT_RADIUS * (layer ? 1.0f : 0.7f);
        s32 scroll = (-(s32)frames * (layer ? 10 : 6)) & 0x7F; // 32 texels = 128 in 10.2; negative = upward

        if (layer == 0) {
            gDPLoadTextureBlock(POLY_XLU_DISP++, object_mizu_objectsTex_007520, G_IM_FMT_IA, G_IM_SIZ_16b, 32, 32,
                                0, G_TX_WRAP, G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 120, 185, 140, (u8)(110 * fade)); // the tomb's water: green
            gDPSetEnvColor(POLY_XLU_DISP++, 20, 65, 35, (u8)(35 * fade));
        } else {
            gDPLoadTextureBlock_4b(POLY_XLU_DISP++, gBotwTex_0060F0, G_IM_FMT_I, 32, 32, 0, G_TX_WRAP, G_TX_WRAP, 5,
                                   5, G_TX_NOLOD, G_TX_NOLOD);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 170, 215, 180, (u8)(95 * fade));
            gDPSetEnvColor(POLY_XLU_DISP++, 30, 85, 50, 0);
        }
        gDPSetTileSize(POLY_XLU_DISP++, G_TX_RENDERTILE, 0, scroll, 31 << 2, scroll + (31 << 2));
        Matrix_Translate(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y, this->dyna.actor.world.pos.z,
                         MTXMODE_NEW);
        Matrix_Scale(r / 100.0f, h / 100.0f, r / 100.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(POLY_XLU_DISP++, sColumnVtx, 2 * (COLUMN_SEGMENTS + 1), 0);
        for (s32 i = 0; i < COLUMN_SEGMENTS; i++) {
            s32 b0 = 2 * i, t0 = 2 * i + 1, b1 = 2 * i + 2, t1 = 2 * i + 3;
            gSP2Triangles(POLY_XLU_DISP++, b0, b1, t1, 0, b0, t1, t0, 0);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// Frozen: Ice Cavern push blocks stacked into a pillar of the right width and height.
static void HtWaterSpout_DrawPillar(HtWaterSpout* this, PlayState* play) {
    s32 blocks = (s32)(this->height / 60.0f + 0.5f);
    f32 blockH;
    f32 sxz = this->width / ICE_BLOCK_WIDTH;

    if (blocks < 1) {
        blocks = 1;
    }
    blockH = this->height * this->progress / blocks;
    for (s32 i = 0; i < blocks; i++) {
        Matrix_Translate(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y + blockH * i,
                         this->dyna.actor.world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(i * 0x2000), MTXMODE_APPLY); // vary the faces a little
        Matrix_Scale(sxz, blockH / ICE_BLOCK_HEIGHT, sxz, MTXMODE_APPLY);
        Gfx_DrawDListOpa(play, object_ice_objects_DL_000190);
    }
}

void HtWaterSpout_Draw(Actor* thisx, PlayState* play) {
    HtWaterSpout* this = (HtWaterSpout*)thisx;

    // the stone mouth the water comes out of (an erupting spout has none: it bursts from the floor)
    if (!(this->dyna.actor.params & HT_WATER_SPOUT_ERUPT)) {
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Translate(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y, this->dyna.actor.world.pos.z,
                         MTXMODE_NEW);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_SPOUT_MOUTH_DL);
        CLOSE_DISPS(play->state.gfxCtx);
    }

    if (this->state == SPOUT_FLOWING) {
        return; // the flowing spout is all splash effects (see HtWaterSpout_Flow)
    }
    if ((this->state != SPOUT_FROZEN) || (this->iceAlpha >= 255.0f) || (this->timer & 4)) {
        // blinks while it's about to thaw
        HtWaterSpout_DrawPillar(this, play);
    }
}
