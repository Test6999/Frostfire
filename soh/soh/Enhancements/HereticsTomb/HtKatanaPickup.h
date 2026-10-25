#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: the Ice Katana lying on the floor where the Frozen Ronin shattered, sheathed in its scabbard.
// Link picks it up like any item on the ground (A when close): the usual get-item, which the katana code turns into
// the Ice Katana. The tomb's treasure flag HT_KATANA_TREASURE_FLAG marks it taken, so it never comes back.

#define HT_KATANA_TREASURE_FLAG 0x06
// the Frozen Ronin is beaten (a switch flag, kept with the scene's): the katana waits in his arena from then until it's
// taken. The arena's own pickup (params 1, placed with the room) stays hidden until then, so it's there every time
// Link comes back, wherever he left it.
#define HT_RONIN_DEAD_FLAG 0x1E // (below 0x20: kept in the save, not cleared on leaving the tomb)

typedef struct HtKatanaPickup {
    Actor actor;
    s16 timer;
} HtKatanaPickup;

void HtKatanaPickup_Init(Actor* thisx, PlayState* play);
void HtKatanaPickup_Destroy(Actor* thisx, PlayState* play);
void HtKatanaPickup_Update(Actor* thisx, PlayState* play);
void HtKatanaPickup_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
