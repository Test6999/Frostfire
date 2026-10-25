// Heretics Tomb: frost shell over katana-frozen enemies, and fire enemies losing their flames. See HtFrostShell.h.

#include "HtFrostShell.h"
#include "HtFlameJet.h" // HtIce_DrawCrystals
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "overlays/actors/ovl_En_Firefly/z_en_firefly.h"

#define MAX_SHELLS 16
#define SHELL_FADE_FRAMES 15

static HtFrostShell* sShells[MAX_SHELLS];

static void HtFrostShell_Track(HtFrostShell* this, s32 add) {
    for (s32 i = 0; i < MAX_SHELLS; i++) {
        if (add ? (sShells[i] == NULL) : (sShells[i] == this)) {
            sShells[i] = add ? this : NULL;
            return;
        }
    }
}

void HtFrostShell_OnActorDestroy(Actor* actor) {
    for (s32 i = 0; i < MAX_SHELLS; i++) {
        if ((sShells[i] != NULL) && (sShells[i]->target == actor)) {
            sShells[i]->target = NULL;
        }
    }
}

void HtFrostShell_Apply(PlayState* play, Actor* target, s16 frames) {
    HtFrostShell* shell;

    Actor_SetColorFilter(target, 0, 0xFF, 0, frames);
    for (s32 i = 0; i < MAX_SHELLS; i++) {
        if ((sShells[i] != NULL) && (sShells[i]->target == target)) {
            sShells[i]->timer = frames; // already frozen: refresh
            return;
        }
    }
    shell = (HtFrostShell*)Actor_Spawn(&play->actorCtx, play, HT_ACTOR_FROST_SHELL, target->world.pos.x,
                                       target->world.pos.y, target->world.pos.z, 0, 0, 0, 0);
    if (shell != NULL) {
        shell->target = target;
        shell->timer = frames;
    }
}

// small exported hooks added to those enemies' own files
void EnBb_HtChill(Actor* thisx, PlayState* play, s32 fromBlade);
void EnBw_HtChill(Actor* thisx, PlayState* play, s32 fromBlade);
void EnDodongo_HtChill(Actor* thisx, PlayState* play, s32 fromBlade);

static s32 HtFrostShell_Chill(PlayState* play, Actor* target, s32 fromBlade) {
    switch (target->id) {
        case ACTOR_EN_BB:
            EnBb_HtChill(target, play, fromBlade);
            return false;
        case ACTOR_EN_BW:
            EnBw_HtChill(target, play, fromBlade);
            return true;
        case ACTOR_EN_DODONGO:
            EnDodongo_HtChill(target, play, fromBlade);
            return true;
    }
    if (target->id == ACTOR_EN_FIREFLY) {
        EnFirefly* keese = (EnFirefly*)target;

        if (keese->onFire) {
            // same as the Keese's own EnFirefly_Extinguish: it becomes a plain Keese for good
            keese->actor.params += 2;
            keese->collider.elements[0].info.toucher.effect = 0;
            keese->auraType = 0; // no aura
            keese->onFire = false;
            keese->actor.naviEnemyId = 0x12; // Keese
            Audio_PlayActorSound2(target, NA_SE_EN_EXTINCT);
        }
    }
    return false;
}

s32 HtFrostShell_ChillEnemy(PlayState* play, Actor* target) {
    return HtFrostShell_Chill(play, target, false);
}

void Katana_OnBladeHit(PlayState* play, Actor* target) {
    if ((target != NULL) && (target->category == ACTORCAT_ENEMY) && (target->update != NULL)) {
        HtFrostShell_Chill(play, target, true);
    }
}

void HtFrostShell_Init(Actor* thisx, PlayState* play) {
    HtFrostShell* this = (HtFrostShell*)thisx;

    this->seed = (s16)(Rand_ZeroOne() * 1000.0f);
    HtFrostShell_Track(this, true);
}

void HtFrostShell_Destroy(Actor* thisx, PlayState* play) {
    HtFrostShell_Track((HtFrostShell*)thisx, false);
}

void HtFrostShell_Update(Actor* thisx, PlayState* play) {
    HtFrostShell* this = (HtFrostShell*)thisx;

    if ((this->target == NULL) || (this->target->update == NULL) || (--this->timer <= 0)) {
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.world.pos = this->target->world.pos; // keeps culling and sound in the right place
}

void HtFrostShell_Draw(Actor* thisx, PlayState* play) {
    HtFrostShell* this = (HtFrostShell*)thisx;
    Actor* t = this->target;
    f32 radius, height;
    s32 layers;
    u8 alpha;

    if ((t == NULL) || (t->update == NULL)) {
        return;
    }
    radius = (t->colChkInfo.cylRadius > 0) ? t->colChkInfo.cylRadius : 25.0f;
    height = (t->colChkInfo.cylHeight > 0) ? t->colChkInfo.cylHeight : 40.0f;
    radius = CLAMP(radius, 12.0f, 60.0f);
    height = CLAMP(height, 20.0f, 120.0f);
    alpha = (this->timer < SHELL_FADE_FRAMES) ? (u8)(this->timer * 255 / SHELL_FADE_FRAMES) : 255;

    // crystals in rings up the body, centred on the enemy's own position (flyers included)
    layers = (s32)(height / 30.0f) + 1;
    for (s32 i = 0; i < layers; i++) {
        Vec3f center = t->world.pos;

        center.y += (layers > 1) ? (height * i / (layers - 1)) : (height * 0.5f);
        if (!(t->bgCheckFlags & BGCHECKFLAG_GROUND)) {
            center.y -= height * 0.5f; // airborne enemies (Keese...) have their body around world.pos
        }
        HtIce_DrawCrystals(play, &center, radius, 5, this->seed + i * 17, alpha);
    }
}
