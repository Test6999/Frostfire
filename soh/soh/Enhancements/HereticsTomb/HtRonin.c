// Heretics Tomb mini-boss: the Frozen Ronin (a samurai Stalfos with the Ice Katana). See HtRonin.h for the moves.

#include "HtRonin.h"
#include "Katana.h"
#include "HtKatanaPickup.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/object_sk2/object_sk2.h"
#include "overlays/actors/ovl_En_Test/z_en_test.h" // STALFOS_LIMB_*
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h" // EFFECT_HITMARK_METAL
#include <string.h>
#include <math.h>

typedef enum {
    RONIN_WAIT,
    RONIN_RISE,
    RONIN_STALK,
    RONIN_DRAW_WINDUP, // hand to the hilt
    RONIN_DRAW_CUT,    // the quick draw itself
    RONIN_SHEATHE,     // blade back in the scabbard
    RONIN_CHARGE,
    RONIN_LUNGE,
    RONIN_LUNGE_RECOVER,
    RONIN_ICICLE,
    RONIN_GUARD,
    RONIN_REBOUND,
    RONIN_HURT,
    RONIN_BACKJUMP,
    RONIN_STUN,
    RONIN_PHASE,
    RONIN_DYING,
    RONIN_FLASH, // the flash step: gone into the mist, reappearing by Link
} HtRoninAction;

// the kasa (a wide straw sandogasa, built with the tomb: build_heretics_dungeon.py ht_kasa)
#define HT_RONIN_KASA_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_kasa_dl"
#define RONIN_HEALTH 24
#define RONIN_SCALE 0.02f    // a vanilla Stalfos is 0.015: a head taller than Link
#define RONIN_WAKE_DIST 360.0f
#define RONIN_IFRAMES 40     // after a hit he can't be hit again for 2 s (no Biggoron spin-attack spam); he flickers
#define RONIN_BURN 60        // dead: after he falls, this long burning away in blue fire (the last 15: only the fire)
#define ICICLE_SPACING 38.0f // a new icicle every ICICLE_EVERY frames, this much further along the line
#define ICICLE_EVERY 3
#define ICICLE_PER_LINE 13   // about 500 units of line
#define ICICLE_LIFE 45       // frames standing before it shatters
#define ICICLE_RISE 4        // frames to burst up to full size
#define ICICLE_HURT 12       // frames (from bursting) during which it freezes Link

#define PARRY_CHANCE_1 0.4f  // a sword swing at him while he's poised: parried and answered (phase 1 / 2)
#define PARRY_CHANCE_2 0.6f
#define FLASH_FRAMES 18      // the flash step: gone this long; the mist marks the spot for the last FLASH_TELL
#define FLASH_TELL 10
#define FLASH_DIST 95.0f     // how far from Link he reappears
#define SHIELD_PATIENCE 30   // phase 2: frames of Link behind his shield before he steps round it

#define SAYA_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_saya_dl"
#define ICICLE_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_icicle_dl"

// The katana model is built for Link's hand (1000 units = 10 world units at Link's 0.01 scale). In the Ronin's sword
// limb (actor scale 0.02) it is drawn at this fraction, so the blade is ~90 long - a long katana for a tall swordsman.
#define KATANA_IN_HAND_SCALE 0.7f
// The scabbard on the belt, worn samurai fashion on the left hip: hilt forward, point back and a little down. Placed
// in the actor's own frame (facing +z, left = +x) from the waist's position, so it never depends on the Stalfos
// limb axes. Model scale and offsets (world units).
#define SAYA_SCALE 0.014f
#define SAYA_SIDE 17.0f   // out from the spine toward the left
#define SAYA_FWD 10.0f    // the hilt sits a little ahead of the hip
#define SAYA_DROP -4.0f
#define SAYA_TILT -0.35f  // point down, radians

static ColliderCylinderInit sBodyInit = {
    { COLTYPE_HIT0, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 26, 90, 0, { 0, 0, 0 } },
};

static ColliderQuadInit sSwordInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x10 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
      BUMP_NONE, OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// the cut's hitbox: a cylinder out in front of him for the swing's cutting frames, where the blade sweeps. (The blade
// itself - the two-handed grip, worked out at draw time - made a hitbox that kept missing Link, swept or not.)
static ColliderCylinderInit sCutInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x10 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_NONE },
    { 48, 80, 5, { 0, 0, 0 } },
};
#define CUT_REACH 58.0f          // the cut cylinder's middle, ahead of him (its radius 48: from 10 to 106 out)
#define CUT_FROM 0.15f           // the cutting part of a slash animation (fractions of it)
#define CUT_TO 0.7f

