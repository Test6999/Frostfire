#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: the boss arena's round pedestal, the Triforce inlaid on its top (B3's boss room). Two actors, one per
// tier: once the fight has begun (the Lynel's intro over, its title card gone) both sink, telescoping, till their tops
// are flush with the arena floor - under the water, the Triforce still showing through it (the Lynel is cornered on
// it in phase 3).
// Params: bit 0: the top tier (with the Triforce; sinks HT_PEDESTAL_TOP_DROP), else the lower step (HT_PEDESTAL_LOW_DROP).
// Model / collision: built with the rooms, in world units (actor scale 1), the actor standing at the arena's middle
// on its floor.
#define HT_PEDESTAL_TOP_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_pedestal_top_dl"
#define HT_PEDESTAL_TOP_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_pedestal_top_col"
#define HT_PEDESTAL_LOW_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_pedestal_low_dl"
#define HT_PEDESTAL_LOW_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_pedestal_low_col"
#define HT_PEDESTAL_TOP 1
#define HT_PEDESTAL_TOP_DROP 40.0f
#define HT_PEDESTAL_LOW_DROP 20.0f
#define HT_PEDESTAL_SINK_FLAG 0x3C   // set (HtLynel) when it should sink; set at load: already down
#define HT_PEDESTAL_SINK_FRAMES 70
// phase 3's dig: two more pieces (spawned by HtLynel, flush with the floor), moved down (and the pit up again) on cue
#define HT_PEDESTAL_DIG_RING 2      // params: the ring Link fights on (one level down)...
#define HT_PEDESTAL_DIG_PIT 3       // ...and the Triforce's disc in the middle, the Lynel on it (two)
#define HT_PEDESTAL_DIG_RING_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_dig_ring_dl"
#define HT_PEDESTAL_DIG_RING_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_dig_ring_col"
#define HT_PEDESTAL_DIG_PIT_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_dig_pit_dl"
#define HT_PEDESTAL_DIG_PIT_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_dig_pit_col"
#define HT_PEDESTAL_DIG_BAND 4      // ...the old pedestal's outer band (stays put)...
#define HT_PEDESTAL_DIG_RING2 5     // ...and the ring without its rim (the Lynel dead: onto the disc)
#define HT_PEDESTAL_DIG_BAND_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_dig_band_dl"
#define HT_PEDESTAL_DIG_BAND_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_dig_band_col"
#define HT_PEDESTAL_DIG_RING2_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_dig_ring2_col"
#define HT_DIG_RING_DROP 180.0f
#define HT_DIG_PIT_DROP 250.0f
#define HT_DIG_PIT_R 120.0f
#define HT_DIG_RING_R 215.0f

typedef struct HtPedestal {
    DynaPolyActor dyna;
    s16 timer;
    f32 drop;
    f32 moveFrom, moveTo; // (the dig pieces: a move under way, from one depth to another...)
    s16 moveT, moveDur;   // (...over so many frames)
} HtPedestal;

// the dig pieces: go to this depth below the floor over so many frames (eased)
void HtPedestal_MoveTo(Actor* thisx, f32 depth, s16 frames);

// how far down the top tier is at frame t of the sinking (0..HT_PEDESTAL_SINK_FRAMES): eased, slow at both ends
f32 HtPedestal_SinkCurve(s32 t);

void HtPedestal_Init(Actor* thisx, PlayState* play);
void HtPedestal_Destroy(Actor* thisx, PlayState* play);
void HtPedestal_Update(Actor* thisx, PlayState* play);
void HtPedestal_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
