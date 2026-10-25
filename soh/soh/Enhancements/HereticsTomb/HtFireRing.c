// Heretics Tomb: the Lynel's fire cage (phase 2) - see HtFireRing.h.

#include "HtFireRing.h"
#include "KatanaIceSlash.h"
#include "Katana.h"
#include "functions.h"
#include <math.h>
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/frame_interpolation.h"

#define FIRE_TOUCH 24.0f          // how near the flames burn
#define FIRE_DMG 0x08             // half a heart (and he catches fire)
#define FIRE_DMG_GORON 0x04       // a quarter, in the Goron Tunic (the rings only)
#define FIRE_RISE 15              // frames for the flames to grow up out of the floor
#define FIRE_END_R 14.0f          // a ring this small has shut on its spot: gone
#define FIRE_SEG_ANG (2.0f * M_PI / HT_FIRE_SEGS)
#define FIRE_ALL 0xFFFFFFFFu

// steam (the frost meeting fire, the water boiling): a pale dust puff, rising
static void HtFireRing_Steam(PlayState* play, Vec3f* pos, Vec3f* vel, Vec3f* acc, s16 scale) {
    static Color_RGBA8 prim = { 240, 240, 240, 150 };
    static Color_RGBA8 env = { 190, 190, 200, 0 };

    EffectSsDust_Spawn(play, 0, pos, vel, acc, &prim, &env, scale / 3, 12, 26, 0);
}

// the segment of a ring (about its own centre) a point lies in
static s32 HtFireRing_SegAt(f32 dx, f32 dz) {
    f32 a = atan2f(dx, dz) + M_PI; // 0 .. 2 pi

    return CLAMP((s32)(a / FIRE_SEG_ANG), 0, HT_FIRE_SEGS - 1);
}

static void HtFireRing_SegPoint(HtFireRing* this, HtFireLayer* l, f32 seg, Vec3f* out) {
    f32 a = seg * FIRE_SEG_ANG - M_PI;

    out->x = l->cx + sinf(a) * l->r;
    out->y = this->actor.world.pos.y;
    out->z = l->cz + cosf(a) * l->r;
}

// a ring's place now: from the wall (s 0) to a point on its spot (s 1)
static void HtFireRing_Place(HtFireRing* this, HtFireLayer* l) {
    l->cx = this->actor.world.pos.x + (l->tx - this->actor.world.pos.x) * l->s;
    l->cz = this->actor.world.pos.z + (l->tz - this->actor.world.pos.z) * l->s;
    l->r = HT_FIRE_WALL_R * (1.0f - l->s);
}

void HtFireRing_Init(Actor* thisx, PlayState* play) {
    HtFireRing* this = (HtFireRing*)thisx;
    s32 i;

    for (i = 0; i < HT_FIRE_LAYERS; i++) {
        this->layers[i].on = false;
    }
    // the cage: up at once, out of the floor
    this->layers[0].on = true;
    this->layers[0].s = 0.0f;
    this->layers[0].tx = this->actor.world.pos.x;
    this->layers[0].tz = this->actor.world.pos.z;
    this->layers[0].alive = FIRE_ALL;
    HtFireRing_Place(this, &this->layers[0]);
    this->rise = 0;
    this->emitTimer = 40;
    this->emitInterval = 0;
    this->maxRings = 2;
    this->speed = 2.6f;
    this->gen = 1;
    this->cutCount = 0;
    this->burnCool = 0;
    this->dying = false;
    Actor_SetScale(&this->actor, 1.0f);
}

void HtFireRing_Destroy(Actor* thisx, PlayState* play) {
}

f32 HtFireRing_Openness(Actor* thisx) {
    HtFireRing* this = (HtFireRing*)thisx;
    s32 i, k, out = 0, total = 0;

    for (i = 1; i < HT_FIRE_LAYERS; i++) {
        if (!this->layers[i].on) {
            continue;
        }
        for (k = 0; k < HT_FIRE_SEGS; k++) {
            total++;
            if (!(this->layers[i].alive & (1u << k))) {
                out++;
            }
        }
    }
    return (total == 0) ? 1.0f : (f32)out / total;
}

