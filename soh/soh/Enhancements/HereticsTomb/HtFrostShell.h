#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: the frost that cakes an enemy frozen by the Ice Katana. Makes the katana's freeze look like a
// freeze (ice crystals over the body, blue tint) instead of the Deku-nut-style stun the hit actually uses.
// Spawned by the slash wave; follows its target until the freeze runs out or the target is gone.

typedef struct HtFrostShell {
    Actor actor;
    Actor* target;
    s16 timer;
    s16 seed;
} HtFrostShell;

void HtFrostShell_Init(Actor* thisx, PlayState* play);
void HtFrostShell_Destroy(Actor* thisx, PlayState* play);
void HtFrostShell_Update(Actor* thisx, PlayState* play);
void HtFrostShell_Draw(Actor* thisx, PlayState* play);

// Freeze `target` for `frames`: blue tint plus frost. Refreshes an existing shell instead of stacking a second.
void HtFrostShell_Apply(PlayState* play, Actor* target, s16 frames);
// Called whenever any actor is destroyed, so no shell keeps pointing at it.
void HtFrostShell_OnActorDestroy(Actor* actor);
// The katana's cold on fire enemies: Fire Keese and Red/Blue Bubbles lose their flames, Torch Slugs recoil with
// their flame out, Dodongos freeze in place (cutting off a fire breath). Returns true when the enemy shows its own
// vanilla ice for this, so no frost shell is needed on top.
s32 HtFrostShell_ChillEnemy(PlayState* play, Actor* target);

#ifdef __cplusplus
}
#endif
