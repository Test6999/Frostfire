#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb's dungeon map and compass (the tomb's own: its maps are drawn from shapes baked from the rooms,
// HtMapData.h, not the vanilla map textures).
// - The pause screen's map page: the floors (1F, B1, B2, B3) as buttons, Link's head by the floor he's on; the floor
//   picked drawn room by room - rooms he's been in lit, the one he's in pulsing; with the Dungeon Map every room
//   shows (the ones not yet visited dim) and every floor can be picked. With the Compass: the boss's skull by its
//   floor and on its arena, the chests not yet opened.
// - The minimap (Dungeon Map): the room he's in. With the Compass: his arrow, where he came in, its unopened chests.
// Rooms visited are saved (Katana.cpp).

#define HT_MAP_FLOOR_SLOT0 2 // the floor buttons used: slots 2..5 of the 8 (centred in the column)

void HtMap_Update(PlayState* play);           // (z_map_exp.c Map_Update: each frame in the tomb, unpaused)
void HtMap_DrawMinimap(PlayState* play);      // (z_map_exp.c Minimap_Draw)
bool HtMap_FloorShown(s32 floor);             // a floor's button, on the pause map
s32 HtMap_FloorCount(void);
s32 HtMap_BossFloor(void);                    // -1: none
// the pause map: the floor's rooms, and the compass's marks (the page's matrix is current)
void HtMap_DrawPauseFloor(PlayState* play, GraphicsContext* gfxCtx, s32 floor, s16 x0, s16 y0, u8 alpha);

// Katana.cpp: saved
u32 HtMap_GetVisited(void);
void HtMap_SetVisited(u32 pieces);

#ifdef __cplusplus
}
#endif
