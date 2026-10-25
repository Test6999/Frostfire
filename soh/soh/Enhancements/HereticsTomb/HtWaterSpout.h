#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: a water spout the Ice Katana freezes into a solid ice pillar you can stand on.
//
// Placed at the bottom of a water channel or pit; the spout shoots up past the top of the pillar it would make.
// A katana slash wave, an Iai wave or an ice arrow freezes it: an ice pillar (flat top, solid) stands until
// it thaws, with the timed-switch tick, then it collapses back into water.
// Params:
//   bits 0-5   freeze time in seconds (0 = 10 s, 63 = stays frozen for good)
//   bits 6-9   pillar height in 20-unit steps (0 = 160), measured from the actor up to the pillar's top
//   bits 10-11 pillar width: 80 / 120 / 160 / 200
//   bit 12     tall: the height steps are 40 units (up to 600)
//   bit 13     erupting: no stone mouth (it bursts from the floor), and the column shoots up from nothing
#define HT_WATER_SPOUT_PARAMS(freezeSec, heightSteps, widthIdx) \
    (((freezeSec) & 0x3F) | (((heightSteps) & 0xF) << 6) | (((widthIdx) & 3) << 10))

// Collision for the frozen pillar: a 100-unit cube (base at y 0), scaled by the actor to the pillar's size.
#define HT_WATER_SPOUT_ERUPT 0x2000

#define HT_ICE_PILLAR_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_ice_pillar_col"
#define HT_SPOUT_MOUTH_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_spout_mouth_dl"

typedef struct HtWaterSpout {
    DynaPolyActor dyna;
    ColliderCylinder collider;
    CollisionHeader* colHeader;
    f32 height;
    f32 width;
    f32 iceAlpha;
    s16 state;
    s16 timer;
    s16 freezeFrames; // -1 = permanent
    s16 splashTimer;
    f32 progress;     // 0 = all water, 1 = full ice pillar (it grows and melts between the two)
    f32 grow;         // the column's height, 0..1 (an erupting spout's shoots up)
} HtWaterSpout;

// A flame plays on it: a frozen pillar melts at once (a burning HtFlameJet calls this).
void HtWaterSpout_Thaw(Actor* thisx, PlayState* play);
// Solid: freezing or frozen (a pillar something can run into)
s32 HtWaterSpout_IsSolid(Actor* thisx);
// Width of its pillar
f32 HtWaterSpout_Width(Actor* thisx);

void HtWaterSpout_Init(Actor* thisx, PlayState* play);
void HtWaterSpout_Destroy(Actor* thisx, PlayState* play);
void HtWaterSpout_Update(Actor* thisx, PlayState* play);
void HtWaterSpout_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