static ColliderCylinderInit sIcicleInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_ICE, 0x08 }, { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NONE, BUMP_NONE, OCELEM_NONE },
    { 20, 45, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sColChkInfoInit = { RONIN_HEALTH, 26, 90, MASS_HEAVY };

static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0), // no stun: nothing stops him for free
    /* Deku stick    */ DMG_ENTRY(2, 0x0),
    /* Slingshot     */ DMG_ENTRY(0, 0x0),
    /* Explosive     */ DMG_ENTRY(2, 0x0),
    /* Boomerang     */ DMG_ENTRY(0, 0x0),
    /* Normal arrow  */ DMG_ENTRY(0, 0x0), // arrows glance off him (they made him too easy)
    /* Hammer swing  */ DMG_ENTRY(4, 0x0),
    /* Hookshot      */ DMG_ENTRY(0, 0x0),
    /* Kokiri sword  */ DMG_ENTRY(1, 0x0),
    /* Master sword  */ DMG_ENTRY(2, 0x0),
    /* Giant's Knife */ DMG_ENTRY(4, 0x0),
    /* Fire arrow    */ DMG_ENTRY(0, 0x0),
    /* Ice arrow     */ DMG_ENTRY(0, 0x0),
    /* Light arrow   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 1   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 2   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(0, 0x0),
    /* Fire magic    */ DMG_ENTRY(0, 0x0), // Din's Fire: nothing (it was a free guard break)
    /* Ice magic     */ DMG_ENTRY(0, 0x0),
    /* Light magic   */ DMG_ENTRY(3, 0x0),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(0, 0x0), // spin attacks glance off: no spamming him down
    /* Giant spin    */ DMG_ENTRY(0, 0x0),
    /* Master spin   */ DMG_ENTRY(0, 0x0),
    /* Kokiri jump   */ DMG_ENTRY(2, 0x0),
    /* Giant jump    */ DMG_ENTRY(8, 0x0),
    /* Master jump   */ DMG_ENTRY(4, 0x0),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(4, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

// The katana's blade in the sword limb's space: the Stalfos sword limb, like Link's hand, holds its blade along +x.
static Vec3f sBladeBase = { 300.0f, 0.0f, 0.0f };
static Vec3f sBladeTip = { 5600.0f, -250.0f, 0.0f };
static Vec3f sBladeSideA = { 3200.0f, -150.0f, -900.0f };
static Vec3f sBladeSideB = { 3200.0f, -150.0f, 900.0f };

static Color_RGBA8 sFrostPrim = { 220, 245, 255, 255 };
static Color_RGBA8 sFrostEnv = { 90, 170, 255, 0 };

// ---------------------------------------------------------------------------------------------------------------

static void Ronin_Anim(HtRonin* this, AnimationHeader* anim, f32 speed, u8 mode, f32 morph) {
    Animation_Change(&this->skelAnime, anim, speed, 0.0f, Animation_GetLastFrame(anim), mode, morph);
}

static void Ronin_Loop(HtRonin* this, AnimationHeader* anim, f32 speed) {
    if ((this->skelAnime.animation != anim) || (this->skelAnime.mode != ANIMMODE_LOOP)) {
        Ronin_Anim(this, anim, speed, ANIMMODE_LOOP, -4.0f);
    }
    this->skelAnime.playSpeed = speed;
}

static f32 Ronin_Speed(HtRonin* this) {
    return this->phase ? 1.3f : 1.0f;
}

static void Ronin_Sparkle(PlayState* play, Vec3f* pos, f32 spread) {
    Vec3f p = { pos->x + Rand_CenteredFloat(spread), pos->y + Rand_CenteredFloat(spread),
                pos->z + Rand_CenteredFloat(spread) };
    Vec3f vel = { 0.0f, 0.6f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    EffectSsKiraKira_SpawnSmall(play, &p, &vel, &accel, &sFrostPrim, &sFrostEnv);
}

static void Ronin_IceSmoke(PlayState* play, Vec3f* pos, s16 scale) {
    Vec3f p = { pos->x + Rand_CenteredFloat(20.0f), pos->y, pos->z + Rand_CenteredFloat(20.0f) };
    Vec3f vel = { 0.0f, 1.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.1f, 0.0f };

    EffectSsIceSmoke_Spawn(play, &p, &vel, &accel, scale);
}

// He fights with both blades out all the time now (the empty scabbard stays on his belt): drawing and sheathing are
// only the cut's start and end.
static void Ronin_Unsheathe(HtRonin* this) {
    this->bladeOut = true;
}

static void Ronin_Sheathe(HtRonin* this) {
    this->swordActive = false;
}

// --- setups ----------------------------------------------------------------------------------------------------

static void Ronin_SetupStalk(HtRonin* this, s16 minFrames) {
    this->action = RONIN_STALK;
    this->swordActive = false;
    this->airborne = false;
    this->timer = minFrames + (s16)(Rand_ZeroOne() * (this->phase ? 20.0f : 35.0f));
    if (Rand_ZeroOne() < 0.5f) {
        this->strafeDir = -this->strafeDir;
    }
}

static void Ronin_SetupRise(HtRonin* this) {
    this->action = RONIN_RISE;
    Ronin_Anim(this, (AnimationHeader*)gStalfosReleaseAttentionAnim, 1.0f, ANIMMODE_ONCE, -4.0f);
    this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    func_800F5ACC(NA_BGM_MINI_BOSS);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_WARAU);
}

static void Ronin_SetupDraw(HtRonin* this) {
    // the hand goes to the hilt: a short, readable pause before the cut
    this->action = RONIN_DRAW_WINDUP;
    this->actor.speedXZ = 0.0f;
    this->timer = this->phase ? 5 : 9;
    Ronin_Anim(this, (AnimationHeader*)gStalfosMiddleGuardAnim, 0.0f, ANIMMODE_ONCE, -3.0f);
}

static void Ronin_SetupCut(HtRonin* this, s32 damage) {
    // the draw: blade out of the scabbard and through the target in one motion
    this->action = RONIN_DRAW_CUT;
    this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.speedXZ = 7.0f;
    this->cutCollider.info.toucher.damage = damage;
    this->cutCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    Ronin_Unsheathe(this);
    Ronin_Anim(this,
               (AnimationHeader*)((this->cuts % 2) ? gStalfosDownSlashAnim : gStalfosUpSlashAnim),
               2.0f * Ronin_Speed(this), ANIMMODE_ONCE, 0.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
}

// the counter-cut (a parry, or a sword taken on the raised guard): Link is right on him, inside the swing's arc, so
// he cuts while stepping back - the blade meets Link instead of sweeping past behind him
static void Ronin_SetupCounter(HtRonin* this) {
    this->cuts = 1;
    Ronin_SetupCut(this, 0x18);
    this->actor.speedXZ = -7.0f;
}

static void Ronin_SetupSheathe(HtRonin* this) {
    this->action = RONIN_SHEATHE;
    this->swordActive = false;
    this->timer = this->phase ? 10 : 16; // blade held out a beat, then snapped back in: the opening to punish
    Ronin_Anim(this, (AnimationHeader*)gStalfosMiddleGuardAnim, 1.0f, ANIMMODE_LOOP, -6.0f);
}

static void Ronin_SetupCharge(HtRonin* this) {
    this->action = RONIN_CHARGE;
    this->actor.speedXZ = 0.0f;
    this->timer = this->phase ? 18 : 28;
    Ronin_Anim(this, (AnimationHeader*)gStalfosJumpAnim, 0.0f, ANIMMODE_ONCE, -4.0f); // crouched on its first frame
    HtSfx_PlayFor(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, NA_SE_IT_SWORD_CHARGE,
                  this->timer);
}

static void Ronin_SetupLunge(HtRonin* this) {
    f32 dist = this->actor.xzDistToPlayer;

    this->action = RONIN_LUNGE;
    this->airborne = true;
    this->timer = 6;
    this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.velocity.y = 9.0f;
    this->actor.speedXZ = CLAMP((dist - 20.0f) / 18.0f, 8.0f, 24.0f); // lands about where Link stood
    this->cutCollider.info.toucher.damage = 0x20;
    this->cutCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    Ronin_Unsheathe(this);
    this->swordActive = true;
    Ronin_Anim(this, (AnimationHeader*)gStalfosJumpslashAnim, 1.0f, ANIMMODE_ONCE, -2.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_JUMP);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_SAKEBI);
}

static void Ronin_SetupIcicle(HtRonin* this) {
    this->action = RONIN_ICICLE;
    this->actor.speedXZ = 0.0f;
    this->timer = 0;
    this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    Ronin_Unsheathe(this);
    Ronin_Anim(this, (AnimationHeader*)gStalfosDownSlashAnim, 1.1f * Ronin_Speed(this), ANIMMODE_ONCE, -3.0f);
}

static void Ronin_SetupGuard(HtRonin* this) {
    this->action = RONIN_GUARD;
    this->actor.speedXZ = 0.0f;
    Ronin_Sheathe(this);
    this->timer = this->phase ? 60 : 80;
    Ronin_Anim(this, (AnimationHeader*)gStalfosBlockWithShieldAnim, 2.0f, ANIMMODE_ONCE, 2.0f);
}

static void Ronin_SetupRebound(HtRonin* this) {
    this->action = RONIN_REBOUND;
    this->swordActive = false;
    this->actor.speedXZ = -4.0f;
    Ronin_Anim(this, (AnimationHeader*)gStalfosFlinchFromHitFrontAnim, 1.5f, ANIMMODE_ONCE, 0.0f);
}

static void Ronin_SetupHurt(HtRonin* this) {
    this->action = RONIN_HURT;
    this->swordActive = false;
    this->actor.speedXZ = -3.0f;
    Ronin_Anim(this, (AnimationHeader*)gStalfosFlinchFromHitFrontAnim, 1.0f, ANIMMODE_ONCE, 0.0f);
}

static void Ronin_SetupBackjump(HtRonin* this) {
    this->action = RONIN_BACKJUMP;
    this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.speedXZ = -11.0f;
    Ronin_Sheathe(this);
    Ronin_Anim(this, (AnimationHeader*)gStalfosJumpBackwardsAnim, 1.0f, ANIMMODE_ONCE, -2.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_JUMP);
}

static void Ronin_SetupStun(HtRonin* this) {
    this->action = RONIN_STUN;
    this->swordActive = false;
    this->actor.speedXZ = 0.0f;
    this->timer = 45;
    Actor_SetColorFilter(&this->actor, 0, 120, 0, 45);
    Ronin_Anim(this, (AnimationHeader*)gStalfosFlinchFromHitFrontAnim, 0.4f, ANIMMODE_ONCE, 0.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_DAMAGE);
}

static void Ronin_SetupPhase(HtRonin* this) {
    this->action = RONIN_PHASE;
    this->actor.speedXZ = 0.0f;
    Ronin_Sheathe(this);
    this->timer = 40;
    Ronin_Anim(this, (AnimationHeader*)gStalfosLaughAnim, 1.0f, ANIMMODE_LOOP, -4.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_WARAU);
}

static void Ronin_SetupDying(HtRonin* this) {
    s32 i;

    this->action = RONIN_DYING;
    this->swordActive = false;
    this->actor.speedXZ = 0.0f;
    this->timer = 45 + RONIN_BURN; // (the fall, then the burning)
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    for (i = 0; i < HT_RONIN_LINES; i++) {
        this->lines[i].active = false;
    }
    Actor_SetColorFilter(&this->actor, 0, 255, 0, 45);
    Ronin_Anim(this, (AnimationHeader*)gStalfosFallOverForwardsAnim, 1.0f, ANIMMODE_ONCE, -4.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_DEAD);
}

static void Ronin_SetupFlash(HtRonin* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 r = Rand_ZeroOne();
    // behind him (half the time), else to one side
    s16 ang = player->actor.shape.rot.y + ((r < 0.5f) ? 0x8000 : ((r < 0.75f) ? 0x4000 : -0x4000));
    Vec3f t;
    s32 i;

    t.x = player->actor.world.pos.x + Math_SinS(ang) * FLASH_DIST;
    t.y = this->actor.world.pos.y;
    t.z = player->actor.world.pos.z + Math_CosS(ang) * FLASH_DIST;
    // (inside the arena: its walls are 400 from his home in the middle)
    t.x = CLAMP(t.x, this->actor.home.pos.x - 340.0f, this->actor.home.pos.x + 340.0f);
    t.z = CLAMP(t.z, this->actor.home.pos.z - 340.0f, this->actor.home.pos.z + 340.0f);
    this->flashTarget = t;
    this->action = RONIN_FLASH;
    this->timer = FLASH_FRAMES;
    this->hidden = true;
    this->swordActive = false;
    this->actor.speedXZ = 0.0f;
    this->actor.shape.shadowAlpha = 0;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    for (i = 0; i < 8; i++) {
        Ronin_IceSmoke(play, &this->actor.world.pos, 220);
        Ronin_Sparkle(play, &this->actor.focus.pos, 50.0f);
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_SWING);
}

static void Ronin_ChooseAttack(HtRonin* this, PlayState* play) {
    f32 dist = this->actor.xzDistToPlayer;
    s32 pick = RONIN_DRAW_WINDUP;
    s32 tries;

    // phase 2: now and then he steps out of the mist beside Link instead
    if (this->phase && (this->lastAttack != RONIN_FLASH) && (Rand_ZeroOne() < 0.25f)) {
        this->lastAttack = RONIN_FLASH;
        Ronin_SetupFlash(this, play);
        return;
    }

    for (tries = 0; tries < 2; tries++) {
        f32 r = Rand_ZeroOne();

        if (dist < 140.0f) {
            pick = (r < 0.55f) ? RONIN_DRAW_WINDUP : ((r < 0.85f) ? RONIN_GUARD : RONIN_BACKJUMP);
        } else if (dist < 350.0f) {
            pick = (r < 0.4f) ? RONIN_CHARGE : ((r < 0.75f) ? RONIN_ICICLE : RONIN_GUARD);
        } else {
            pick = (r < 0.55f) ? RONIN_ICICLE : RONIN_CHARGE;
        }
        if ((pick != this->lastAttack) || (pick == RONIN_DRAW_WINDUP)) {
            break;
        }
    }
    this->lastAttack = pick;

    switch (pick) {
        case RONIN_GUARD:
            Ronin_SetupGuard(this);
            break;
        case RONIN_BACKJUMP:
            Ronin_SetupBackjump(this);
            break;
        case RONIN_CHARGE:
            Ronin_SetupCharge(this);
            break;
        case RONIN_ICICLE:
            Ronin_SetupIcicle(this);
            break;
        default:
            this->cuts = this->phase ? 2 : 1;
            Ronin_SetupDraw(this);
            break;
    }
}

// --- the icicle lines ------------------------------------------------------------------------------------------

static void Ronin_StartLines(HtRonin* this, PlayState* play) {
    s32 count = this->phase ? 3 : 1;
    s32 i;

    for (i = 0; i < count; i++) {
        HtRoninLine* line = &this->lines[i];

        line->active = true;
        line->count = 0;
        line->cooldown = 0;
        line->yaw = this->actor.shape.rot.y + ((count == 1) ? 0 : (i - 1) * 0x1400);
        line->origin = this->actor.world.pos;
        line->origin.y = this->actor.floorHeight;
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_SWING);
}

static void Ronin_RaiseIcicle(HtRonin* this, PlayState* play, HtRoninLine* line) {
    f32 d = 50.0f + line->count * ICICLE_SPACING;
    Vec3f pos = { line->origin.x + Math_SinS(line->yaw) * d, line->origin.y,
                  line->origin.z + Math_CosS(line->yaw) * d };
    s32 i;

    // stop at the arena walls (the actor's home is the arena's centre, 400 from each wall)
    if ((fabsf(pos.x - this->actor.home.pos.x) > 380.0f) || (fabsf(pos.z - this->actor.home.pos.z) > 380.0f)) {
        line->active = false;
        return;
    }
    for (i = 0; i < HT_RONIN_ICICLES; i++) {
        if (this->icicles[i].timer == 0) {
            this->icicles[i].pos = pos;
            this->icicles[i].timer = 1;
            this->icicles[i].yaw = (s16)(Rand_ZeroOne() * 0x10000);
            this->icicleColliders[i].base.atFlags &= ~(AT_HIT | AT_BOUNCED);
            break;
        }
    }
    Ronin_IceSmoke(play, &pos, 150);
    if ((line->count % 2) == 0) {
        SoundSource_PlaySfxAtFixedWorldPos(play, &pos, 10, NA_SE_EV_ICE_FREEZE);
    }
}

static void Ronin_UpdateIcicles(HtRonin* this, PlayState* play) {
    s32 i;


    for (i = 0; i < HT_RONIN_LINES; i++) {
        HtRoninLine* line = &this->lines[i];

        if (!line->active) {
            continue;
        }
        if (line->cooldown > 0) {
            line->cooldown--;
            continue;
        }
        Ronin_RaiseIcicle(this, play, line);
        line->count++;
        line->cooldown = ICICLE_EVERY - 1;
        if (line->count >= ICICLE_PER_LINE) {
            line->active = false;
        }
    }

    for (i = 0; i < HT_RONIN_ICICLES; i++) {
        HtRoninIcicle* ice = &this->icicles[i];
        ColliderCylinder* col = &this->icicleColliders[i];

        if (ice->timer == 0) {
            continue;
        }
        if (ice->timer > ICICLE_LIFE) {
            // shatters
            EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &ice->pos, 150, 150, 150, 250, 235, 245, 255, 0.6f);
            ice->timer = 0;
            continue;
        }
        if ((ice->timer <= ICICLE_HURT) && !(col->base.atFlags & AT_HIT)) {
            col->dim.pos.x = ice->pos.x;
            col->dim.pos.y = ice->pos.y;
            col->dim.pos.z = ice->pos.z;
            CollisionCheck_SetAT(play, &play->colChkCtx, &col->base);
        }
        ice->timer++;
    }
}

