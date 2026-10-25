#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Frostfire Mausoleum: Morpha's lair's wall spikes and the low wedges under them, in Armorpha's arena. Nothing of
// them is shipped: both are built when the room loads from the player's own game data - the spikes from the lair's
// room model, the wedges (their collision: the sides hurt) from the lair's collision - placed mirrored in the arena
// (HtMoSpikesRecipe.h). Spawned once, at the world's origin.
#define HT_MO_SPIKE_MAT "__OTR__scenes/shared/heretics_tomb_scene/ht_mo_spike_mat"

typedef struct HtMoSpikes {
    DynaPolyActor dyna;
    Vtx* vtx;       // the spikes, three corners per triangle (world positions)
    s32 vtxCount;
    CollisionHeader col;
} HtMoSpikes;

void HtMoSpikes_Init(Actor* thisx, PlayState* play);
void HtMoSpikes_Destroy(Actor* thisx, PlayState* play);
void HtMoSpikes_Update(Actor* thisx, PlayState* play);
void HtMoSpikes_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
