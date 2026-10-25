#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: the Shadow Temple's boat, sailing the B2 ring - a circular channel round the B2 central room.
//
// Zelda's Lullaby played on board starts it (once, F_STARTED). After that it sails whenever Link stands on it, one
// stop at a time, round the ring the one way (clockwise seen from above):
//   stop 0  the port (its left side against the inner bank, below the central room's corridor)
//   stop 1  a quarter round: switch room 1's door on its right; the portcullis just ahead (gate 1) bars the way on
//   stop 2  half way: switch room 2; gate 2 ahead
//   stop 3  three quarters: the mini-boss's door; on from there the ring is open back to the port
// Half way between stops it is attacked (once each: waves 1-3) and holds still until they're all beaten: Stalfos,
// then twenty Torch Slugs board it; then Morpha's water tentacles rise beside it, 1, then 2, then 4 (HtTentacle.c).
// When Link is ashore by one of the stops it can reach and it isn't there, it sails over to him (empty).
// Path, docks and gates are shared with the room generator (build_heretics_dungeon.py: B2 RING); keep them in step.
//
// The boat model is the vanilla ship (object_haka_objects), drawn at scale 0.1 and shifted so the actor's origin is
// the middle of its deck; the deck, guardrails (open midships on both sides) and rail model are generated.
#define HT_BOAT_RAILS_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_boat_rails_dl"
#define HT_BOAT_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_boat_col"

#define HT_BOAT_CX 0.0f
#define HT_BOAT_CZ (-1200.0f)
#define HT_BOAT_Y (-1932.0f)     // actor (hull bottom); the deck is 232 above, level with the docks (-1700)
#define HT_BOAT_DECK 232.0f
#define HT_BOAT_R_CRUISE 1450.0f
#define HT_BOAT_R_DOCK_OUT 1630.0f // its right side 10 short of an outer dock face (inradius 1840)
#define HT_BOAT_R_DOCK_IN 1360.0f  // its left side 10 short of the port's face (inner inradius 1150)
#define HT_BOAT_PORT_PHI (-0.5f * M_PI)

#define HT_BOAT_F_STARTED 0x30
#define HT_BOAT_F_GATE1 0x31
#define HT_BOAT_F_GATE2 0x32
#define HT_BOAT_F_WAVE1 0x33 // ..0x35

typedef struct HtBoat {
    DynaPolyActor dyna;
    s16 state;
    s16 stop;    // docked at / sailing to
    s16 dir;     // +1 forward (with Link), -1 back
    s16 timer;
    s16 wave;    // halted for this wave (0-2)
    s16 aboard;  // frames since Link last stood on it (0 = on it now)
    s16 ocState;
    u8 waveSpawned[3];
    s16 tentRound; // the last wave's round of tentacles (0-2)
    f32 s;       // angle sailed from the port (radians, 0 .. 2 pi)
    f32 speed;   // units per frame along the path
    Vec3f bellPos;
} HtBoat;

void HtBoat_Init(Actor* thisx, PlayState* play);
void HtBoat_Destroy(Actor* thisx, PlayState* play);
void HtBoat_Update(Actor* thisx, PlayState* play);
void HtBoat_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
