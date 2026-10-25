#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: the Lynel's fire cage (phase 2, B3's boss arena). A wall of fire round the arena's middle that Link
// can't cut (the cage: Ganon's ring of fire), and inside it rings of fire it sends out one after another: each starts
// as the wall itself and closes in on where Link stood when it set out - shrinking and sliding toward that spot till
// it closes on it and dies. Link can't outrun one inside the cage: he cuts a gap in it with the katana (a plain slash
// puts out the segments of one ring it passes through, the Iai: of all of them) and lets it pass round him. Touching
// the flames burns him and throws him out through the ring (behind it: where it has been).
// Spawned by HtLynel at the arena's middle, on its floor; it tells the Lynel how much Link has cut (cutCount).
#define HT_FIRE_LAYERS 7          // the wall and up to six rings
#define HT_FIRE_SEGS 32           // segments per ring (11.25 degrees each)
#define HT_FIRE_WALL_R 470.0f     // the cage
#define HT_FIRE_WALL_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonFireRingDL" // the cage: Ganon's ring of fire

typedef struct {
    u8 on;
    f32 s;           // how far it has closed in (0: the wall, 1: shut on its spot)
    f32 tx, tz;      // its spot (where Link stood)
    f32 cx, cz, r;   // its centre and radius now
    u32 alive;       // a bit a segment
    s16 age;
    u16 gen;         // (which ring this is: the frame interpolation's flames keep to their own ring)
} HtFireLayer;

typedef struct HtFireRing {
    Actor actor;
    HtFireLayer layers[HT_FIRE_LAYERS]; // [0]: the cage wall (never put out)
    s16 rise;        // frames since it erupted (the flames grow up out of the floor)
    s16 emitTimer;   // till the next ring
    s16 emitInterval; // (the Lynel sets it: shorter as the fight goes on; 0: none)
    s16 maxRings;    // (and how many may stand at once)
    f32 speed;       // (and how fast they close: units a frame)
    u16 gen;
    s16 cutCount;    // segments Link has put out (the Lynel reads and resets it)
    s16 burnCool;
    s16 scroll;
    u8 dying;        // going out (the fight's moved on)
    u8 paused;       // holding still: no ring moves, none set out (the Lynel's big charge)
} HtFireRing;

void HtFireRing_Init(Actor* thisx, PlayState* play);
void HtFireRing_Destroy(Actor* thisx, PlayState* play);
void HtFireRing_Update(Actor* thisx, PlayState* play);
void HtFireRing_Draw(Actor* thisx, PlayState* play);

// how open Link's cage is: the share of ring segments (not the wall's) that are out or were never there - 0 crowded,
// 1 empty
f32 HtFireRing_Openness(Actor* thisx);
// is (x, z) inside a burning segment? (the Lynel's charge goes through fire - this is for hazards it drops)
s32 HtFireRing_IsBurning(Actor* thisx, f32 x, f32 z);

#ifdef __cplusplus
}
#endif
