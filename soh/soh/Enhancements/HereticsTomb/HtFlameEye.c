// Heretics Tomb: the silver eye that shoots flames. See HtFlameEye.h.

#include "HtFlameEye.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include <math.h>

#define FLAME_SPEED 8.0f
#define FLAME_LIFE 300     // frames before it burns out on its own
#define FLAME_RANGE 1400.0f // Link must be this close for the eye to fire
#define FLAME_SCALE 0.006f

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x01, 0x08 }, // fire: burns Link for half a heart
        // a sword cut (the katana), the frost wave (boomerang damage type), ice arrows and Ice magic put it out
        { DMG_SWORD | DMG_BOOMERANG, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 14, 28, -14, { 0, 0, 0 } },
};

void HtFlameEye_Init(Actor* thisx, PlayState* play) {
    HtFlameEye* this = (HtFlameEye*)thisx;
    s32 sec = thisx->params & 0x1F;

    Actor_SetScale(thisx, 0.1f); // the eye switch's own scale
    this->period = ((sec == 0) ? 4 : sec) * 20;
    this->timer = this->period;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    thisx->uncullZoneForward = 3000.0f;
}

void HtFlameEye_Destroy(Actor* thisx, PlayState* play) {
    HtFlameEye* this = (HtFlameEye*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void HtFlameEye_PutOut(HtFlameEye* this, PlayState* play) {
    static Color_RGBA8 prim = { 220, 245, 255, 255 };
    static Color_RGBA8 env = { 90, 170, 255, 255 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f drift = { 0.0f, 0.5f, 0.0f };
    s32 i;

    for (i = 0; i < 8; i++) {
        Vec3f pos = this->flamePos;

        pos.x += Rand_CenteredFloat(20.0f);
        pos.y += Rand_CenteredFloat(20.0f);
        pos.z += Rand_CenteredFloat(20.0f);
        EffectSsKiraKira_SpawnDispersed(play, &pos, &zero, &drift, &prim, &env, 2500, 16);
    }
    HtSfx_PlayFor(this->flamePos.x, this->flamePos.y, this->flamePos.z, NA_SE_EN_EXTINCT, 10);
    this->flameLife = 0;
}

void HtFlameEye_Update(Actor* thisx, PlayState* play) {
    HtFlameEye* this = (HtFlameEye*)thisx;
    Player* player = GET_PLAYER(play);

    this->scroll++;
    if (this->flameLife > 0) {
        Vec3f next, hit;
        CollisionPoly* poly;
        s32 bgId;

        if (this->collider.base.acFlags & AC_HIT) {
            this->collider.base.acFlags &= ~AC_HIT;
            HtFlameEye_PutOut(this, play);
            return;
        }
        if (this->collider.base.atFlags & AT_HIT) {
            this->collider.base.atFlags &= ~AT_HIT;
            this->flameLife = 0; // it burned him: spent
            return;
        }
        next.x = this->flamePos.x + this->flameVel.x;
        next.y = this->flamePos.y + this->flameVel.y;
        next.z = this->flamePos.z + this->flameVel.z;
        if ((--this->flameLife <= 0) ||
            BgCheck_EntityLineTest1(&play->colCtx, &this->flamePos, &next, &hit, &poly, true, true, true, true, &bgId)) {
            this->flameLife = 0;
            return;
        }
        this->flamePos = next;
        this->collider.dim.pos.x = this->flamePos.x;
        this->collider.dim.pos.y = this->flamePos.y;
        this->collider.dim.pos.z = this->flamePos.z;
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        Audio_PlayActorSound2(thisx, NA_SE_EV_FIRE_PILLAR_S - SFX_FLAG);
    }

    // the eye: half shut while it gathers, wide open as it fires
    if (this->timer > 0) {
        this->timer--;
    }
    this->eyeTex = (this->timer < 15) ? 1 : 0;
    if ((this->timer == 0) && (this->flameLife == 0)) {
        if (Actor_WorldDistXYZToActor(thisx, &player->actor) < FLAME_RANGE) {
            f32 fx = Math_SinS(thisx->shape.rot.y), fz = Math_CosS(thisx->shape.rot.y);
            Vec3f d;
            f32 len;

            this->flamePos.x = thisx->world.pos.x + fx * 20.0f;
            this->flamePos.y = thisx->world.pos.y;
            this->flamePos.z = thisx->world.pos.z + fz * 20.0f;
            // straight at where Link is now (his chest)
            d.x = player->actor.world.pos.x - this->flamePos.x;
            d.y = (player->actor.world.pos.y + 30.0f) - this->flamePos.y;
            d.z = player->actor.world.pos.z - this->flamePos.z;
            len = sqrtf(SQ(d.x) + SQ(d.y) + SQ(d.z));
            if (len > 1.0f) {
                this->flameVel.x = d.x / len * FLAME_SPEED;
                this->flameVel.y = d.y / len * FLAME_SPEED;
                this->flameVel.z = d.z / len * FLAME_SPEED;
                this->flameLife = FLAME_LIFE;
                this->collider.base.acFlags &= ~AC_HIT;
                this->collider.base.atFlags &= ~AT_HIT;
                Audio_PlayActorSound2(thisx, NA_SE_EV_FLAME_IGNITION);
            }
        }
        this->timer = this->period;
    }
}

void HtFlameEye_Draw(Actor* thisx, PlayState* play) {
    static void* sEyeTex[] = { gEyeSwitchSilverOpenTex, gEyeSwitchSilverHalfTex, gEyeSwitchSilverClosedTex };
    HtFlameEye* this = (HtFlameEye*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTex[this->eyeTex]));
    gSPDisplayList(POLY_OPA_DISP++, gEyeSwitch2DL);

    if (this->flameLife > 0) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0, (this->scroll * -20) & 0x1FF, 0x20,
                                      0x80, 0, 0, 0, -20));
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 220, 120, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 40, 0, 0);
        Matrix_Translate(this->flamePos.x, this->flamePos.y - 12.0f, this->flamePos.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(FLAME_SCALE, FLAME_SCALE, FLAME_SCALE, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
