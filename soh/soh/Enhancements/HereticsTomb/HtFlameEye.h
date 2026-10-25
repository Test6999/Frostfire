#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: a silver eye switch (the Shadow Temple's / Bottom of the Well's grey eye) set in a wall that, every few
// seconds, opens and looses a single flame straight at Link. The flame burns him; a katana cut (or its frost wave, or
// an ice arrow) puts it out. Only one flame in the air at a time.
// Faces along rotY. Params: bits 0-4 seconds between flames (0 = 4).
typedef struct HtFlameEye {
    Actor actor;
    ColliderCylinder collider; // the flame
    Vec3f flamePos;
    Vec3f flameVel;
    s16 timer;
    s16 period;
    s16 flameLife;  // > 0: a flame is flying
    s16 eyeTex;     // 0 open, 1 half, 2 closed
    s16 scroll;
} HtFlameEye;

void HtFlameEye_Init(Actor* thisx, PlayState* play);
void HtFlameEye_Destroy(Actor* thisx, PlayState* play);
void HtFlameEye_Update(Actor* thisx, PlayState* play);
void HtFlameEye_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
