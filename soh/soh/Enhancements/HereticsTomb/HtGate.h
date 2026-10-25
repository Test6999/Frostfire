#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: an iron portcullis barring a channel. Once its switch flag is set it rises into the ceiling (or is
// already up when the room loads).
// Params: bits 0-5 switch flag; bit 6: the small fence instead (a tunnel's grille, 120 wide and 130 high, rising
// HT_FENCE_RISE).
// Model / collision: built with the rooms, in world units (actor scale 1): the gate spans local x -400..400, from y 0
// up to HT_GATE_HEIGHT, 20 thick, facing local z.
#define HT_GATE_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_gate_dl"
#define HT_GATE_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_gate_col"
#define HT_GATE_RISE 1150.0f
#define HT_FENCE_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_fence_dl"
#define HT_FENCE_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_fence_col"
#define HT_FENCE_RISE 140.0f
#define HT_GATE_FENCE 0x40

typedef struct HtGate {
    DynaPolyActor dyna;
    s16 flag;
    s16 timer;
    f32 lift; // 0 down .. rise up
    f32 rise;
} HtGate;

void HtGate_Init(Actor* thisx, PlayState* play);
void HtGate_Destroy(Actor* thisx, PlayState* play);
void HtGate_Update(Actor* thisx, PlayState* play);
void HtGate_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
