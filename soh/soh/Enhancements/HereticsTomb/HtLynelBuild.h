#pragma once

// Heretics Tomb: the Lynel's model, animations and textures, built at run time from the player's own game data
// (Ganondorf's, Ganon's, the Gerudo's, Ganon's horse's and Epona's models and animations, read from oot.o2r) by
// the recipes in HtLynelRecipe.h / HtLynelAnimRecipe.h. Nothing of Nintendo's is shipped with the mod: only which
// vanilla vertex each corner is, our own geometry and poses, and how the textures are recoloured.

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

typedef struct {
    u16 op, a, b, c;
} HtLynelOp;
typedef struct {
    Gfx* material;
    const Vtx* vtx;
    const HtLynelOp* ops;
} HtLynelGroup;

#define HT_LYNEL_PARTS 10
#define LYNEL_CROUCH_JOINTS 54

// builds everything once (the first Lynel spawned); false if the game data isn't what the recipes expect
s32 HtLynelBuild_Init(void);

extern HtLynelGroup* gHtLynelGroups;
extern s32 gHtLynelGroupCount;
extern HtLynelGroup gHtLynelHaloGroup;

// the upper body's animations: per animation, per frame, per part, rows 0-2 of part-local -> upper-local
extern const f32 (*gHtLynelAnims[])[HT_LYNEL_PARTS][12];

// the horse's own poses (joint tables): lying down, getting up, stuck, the leap, heaving, the double kick
extern Vec3s gHtLynelCrouchPose[LYNEL_CROUCH_JOINTS];
extern Vec3s (*gHtLynelStandUp)[LYNEL_CROUCH_JOINTS];
extern Vec3s (*gHtLynelStuckIn)[LYNEL_CROUCH_JOINTS];
extern Vec3s (*gHtLynelStuckLoop)[LYNEL_CROUCH_JOINTS];
extern Vec3s (*gHtLynelJump)[LYNEL_CROUCH_JOINTS];
extern Vec3s (*gHtLynelPush)[LYNEL_CROUCH_JOINTS];
extern Vec3s (*gHtLynelKick)[LYNEL_CROUCH_JOINTS];

// textures (RGBA16): the horse's tail recoloured, its eye shut and half open
extern u16* gHtLynelHairTex;
extern u16* gHtLynelEyeClosedTex;
extern u16* gHtLynelEyeHalfTex;

// the black horse's borrowed animations (Epona's refusal and jumps on Ganon's horse): joint tables
s32 HtLynelBuild_HorseAnims(void);
extern Vec3s* gHtHorseRefuse;
extern Vec3s* gHtHorseJumpLow;
extern Vec3s* gHtHorseJumpHigh;
extern s32 gHtHorseRefuseFrames, gHtHorseJumpLowFrames, gHtHorseJumpHighFrames;

// a vanilla resource from the player's own archive (exactly that one, never an alternate asset's), or NULL
const void* HtRes_LoadExact(const char* path, size_t* size);

#ifdef __cplusplus
}
#endif
