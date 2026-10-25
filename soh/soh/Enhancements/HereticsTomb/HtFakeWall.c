// Heretics Tomb: illusory wall panel, gone through the Lens of Truth. See HtFakeWall.h.

#include "HtFakeWall.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

void HtFakeWall_Init(Actor* thisx, PlayState* play) {
    s32 w = thisx->params & 0xF;
    s32 h = (thisx->params >> 4) & 0xF;

    // the panel model is 100 x 100 units at scale 1 (bottom edge on the actor, centred across)
    thisx->scale.x = ((w == 0) ? 100.0f : w * 20.0f) / 100.0f;
    thisx->scale.y = ((h == 0) ? 100.0f : h * 20.0f) / 100.0f;
    thisx->scale.z = 1.0f;
    // (not a vanilla lens actor: the game's lens-circle masking doesn't reach this renderer, so the panel simply
    // isn't drawn while the Lens of Truth is on)
}

void HtFakeWall_Destroy(Actor* thisx, PlayState* play) {
}

void HtFakeWall_Update(Actor* thisx, PlayState* play) {
}

void HtFakeWall_Draw(Actor* thisx, PlayState* play) {
    if (play->actorCtx.lensActive) {
        return; // the Lens of Truth sees straight through it
    }
    Gfx_DrawDListOpa(play, (Gfx*)HT_FAKE_WALL_DL);
}
