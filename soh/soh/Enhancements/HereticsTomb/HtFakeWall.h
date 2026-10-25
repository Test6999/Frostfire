#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: an illusory wall panel. It looks like the tomb's stone but has no collision (the hookshot and
// Link go straight through), and it vanishes inside the Lens of Truth (its room must leave lens mode on "show").
// The panel stands on the actor's position, facing along rotY.
// Params:
//   bits 0-3   width in 20-unit steps (0 = 100)
//   bits 4-7   height in 20-unit steps (0 = 100)
#define HT_FAKE_WALL_PARAMS(widthSteps, heightSteps) (((widthSteps) & 0xF) | (((heightSteps) & 0xF) << 4))
#define HT_FAKE_WALL_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_fake_wall_dl"

typedef struct HtFakeWall {
    Actor actor;
} HtFakeWall;

void HtFakeWall_Init(Actor* thisx, PlayState* play);
void HtFakeWall_Destroy(Actor* thisx, PlayState* play);
void HtFakeWall_Update(Actor* thisx, PlayState* play);
void HtFakeWall_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
