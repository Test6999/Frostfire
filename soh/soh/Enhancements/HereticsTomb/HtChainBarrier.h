#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: chains sealing a passage that only the Ice Katana's Iai draw can cut.
//
// Stands on the floor at the middle of the passage, facing along it (rotY). Solid until cut.
// An Iai draw cuts it, up close (the blade) or from range (the Iai wave). Normal katana slashes, the plain
// slash wave, other swords and the boomerang just clang off.
// Params:
//   bits 0-5   switch flag set when cut, so it stays cut (63 = none)
//   bits 6-9   width in 20-unit steps (0 = 120)
//   bits 10-13 height in 20-unit steps (0 = 120)
//   bit  14    bars: straight hanging chains (a cell front) instead of crossed pairs
//   bit  15    Navi says outright to use an Iai strike (for chains met once the katana is in hand)
#define HT_CHAIN_BARRIER_PARAMS(switchFlag, widthSteps, heightSteps) \
    (((switchFlag) & 0x3F) | (((widthSteps) & 0xF) << 6) | (((heightSteps) & 0xF) << 10))

#define HT_CHAIN_BARRIER_BARS 0x4000
#define HT_CHAIN_BARRIER_FULL_HINT 0x8000
// one hanging chain: two crossed 12 x 100 strips of the door-chain texture (scaled to the barrier height)
#define HT_CHAIN_BAR_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_chain_bar_dl"

// Collision for the barrier: a 100x100 two-sided wall, scaled by the actor to the barrier's size.
#define HT_CHAIN_BARRIER_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_chain_barrier_col"

typedef struct HtChainBarrier {
    DynaPolyActor dyna;
    ColliderCylinder collider;
    f32 width;
    f32 height;
    s16 switchFlag;
    s16 cutTimer;  // counts down while the chains fall away
    s16 clangCooldown;
    f32 hitAcross; // where along the barrier the Iai struck (the frost spreads out through the chains from there)
} HtChainBarrier;

void HtChainBarrier_Init(Actor* thisx, PlayState* play);
void HtChainBarrier_Destroy(Actor* thisx, PlayState* play);
void HtChainBarrier_Update(Actor* thisx, PlayState* play);
void HtChainBarrier_Draw(Actor* thisx, PlayState* play);

// True while an uncut chain barrier stands near this position: doors behind chains can't be opened.
s32 HtChainBarrier_BlocksPosition(Vec3f* pos);

// True when an uncut chain barrier stands between the two points (on opposite sides of it, b within 700 units):
// what's penned behind the chains can't be hurt from outside.
s32 HtChainBarrier_Separates(Vec3f* a, Vec3f* b);

#ifdef __cplusplus
}
#endif
