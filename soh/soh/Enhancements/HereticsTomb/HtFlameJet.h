#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: wall flamethrower that the Ice Katana (or an ice arrow) freezes for a while.
//
// Placed at the nozzle mouth; the flame shoots along the actor's facing (rotY, tilted by rotX).
// Params:
//   bits 0-5   freeze time in seconds (0 = 8 s, 63 = stays frozen for good)
//   bits 6-9   flame length in 40-unit steps (0 = 200 units, max 15 = 600)
//   bits 10-11 mode: 0 = always on, 1 = cycles 2 s on / 2 s off, 2 = same cycle starting off,
//              3 = relay: 2 s on / 6 s off, started bits 12-14 seconds into that cycle; ice doesn't stop it
// A burning jet also melts any frozen water spout pillar it plays on (HtWaterSpout_Thaw).
// rotZ bit 0: only its nozzle takes the ice (the flame it throws doesn't).
#define HT_FLAME_JET_PARAMS(freezeSec, lengthSteps, mode) \
    (((freezeSec) & 0x3F) | (((lengthSteps) & 0xF) << 6) | (((mode) & 3) << 10))
#define HT_FLAME_JET_RELAY_PARAMS(lengthSteps, phaseSec) \
    ((((lengthSteps) & 0xF) << 6) | (3 << 10) | (((phaseSec) & 7) << 12))

#define HT_FLAME_JET_SPHERES 6
#define HT_FLAME_NOZZLE_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_flame_nozzle_dl"

typedef struct HtFlameJet {
    Actor actor;
    ColliderJntSph collider;
    ColliderJntSphElement elements[HT_FLAME_JET_SPHERES];
    Vec3f dir;          // unit vector the flame travels along
    f32 length;         // full flame length
    f32 reach;          // current flame length (grows on ignition, shrinks when cut off)
    f32 iceAlpha;       // 0..255, the ice crust on the nozzle
    s16 state;
    s16 timer;
    s16 freezeFrames;   // -1 = permanent
    s16 mode;
    s16 knockCooldown;  // frames before the jet can throw Link back again
} HtFlameJet;

void HtFlameJet_Init(Actor* thisx, PlayState* play);
void HtFlameJet_Destroy(Actor* thisx, PlayState* play);
void HtFlameJet_Update(Actor* thisx, PlayState* play);
void HtFlameJet_Draw(Actor* thisx, PlayState* play);

// Draws a cluster of ice crystals (the same crystal the ice arrow uses) around a point.
// Shared so frozen enemies can get the same look in the combat pass.
void HtIce_DrawCrystals(PlayState* play, Vec3f* center, f32 radius, s32 count, s32 seed, u8 alpha);
// a sleeve of ice grown along a length of chain, from a to b (radius: its thickness)
void HtIce_DrawSleeve(PlayState* play, Vec3f* a, Vec3f* b, f32 radius, u8 alpha);
// translucent ice models (built with the tomb), drawn into the current matrix with this alpha
#define HT_ICE_SLEEVE_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_ice_sleeve_dl"
#define HT_ICE_BLOCK_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_ice_block_dl"
void HtIce_DrawModel(PlayState* play, const char* dl, u8 alpha);

#ifdef __cplusplus
}
#endif
