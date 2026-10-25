// Heretics Tomb: the boat of the B2 ring. See HtBoat.h.

#include "HtBoat.h"
#include "HtArmorpha.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include <math.h>
#include "objects/object_haka_objects/object_haka_objects.h"
#include "HtTentacle.h"

enum { BOAT_DOCKED, BOAT_SAIL, BOAT_WAVE };

#define BOAT_TWO_PI (2.0f * M_PI)
#define BOAT_STOP_S(i) ((i) * (0.5f * M_PI))
#define BOAT_GATE_S(i) (BOAT_STOP_S(i) + (25.0f * M_PI / 180.0f)) // gate i (1, 2) stands just past stop i
#define BOAT_WAVE_S(w) (BOAT_STOP_S(w) + (0.25f * M_PI))            // wave w (0-2) half way on from stop w
#define BOAT_BUMP (20.0f * M_PI / 180.0f)                          // docking: the swing out to a dock face
#define BOAT_MAX_SPEED 6.0f
#define BOAT_ACCEL 0.12f
#define BOAT_DEPART_DELAY 40
#define BOAT_DOCK_REACH 420.0f // Link ashore within this of a dock: the boat comes to him

typedef struct {
    s16 id;
    s16 params;
    s16 x, z; // on the deck (from its middle; x toward the stern, z to the left)
} BoatBoarder;

static const BoatBoarder sWave1[] = { { ACTOR_EN_TEST, 2, -150, 0 }, { ACTOR_EN_TEST, 2, 150, 0 } };
// the last wave: Morpha's water tentacles rise from the water on either side, three rounds (1, then 2, then 4); each
// one has to be frozen and smashed with the hammer (HtTentacle.c). Positions beside the deck (x along it, z: + left)
static const Vec2f sTentRound0[] = { { 0.0f, -270.0f } };
static const Vec2f sTentRound1[] = { { -120.0f, 270.0f }, { 120.0f, -270.0f } };
static const Vec2f sTentRound2[] = { { -170.0f, 270.0f }, { 170.0f, 270.0f }, { -170.0f, -270.0f }, { 170.0f, -270.0f } };
#define BOAT_WATER_Y (-1840.0f) // the ring's water surface

static f32 Boat_Wrap(f32 a) {
    while (a < 0.0f) {
        a += BOAT_TWO_PI;
    }
    while (a >= BOAT_TWO_PI) {
        a -= BOAT_TWO_PI;
    }
    return a;
}

// the path: angle round the ring from the port, radius swinging out to each dock as the boat comes alongside
static f32 Boat_Radius(f32 s) {
    f32 r = HT_BOAT_R_CRUISE;
    s32 i;

    for (i = 0; i < 4; i++) {
        f32 d = fabsf(Boat_Wrap(s - BOAT_STOP_S(i) + M_PI) - M_PI);

        if (d < BOAT_BUMP) {
            r += ((i == 0) ? (HT_BOAT_R_DOCK_IN - HT_BOAT_R_CRUISE) : (HT_BOAT_R_DOCK_OUT - HT_BOAT_R_CRUISE)) * 0.5f *
                 (1.0f + cosf(M_PI * d / BOAT_BUMP));
        }
    }
    return r;
}

static void Boat_PathPos(f32 s, Vec3f* pos, s16* yaw) {
    f32 phi = HT_BOAT_PORT_PHI - s;
    f32 r = Boat_Radius(s);

    pos->x = HT_BOAT_CX + r * cosf(phi);
    pos->y = HT_BOAT_Y;
    pos->z = HT_BOAT_CZ + r * sinf(phi);
    if (yaw != NULL) {
        // the bow (model -x) along the way round (phi decreasing): its right side (model -z) faces out
        *yaw = (s16)(s32)(atan2f(-cosf(phi), -sinf(phi)) * (32768.0f / M_PI));
    }
}

static s32 Boat_Reachable(PlayState* play, s32 stop) {
    switch (stop) {
        case 0:
            return true;
        case 1:
            return Flags_GetSwitch(play, HT_BOAT_F_STARTED);
        case 2:
            return Flags_GetSwitch(play, HT_BOAT_F_GATE1);
        default:
            return Flags_GetSwitch(play, HT_BOAT_F_GATE2);
    }
}

// would sailing `dist` (radians, signed) from s pass a portcullis that is still down?
static s32 Boat_PathBlocked(PlayState* play, f32 s, f32 dist) {
    s32 g;

    for (g = 1; g <= 2; g++) {
        f32 gs = BOAT_GATE_S(g);
        f32 ahead = (dist >= 0.0f) ? Boat_Wrap(gs - s) : Boat_Wrap(s - gs);

        if (!Flags_GetSwitch(play, (g == 1) ? HT_BOAT_F_GATE1 : HT_BOAT_F_GATE2) && (ahead < fabsf(dist))) {
            return true;
        }
    }
    return false;
}