// --- hits ------------------------------------------------------------------------------------------------------

static void Ronin_CheckHits(HtRonin* this, PlayState* play) {
    u32 flags;

    if (!(this->bodyCollider.base.acFlags & AC_HIT)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~(AC_HIT | AC_BOUNCED);
    flags = (this->bodyCollider.info.acHitInfo != NULL) ? this->bodyCollider.info.acHitInfo->toucher.dmgFlags : 0;

    if (this->action == RONIN_GUARD) {
        if (flags & (DMG_SWORD | DMG_HAMMER | DMG_DEKU_STICK)) {
            // parried on the scabbard: answer with a draw
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
            Ronin_SetupCounter(this);
        } else if (flags & DMG_EXPLOSIVE) {
            Ronin_SetupStun(this); // a bomb knocks the guard aside (not arrows, nuts, the hookshot or Din's Fire)
        } else if (flags & DMG_ARROW) {
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW); // (cut out of the air)
        }
        return;
    }
    if (this->action == RONIN_WAIT) {
        Ronin_SetupRise(this);
        return;
    }
    if ((this->action == RONIN_RISE) || (this->action == RONIN_PHASE) || (this->action == RONIN_DYING) ||
        (this->hurtTimer > 0)) {
        return;
    }
    if (flags & (DMG_HOOKSHOT | DMG_DEKU_NUT)) {
        return; // impervious: no stun-locking him
    }
    if (flags & DMG_ARROW) {
        // he cuts arrows out of the air: a clang and a spark, no harm done
        Vec3f spark = this->actor.focus.pos;

        EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &spark);
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
        return;
    }
    if (flags & DMG_SPIN_ATTACK) {
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW); // a spin glances off him
        return;
    }
    if ((this->action == RONIN_STALK) && (flags & DMG_SWORD) &&
        (Rand_ZeroOne() < (this->phase ? PARRY_CHANCE_2 : PARRY_CHANCE_1))) {
        // poised, he reads the swing: the blade turned aside with a ringing clang, and the counter-cut at once
        Vec3f spark = this->actor.focus.pos;

        spark.y -= 25.0f;
        EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &spark);
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
        Ronin_SetupCounter(this);
        return;
    }
    if (this->actor.colChkInfo.damage == 0) {
        return;
    }

    Actor_ApplyDamage(&this->actor);
    this->hurtTimer = RONIN_IFRAMES;
    Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 8);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_DAMAGE);

    if (this->actor.colChkInfo.health == 0) {
        Ronin_Sheathe(this);
        Enemy_StartFinishingBlow(play, &this->actor);
        Ronin_SetupDying(this);
    } else if (!this->phase && (this->actor.colChkInfo.health <= RONIN_HEALTH / 2)) {
        Ronin_SetupPhase(this);
    } else if ((this->action != RONIN_LUNGE) && (this->action != RONIN_STUN)) {
        Ronin_Sheathe(this);
        Ronin_SetupHurt(this);
    }
}

