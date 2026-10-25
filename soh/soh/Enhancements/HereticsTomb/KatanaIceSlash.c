// Heretics Tomb: the Ice Katana's slash projectile.
//
// A wave of frost that flies forward from the blade: level for a plain slash; for the Iai draw, slanted like the cut
// (low on Link's right, high on his left) and sent off at the moment of the cut. It carries the boomerang's
// damage type, so each enemy reacts with its own existing stun behaviour (most freeze in place
// and stay hittable), then gets an ice-blue tint and Kotake's freeze sound. A plain slash shows only its sparkle
// trail; the Iai's wave is its own thing: a chevron of crystal ice flying point first (ht_iai_wave), a frost halo round
// it, mist curling off its wing tips, and shards of ice where it breaks.
// It ends at the first enemy it hits, at a wall, or after a short range.
//
// Registered at runtime through ActorDB (see Katana.cpp), so no vanilla actor table changes.

#include "KatanaIceSlash.h"
#include "Katana.h"
#include "HtFrostShell.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include <math.h>

#define ICE_SLASH_SPEED 22.0f      // units per frame
#define ICE_SLASH_LIFETIME 10      // frames (about 220 units of range)
#define ICE_SLASH_HALF_WIDTH 30.0f // the wave is 60 units wide
#define ICE_SLASH_FREEZE_TINT 60   // frames of blue tint on a frozen enemy
#define ICE_SLASH_SPARKLES 7       // sparkles per frame across the wave
#define ICE_SLASH_SPARKLE_SCALE 2500 // vanilla small sparkle is 1000

// Iai draw wave: wider, longer range, and deals Biggoron spin damage on top of the freeze
#define IAI_SLASH_LIFETIME 16        // frames (about 350 units of range)
#define IAI_SLASH_HALF_WIDTH 55.0f   // the wave is 110 units wide
#define IAI_SLASH_DMG_FLAGS 0x00800000 // Giant (Biggoron) spin
#define IAI_SLASH_SPARKLES 11          // denser, bigger and whiter than a normal slash
#define IAI_SLASH_SPARKLE_SCALE 4000
#define IAI_SLASH_SLANT 0.8f           // the ends rise/fall this much per unit out from the middle
#define IAI_SLASH_HEIGHT 45.0f         // the middle, above Link's feet (a plain slash: 30)
#define IAI_WAVE_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_iai_wave_dl"
#define IAI_WAVE_GROW 1                // frames to grow to full size once sent off
#define IAI_WAVE_FADE 4                // frames fading out at the end of its flight

static ColliderQuadInit sIceSlashQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000010, 0x00, 0x00 }, // boomerang damage type: zero damage, enemy's own stun
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static Color_RGBA8 sFrostPrim = { 220, 245, 255, 255 };
static Color_RGBA8 sIaiPrim = { 255, 255, 255, 255 };
static Color_RGBA8 sFrostEnv = { 90, 170, 255, 255 };

static void KatanaIceSlash_GetEnds(KatanaIceSlash* this, Vec3f* left, Vec3f* right) {
    f32 px = Math_CosS(this->actor.world.rot.y) * this->halfWidth; // perpendicular to travel
    f32 pz = -Math_SinS(this->actor.world.rot.y) * this->halfWidth;

    *left = this->actor.world.pos;
    *right = this->actor.world.pos;
    left->x += px; // Link's left
    left->z += pz;
    right->x -= px;
    right->z -= pz;
    if (this->isIai) {
        left->y += this->halfWidth * IAI_SLASH_SLANT;
        right->y -= this->halfWidth * IAI_SLASH_SLANT;
    }
}

// the Iai's wave breaking: shards of ice and a puff of frost
static void KatanaIceSlash_Shatter(PlayState* play, KatanaIceSlash* this, Vec3f* at) {
    Vec3f vel = { 0.0f, 1.0f, 0.0f };
    Vec3f acc = { 0.0f, 0.1f, 0.0f };

    if (!this->isIai) {
        return;
    }
    for (s32 i = 0; i < 6; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, at, 150, 150, 150, 250, 235, 245, 255, 0.9f);
    }
    for (s32 i = 0; i < 3; i++) {
        EffectSsIceSmoke_Spawn(play, at, &vel, &acc, 150);
    }
}

