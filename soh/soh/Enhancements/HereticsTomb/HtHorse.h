#pragma once

// Heretics Tomb: the black horse - the Lynel's horse, freed, found again in Hyrule Field and ridden as Link's horse.
// Link has one horse at a time: the other (once he owns both) waits in Lon Lon Ranch's stable, where talking to it
// swaps them. It's Epona's riding code (En_Horse) on Ganondorf's horse: a little faster, the sky darkening while
// it's ridden, flames trailing off its hooves at a gallop. (State and save: Katana.cpp; the rest: HtHorse.c.)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct PlayState;
struct EnHorse;

bool HtHorse_BlackOwned(void);
bool HtHorse_BlackActive(void);              // the black horse is Link's horse now (the other in the stable)
void HtHorse_SetBlackActive(bool active);
void HtHorse_Free(void);                     // the Lynel beaten: the horse his (and active), waiting in Hyrule Field
// does Link have a horse here (Epona, or the black horse) - for spawning and calling his horse
bool HtHorse_HasHorse(struct PlayState* play);
bool HtHorse_EponaObtained(void);
// set while z_horse.c spawns the black horse in Lon Lon Ranch beside the ranch's own Epona (before she's won)
extern int gHtHorseSpawnBlack;

#define HT_HORSE_SPEED_K 1.2f                // its top speeds against Epona's
#define HT_HORSE_BOOST_SPEED 16              // (Epona's: 14)
#define HT_HORSE_SCALE 0.0087f              // (Epona's size: her riding - mounting, the saddle - fits it)
// (not 0x06xx: while a 0x06xx text is up the game doesn't update the actor talked to - the choice was never read)
#define HT_HORSE_SWAP_TEXT 0x9EF0            // the stable: "switch horses?"
#define HT_HORSE_SWAPPED_TEXT 0x9EF1

// En_Horse's hooks (z_en_horse.c)
void* HtHorse_AnimTable(void); // (AnimationHeader*[9]: En_Horse's animations, in its order)
void HtHorse_Update(struct EnHorse* horse, struct PlayState* play);
void HtHorse_OnSpawnActors(struct PlayState* play); // (Lon Lon's stable: the horse waiting in the free stall)
void HtHorse_Draw(struct EnHorse* horse, struct PlayState* play);

#ifdef __cplusplus
}
#endif