s32 HtFireRing_IsBurning(Actor* thisx, f32 x, f32 z) {
    HtFireRing* this = (HtFireRing*)thisx;
    s32 i;

    for (i = 0; i < HT_FIRE_LAYERS; i++) {
        HtFireLayer* l = &this->layers[i];
        f32 dx, dz;

        if (!l->on) {
            continue;
        }
        dx = x - l->cx;
        dz = z - l->cz;
        if ((fabsf(sqrtf(SQ(dx) + SQ(dz)) - l->r) < FIRE_TOUCH + 10.0f) &&
            (l->alive & (1u << HtFireRing_SegAt(dx, dz)))) {
            return true;
        }
    }
    return false;
}

// a new ring, out of the cage wall: it closes in on where Link stands now
static void HtFireRing_Emit(HtFireRing* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 dx = player->actor.world.pos.x - this->actor.world.pos.x;
    f32 dz = player->actor.world.pos.z - this->actor.world.pos.z;
    f32 d = sqrtf(SQ(dx) + SQ(dz));
    s32 i, free = -1, standing = 0;
    HtFireLayer* l;

    for (i = 1; i < HT_FIRE_LAYERS; i++) {
        if (this->layers[i].on) {
            standing++;
        } else if (free < 0) {
            free = i;
        }
    }
    if ((free < 0) || (standing >= this->maxRings)) {
        return;
    }
    if (d > HT_FIRE_WALL_R - 40.0f) { // (its spot inside the cage)
        dx *= (HT_FIRE_WALL_R - 40.0f) / d;
        dz *= (HT_FIRE_WALL_R - 40.0f) / d;
    }
    l = &this->layers[free];
    l->on = true;
    l->s = 0.03f;
    l->tx = this->actor.world.pos.x + dx;
    l->tz = this->actor.world.pos.z + dz;
    l->alive = FIRE_ALL;
    l->age = 0;
    l->gen = ++this->gen;
    HtFireRing_Place(this, l);
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_IGNITION);
}

// the katana's frost: a slash flying through the rings puts out the segments it passes - a plain one stops at the
// first ring it cuts, the Iai goes through them all
static void HtFireRing_Extinguish(HtFireRing* this, PlayState* play) {
    Actor* a = play->actorCtx.actorLists[ACTORCAT_MISC].head;

    for (; a != NULL; a = a->next) {
        KatanaIceSlash* s;
        s32 i, k, spent = false;

        if ((a->id != HT_ACTOR_ICE_SLASH) || (a->update == NULL)) {
            continue;
        }
        s = (KatanaIceSlash*)a;
        if (fabsf(a->world.pos.y - this->actor.world.pos.y) > 140.0f) {
            continue;
        }
        for (i = 1; (i < HT_FIRE_LAYERS) && !spent; i++) {
            HtFireLayer* l = &this->layers[i];
            s32 cut = false;

            if (!l->on) {
                continue;
            }
            for (k = 0; k < HT_FIRE_SEGS; k++) {
                Vec3f c;

                if (!(l->alive & (1u << k))) {
                    continue;
                }
                HtFireRing_SegPoint(this, l, k + 0.5f, &c);
                if (SQ(c.x - a->world.pos.x) + SQ(c.z - a->world.pos.z) < SQ(s->halfWidth + 22.0f)) {
                    Vec3f vel = { 0.0f, 2.5f, 0.0f };
                    Vec3f acc = { 0.0f, 0.3f, 0.0f };

                    l->alive &= ~(1u << k);
                    this->cutCount++;
                    cut = true;
                    c.y += 10.0f;
                    HtFireRing_Steam(play, &c, &vel, &acc, 220);
                }
            }
            if (cut) {
                HtSfx_PlayFor(a->world.pos.x, a->world.pos.y, a->world.pos.z, NA_SE_EN_EXTINCT, 1);
                if (l->alive == 0) {
                    l->on = false;
                }
                if (!s->isIai) {
                    spent = true; // (a plain slash: its frost spent on this ring)
                }
            }
        }
        if (spent) {
            Actor_Kill(a);
        }
    }
}