// --- actions ---------------------------------------------------------------------------------------------------

static void Ronin_Stalk(HtRonin* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 dist = this->actor.xzDistToPlayer;

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x1000, 0x100);
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_WALL) || (Rand_ZeroOne() < 0.015f)) {
        this->strafeDir = -this->strafeDir;
    }

    if (dist > 280.0f) {
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        Math_StepToF(&this->actor.speedXZ, 4.0f * Ronin_Speed(this), 0.5f);
        Ronin_Loop(this, (AnimationHeader*)gStalfosFastAdvanceAnim, 1.0f);
    } else if (dist > 110.0f) {
        this->actor.world.rot.y = this->actor.yawTowardsPlayer + this->strafeDir * 0x4000;
        Math_StepToF(&this->actor.speedXZ, 2.5f * Ronin_Speed(this), 0.5f);
        Ronin_Loop(this, (AnimationHeader*)gStalfosSidestepAnim, (this->strafeDir > 0) ? 1.0f : -1.0f);
    } else {
        Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
        Ronin_Loop(this, (AnimationHeader*)gStalfosMiddleGuardAnim, 1.0f);
    }

    // phase 2: a Link who hides behind his shield gets stepped round
    if (player->stateFlags1 & PLAYER_STATE1_SHIELDING) {
        this->shieldFrames++;
    } else {
        this->shieldFrames = 0;
    }
    if (this->phase && (this->shieldFrames > SHIELD_PATIENCE) && (dist < 320.0f)) {
        this->shieldFrames = 0;
        this->lastAttack = RONIN_FLASH;
        Ronin_SetupFlash(this, play);
        return;
    }
    // reads Link's swings: sometimes raises the scabbard guard when he attacks from close by
    if ((dist < 150.0f) && (player->meleeWeaponState != 0) && (this->lastAttack != RONIN_GUARD) &&
        (Rand_ZeroOne() < 0.1f)) {
        this->lastAttack = RONIN_GUARD;
        Ronin_SetupGuard(this);
        return;
    }
    if (DECR(this->timer) == 0) {
        Ronin_ChooseAttack(this, play);
    }
}

static void Ronin_Flash(HtRonin* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((this->timer <= FLASH_TELL) && ((this->timer % 2) == 0)) {
        // the tell: mist gathering where he'll step out
        Vec3f up = this->flashTarget;

        up.y += 40.0f;
        Ronin_IceSmoke(play, &this->flashTarget, 170);
        Ronin_Sparkle(play, &up, 40.0f);
    }
    if (DECR(this->timer) == 0) {
        s32 i;

        this->actor.world.pos.x = this->actor.prevPos.x = this->flashTarget.x;
        this->actor.world.pos.z = this->actor.prevPos.z = this->flashTarget.z;
        this->actor.shape.rot.y = this->actor.world.rot.y =
            Math_Vec3f_Yaw(&this->actor.world.pos, &player->actor.world.pos);
        this->hidden = false;
        this->actor.shape.shadowAlpha = 255;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        for (i = 0; i < 6; i++) {
            Ronin_IceSmoke(play, &this->actor.world.pos, 200);
        }
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_STAL_WARAU);
        this->cuts = 1;
        Ronin_SetupDraw(this);
        this->timer = 6; // (a breath to react: the mist already gave him away)
    }
}

static void Ronin_Cut(HtRonin* this, PlayState* play, s32 animDone) {
    f32 last = Animation_GetLastFrame(this->skelAnime.animation);

    Math_StepToF(&this->actor.speedXZ, 0.0f, 1.0f);
    this->swordActive = (this->skelAnime.curFrame <= last * 0.7f);

    if (this->cutCollider.base.atFlags & AT_BOUNCED) {
        // taken on Link's shield: the blade glances off and he staggers
        this->cutCollider.base.atFlags &= ~AT_BOUNCED;
        Ronin_SetupRebound(this);
        return;
    }
    if (animDone) {
        if (--this->cuts > 0) {
            Ronin_SetupCut(this, 0x10);
        } else {
            Ronin_SetupSheathe(this);
        }
    }
}

static void Ronin_Charge(HtRonin* this, PlayState* play) {
    if (this->timer > 6) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 2, 0x1800, 0x100);
    }
    Ronin_Sparkle(play, &this->handPos, 30.0f); // frost gathering on the scabbard
    if ((this->timer % 4) == 0) {
        Ronin_IceSmoke(play, &this->actor.world.pos, 120);
    }
    if (DECR(this->timer) == 0) {
        Ronin_SetupLunge(this);
    }
}

static void Ronin_Lunge(HtRonin* this, PlayState* play, s32 animDone) {
    if (this->cutCollider.base.atFlags & AT_BOUNCED) {
        this->cutCollider.base.atFlags &= ~AT_BOUNCED;
        this->swordActive = false;
    }
    if (this->timer > 0) {
        this->timer--;
    } else if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        // landed: a burst of frost, then he is open for a moment
        s32 i;

        this->airborne = false;
        this->actor.speedXZ = 0.0f;
        for (i = 0; i < 6; i++) {
            Ronin_IceSmoke(play, &this->actor.world.pos, 200);
        }
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_DODO_M_GND);
        this->action = RONIN_LUNGE_RECOVER;
        this->timer = this->phase ? 18 : 28;
        this->swordActive = false;
        Ronin_Anim(this, (AnimationHeader*)gStalfosRecoverFromDownSlashAnim, 0.8f, ANIMMODE_ONCE, -3.0f);
    }
}

static void Ronin_Icicle(HtRonin* this, PlayState* play, s32 animDone) {
    f32 strike = Animation_GetLastFrame(this->skelAnime.animation) * 0.45f;

    if ((this->timer == 0) && (this->skelAnime.curFrame >= strike)) {
        this->timer = 1; // the blade bites the floor: the line starts
        Ronin_StartLines(this, play);
    }
    if (animDone) {
        Ronin_SetupSheathe(this);
    }
}

static void Ronin_Guard(HtRonin* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x0C00, 0x100);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (DECR(this->timer) == 0) {
        Ronin_SetupStalk(this, 10);
    }
}

