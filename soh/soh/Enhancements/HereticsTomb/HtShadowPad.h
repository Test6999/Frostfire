#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb's door in Hyrule Field. A shadow warp pad in the field's north-west corner, on the stream's north
// bank by the culvert: stand on it and play the Nocturne of Shadow and, in a short cutscene, the iron bars
// across the culvert at the stream's west end rise. Swim (dive) or walk (Iron Boots) in under the wall and the tomb's
// entrance hall is through there: he surfaces in its pool. The way back out is that pool's flooded tunnel (low in its
// south wall): dive into it and he's back out of the culvert, swimming.
// The bars are the field's own (spot00 room 0): their 4 vertices are slid up behind the stone lintel, in place in the
// loaded room mesh, and their 2 collision triangles are made passable. Spawned by HtShadowPad_OnSpawnActors.
// The pad is the vanilla shadow warp pad (Bg_Mjin, spawned beside this actor: the one the Nocturne lands Link on by
// the Shadow Temple). On the stream's north bank, a little way east of the culvert.
#define HT_SHADOW_PAD_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_shadow_pad_dl"
#define HT_SHADOW_PAD_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_shadow_pad_col"
#define HT_GY_PAD_DL "__OTR__scenes/shared/spot02_scene/spot02_room_0DL_0025E0"  // the wedges
#define HT_GY_STEP_DL "__OTR__scenes/shared/spot02_scene/spot02_room_0DL_002030" // the hexagonal step
#define HT_GY_PAD_X 2136.0f // (the graveyard pad's middle, in the graveyard)
#define HT_GY_PAD_Y 100.0f
#define HT_GY_PAD_Z 80.0f
#define HT_SHADOW_PAD_TOP 2.0f // (our disc's top: the graveyard's wedges sit on it)
#define HT_SHADOW_PAD_X -5620.0f
#define HT_SHADOW_PAD_Y -300.0f
#define HT_SHADOW_PAD_Z 260.0f

typedef struct HtShadowPad {
    DynaPolyActor dyna;
    s16 state;
    s16 timer;
    s16 subCamId;
    f32 lift;  // the bars: 0 down .. HT_BARS_RISE up
    f32 glow;  // 0..1: the flash after the song
    Vec3f camAt;
    Vec3f camEye;
    s16 arriveTimer; // the blue warp's arrival: frames of the low camera on Link coming down
    u8 arriveDone;
} HtShadowPad;

void HtShadowPad_Init(Actor* thisx, PlayState* play);
void HtShadowPad_Destroy(Actor* thisx, PlayState* play);
void HtShadowPad_Update(Actor* thisx, PlayState* play);
void HtShadowPad_Draw(Actor* thisx, PlayState* play);

void HtShadowPad_OnSceneInit(PlayState* play);   // (arriving from the tomb: Link put in the stream, by the culvert)
void HtShadowPad_OnSpawnActors(PlayState* play); // (Hyrule Field: the pad)
void HtShadowPad_TombUpdate(PlayState* play);    // (in the tomb: the way out)
// z_message_PAL.c: the Nocturne of Shadow just played (free play) - true if Link stands on the pad and it opens the
// tomb (then the warp's "Warp to the Graveyard?" isn't asked)
bool HtShadowPad_TakeNocturne(PlayState* play);
// HtLynel.c: the boss's blue warp lands Link on the shadow pad; ArrivedByWarp: this field visit began so
void HtShadowPad_ArriveByWarp(void);
bool HtShadowPad_ArrivedByWarp(void);
// where the freed black horse waits for Link then: the stream's south bank, across from the pad, facing it
#define HT_HORSE_WAIT_X -5560.0f
#define HT_HORSE_WAIT_Z 1130.0f

// Katana.cpp: saved
bool HtTomb_IsOpen(void);
void HtTomb_SetOpen(bool open);
// z_play.c, right after the actors' context is set up (before the rooms' actors spawn): the mausoleum's puzzle flags
// back. The game keeps switch flags 0x20-0x3F only while Link is in a scene, and nearly all the dungeon's are up
// there: kept here (Katana.cpp, in the save) so the dungeon stays as he left it.
void HtTomb_RestoreFlags(PlayState* play);

#ifdef __cplusplus
}
#endif
