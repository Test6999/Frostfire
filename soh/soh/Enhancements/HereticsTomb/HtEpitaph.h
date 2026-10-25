#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Frostfire Mausoleum: a readable epitaph (A in front of it, like a sign). Stands on the floor facing rotY.
// Params:
//   0  the Frozen Ronin's gravestone (his arena, against the back wall): unseen until he's beaten (his flag,
//      HT_RONIN_DEAD_FLAG); it rises out of blue fire as he burns away
//   1  Ganondorf's epitaph by his chained horse (the boss arena; the same tomb stone): there while the horse lies chained, gone with the
//      chain's anchor once the fight is on (HtLynel_ChainShown)
#define HT_EPITAPH_RONIN_TEXT 0x9EF2
#define HT_EPITAPH_HORSE_TEXT 0x9EF3
// the Ronin's grave: the Royal Family's tomb stone from Kakariko's graveyard (Bg_Spot02_Objects params 2), its crest
// on its -z face; drawn small, turned to face rotY
#define HT_GRAVE_STONE_DL "__OTR__objects/object_spot02_objects/object_spot02_objects_DL_0130B0"
#define HT_GRAVE_STONE_SCALE 0.06f
#define HT_PLAQUE_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_plaque_dl"

typedef struct HtEpitaph {
    Actor actor;
    ColliderCylinder collider;
    s16 timer;  // the grave post: frames since it appeared (its blue fire)
    u8 shown;
} HtEpitaph;

void HtEpitaph_Init(Actor* thisx, PlayState* play);
void HtEpitaph_Destroy(Actor* thisx, PlayState* play);
void HtEpitaph_Update(Actor* thisx, PlayState* play);
void HtEpitaph_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