static void Ronin_Phase(HtRonin* this, PlayState* play) {
    if (this->timer == 20) {
        Vec3f pos = this->actor.world.pos;
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        s32 i;

        // frost bursts off him: from here on he is faster, cuts twice and casts three icicle lines
        this->phase = 1;
        EffectSsBlast_SpawnWhiteShockwave(play, &pos, &zero, &zero);
        for (i = 0; i < 12; i++) {
            Ronin_IceSmoke(play, &pos, 220);
        }
        Actor_SetColorFilter(&this->actor, 0, 200, 0, 20);
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_FREEZE);
    }
    Ronin_Sparkle(play, &this->handPos, 40.0f);
    if (DECR(this->timer) == 0) {
        Ronin_SetupStalk(this, 5);
    }
}

static void Ronin_Shatter(HtRonin* this, PlayState* play) {
    s32 i;

    // (burnt away: nothing of him left; his grave post rises at the back of the arena - HtEpitaph)
    (void)i;
    // the katana outlasts its master: it drops to the floor where he stood - the arena's own pickup (waiting,
    // unseen, for this flag) is moved there; it stays in the arena until taken, also if Link leaves and comes back
    Flags_SetSwitch(play, HT_RONIN_DEAD_FLAG);
    {
        Actor* it = play->actorCtx.actorLists[ACTORCAT_MISC].head;

        for (; it != NULL; it = it->next) {
            if ((it->id == HT_ACTOR_KATANA_PICKUP) && (it->params == 1)) {
                break;
            }
        }
        if (it != NULL) {
            it->world.pos.x = it->home.pos.x = this->actor.world.pos.x;
            it->world.pos.y = it->home.pos.y = this->actor.floorHeight;
            it->world.pos.z = it->home.pos.z = this->actor.world.pos.z;
            it->shape.rot.y = this->actor.shape.rot.y;
        } else {
            Actor_Spawn(&play->actorCtx, play, HT_ACTOR_KATANA_PICKUP, this->actor.world.pos.x,
                        this->actor.floorHeight, this->actor.world.pos.z, 0, this->actor.shape.rot.y, 0, 0);
        }
    }
    Actor_Kill(&this->actor);
}

// ---------------------------------------------------------------------------------------------------------------

void HtRonin_Init(Actor* thisx, PlayState* play) {
    HtRonin* this = (HtRonin*)thisx;
    EffectBlureInit1 blure;
    s32 i;

    if (Flags_GetTreasure(play, HT_KATANA_TREASURE_FLAG) || Flags_GetSwitch(play, HT_RONIN_DEAD_FLAG) ||
        Flags_GetClear(play, this->actor.room)) {
        // already beaten: only the katana remains, if Link left it behind - the arena's own pickup (placed with the
        // room, HtKatanaPickup params 1) shows itself now that the Ronin's flag is set
        Flags_SetSwitch(play, HT_RONIN_DEAD_FLAG);
        this->action = RONIN_WAIT; // so Destroy leaves the music alone
        this->bodyPartCount = -1;  // and knows nothing was set up
        Actor_Kill(&this->actor);
        return;
    }

    Actor_SetScale(&this->actor, RONIN_SCALE);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 55.0f);
    this->actor.targetMode = 3;
    this->actor.naviEnemyId = HT_RONIN_NAVI_TEXT - 0x600; // Navi's C-up (Katana.cpp)
    this->actor.gravity = -1.0f;

    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)gStalfosSkel, (AnimationHeader*)gStalfosStandAttentionAnim,
                   this->jointTable, this->morphTable, HT_RONIN_LIMBS);

    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyInit);
    Collider_InitQuad(play, &this->swordCollider);
    Collider_SetQuad(play, &this->swordCollider, &this->actor, &sSwordInit);
    Collider_InitCylinder(play, &this->cutCollider);
    Collider_SetCylinder(play, &this->cutCollider, &this->actor, &sCutInit);
    for (i = 0; i < HT_RONIN_ICICLES; i++) {
        Collider_InitCylinder(play, &this->icicleColliders[i]);
        Collider_SetCylinder(play, &this->icicleColliders[i], &this->actor, &sIcicleInit);
    }
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);

    memset(&blure, 0, sizeof(blure));
    blure.p1StartColor[0] = 210;
    blure.p1StartColor[1] = 240;
    blure.p1StartColor[2] = 255;
    blure.p1StartColor[3] = 255;
    blure.p2StartColor[0] = 120;
    blure.p2StartColor[1] = 190;
    blure.p2StartColor[2] = 255;
    blure.p2StartColor[3] = 64;
    blure.p1EndColor[0] = 120;
    blure.p1EndColor[1] = 190;
    blure.p1EndColor[2] = 255;
    blure.p2EndColor[0] = 120;
    blure.p2EndColor[1] = 190;
    blure.p2EndColor[2] = 255;
    blure.elemDuration = 4;
    blure.unkFlag = 0;
    blure.calcMode = 2;
    Effect_Add(play, &this->effectIndex, EFFECT_BLURE1, 0, 0, &blure);

    this->strafeDir = 1;
    this->lastAttack = -1;
    this->action = RONIN_WAIT;
    this->bladeOut = true;
}

void HtRonin_Destroy(Actor* thisx, PlayState* play) {
    HtRonin* this = (HtRonin*)thisx;
    s32 i;

    if (this->bodyPartCount < 0) {
        return; // killed in Init (already beaten): no colliders or trail to free
    }
    if (this->action != RONIN_WAIT) {
        func_800F5B58(); // back to the dungeon music
    }
    Effect_Delete(play, this->effectIndex);
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->swordCollider);
    Collider_DestroyCylinder(play, &this->cutCollider);
    for (i = 0; i < HT_RONIN_ICICLES; i++) {
        Collider_DestroyCylinder(play, &this->icicleColliders[i]);
    }
}