static void KatanaIceSlash_SpawnFrost(PlayState* play, KatanaIceSlash* this) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f drift = { 0.0f, 0.4f, 0.0f };
    Vec3f left, right, pos;

    // Sparkles only: a mist trail here filled the bottom of the screen right behind Link.
    KatanaIceSlash_GetEnds(this, &left, &right);
    if (this->isIai) {
        // the Iai's wave: a few sparkles off its leading edges, and frost mist curling off its wing tips once it's
        // clear of Link (a mist right behind him fills the screen)
        Vec3f tip = this->actor.world.pos;
        Vec3f smokeVel = { 0.0f, 0.6f, 0.0f };
        Vec3f smokeAcc = { 0.0f, 0.05f, 0.0f };

        tip.x += Math_SinS(this->actor.world.rot.y) * 20.0f;
        tip.z += Math_CosS(this->actor.world.rot.y) * 20.0f;
        for (s32 i = 0; i < 4; i++) {
            f32 t = Rand_ZeroOne();
            Vec3f* end = (i % 2) ? &left : &right;

            pos.x = tip.x + (end->x - tip.x) * t;
            pos.y = tip.y + (end->y - tip.y) * t;
            pos.z = tip.z + (end->z - tip.z) * t;
            EffectSsKiraKira_SpawnDispersed(play, &pos, &zero, &drift, &sIaiPrim, &sFrostEnv, IAI_SLASH_SPARKLE_SCALE,
                                            12);
        }
        if (this->age >= 2) {
            EffectSsIceSmoke_Spawn(play, &left, &smokeVel, &smokeAcc, 70);
            EffectSsIceSmoke_Spawn(play, &right, &smokeVel, &smokeAcc, 70);
        }
        return;
    }
    s32 count = this->isIai ? IAI_SLASH_SPARKLES : ICE_SLASH_SPARKLES;
    s16 scale = this->isIai ? IAI_SLASH_SPARKLE_SCALE : ICE_SLASH_SPARKLE_SCALE;
    for (s32 i = 0; i <= count - 1; i++) {
        f32 t = i / (f32)(count - 1);
        pos.x = left.x + (right.x - left.x) * t;
        pos.y = left.y + (right.y - left.y) * t + Rand_CenteredFloat(6.0f);
        pos.z = left.z + (right.z - left.z) * t;
        EffectSsKiraKira_SpawnDispersed(play, &pos, &zero, &drift, this->isIai ? &sIaiPrim : &sFrostPrim, &sFrostEnv,
                                        scale, 16);
    }
}

void KatanaIceSlash_Init(Actor* thisx, PlayState* play) {
    KatanaIceSlash* this = (KatanaIceSlash*)thisx;

    this->isIai = (this->actor.params == 1);
    this->timer = this->isIai ? IAI_SLASH_LIFETIME : ICE_SLASH_LIFETIME;
    this->halfWidth = this->isIai ? IAI_SLASH_HALF_WIDTH : ICE_SLASH_HALF_WIDTH;
    Collider_InitQuad(play, &this->collider);
    Collider_SetQuad(play, &this->collider, &this->actor, &sIceSlashQuadInit);
    if (this->isIai) {
        this->collider.info.toucher.dmgFlags = IAI_SLASH_DMG_FLAGS;
        this->delay = KATANA_IAI_WAVE_DELAY;
        this->actor.world.pos.y += IAI_SLASH_HEIGHT - 30.0f;
    }
    KatanaIceSlash_GetEnds(this, &this->prevLeft, &this->prevRight);
}

void KatanaIceSlash_Destroy(Actor* thisx, PlayState* play) {
    KatanaIceSlash* this = (KatanaIceSlash*)thisx;

    Collider_DestroyQuad(play, &this->collider);
}

