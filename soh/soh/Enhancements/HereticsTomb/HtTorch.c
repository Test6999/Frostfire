// Heretics Tomb: torch the Ice Katana blows out. See HtTorch.h for params and the puzzle rule.
// Looks: the vanilla torch stand and flame (same model, flame and glow as the Light-the-Torches puzzles).

#include "HtTorch.h"
#include "Katana.h"
#include "functions.h"
#include <math.h>
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_syokudai/object_syokudai.h"

#define NO_GROUP 0x3F

// flame colours: orange, red, blue, green, purple
static const Color_RGB8 sFlamePrim[] = { { 255, 255, 0 }, { 255, 150, 110 }, { 170, 255, 255 }, { 200, 255, 150 }, { 255, 170, 255 }, { 255, 255, 190 } };
static const Color_RGB8 sFlameEnv[] = { { 255, 0, 0 }, { 220, 0, 0 }, { 0, 90, 255 }, { 0, 190, 0 }, { 150, 0, 255 }, { 255, 190, 0 } };
static const Color_RGB8 sFlameLight[] = { { 255, 255, 0 }, { 255, 60, 40 }, { 60, 140, 255 }, { 80, 255, 80 }, { 200, 80, 255 }, { 255, 220, 60 } };

#define FLAME_Y 52.0f

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_METAL,
        AT_NONE,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000000, 0x00, 0x00 },
        { 0xEE01FFFF | DMG_SPIN_GIANT | DMG_MAGIC_FIRE, 0x00, 0x00 }, // vanilla torch stand, the Iai wave, Din's Fire
        TOUCH_NONE,
        BUMP_ON, // not hookable: the tomb's torches must not be hookshot shortcuts (vanilla torches unchanged)
        OCELEM_ON,
    },
    { 16, 80, 0, { 0, 0, 0 } }, // tall enough to reach the flame: the slash wave flies at Link's waist
};

static s32 HtTorch_Group(HtTorch* this) {
    return this->actor.params & 0x3F;
}

// Is any other torch of this group still burning?
static s32 HtTorch_GroupHasFlame(HtTorch* this, PlayState* play) {
    Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head;

    for (; a != NULL; a = a->next) {
        if ((a->id == HT_ACTOR_TORCH) && (a->update != NULL) && (HtTorch_Group((HtTorch*)a) == HtTorch_Group(this)) &&
            ((HtTorch*)a)->lit) {
            return true;
        }
    }
    return false;
}

void HtTorch_Init(Actor* thisx, PlayState* play) {
    HtTorch* this = (HtTorch*)thisx;

    // the torch model is built at full scale (actors default to 0.01, which drew it 100 times too small)
    Actor_SetScale(&this->actor, 1.0f);
    this->color = (this->actor.params >> 10) & 7;
    if (this->color > 5) { // 5: yellow
        this->color = 0;
    }
    this->doneFlag = (this->actor.home.rot.z & 0x100) ? (this->actor.home.rot.z & 0x3F) : -1;
    this->fireGroup = (this->actor.home.rot.z & 0x200) != 0;
    this->pattern = (this->actor.home.rot.z & 0x400) != 0;
    this->want = (this->actor.home.rot.z & 0x800) != 0;
    this->resetFlag = this->actor.home.rot.z & 0x3F;
    this->actor.home.rot.z = this->actor.world.rot.z = this->actor.shape.rot.z = 0;
    this->actor.uncullZoneForward = 4000.0f;
    this->actor.uncullZoneScale = 800.0f;
    this->actor.uncullZoneDownward = 800.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    Lights_PointGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y + 70.0f,
                            this->actor.world.pos.z, 255, 255, 180, -1);
    // the drawing rooms hold up to 25 torches: no glow each (the scene's light pool is small)
    this->lightNode = this->pattern ? NULL : LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
    this->flameScroll = (s16)(Rand_ZeroOne() * 20.0f);
    this->flameGrow = 10;
    if (this->pattern) {
        // a solved drawing stays drawn; otherwise every torch starts burning
        this->lit = Flags_GetSwitch(play, HtTorch_Group(this)) ? this->want : true;
    } else if (this->fireGroup) {
        this->lit = Flags_GetSwitch(play, HtTorch_Group(this)); // the group was solved: burns for good
    } else if (this->actor.params & HT_TORCH_ETERNAL) {
        this->lit = true;
    } else if (this->actor.params & HT_TORCH_SWITCH_LIT) {
        this->lit = Flags_GetSwitch(play, HtTorch_Group(this)); // lit once its switch is set
    } else {
        // a solved group stays dark
        this->lit = !((HtTorch_Group(this) != NO_GROUP) && Flags_GetSwitch(play, HtTorch_Group(this)));
    }
    Actor_SetFocus(&this->actor, 60.0f);
}