void HtRonin_Update(Actor* thisx, PlayState* play) {
    HtRonin* this = (HtRonin*)thisx;
    s32 animDone;

    if (this->hurtTimer > 0) {
        this->hurtTimer--;
    }
    Ronin_CheckHits(this, play);
    animDone = SkelAnime_Update(&this->skelAnime);

    switch (this->action) {
        case RONIN_WAIT:
            if ((this->actor.xzDistToPlayer < RONIN_WAKE_DIST) && (fabsf(this->actor.yDistToPlayer) < 100.0f)) {
                Ronin_SetupRise(this);
            } else if ((play->gameplayFrames % 8) == 0) {
                Ronin_Sparkle(play, &this->handPos, 40.0f);
            }
            break;
        case RONIN_RISE:
            if (animDone) {
                Ronin_SetupStalk(this, 15);
            }
            break;
        case RONIN_STALK:
            Ronin_Stalk(this, play);
            break;
        case RONIN_DRAW_WINDUP:
            Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 2, 0x2000, 0x100);
            if (DECR(this->timer) == 0) {
                Ronin_SetupCut(this, 0x10);
            }
            break;
        case RONIN_DRAW_CUT:
            Ronin_Cut(this, play, animDone);
            break;
        case RONIN_SHEATHE:
            if (DECR(this->timer) == 0) {
                Ronin_Sheathe(this);
                Ronin_SetupStalk(this, 10);
            }
            break;
        case RONIN_CHARGE:
            Ronin_Charge(this, play);
            break;
        case RONIN_LUNGE:
            Ronin_Lunge(this, play, animDone);
            break;
        case RONIN_LUNGE_RECOVER:
            if ((DECR(this->timer) == 0) && animDone) {
                Ronin_Sheathe(this);
                Ronin_SetupStalk(this, 10);
            }
            break;
        case RONIN_ICICLE:
            Ronin_Icicle(this, play, animDone);
            break;
        case RONIN_GUARD:
            Ronin_Guard(this, play);
            break;
        case RONIN_REBOUND:
            Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
            if (animDone) {
                Ronin_Sheathe(this);
                Ronin_SetupStalk(this, 10);
            }
            break;
        case RONIN_HURT:
            Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
            if (animDone) {
                if (Rand_ZeroOne() < 0.5f) {
                    Ronin_SetupBackjump(this);
                } else {
                    Ronin_SetupStalk(this, 5);
                }
            }
            break;
        case RONIN_BACKJUMP:
            Math_StepToF(&this->actor.speedXZ, 0.0f, 0.8f);
            if (animDone) {
                Ronin_SetupStalk(this, 8);
            }
            break;
        case RONIN_STUN:
            if (DECR(this->timer) == 0) {
                Ronin_SetupStalk(this, 5);
            }
            break;
        case RONIN_PHASE:
            Ronin_Phase(this, play);
            break;
        case RONIN_FLASH:
            Ronin_Flash(this, play);
            break;
        case RONIN_DYING:
            if (this->timer > RONIN_BURN) {
                Ronin_Sparkle(play, &this->actor.focus.pos, 60.0f); // falling
            } else {
                // fallen: the old bones burn away in ethereal blue fire, the body gone into it at the end
                static Vec3f sZero = { 0.0f, 0.0f, 0.0f };
                Vec3f up = { 0.0f, 1.2f, 0.0f };
                s32 n = (this->timer > 15) ? 2 : 3;

                if (this->timer == RONIN_BURN) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_IGNITION);
                }
                for (s32 i = 0; i < n; i++) {
                    Vec3f pos = (this->bodyPartCount > 0) ? this->bodyPartsPos[(s32)Rand_ZeroFloat(this->bodyPartCount) %
                                                                             this->bodyPartCount]
                                                          : this->actor.world.pos;

                    pos.x += Rand_CenteredFloat(14.0f);
                    pos.y += Rand_CenteredFloat(10.0f);
                    pos.z += Rand_CenteredFloat(14.0f);
                    EffectSsDeadDb_Spawn(play, &pos, &up, &sZero, (this->timer > 15) ? 110 : 150, -3, 190, 235, 255,
                                         255, 40, 110, 255, 1, 12, false);
                }
                Actor_PlaySfx_Flagged(&this->actor, NA_SE_EN_FANTOM_FIRE - SFX_FLAG);
                if (this->timer == 15) {
                    this->hidden = true; // (consumed: only the fire's last flicker left)
                    this->actor.shape.shadowAlpha = 0;
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_PO_DISAPPEAR);
                }
            }
            if (DECR(this->timer) == 0) {
                Ronin_Shatter(this, play);
                return;
            }
            break;
    }

    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 40.0f, 30.0f, 60.0f, 0x1D);
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 85.0f;

    Ronin_UpdateIcicles(this, play);

    // swords glance off the raised scabbard; everything else is sorted out in Ronin_CheckHits
    if (this->action == RONIN_GUARD) {
        this->bodyCollider.base.acFlags |= AC_HARD;
        this->bodyCollider.base.colType = COLTYPE_METAL;
    } else {
        this->bodyCollider.base.acFlags &= ~AC_HARD;
        this->bodyCollider.base.colType = COLTYPE_HIT0;
    }
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    if ((this->action != RONIN_DYING) && (this->action != RONIN_PHASE) && (this->action != RONIN_RISE) &&
        !this->hidden && ((this->hurtTimer == 0) || (this->action == RONIN_GUARD))) {
        // invulnerable frames: attacks pass straight through rather than clanging off for no damage
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if ((this->action != RONIN_DYING) && !this->hidden) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if (this->swordActive && this->bladeOut) {
        // the cut: ahead of him, through the swing's cutting frames (the lunge: all the way, in the air and landing)
        f32 last = Animation_GetLastFrame(this->skelAnime.animation);
        s32 cutting = (this->action == RONIN_LUNGE) ||
                      ((this->skelAnime.curFrame >= last * CUT_FROM) && (this->skelAnime.curFrame <= last * CUT_TO));

        if (cutting) {
            this->cutCollider.dim.pos.x = this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y) * CUT_REACH;
            this->cutCollider.dim.pos.y = this->actor.world.pos.y;
            this->cutCollider.dim.pos.z = this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y) * CUT_REACH;
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->cutCollider.base);
        }
    }
}

// --- drawing ---------------------------------------------------------------------------------------------------

// --- the two-handed grip ---------------------------------------------------------------------------------------
// The Stalfos animations swing the sword in the right hand with the left arm held out (where the shield was). To hold
// the katana in both hands, each frame: run the animation's right arm (the game's own joint transforms) to find where
// its sword hand would be and which way the blade would point; bring the hands to the middle of his body and out in
// front of his ribs (GRIP_LATERAL, GRIP_FORWARD_MIN: otherwise the left arm reaches through his chest), put the
// katana there along the same line, and bend both arms (two-bone IK) so the wrists meet the hilt - right hand at the
// guard, left hand below it. Worked out with the Stalfos skeleton and these animations in an offline preview.
#define GRIP_LATERAL -0.2f       // the sword hand's sideways offset from the chest, scaled (a little to his left)
#define GRIP_FORWARD_MIN 2600.0f // the grip is at least this far in front of the chest (model units)
#define GRIP_SPACING 750.0f   // model units between the wrists along the hilt (a hand is ~700 long)
#define GRIP_RIGHT_BACK -40.0f // the right wrist sits this far behind the katana's origin (model units)
#define ARM_UPPER 1231.0f     // Stalfos upper arm / forearm lengths (model units)
#define ARM_FORE 965.0f