void KatanaIceSlash_Update(Actor* thisx, PlayState* play) {
    KatanaIceSlash* this = (KatanaIceSlash*)thisx;
    Vec3f from = this->actor.world.pos;
    Vec3f to, hitPos, left, right;
    CollisionPoly* poly;
    s32 bgId;

    // The Iai wave waits at the blade (following Link's step in) until the cut sends it off
    if (this->delay > 0) {
        Player* player = GET_PLAYER(play);

        this->delay--;
        this->actor.world.rot.y = player->actor.shape.rot.y;
        this->actor.world.pos.x = player->actor.world.pos.x + Math_SinS(this->actor.world.rot.y) * 45.0f;
        this->actor.world.pos.y = player->actor.world.pos.y + IAI_SLASH_HEIGHT;
        this->actor.world.pos.z = player->actor.world.pos.z + Math_CosS(this->actor.world.rot.y) * 45.0f;
        KatanaIceSlash_GetEnds(this, &this->prevLeft, &this->prevRight);
        return;
    }

    // Did last frame's sweep hit an enemy? Freeze it and end the wave there.
    if (this->collider.base.atFlags & AT_HIT) {
        Actor* target = this->collider.base.at;

        if ((target != NULL) && (target->category == ACTORCAT_ENEMY) && (target->id != HT_ACTOR_RONIN)) {
            // fire enemies lose their flames; everything hit gets frost over it while the stun lasts
            // (a Beamos's eye is frozen over by the Beamos itself: z_en_vm.c)
            if ((target->id != ACTOR_EN_VM) && !HtFrostShell_ChillEnemy(play, target)) {
                HtFrostShell_Apply(play, target, ICE_SLASH_FREEZE_TINT);
            }
            // continuous sound: a short burst only (played once, it would loop until the enemy is gone)
            HtSfx_PlayFor(target->world.pos.x, target->world.pos.y, target->world.pos.z, NA_SE_EN_TWINROBA_SHOOT_FREEZE, 20);
            if (this->isIai) {
                Audio_PlaySfxGeneral(NA_SE_IT_EXPLOSION_ICE, &target->projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        }
        KatanaIceSlash_Shatter(play, this, &this->actor.world.pos);
        Actor_Kill(&this->actor);
        return;
    }

    if (--this->timer <= 0) {
        Actor_Kill(&this->actor);
        return;
    }

    // Move forward; stop at walls.
    to = from;
    to.x += Math_SinS(this->actor.world.rot.y) * ICE_SLASH_SPEED;
    to.z += Math_CosS(this->actor.world.rot.y) * ICE_SLASH_SPEED;
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hitPos, &poly, true, false, false, true, &bgId)) {
        Actor* bg = (bgId != BGCHECK_SCENE) ? DynaPoly_GetActor(&play->colCtx, bgId) : NULL;

        // (the B2 boat's guardrails don't stop it: from the deck it must reach what rises beside the boat)
        if ((bg == NULL) || (bg->id != HT_ACTOR_BOAT)) {
            KatanaIceSlash_Shatter(play, this, &hitPos);
            Actor_Kill(&this->actor);
            return;
        }
    }
    this->actor.world.pos = to;
    this->age++;

    // Sweep the hurtbox between last frame's edge and this frame's edge, like a sword trail.
    KatanaIceSlash_GetEnds(this, &left, &right);
    Collider_SetQuadVertices(&this->collider, &left, &right, &this->prevLeft, &this->prevRight);
    this->prevLeft = left;
    this->prevRight = right;
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);

    KatanaIceSlash_SpawnFrost(play, this);
}

// the Iai's wave: the chevron of ice, slanted like the cut, with a fainter, larger frost halo round it; it grows in as
// it leaves the blade and fades at the end of its flight
void KatanaIceSlash_Draw(Actor* thisx, PlayState* play) {
    KatanaIceSlash* this = (KatanaIceSlash*)thisx;
    f32 grow, fade;

    if (!this->isIai || (this->delay > 0)) {
        return; // (nothing at the blade before the cut: it appears as the cut sends it off)
    }
    grow = CLAMP(0.8f + 0.2f * this->age / (f32)IAI_WAVE_GROW, 0.8f, 1.0f);
    fade = CLAMP(this->timer / (f32)IAI_WAVE_FADE, 0.0f, 1.0f);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (s32 pass = 0; pass < 2; pass++) {
        f32 k = grow * (pass ? 1.25f : 1.0f);

        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(this->actor.world.rot.y * (M_PI / 0x8000), MTXMODE_APPLY);
        Matrix_RotateZ(atanf(IAI_SLASH_SLANT), MTXMODE_APPLY); // (its left wing up, like the cut)
        Matrix_Scale(k, grow * (pass ? 2.0f : 1.0f), k, MTXMODE_APPLY);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (u8)((pass ? 70.0f : 215.0f) * fade));
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)IAI_WAVE_DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