void HtTorch_Destroy(Actor* thisx, PlayState* play) {
    HtTorch* this = (HtTorch*)thisx;

    LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
    Collider_DestroyCylinder(play, &this->collider);
}

static void HtTorch_BlowOut(HtTorch* this, PlayState* play) {
    static Color_RGBA8 prim = { 220, 245, 255, 255 };
    static Color_RGBA8 env = { 90, 170, 255, 255 };
    Vec3f flame = this->actor.world.pos;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f drift = { 0.0f, 0.6f, 0.0f };
    s32 steps = (this->actor.params >> 6) & 0xF;

    this->lit = false;
    this->relightTimer = steps * 40;
    flame.y += FLAME_Y;
    for (s32 i = 0; i < 6; i++) {
        Vec3f p = { flame.x + Rand_CenteredFloat(14.0f), flame.y + Rand_ZeroFloat(16.0f), flame.z + Rand_CenteredFloat(14.0f) };
        EffectSsKiraKira_SpawnDispersed(play, &p, &zero, &drift, &prim, &env, 2500, 20);
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_EXTINCT);

    if ((HtTorch_Group(this) != NO_GROUP) && !Flags_GetSwitch(play, HtTorch_Group(this)) &&
        !HtTorch_GroupHasFlame(this, play)) {
        Flags_SetSwitch(play, HtTorch_Group(this));
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    }
}

// --- pattern torches (the torch-drawing rooms) ---

static void HtTorch_PuffOut(HtTorch* this, PlayState* play) {
    static Color_RGBA8 prim = { 220, 245, 255, 255 };
    static Color_RGBA8 env = { 90, 170, 255, 255 };
    Vec3f flame = this->actor.world.pos;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f drift = { 0.0f, 0.6f, 0.0f };

    this->lit = false;
    flame.y += FLAME_Y;
    for (s32 i = 0; i < 6; i++) {
        Vec3f p = { flame.x + Rand_CenteredFloat(14.0f), flame.y + Rand_ZeroFloat(16.0f), flame.z + Rand_CenteredFloat(14.0f) };
        EffectSsKiraKira_SpawnDispersed(play, &p, &zero, &drift, &prim, &env, 2500, 20);
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_EXTINCT);
}

static void HtTorch_Ignite(HtTorch* this) {
    this->lit = true;
    this->flameGrow = 0;
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_IGNITION);
}

// Does every pattern torch of this group match the drawing? Then the group's flag is set.
static void HtTorch_PatternCheck(HtTorch* this, PlayState* play) {
    Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head;

    for (; a != NULL; a = a->next) {
        HtTorch* t = (HtTorch*)a;

        if ((a->id == HT_ACTOR_TORCH) && (a->update != NULL) && t->pattern &&
            (HtTorch_Group(t) == HtTorch_Group(this)) && (t->lit != t->want)) {
            return;
        }
    }
    Flags_SetSwitch(play, HtTorch_Group(this));
    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
}