static Vec3f Ronin_V(f32 x, f32 y, f32 z) {
    Vec3f v = { x, y, z };
    return v;
}
static Vec3f Ronin_Sub(Vec3f a, Vec3f b) {
    return Ronin_V(a.x - b.x, a.y - b.y, a.z - b.z);
}
static Vec3f Ronin_Add(Vec3f a, Vec3f b) {
    return Ronin_V(a.x + b.x, a.y + b.y, a.z + b.z);
}
static Vec3f Ronin_Scale(Vec3f a, f32 k) {
    return Ronin_V(a.x * k, a.y * k, a.z * k);
}
static f32 Ronin_Dot(Vec3f a, Vec3f b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static Vec3f Ronin_Cross(Vec3f a, Vec3f b) {
    return Ronin_V(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static Vec3f Ronin_Unit(Vec3f a) {
    f32 n = sqrtf(Ronin_Dot(a, a));
    return (n > 1e-6f) ? Ronin_Scale(a, 1.0f / n) : a;
}
static Vec3f Ronin_Origin(MtxF* m) {
    return Ronin_V(m->xw, m->yw, m->zw);
}
// world matrix: origin o, axes x / y / z (unit), uniform scale s
static void Ronin_Frame(MtxF* m, Vec3f o, Vec3f x, Vec3f y, Vec3f z, f32 s) {
    m->xx = x.x * s; m->yx = x.y * s; m->zx = x.z * s; m->wx = 0.0f;
    m->xy = y.x * s; m->yy = y.y * s; m->zy = y.z * s; m->wy = 0.0f;
    m->xz = z.x * s; m->yz = z.y * s; m->zz = z.z * s; m->wz = 0.0f;
    m->xw = o.x;     m->yw = o.y;     m->zw = o.z;     m->ww = 1.0f;
}
// the game's own joint transform of limb `index` (1-based, as STALFOS_LIMB_*), applied to the current matrix
static void Ronin_Joint(HtRonin* this, s32 index) {
    StandardLimb* limb = SEGMENTED_TO_VIRTUAL(((void**)this->skelAnime.skeleton)[index - 1]);
    Vec3f pos = { limb->jointPos.x, limb->jointPos.y, limb->jointPos.z };

    Matrix_TranslateRotateZYX(&pos, &this->skelAnime.jointTable[index]);
}

// One arm: shoulder at the origin of `rootMtx` (the arm root; the upper-arm root sits on it), wrist to `hand`.
static void Ronin_SolveArm(MtxF* rootMtx, Vec3f hand, Vec3f pole, f32 s, MtxF* upperOut, MtxF* foreOut) {
    Vec3f sh = Ronin_Origin(rootMtx);
    Vec3f d = Ronin_Sub(hand, sh);
    f32 a = ARM_UPPER * s, b = ARM_FORE * s;
    f32 len = sqrtf(Ronin_Dot(d, d));
    Vec3f u = Ronin_Unit(d), v, elbow, x1, z1, y1, x2, z2, y2;
    f32 c;

    len = CLAMP(len, 1.0f, a + b - 0.01f);
    c = CLAMP((a * a + len * len - b * b) / (2.0f * a * len), -1.0f, 1.0f);
    v = Ronin_Unit(Ronin_Sub(pole, Ronin_Scale(u, Ronin_Dot(pole, u))));
    elbow = Ronin_Add(sh, Ronin_Scale(Ronin_Add(Ronin_Scale(u, c), Ronin_Scale(v, sqrtf(1.0f - c * c))), a));
    x1 = Ronin_Unit(Ronin_Sub(elbow, sh));
    z1 = Ronin_Unit(Ronin_Cross(x1, pole));
    y1 = Ronin_Cross(z1, x1);
    Ronin_Frame(upperOut, sh, x1, y1, z1, s);
    x2 = Ronin_Unit(Ronin_Sub(hand, elbow));
    z2 = Ronin_Unit(Ronin_Cross(x2, pole));
    y2 = Ronin_Cross(z2, x2);
    Ronin_Frame(foreOut, elbow, x2, y2, z2, s);
}

// Called with the matrix of the arms' parent (the upper core) on top of the stack.
static void HtRonin_SolveGrip(HtRonin* this) {
    MtxF parent, swordM, leftRoot, rightRoot;
    Vec3f chest, swordO, bladeDir, edge, grip, rightHand, leftHand, side, zAxis;
    f32 s = this->actor.scale.x;
    f32 yaw = this->actor.shape.rot.y * (M_PI / 0x8000);
    Vec3f right = Ronin_V(-cosf(yaw), 0.0f, sinf(yaw));
    Vec3f down = Ronin_V(0.0f, -1.0f, 0.0f);

    Matrix_Get(&parent);
    chest = Ronin_Origin(&parent);
    // the animation's right arm, down to the sword
    Matrix_Push();
    Ronin_Joint(this, STALFOS_LIMB_ARM_R_ROOT);
    Matrix_Get(&rightRoot);
    Ronin_Joint(this, STALFOS_LIMB_UPPERARM_R_ROOT);
    Ronin_Joint(this, STALFOS_LIMB_FOREARM_R_ROOT);
    Ronin_Joint(this, STALFOS_LIMB_HAND_R_ROOT);
    Ronin_Joint(this, STALFOS_LIMB_SWORD);
    Matrix_Get(&swordM);
    Matrix_Put(&parent);
    Ronin_Joint(this, STALFOS_LIMB_ARM_L_ROOT);
    Matrix_Get(&leftRoot);
    Matrix_Pop();

    swordO = Ronin_Origin(&swordM);
    bladeDir = Ronin_Unit(Ronin_V(swordM.xx, swordM.yx, swordM.zx));
    edge = Ronin_Unit(Ronin_V(swordM.xy, swordM.yy, swordM.zy));
    {
        Vec3f fwd = Ronin_V(sinf(yaw), 0.0f, cosf(yaw));
        Vec3f v = Ronin_Sub(swordO, chest);
        f32 vr = Ronin_Dot(v, right) * GRIP_LATERAL;
        f32 vf = MAX(Ronin_Dot(v, fwd), GRIP_FORWARD_MIN * s);

        grip = Ronin_Add(chest, Ronin_Add(Ronin_Add(Ronin_Scale(right, vr), Ronin_Scale(fwd, vf)),
                                          Ronin_V(0.0f, v.y, 0.0f)));
    }
    rightHand = Ronin_Sub(grip, Ronin_Scale(bladeDir, GRIP_RIGHT_BACK * s));
    leftHand = Ronin_Sub(grip, Ronin_Scale(bladeDir, GRIP_SPACING * s));

    side = Ronin_Scale(right, -1.0f); // his left
    // elbows down, out and forward: the upper arms stay outside the ribcage
    Ronin_SolveArm(&leftRoot, leftHand,
                   Ronin_Unit(Ronin_Add(Ronin_Add(Ronin_Scale(down, 0.4f), Ronin_Scale(side, 0.8f)),
                                        Ronin_V(sinf(yaw) * 0.6f, 0.0f, cosf(yaw) * 0.6f))), s,
                   &this->armMtx[0], &this->armMtx[1]);
    Ronin_SolveArm(&rightRoot, rightHand,
                   Ronin_Unit(Ronin_Add(Ronin_Add(Ronin_Scale(down, 0.4f), Ronin_Scale(right, 0.8f)),
                                        Ronin_V(sinf(yaw) * 0.6f, 0.0f, cosf(yaw) * 0.6f))), s,
                   &this->armMtx[2], &this->armMtx[3]);

    // the katana: at the grip, along the animation's blade line (model drawn at KATANA_IN_HAND_SCALE)
    zAxis = Ronin_Unit(Ronin_Cross(bladeDir, edge));
    edge = Ronin_Cross(zAxis, bladeDir);
    Ronin_Frame(&this->katanaMtx, grip, bladeDir, edge, zAxis, s * KATANA_IN_HAND_SCALE);
    this->gripOn = true;
}

static s32 HtRonin_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                    void* thisx) {
    HtRonin* this = (HtRonin*)thisx;

    if (limbIndex == STALFOS_LIMB_ARM_L_ROOT) {
        // both hands on the hilt - except in the guard, where the katana passes to the left hand alone
        this->gripOn = false;
        if (this->action != RONIN_GUARD) {
            HtRonin_SolveGrip(this);
        }
    } else if (this->gripOn && ((limbIndex == STALFOS_LIMB_UPPERARM_L_ROOT) || (limbIndex == STALFOS_LIMB_FOREARM_L_ROOT) ||
                                (limbIndex == STALFOS_LIMB_UPPERARM_R_ROOT) || (limbIndex == STALFOS_LIMB_FOREARM_R_ROOT))) {
        s32 k = (limbIndex == STALFOS_LIMB_UPPERARM_L_ROOT) ? 0
                : (limbIndex == STALFOS_LIMB_FOREARM_L_ROOT) ? 1
                : (limbIndex == STALFOS_LIMB_UPPERARM_R_ROOT) ? 2 : 3;

        // the solved world matrix replaces the joint: zero joint transform on top of it
        Matrix_Put(&this->armMtx[k]);
        pos->x = pos->y = pos->z = 0.0f;
        rot->x = rot->y = rot->z = 0;
    }
    if ((limbIndex == STALFOS_LIMB_SHIELD) || (limbIndex == STALFOS_LIMB_SWORD)) {
        *dList = NULL; // no shield; the katana and its scabbard are drawn in PostLimbDraw
    } else if (limbIndex == STALFOS_LIMB_HEAD_ROOT) {
        Matrix_Scale(0.85f, 0.85f, 0.85f, MTXMODE_APPLY); // a small skull on a tall frame
    }
    return false;
}

static void HtRonin_DrawDL(PlayState* play, const char* dl) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void HtRonin_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    HtRonin* this = (HtRonin*)thisx;

    if (limbIndex == STALFOS_LIMB_SWORD) {
        Matrix_Push();
        if (this->gripOn) {
            Matrix_Put(&this->katanaMtx); // held in both hands (HtRonin_SolveGrip)
        } else {
            Matrix_Scale(KATANA_IN_HAND_SCALE, KATANA_IN_HAND_SCALE, KATANA_IN_HAND_SCALE, MTXMODE_APPLY);
        }
        Matrix_MultVec3f(&sBladeBase, &this->bladeBase);
        Matrix_MultVec3f(&sBladeTip, &this->bladeTip);
        // the hitbox: the band the blade swept since last frame (tip and base then, tip and base now), as Link's
        // sword trail does it; while the blade isn't cutting, it only follows
        if (this->swordActive && this->bladeOut && this->sweepValid) {
            Collider_SetQuadVertices(&this->swordCollider, &this->prevBladeTip, &this->bladeTip, &this->prevBladeBase,
                                     &this->bladeBase);
        } else {
            Collider_SetQuadVertices(&this->swordCollider, &this->bladeTip, &this->bladeTip, &this->bladeBase,
                                     &this->bladeBase);
        }
        this->sweepValid = this->swordActive && this->bladeOut;
        this->prevBladeTip = this->bladeTip;
        this->prevBladeBase = this->bladeBase;

        if (this->action != RONIN_GUARD) { // guarding, it moves to the left hand (see HtRonin_Draw)
            HtRonin_DrawDL(play, KATANA_HELD_DL);
        }
        if (this->swordActive && this->bladeOut) {
            EffectBlure_AddVertex(Effect_GetByIndex(this->effectIndex), &this->bladeBase, &this->bladeTip);
            this->trailOn = true;
        } else if (this->trailOn) {
            EffectBlure_AddSpace(Effect_GetByIndex(this->effectIndex));
            this->trailOn = false;
        }
        Matrix_Pop();
    } else if (limbIndex == STALFOS_LIMB_HEAD) {
        HtRonin_DrawDL(play, HT_RONIN_KASA_DL); // his straw kasa, in the skull's space
    } else if (limbIndex == STALFOS_LIMB_HAND_L) {
        Matrix_MultVec3f(&sZeroVec, &this->leftHandPos);
    } else if (limbIndex == STALFOS_LIMB_WAIST) {
        Matrix_MultVec3f(&sZeroVec, &this->waistPos);
    }

    switch (limbIndex) {
        case STALFOS_LIMB_HEAD:
        case STALFOS_LIMB_CHEST:
        case STALFOS_LIMB_UPPERARM_R:
        case STALFOS_LIMB_UPPERARM_L:
        case STALFOS_LIMB_FOREARM_R:
        case STALFOS_LIMB_FOREARM_L:
        case STALFOS_LIMB_WAIST:
        case STALFOS_LIMB_THIGH_L:
        case STALFOS_LIMB_THIGH_R:
        case STALFOS_LIMB_FOOT_L:
            if (limbIndex == STALFOS_LIMB_HEAD) {
                this->bodyPartCount = 0;
            }
            if (this->bodyPartCount < HT_RONIN_BODYPARTS) {
                Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartCount++]);
            }
            break;
    }
}

