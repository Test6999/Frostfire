#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: a bed of iron spikes lining a wall. Touching it hurts and knocks Link back. Not solid: the wall behind
// it is part of the room.
// The bed stands on the actor's position (the middle of its bottom edge, on the wall), the spikes pointing along rotY.
// Params:
//   bits 0-5   width in 20-unit steps (0 = 100)
//   bits 6-11  height in 20-unit steps (0 = 100)
#define HT_SPIKES_PARAMS(widthSteps, heightSteps) (((widthSteps) & 0x3F) | (((heightSteps) & 0x3F) << 6))
#define HT_SPIKES_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_spikes_dl"
#define HT_SPIKES_CELL 40.0f  // one spike per 40 x 40 of the bed (Morpha's spacing)
#define HT_SPIKES_DEPTH 62.0f // how far the points stand off the wall (Morpha's spikes)

typedef struct HtSpikes {
    Actor actor;
    ColliderTris collider;
    ColliderTrisElement colliderItems[2];
    f32 width;
    f32 height;
} HtSpikes;

void HtSpikes_Init(Actor* thisx, PlayState* play);
void HtSpikes_Destroy(Actor* thisx, PlayState* play);
void HtSpikes_Update(Actor* thisx, PlayState* play);
void HtSpikes_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