static void HtTorch_UpdatePattern(HtTorch* this, PlayState* play) {
    s32 changed = false;

    if (Flags_GetSwitch(play, HtTorch_Group(this))) {
        this->collider.base.acFlags &= ~AC_HIT; // solved: the drawing stays
        return;
    }
    // the reset switch: every torch of the room lights again, and the switch pops back up
    if ((this->resetFlag != 0) && Flags_GetSwitch(play, this->resetFlag)) {
        Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head;

        for (; a != NULL; a = a->next) {
            HtTorch* t = (HtTorch*)a;

            if ((a->id == HT_ACTOR_TORCH) && (a->update != NULL) && t->pattern && (t->resetFlag == this->resetFlag) &&
                !t->lit) {
                HtTorch_Ignite(t);
            }
        }
        Flags_UnsetSwitch(play, this->resetFlag);
        changed = true;
    }
    if (this->collider.base.acFlags & AC_HIT) {
        Actor* attacker = this->collider.base.ac;
        ColliderInfo* hitBy = this->collider.info.acHitInfo;

        this->collider.base.acFlags &= ~AC_HIT;
        if (this->lit && (((attacker != NULL) && (attacker->id == HT_ACTOR_ICE_SLASH)) ||
                          ((attacker != NULL) && (attacker->id == ACTOR_PLAYER) && Katana_IsEquipped()) ||
                          ((hitBy != NULL) && (hitBy->toucher.dmgFlags & DMG_ARROW_ICE)))) {
            HtTorch_PuffOut(this, play);
            changed = true;
        } else if (!this->lit && (hitBy != NULL) && (hitBy->toucher.dmgFlags & (DMG_ARROW_FIRE | DMG_MAGIC_FIRE))) {
            HtTorch_Ignite(this);
            changed = true;
        }
    }
    if (changed) {
        HtTorch_PatternCheck(this, play);
    }
}

void HtTorch_TryLight(Actor* thisx, PlayState* play) {
    HtTorch* this = (HtTorch*)thisx;

    // (pattern torches too: a flamethrower relights a dark dot of the drawing until the drawing is done)
    if (this->lit || this->fireGroup || (this->actor.params & (HT_TORCH_SWITCH_LIT | HT_TORCH_ETERNAL)) || ((HtTorch_Group(this) != NO_GROUP) && Flags_GetSwitch(play, HtTorch_Group(this)))) {
        return;
    }
    this->lit = true;
    this->flameGrow = 0;
    this->relightTimer = 0;
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_IGNITION);
}

// Switch-lit torches: light up when their switch is set; a full set of them sets its done flag.
static void HtTorch_UpdateSwitchLit(HtTorch* this, PlayState* play) {
    this->collider.base.acFlags &= ~AC_HIT;
    if (!this->lit && Flags_GetSwitch(play, HtTorch_Group(this))) {
        this->lit = true;
        this->flameGrow = 0;
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_IGNITION);
    }
    if (this->lit && (this->doneFlag >= 0) && !Flags_GetSwitch(play, this->doneFlag)) {
        Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head;

        for (; a != NULL; a = a->next) {
            HtTorch* t = (HtTorch*)a;

            if ((a->id == HT_ACTOR_TORCH) && (a->update != NULL) && (a->params & HT_TORCH_SWITCH_LIT) &&
                (t->doneFlag == this->doneFlag) && !t->lit) {
                return;
            }
        }
        Flags_SetSwitch(play, this->doneFlag);
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    }
}

// Fire-lit group (rotZ 0x200): dark torches lit by fire arrows or Din's Fire. Each burns for its relight time
// (bits 6-9, 2 s steps) then goes out - unless every torch of its group (switch flag, bits 0-5) is burning at
// once: then the flag is set (with the chime) and they all burn for good.
static void HtTorch_UpdateFireGroup(HtTorch* this, PlayState* play) {
    s32 solved = Flags_GetSwitch(play, HtTorch_Group(this));

    if (this->collider.base.acFlags & AC_HIT) {
        ColliderInfo* hitBy = this->collider.info.acHitInfo;

        this->collider.base.acFlags &= ~AC_HIT;
        if (!this->lit && !solved && (hitBy != NULL) && (hitBy->toucher.dmgFlags & (DMG_ARROW_FIRE | DMG_MAGIC_FIRE))) {
            s32 steps = (this->actor.params >> 6) & 0xF;

            this->lit = true;
            this->flameGrow = 0;
            this->relightTimer = ((steps == 0) ? 4 : steps) * 40; // here: how long it burns
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_IGNITION);
        }
    }
    if (solved) {
        if (!this->lit) {
            this->lit = true;
            this->flameGrow = 0;
        }
        return;
    }
    if (this->lit) {
        Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head;
        s32 allLit = true;

        for (; a != NULL; a = a->next) {
            HtTorch* t = (HtTorch*)a;

            if ((a->id == HT_ACTOR_TORCH) && (a->update != NULL) && t->fireGroup &&
                (HtTorch_Group(t) == HtTorch_Group(this)) && !t->lit) {
                allLit = false;
                break;
            }
        }
        if (allLit) {
            Flags_SetSwitch(play, HtTorch_Group(this));
            Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
            return;
        }
        if (--this->relightTimer <= 0) {
            this->lit = false; // too slow: it gutters out
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_EXTINCT);
        }
    }
}