void HtRonin_Draw(Actor* thisx, PlayState* play) {
    HtRonin* this = (HtRonin*)thisx;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // frost on old bones: a cold blue cast over the whole skeleton
    if ((this->action == RONIN_DYING) && (this->timer <= RONIN_BURN)) {
        // burning: a cold blue glow taking over the bones
        s32 near = 990 - (RONIN_BURN - this->timer) * 20;

        POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP, 120, 190, 255, 0, CLAMP(near, 0, 990), 1000);
    } else if ((this->hurtTimer != 0) && (this->action != RONIN_DYING) && ((this->hurtTimer & 4) != 0)) {
        // hit: he flickers, and flashes red (as Armorpha does)
        POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP, 255, 20, 10, 0, 0, 1400);
    } else if (this->actor.colorFilterTimer == 0) {
        POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP, 110, 170, 230, 0, 0, 3200);
    }
    if (!this->hidden && ((this->hurtTimer == 0) || (this->action == RONIN_DYING) || ((this->hurtTimer & 2) == 0))) {
        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, HtRonin_OverrideLimbDraw, HtRonin_PostLimbDraw, this);
    }
    // guarding, the katana passes to the left hand and is held level across his front: placed from the hand's
    // position in the actor's own frame (the Stalfos limb axes are unknown)
    if ((this->action == RONIN_GUARD) &&
        ((this->hurtTimer == 0) || ((this->hurtTimer & 2) == 0))) {
        f32 yaw = this->actor.shape.rot.y * (M_PI / 0x8000);
        f32 fx = sinf(yaw), fz = cosf(yaw);       // forward
        f32 rx = -fz, rz = fx;                    // his right
        Vec3f dir;
        f32 h, heading, pitch;

        if (this->action == RONIN_GUARD) {
            dir.x = rx * 0.95f + fx * 0.3f;
            dir.y = 0.0f;
            dir.z = rz * 0.95f + fz * 0.3f;
        } else {
            dir.x = fx * 0.75f - rx * 0.25f;
            dir.y = -0.6f;
            dir.z = fz * 0.75f - rz * 0.25f;
        }
        h = sqrtf(SQ(dir.x) + SQ(dir.z));
        heading = atan2f(-dir.z, dir.x);           // RotateY(a) takes the model's +x to (cos a, 0, -sin a)
        pitch = atan2f(dir.y, h);
        Matrix_Translate(this->leftHandPos.x, this->leftHandPos.y, this->leftHandPos.z, MTXMODE_NEW);
        Matrix_RotateY(heading, MTXMODE_APPLY);
        Matrix_RotateZ(pitch, MTXMODE_APPLY);
        Matrix_Scale(RONIN_SCALE * KATANA_IN_HAND_SCALE, RONIN_SCALE * KATANA_IN_HAND_SCALE,
                     RONIN_SCALE * KATANA_IN_HAND_SCALE, MTXMODE_APPLY);
        HtRonin_DrawDL(play, KATANA_HELD_DL);
    }
    // the scabbard on the left hip (and the katana in it, when it isn't drawn)
    if (!this->hidden && ((this->hurtTimer == 0) || (this->action == RONIN_DYING) || ((this->hurtTimer & 2) == 0))) {
        static Vec3f sSayaMid = { 3000.0f, 0.0f, 0.0f };

        Matrix_Translate(this->waistPos.x, this->waistPos.y, this->waistPos.z, MTXMODE_NEW);
        Matrix_RotateY(this->actor.shape.rot.y * (M_PI / 0x8000), MTXMODE_APPLY);
        Matrix_Translate(SAYA_SIDE, SAYA_DROP, SAYA_FWD, MTXMODE_APPLY);
        Matrix_RotateY(M_PI / 2.0f, MTXMODE_APPLY); // the model's +x (toward the point) now runs backward
        Matrix_RotateZ(SAYA_TILT, MTXMODE_APPLY);
        Matrix_Scale(SAYA_SCALE, SAYA_SCALE, SAYA_SCALE, MTXMODE_APPLY);
        Matrix_MultVec3f(&sSayaMid, &this->handPos); // the charge frost gathers on the scabbard
        HtRonin_DrawDL(play, SAYA_DL);
        if (!this->bladeOut) {
            HtRonin_DrawDL(play, KATANA_HELD_DL);
        }
    }
    if (this->actor.colorFilterTimer == 0) {
        func_80026608(play); // the scene's own fog back
    }

    // the icicles: burst up over ICICLE_RISE frames, sink a little before they shatter
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    for (i = 0; i < HT_RONIN_ICICLES; i++) {
        HtRoninIcicle* ice = &this->icicles[i];
        f32 s;

        if (ice->timer == 0) {
            continue;
        }
        s = (ice->timer < ICICLE_RISE) ? (f32)ice->timer / ICICLE_RISE : 1.0f;
        if (ice->timer > ICICLE_LIFE - 8) {
            s *= (f32)(ICICLE_LIFE - ice->timer + 1) / 9.0f;
        }
        Matrix_Translate(ice->pos.x, ice->pos.y, ice->pos.z, MTXMODE_NEW);
        Matrix_RotateY(ice->yaw * (M_PI / 0x8000), MTXMODE_APPLY);
        Matrix_Scale(0.8f + 0.2f * s, s, 0.8f + 0.2f * s, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)ICICLE_DL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