// Link in the flames: burnt, and thrown - by a ring, out through it (behind it, where it has been); by the cage, back
// in
static void HtFireRing_Burn(HtFireRing* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 dx = player->actor.world.pos.x - this->actor.world.pos.x;
    f32 dz = player->actor.world.pos.z - this->actor.world.pos.z;
    f32 d = sqrtf(SQ(dx) + SQ(dz));
    s32 i;

    // the cage holds: Link never ends up outside it (thrown against it, he stops at its foot)
    if (!Player_InCsMode(play) && (d > HT_FIRE_WALL_R - 22.0f)) {
        f32 k = (HT_FIRE_WALL_R - 22.0f) / d;

        player->actor.world.pos.x = this->actor.world.pos.x + dx * k;
        player->actor.world.pos.z = this->actor.world.pos.z + dz * k;
    }
    if (this->burnCool > 0) {
        this->burnCool--;
        return;
    }
    if (Player_InCsMode(play) || (player->actor.world.pos.y > this->actor.world.pos.y + 90.0f) ||
        (this->rise < FIRE_RISE / 2)) {
        return;
    }
    for (i = 0; i < HT_FIRE_LAYERS; i++) {
        HtFireLayer* l = &this->layers[i];
        f32 lx, lz, ld;

        if (!l->on) {
            continue;
        }
        lx = player->actor.world.pos.x - l->cx;
        lz = player->actor.world.pos.z - l->cz;
        ld = sqrtf(SQ(lx) + SQ(lz));
        if ((fabsf(ld - l->r) < ((i == 0) ? FIRE_TOUCH + 4.0f : FIRE_TOUCH)) &&
            (l->alive & (1u << HtFireRing_SegAt(lx, lz)))) {
            s16 toCentre = Math_FAtan2F(-lx, -lz) * (0x8000 / M_PI);
            // (the Goron Tunic: the rings' flames only singe him - the cage's still burns)
            s32 goron = (i != 0) && (player->currentTunic == PLAYER_TUNIC_GORON);
            s32 b;

            Actor_SetPlayerKnockbackSmall(play, &this->actor, (i == 0) ? 7.0f : 9.0f,
                                          (i == 0) ? toCentre : (s16)(toCentre + 0x8000), 4.0f,
                                          goron ? FIRE_DMG_GORON : FIRE_DMG);
            if (!goron) {
                for (b = 0; b < PLAYER_BODYPART_MAX; b++) {
                    player->bodyFlameTimers[b] = Rand_S16Offset(0, 200);
                }
                player->bodyIsBurning = true;
            }
            Audio_PlayActorSound2(&player->actor, NA_SE_EV_FLAME_IGNITION);
            this->burnCool = 25;
            return;
        }
    }
}

void HtFireRing_Update(Actor* thisx, PlayState* play) {
    HtFireRing* this = (HtFireRing*)thisx;
    Player* player = GET_PLAYER(play);
    s32 i;

    this->scroll++;
    if (this->dying) {
        this->rise = MIN(this->rise, FIRE_RISE); // (going out over half a second, rings and all)
        if ((this->rise -= 1) <= 0) {
            Actor_Kill(&this->actor);
        }
        return;
    }
    if (this->rise < 1000) {
        this->rise++;
    }
    if (!this->paused && (this->emitInterval > 0) && (--this->emitTimer <= 0)) {
        HtFireRing_Emit(this, play);
        this->emitTimer = this->emitInterval;
    }
    for (i = 1; i < HT_FIRE_LAYERS; i++) {
        HtFireLayer* l = &this->layers[i];

        if (!l->on) {
            continue;
        }
        if (l->age < 1000) {
            l->age++;
        }
        // closing in: its radius shrinking at the set speed, its centre sliding toward its spot in step
        if (!this->paused) {
            l->s += this->speed / HT_FIRE_WALL_R;
        }
        HtFireRing_Place(this, l);
        if (l->r < FIRE_END_R) {
            // shut on its spot: a last burst, gone
            Vec3f p = { l->tx, this->actor.world.pos.y + 10.0f, l->tz };
            Vec3f vel = { 0.0f, 2.0f, 0.0f };
            Vec3f acc = { 0.0f, 0.2f, 0.0f };

            l->on = false;
            if (l->alive != 0) {
                HtFireRing_Steam(play, &p, &vel, &acc, 300);
            }
        }
    }
    HtFireRing_Extinguish(this, play);
    HtFireRing_Burn(this, play);
    {
        // the roar of the cage, from its nearest stretch to Link
        f32 dx = player->actor.world.pos.x - this->actor.world.pos.x;
        f32 dz = player->actor.world.pos.z - this->actor.world.pos.z;
        f32 d = sqrtf(SQ(dx) + SQ(dz));

        if (d > 1.0f) {
            HtSfx_PlayFor(this->actor.world.pos.x + dx / d * HT_FIRE_WALL_R, this->actor.world.pos.y + 30.0f,
                          this->actor.world.pos.z + dz / d * HT_FIRE_WALL_R, NA_SE_EV_FIRE_PILLAR_S, 2);
        }
    }
}