void HtTorch_Update(Actor* thisx, PlayState* play) {
    HtTorch* this = (HtTorch*)thisx;
    s32 lightRadius = -1;
    u8 brightness = 0;

    if (this->pattern) {
        HtTorch_UpdatePattern(this, play);
    } else if (this->actor.params & HT_TORCH_ETERNAL) {
        this->collider.base.acFlags &= ~AC_HIT; // decoration: always burning
    } else if (this->actor.params & HT_TORCH_SWITCH_LIT) {
        HtTorch_UpdateSwitchLit(this, play);
    } else if (this->fireGroup) {
        HtTorch_UpdateFireGroup(this, play);
    } else if (this->collider.base.acFlags & AC_HIT) {
        Actor* attacker = this->collider.base.ac;
        ColliderInfo* hitBy = this->collider.info.acHitInfo;

        this->collider.base.acFlags &= ~AC_HIT;
        // the slash wave, an ice arrow, or the katana's blade itself (up close the wave is born past the torch)
        if (this->lit && (((attacker != NULL) && (attacker->id == HT_ACTOR_ICE_SLASH)) ||
                          ((attacker != NULL) && (attacker->id == ACTOR_PLAYER) && Katana_IsEquipped()) ||
                          ((hitBy != NULL) && (hitBy->toucher.dmgFlags & DMG_ARROW_ICE)))) {
            HtTorch_BlowOut(this, play);
        }
    }

    if (!this->fireGroup && !this->pattern && !this->lit && (this->relightTimer > 0) &&
        !((HtTorch_Group(this) != NO_GROUP) && Flags_GetSwitch(play, HtTorch_Group(this)))) {
        if (--this->relightTimer == 0) {
            this->lit = true;
            this->flameGrow = 0;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_IGNITION);
        }
    }
    if (this->flameGrow < 10) {
        this->flameGrow++;
    }

    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);

    if (this->lit) {
        lightRadius = 200 * this->flameGrow / 10;
        brightness = (u8)(Rand_ZeroOne() * 127.0f) + 128;
        Actor_PlaySfx_Flagged(&this->actor, NA_SE_EV_TORCH - SFX_FLAG);
    }
    Lights_PointSetColorAndRadius(&this->lightInfo, sFlameLight[this->color].r * brightness / 255,
                                  sFlameLight[this->color].g * brightness / 255,
                                  sFlameLight[this->color].b * brightness / 255, lightRadius);
    this->flameScroll++;
}

void HtTorch_Draw(Actor* thisx, PlayState* play) {
    HtTorch* this = (HtTorch*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (this->actor.params & 0x2000) ? gWoodenTorchDL : gGoldenTorchDL);

    if (this->lit) {
        f32 flameScale = 0.0027f * this->flameGrow / 10.0f;

        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0, (this->flameScroll * -20) & 0x1FF,
                                      0x20, 0x80, 0, 0, 0, -20));
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, sFlamePrim[this->color].r, sFlamePrim[this->color].g,
                        sFlamePrim[this->color].b, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, sFlameEnv[this->color].r, sFlameEnv[this->color].g, sFlameEnv[this->color].b, 0);
        Matrix_Translate(0.0f, FLAME_Y, 0.0f, MTXMODE_APPLY);
        {
            // the flame is a flat sprite turned toward the camera about the vertical; seen from steeply above (the
            // torch rooms' overlook) it would be edge-on and vanish, so then it faces the camera fully
            Camera* cam = GET_ACTIVE_CAM(play);
            f32 dy = cam->eye.y - cam->at.y;
            f32 dxz = sqrtf(SQ(cam->eye.x - cam->at.x) + SQ(cam->eye.z - cam->at.z));

            if (dy > dxz) {
                Matrix_ReplaceRotation(&play->billboardMtxF);
            } else {
                Matrix_RotateY((s16)(Camera_GetCamDirYaw(cam) - this->actor.shape.rot.y + 0x8000) * (M_PI / 0x8000),
                               MTXMODE_APPLY);
            }
        }
        Matrix_Scale(flameScale, flameScale, flameScale, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
