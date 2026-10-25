// Heretics Tomb: the overlook dais. See HtOverlook.h.

#include "HtOverlook.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <math.h>

#define OVERLOOK_RADIUS 45.0f

void HtOverlook_Init(Actor* thisx, PlayState* play) {
    HtOverlook* this = (HtOverlook*)thisx;

    this->subCamId = 0;
    // the look-at point travels in the rotation fields; the actor itself never turns
    this->actor.home.rot.y = 0;
}

static void HtOverlook_End(HtOverlook* this, PlayState* play) {
    if (this->subCamId != 0) {
        Play_ClearCamera(play, this->subCamId);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        this->subCamId = 0;
    }
}

void HtOverlook_Destroy(Actor* thisx, PlayState* play) {
    HtOverlook_End((HtOverlook*)thisx, play);
}

void HtOverlook_Update(Actor* thisx, PlayState* play) {
    HtOverlook* this = (HtOverlook*)thisx;
    Player* player = GET_PLAYER(play);
    f32 dx = player->actor.world.pos.x - this->actor.world.pos.x;
    f32 dz = player->actor.world.pos.z - this->actor.world.pos.z;
    s32 onDais = ((SQ(dx) + SQ(dz)) < SQ(OVERLOOK_RADIUS)) &&
                 (fabsf(player->actor.world.pos.y - this->actor.world.pos.y) < 20.0f) &&
                 (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !Player_InCsMode(play);

    // a few motes rising off the dais, so it reads as something to stand on
    if (!onDais && ((play->gameplayFrames % 10) == 0)) {
        static Color_RGBA8 prim = { 200, 220, 255, 255 };
        static Color_RGBA8 env = { 80, 120, 255, 0 };
        Vec3f pos = { this->actor.world.pos.x + Rand_CenteredFloat(70.0f), this->actor.world.pos.y + 5.0f,
                      this->actor.world.pos.z + Rand_CenteredFloat(70.0f) };
        Vec3f vel = { 0.0f, 1.2f, 0.0f };
        Vec3f accel = { 0.0f, 0.0f, 0.0f };

        EffectSsKiraKira_SpawnSmall(play, &pos, &vel, &accel, &prim, &env);
    }
    if (onDais) {
        Vec3f at = { (f32)this->actor.home.rot.x, this->actor.world.pos.y, (f32)this->actor.home.rot.z };
        // nearly straight down; a slight tilt keeps "up" on screen pointing away from the dais
        Vec3f eye = { at.x + (this->actor.world.pos.x - at.x) * 0.08f, at.y + (f32)this->actor.params,
                      at.z + (this->actor.world.pos.z - at.z) * 0.08f };

        if (this->subCamId == 0) {
            this->subCamId = Play_CreateSubCamera(play);
            if (this->subCamId <= 0) {
                this->subCamId = 0;
                return;
            }
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, this->subCamId, CAM_STAT_ACTIVE);
            Play_CameraSetFov(play, this->subCamId, 60.0f);
        }
        Play_CameraSetAtEye(play, this->subCamId, &at, &eye);
    } else {
        HtOverlook_End(this, play);
    }
}