static void Boat_DockPoint(s32 stop, Vec3f* out) {
    Vec3f p;
    f32 phi = HT_BOAT_PORT_PHI - BOAT_STOP_S(stop);
    f32 side = (stop == 0) ? -210.0f : 210.0f; // the port is on its inner side

    Boat_PathPos(BOAT_STOP_S(stop), &p, NULL);
    out->x = p.x + side * cosf(phi);
    out->y = HT_BOAT_Y + HT_BOAT_DECK;
    out->z = p.z + side * sinf(phi);
}

static void Boat_SetPos(HtBoat* this) {
    Boat_PathPos(this->s, &this->dyna.actor.world.pos, &this->dyna.actor.shape.rot.y);
    this->dyna.actor.world.rot.y = this->dyna.actor.shape.rot.y;
}

static void Boat_SailTo(HtBoat* this, PlayState* play, s32 stop, s32 dir) {
    this->state = BOAT_SAIL;
    this->stop = stop;
    this->dir = dir;
    this->speed = 0.0f;
}

// Link ashore by a stop the boat can reach and it isn't there: go to him (the shorter way that isn't barred)
static s32 Boat_ComeToLink(HtBoat* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 i;

    if ((this->aboard < 30) || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return false;
    }
    for (i = 0; i < 4; i++) {
        Vec3f dock;

        if (!Boat_Reachable(play, i)) {
            continue;
        }
        Boat_DockPoint(i, &dock);
        if (Math_Vec3f_DistXYZ(&dock, &player->actor.world.pos) < BOAT_DOCK_REACH) {
            f32 fwd = Boat_Wrap(BOAT_STOP_S(i) - this->s);
            f32 back = Boat_Wrap(this->s - BOAT_STOP_S(i));

            if ((this->state == BOAT_DOCKED) && (this->stop == i)) {
                return false; // already here
            }
            if ((fwd < 0.001f) || (back < 0.001f)) {
                this->s = BOAT_STOP_S(i);
                this->state = BOAT_DOCKED;
                this->stop = i;
                return true;
            }
            {
                s32 dir;

                if (((fwd <= back) || Boat_PathBlocked(play, this->s, -back)) && !Boat_PathBlocked(play, this->s, fwd)) {
                    dir = 1;
                } else if (!Boat_PathBlocked(play, this->s, -back)) {
                    dir = -1;
                } else {
                    return false;
                }
                if ((this->state == BOAT_SAIL) && (this->stop == i) && (this->dir == dir)) {
                    return false; // on its way already
                }
                Boat_SailTo(this, play, i, dir);
            }
            return true;
        }
    }
    return false;
}

static void Boat_SpawnBoarder(HtBoat* this, PlayState* play, s16 id, s16 params, f32 lx, f32 lz, f32 drop) {
    Actor* a = &this->dyna.actor;
    f32 c = Math_CosS(a->shape.rot.y), sn = Math_SinS(a->shape.rot.y);
    Actor* e;

    e = Actor_SpawnAsChild(&play->actorCtx, a, play, id, a->world.pos.x + lx * c + lz * sn,
                       a->world.pos.y + HT_BOAT_DECK + drop, a->world.pos.z - lx * sn + lz * c, 0,
                       a->yawTowardsPlayer, 0, params);
    (void)e;
}

static void Boat_SpawnTentacles(HtBoat* this, PlayState* play, s32 round) {
    static const Vec2f* sRounds[3] = { sTentRound0, sTentRound1, sTentRound2 };
    static const s32 sCounts[3] = { 1, 2, 4 };
    Actor* a = &this->dyna.actor;
    f32 c = Math_CosS(a->shape.rot.y), sn = Math_SinS(a->shape.rot.y);
    s32 i;

    for (i = 0; i < sCounts[round]; i++) {
        f32 lx = sRounds[round][i].x, lz = sRounds[round][i].y;

        Actor_SpawnAsChild(&play->actorCtx, a, play, HT_ACTOR_TENTACLE, a->world.pos.x + lx * c + lz * sn, BOAT_WATER_Y,
                           a->world.pos.z - lx * sn + lz * c, 0, a->yawTowardsPlayer, 0, 0);
    }
}

static void Boat_SpawnWave(HtBoat* this, PlayState* play, s32 w) {
    s32 i, j;

    switch (w) {
        case 0:
            for (i = 0; i < ARRAY_COUNT(sWave1); i++) {
                Boat_SpawnBoarder(this, play, sWave1[i].id, sWave1[i].params, sWave1[i].x, sWave1[i].z, 2.0f);
            }
            break;
        case 1:
            // a swarm of Torch Slugs: 5 x 4 over the deck (all boarders appear standing on it: dropped from above
            // they hung in the air)
            for (i = 0; i < 5; i++) {
                for (j = 0; j < 4; j++) {
                    Boat_SpawnBoarder(this, play, ACTOR_EN_BW, 0, -200.0f + i * 100.0f, -135.0f + j * 90.0f, 2.0f);
                }
            }
            break;
        default:
            Boat_SpawnTentacles(this, play, this->tentRound);
            break;
    }
}

