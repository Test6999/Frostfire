#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: an overlook dais in the torch-drawing rooms. While Link stands on it, the camera looks straight down
// on the room's torch grid, so the drawing can be read; step off and the normal camera comes back.
// rotX / rotZ: the world x / z to look down on (the grid's centre). Params: camera height above the dais.

typedef struct HtOverlook {
    Actor actor;
    s16 subCamId; // 0 = not viewing
} HtOverlook;

void HtOverlook_Init(Actor* thisx, PlayState* play);
void HtOverlook_Destroy(Actor* thisx, PlayState* play);
void HtOverlook_Update(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