void HtFireRing_Draw(Actor* thisx, PlayState* play) {
    HtFireRing* this = (HtFireRing*)thisx;
    Camera* cam = GET_ACTIVE_CAM(play);
    f32 camYaw = BINANG_TO_RAD((s16)(Camera_GetCamDirYaw(cam) + 0x8000));
    f32 grow = CLAMP((f32)this->rise / FIRE_RISE, 0.0f, 1.0f);
    s32 i, k, j;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    if (this->layers[0].on) {
        // the cage: Ganon's ring of fire (BossGanon2's), brought in to its radius - a sheet of flame, uncuttable,
        // unlike the rings' flickering flames. (Its model: a 12-sided band of radius ~8450 about (200, 91), 3830 high)
        f32 sxz = HT_FIRE_WALL_R / 8450.0f;
        f32 sy = 0.03f * grow;

        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, this->scroll, 0, 32, 64, 1, -this->scroll * 2,
                                      -this->scroll * 8, 32, 32, 1, 0, -2, -8));
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 200, 0, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 128);
        Matrix_Translate(this->actor.world.pos.x - 200.0f * sxz, this->actor.world.pos.y - 2.0f,
                         this->actor.world.pos.z - 91.0f * sxz, MTXMODE_NEW);
        Matrix_Scale(sxz, MAX(sy, 0.001f), sxz, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)HT_FIRE_WALL_DL);
        gDPPipeSync(POLY_XLU_DISP++);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    }
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0, (this->scroll * -20) & 0x1FF, 0x20,
                                  0x80, 0, 0, 0, -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 230, 120, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 40, 0, 0);
    for (i = 1; i < HT_FIRE_LAYERS; i++) {
        HtFireLayer* l = &this->layers[i];
        f32 base = 0.0072f;
        s32 per;

        if (!l->on) {
            continue;
        }
        base *= CLAMP((f32)l->age / 8.0f, 0.2f, 1.0f) * CLAMP(l->r / 60.0f, 0.45f, 1.0f);
        per = 3; // (fixed: a ring's flames keep their places as it shrinks)
        for (k = 0; k < HT_FIRE_SEGS; k++) {
            if (!(l->alive & (1u << k))) {
                continue;
            }
            for (j = 0; j < per; j++) {
                Vec3f p;
                s32 n = k * 7 + j * 3 + l->gen * 13;
                f32 flick = 0.82f + 0.18f * sinf(this->scroll * 0.45f + n);
                f32 sc = base * grow * flick * (0.85f + 0.15f * ((n * 37) % 11) / 10.0f);

                // (each flame its own interpolation child: putting one out mustn't shift the others' pairing)
                FrameInterpolation_RecordOpenChild(this, (l->gen & 0x3FF) * 256 + k * 4 + j);
                HtFireRing_SegPoint(this, l, k + (j + 0.5f) / per, &p);
                Matrix_Translate(p.x, p.y, p.z, MTXMODE_NEW);
                Matrix_RotateY(camYaw, MTXMODE_APPLY);
                Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
                FrameInterpolation_RecordCloseChild();
            }
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