static void Boat_KillBoarders(HtBoat* this, PlayState* play) {
    static const s32 sCats[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };
    s32 c;

    for (c = 0; c < ARRAY_COUNT(sCats); c++) {
        Actor* a = play->actorCtx.actorLists[sCats[c]].head;

        for (; a != NULL; a = a->next) {
            if (a->parent == &this->dyna.actor) {
                Actor_Kill(a);
            }
        }
    }
}

// boarders still fighting (any that fell overboard are gone)
static s32 Boat_BoardersLeft(HtBoat* this, PlayState* play) {
    static const s32 sCats[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };
    s32 n = 0, c;

    for (c = 0; c < ARRAY_COUNT(sCats); c++) {
        Actor* a = play->actorCtx.actorLists[sCats[c]].head;

        for (; a != NULL; a = a->next) {
            if (a->parent != &this->dyna.actor) {
                continue;
            }
            if (a->world.pos.y < this->dyna.actor.world.pos.y - 400.0f) {
                Actor_Kill(a);
                continue;
            }
            n++;
        }
    }
    return n;
}

void HtBoat_Init(Actor* thisx, PlayState* play) {
    HtBoat* this = (HtBoat*)thisx;
    CollisionHeader* colHeader = NULL;
    Player* player = GET_PLAYER(play);
    f32 best = 1e9f;
    s32 i;

    Actor_SetScale(thisx, 0.1f);
    thisx->uncullZoneForward = 6000.0f;
    thisx->uncullZoneScale = 1500.0f;
    thisx->uncullZoneDownward = 1500.0f;
    thisx->flags |= ACTOR_FLAG_UPDATE_DURING_OCARINA;

    // it waits at the stop it can reach nearest to where Link comes in
    this->stop = 0;
    for (i = 0; i < 4; i++) {
        Vec3f dock;
        f32 d;

        if (!Boat_Reachable(play, i)) {
            continue;
        }
        Boat_DockPoint(i, &dock);
        d = (player != NULL) ? Math_Vec3f_DistXYZ(&dock, &player->actor.world.pos) : 0.0f;
        if (d < best) {
            best = d;
            this->stop = i;
        }
    }
    this->s = BOAT_STOP_S(this->stop);
    this->state = BOAT_DOCKED;
    this->aboard = 100;
    Boat_SetPos(this);
    thisx->home.pos = thisx->world.pos;

    DynaPolyActor_Init(&this->dyna, DPM_UNK3); // carries Link and the boarders, turning them with it
    CollisionHeader_GetVirtual(HT_BOAT_COL, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
}

void HtBoat_Destroy(Actor* thisx, PlayState* play) {
    HtBoat* this = (HtBoat*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

// Zelda's Lullaby, played standing on the boat (as an ocarina spot does it: see EnBox)
static s32 Boat_ListenForLullaby(HtBoat* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->aboard > 0) {
        this->ocState = 0;
        return false;
    }
    if (this->ocState == 0) {
        if (!(player->stateFlags2 & PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR)) {
            player->stateFlags2 |= PLAYER_STATE2_NEAR_OCARINA_ACTOR;
            return false;
        }
        this->ocState = 1;
    }
    if (this->ocState == 1) {
        func_8010BD58(play, OCARINA_ACTION_FREE_PLAY);
        this->ocState = 2;
    } else if ((this->ocState == 2) && (play->msgCtx.ocarinaMode == OCARINA_MODE_04)) {
        this->ocState = 0;
        if (play->msgCtx.lastPlayedSong == OCARINA_SONG_LULLABY) {
            return true;
        }
    }
    return false;
}

void HtBoat_Update(Actor* thisx, PlayState* play) {
    HtBoat* this = (HtBoat*)thisx;

    if (DynaPolyActor_IsPlayerOnTop(&this->dyna) ||
        ((GET_PLAYER(play)->actor.parent != NULL) && (GET_PLAYER(play)->actor.parent->id == HT_ACTOR_TENTACLE))) {
        this->aboard = 0; // (held up by a tentacle he's still aboard)
    } else if (this->aboard < 1000) {
        this->aboard++;
    }

    switch (this->state) {
        case BOAT_DOCKED:
            if (!Flags_GetSwitch(play, HT_BOAT_F_STARTED)) {
                if (Boat_ListenForLullaby(this, play)) {
                    Flags_SetSwitch(play, HT_BOAT_F_STARTED);
                    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
                    this->timer = BOAT_DEPART_DELAY - 20;
                }
                break;
            }
            if (this->aboard < 10) {
                s32 next = (this->stop + 1) & 3;

                // (from the mini-boss's door it waits there till Armorpha's beaten)
                if ((Boat_Reachable(play, next) || (next == 0)) &&
                    ((this->stop != 3) || Flags_GetSwitch(play, HT_ARMORPHA_FLAG))) {
                    if (++this->timer >= BOAT_DEPART_DELAY) {
                        this->timer = 0;
                        Boat_SailTo(this, play, next, 1);
                    }
                } else {
                    this->timer = 0;
                }
            } else {
                this->timer = 0;
                Boat_ComeToLink(this, play);
            }
            break;

        case BOAT_SAIL: {
            f32 target = BOAT_STOP_S(this->stop);
            f32 left = (this->dir > 0) ? Boat_Wrap(target - this->s) : Boat_Wrap(this->s - target);
            f32 r = Boat_Radius(this->s);
            f32 step, prev = this->s;

            if (left > M_PI * 1.9f) {
                left = 0.0f; // (overshot by a hair)
            }
            // ease in, ease out into the stop
            Math_StepToF(&this->speed, MIN(BOAT_MAX_SPEED, sqrtf(2.0f * BOAT_ACCEL * left * r) + 0.5f), BOAT_ACCEL);
            step = this->speed / r;
            if (step >= left) {
                this->s = target;
                this->state = BOAT_DOCKED;
                this->timer = 0;
                this->speed = 0.0f;
                Audio_PlayActorSound2(thisx, NA_SE_EV_STONEDOOR_STOP);
            } else {
                this->s = Boat_Wrap(this->s + step * this->dir);
            }
            Audio_PlayActorSound2(thisx, NA_SE_EV_SHIP_BELL - SFX_FLAG);

            // boarders, half way between stops - only with Link aboard, going on round
            if ((this->dir > 0) && (this->aboard < 10)) {
                s32 w;

                for (w = 0; w < 3; w++) {
                    f32 ws = BOAT_WAVE_S(w);

                    if (!Flags_GetSwitch(play, HT_BOAT_F_WAVE1 + w) && (Boat_Wrap(ws - prev) < 1.0f) &&
                        (Boat_Wrap(ws - prev) <= Boat_Wrap(this->s - prev)) && (this->state == BOAT_SAIL)) {
                        this->s = ws;
                        this->state = BOAT_WAVE;
                        this->wave = w;
                        this->timer = 0;
                        this->speed = 0.0f;
                        break;
                    }
                }
            } else if ((this->dir > 0) && (this->aboard > 60)) {
                // Link went overboard: the boat waits for him where he comes ashore
                Boat_ComeToLink(this, play);
            }
            break;
        }

        case BOAT_WAVE:
            this->timer++;
            if ((this->timer == 15) && !this->waveSpawned[this->wave]) {
                this->waveSpawned[this->wave] = true;
                Boat_SpawnWave(this, play, this->wave);
            }
            if ((this->timer > 40) && this->waveSpawned[this->wave] && (Boat_BoardersLeft(this, play) == 0) &&
                (this->wave == 2) && (this->tentRound < 2)) {
                // the tentacles: the next round rises
                this->tentRound++;
                this->waveSpawned[this->wave] = false;
                this->timer = -20;
            } else if ((this->timer > 40) && this->waveSpawned[this->wave] && (Boat_BoardersLeft(this, play) == 0)) {
                Flags_SetSwitch(play, HT_BOAT_F_WAVE1 + this->wave);
                Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
                this->state = BOAT_SAIL; // on to the stop it was making for
                this->speed = 0.0f;
            } else if (this->aboard > 90) {
                // Link fell overboard mid-fight: fetch him (the boarders still aboard wait for the next pass; the
                // tentacles sink, and rise again from the first round when he comes back)
                if (Boat_ComeToLink(this, play) && (this->wave == 2)) {
                    Boat_KillBoarders(this, play);
                    this->waveSpawned[2] = false;
                    this->tentRound = 0;
                }
            }
            break;
    }

    Boat_SetPos(this);
}

void HtBoat_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(600.0f, 0.0f, 0.0f, MTXMODE_APPLY); // the vanilla ship's deck middle to the actor's origin
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, object_haka_objects_DL_00D330);
    Matrix_Translate(-3670.0f, 620.0f, 1150.0f, MTXMODE_APPLY); // its two hanging lanterns
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, object_haka_objects_DL_005A70);
    Matrix_Translate(0.0f, 0.0f, -2300.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, object_haka_objects_DL_005A70);
    Matrix_Pop();
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_BOAT_RAILS_DL);
    CLOSE_DISPS(play->state.gfxCtx);
}
