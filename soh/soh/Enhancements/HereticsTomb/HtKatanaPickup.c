// Heretics Tomb: the Ice Katana left on the floor by the Frozen Ronin. See HtKatanaPickup.h.

#include "HtKatanaPickup.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"

#define SAYA_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_saya_dl"

static Color_RGBA8 sFrostPrim = { 220, 245, 255, 255 };
static Color_RGBA8 sFrostEnv = { 90, 170, 255, 0 };

void HtKatanaPickup_Init(Actor* thisx, PlayState* play) {
    HtKatanaPickup* this = (HtKatanaPickup*)thisx;

    if (Flags_GetTreasure(play, HT_KATANA_TREASURE_FLAG)) {
        Actor_Kill(&this->actor); // already taken
        return;
    }
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.shape.rot.x = this->actor.shape.rot.z = 0;
}

// params 1: the arena's own katana, waiting (unseen, not to be taken) for the Ronin to fall
static s32 HtKatanaPickup_Waiting(HtKatanaPickup* this, PlayState* play) {
    return (this->actor.params == 1) && !Flags_GetSwitch(play, HT_RONIN_DEAD_FLAG);
}

void HtKatanaPickup_Destroy(Actor* thisx, PlayState* play) {
}

void HtKatanaPickup_Update(Actor* thisx, PlayState* play) {
    HtKatanaPickup* this = (HtKatanaPickup*)thisx;

    if (HtKatanaPickup_Waiting(this, play)) {
        return;
    }
    if (Actor_HasParent(&this->actor, play)) {
        // Link has it: the get-item scene (fanfare, text, the katana granted) is the player code's from here
        Flags_SetTreasure(play, HT_KATANA_TREASURE_FLAG);
        Actor_Kill(&this->actor);
        return;
    }
    Actor_OfferGetItem(&this->actor, play, GI_SWORD_BGS, 60.0f, 40.0f);

    // a little frost drifting off it, so it reads from across the room
    if ((this->timer++ % 6) == 0) {
        f32 along = Rand_CenteredFloat(50.0f);
        Vec3f pos = { this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y + 0x4000) * along,
                      this->actor.world.pos.y + 6.0f,
                      this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y + 0x4000) * along };
        Vec3f vel = { 0.0f, 0.5f, 0.0f };
        Vec3f accel = { 0.0f, 0.0f, 0.0f };

        EffectSsKiraKira_SpawnSmall(play, &pos, &vel, &accel, &sFrostPrim, &sFrostEnv);
    }
}

void HtKatanaPickup_Draw(Actor* thisx, PlayState* play) {
    if (HtKatanaPickup_Waiting((HtKatanaPickup*)thisx, play)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    // the model lies along +x with its blade edge toward +y: turn it a quarter about x to lay it flat, centred on
    // the actor (the hilt end at -x), lifted clear of the floor by the guard's radius
    Matrix_Translate(0.0f, 400.0f, 0.0f, MTXMODE_APPLY);
    Matrix_RotateY(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_RotateX(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Translate(-2800.0f, 0.0f, 0.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)SAYA_DL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)KATANA_HELD_DL);

    CLOSE_DISPS(play->state.gfxCtx);
}
