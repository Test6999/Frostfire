// Frostfire Mausoleum: the epitaphs. See HtEpitaph.h.

#include "HtEpitaph.h"
#include "HtKatanaPickup.h" // HT_RONIN_DEAD_FLAG
#include "HtLynel.h"        // HtLynel_ChainShown
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include <math.h>

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 22, 60, 0, { 0, 0, 0 } },
};

static s32 HtEpitaph_Visible(HtEpitaph* this, PlayState* play) {
    if (this->actor.params == 0) {
        return Flags_GetSwitch(play, HT_RONIN_DEAD_FLAG);
    }
    return HtLynel_EpitaphShown();
}

void HtEpitaph_Init(Actor* thisx, PlayState* play) {
    HtEpitaph* this = (HtEpitaph*)thisx;

    Actor_SetScale(&this->actor, 1.0f);
    this->actor.targetMode = 0;
    this->actor.textId = (this->actor.params == 0) ? HT_EPITAPH_RONIN_TEXT : HT_EPITAPH_HORSE_TEXT;
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 50.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->collider.dim.radius = 50; // (the gravestone is broad)
    // (already there: no blue fire on coming back)
    this->shown = HtEpitaph_Visible(this, play);
    this->timer = this->shown ? 100 : 0;
}

void HtEpitaph_Destroy(Actor* thisx, PlayState* play) {
    HtEpitaph* this = (HtEpitaph*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

void HtEpitaph_Update(Actor* thisx, PlayState* play) {
    HtEpitaph* this = (HtEpitaph*)thisx;
    Player* player = GET_PLAYER(play);
    s32 visible = HtEpitaph_Visible(this, play);

    if (!visible) {
        this->shown = false;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        return;
    }
    if (!this->shown) {
        this->shown = true;
        this->timer = 0;
    }
    if (this->timer < 100) {
        this->timer++;
        if ((this->actor.params == 0) && (this->timer < 40) && ((this->timer % 2) == 0)) {
            // the grave post rising out of the Ronin's blue fire
            Vec3f pos = { this->actor.world.pos.x + Rand_CenteredFloat(30.0f), this->actor.world.pos.y + Rand_ZeroFloat(60.0f),
                          this->actor.world.pos.z + Rand_CenteredFloat(20.0f) };
            Vec3f zero = { 0.0f, 0.0f, 0.0f };
            Vec3f up = { 0.0f, 1.5f, 0.0f };

            EffectSsDeadDb_Spawn(play, &pos, &up, &zero, 90, -2, 190, 235, 255, 255, 40, 110, 255, 1, 12,
                                 this->timer == 2);
        }
    }
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;

    // read from the front (as a sign is), standing close
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        // (the text runs; nothing else to do)
    } else {
        s16 yaw = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

        if ((ABS(yaw) < 0x3000) && (this->actor.xzDistToPlayer < 120.0f) &&
            (fabsf(player->actor.world.pos.y - this->actor.world.pos.y) < 40.0f)) {
            Actor_OfferTalk(&this->actor, play, 120.0f);
        }
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

void HtEpitaph_Draw(Actor* thisx, PlayState* play) {
    HtEpitaph* this = (HtEpitaph*)thisx;
    f32 rise;

    if (!this->shown) {
        return;
    }
    // the grave post rises out of the floor in its first second; the plaque is simply there
    rise = (this->actor.params == 0) ? CLAMP(this->timer / 30.0f, 0.0f, 1.0f) : 1.0f; // (it's 80 tall)
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y - (1.0f - rise) * 110.0f,
                     this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
    // both are the Royal Family's tomb stone: its crest faces its own +z, the way the actor faces (to the door, the
    // side it's read from)
    Matrix_Scale(HT_GRAVE_STONE_SCALE, HT_GRAVE_STONE_SCALE, HT_GRAVE_STONE_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_GRAVE_STONE_DL);
    CLOSE_DISPS(play->state.gfxCtx);
}
