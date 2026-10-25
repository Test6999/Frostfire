// Heretics Tomb: moving / spinning / wheel platforms. See HtPlatform.h.
// Looks: a stone slab built from the tomb's own floor and wall textures (generated with the rooms).

#include "HtPlatform.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include <math.h>
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"

enum { PLATFORM_SHUTTLE, PLATFORM_SPINNER, PLATFORM_WHEEL, PLATFORM_ELEVATOR };

static const f32 sSizes[] = { 80.0f, 120.0f, 160.0f, 200.0f };

#define SHUTTLE_PAUSE 30 // frames resting at each end

void HtPlatform_Init(Actor* thisx, PlayState* play) {
    HtPlatform* this = (HtPlatform*)thisx;
    s32 params = this->dyna.actor.params;
    s32 dist = (params >> 2) & 0xF;
    CollisionHeader* colHeader = NULL;
    s32 deep = (this->dyna.actor.home.rot.z & 0x4000) != 0; // (read before the options are cleared)

    this->mode = params & 3;
    this->distance = (dist == 0) ? 160.0f : dist * 40.0f;
    this->speed = (params >> 6) & 7;
    this->angle = ((params >> 9) & 7) * 0x2000;
    {
        s16 opts = this->dyna.actor.home.rot.z;

        this->gateFlag = (opts & 0x100) ? (opts & 0x3F) : -1;
        this->gateClear = (opts & 0x400) != 0;
        this->longPause = (opts & 0x800) != 0;
        this->carryEye = (opts & 0x200) != 0;
        this->extendFlag = (opts & 0x1000) ? (opts & 0x3F) : -1;
        this->oneWay = (opts & 0x2000) != 0;
        this->corridor = (opts & 0x8000) != 0;
        this->dyna.actor.home.rot.z = this->dyna.actor.world.rot.z = this->dyna.actor.shape.rot.z = 0;
    }
    // elevator: rotX = how far below home its third stop lies (0: the same distance as above)
    this->lowerDistance = (this->dyna.actor.home.rot.x > 0) ? (f32)this->dyna.actor.home.rot.x : this->distance;
    this->dyna.actor.home.rot.x = this->dyna.actor.world.rot.x = this->dyna.actor.shape.rot.x = 0;
    this->prevPos = this->dyna.actor.world.pos;

    // the slab model / collision are 100 x 100 units (top at y 0, 100 deep): the actor scale sizes them
    this->dyna.actor.scale.x = sSizes[(params >> 12) & 3] / 100.0f;
    this->dyna.actor.scale.z = sSizes[(params >> 14) & 3] / 100.0f;
    // 0x4000: a deep slab (its sides reach far enough down for Link to catch the edge when he jumps short)
    this->dyna.actor.scale.y = (deep ? HT_PLATFORM_DEEP : HT_PLATFORM_THICKNESS) / 100.0f;
    this->dyna.actor.uncullZoneForward = 3000.0f;
    this->dyna.actor.uncullZoneScale = 500.0f;
    this->dyna.actor.uncullZoneDownward = 2500.0f; // (the great lift goes a long way down)
    if (this->corridor) {
        // full size and never turned: its model is built along the hall (rotY, kept in home, is its run's direction)
        this->dyna.actor.scale.x = this->dyna.actor.scale.y = this->dyna.actor.scale.z = 1.0f;
        this->dyna.actor.world.rot.y = this->dyna.actor.shape.rot.y = 0;
        this->dyna.actor.uncullZoneScale = 900.0f;
    }

    DynaPolyActor_Init(&this->dyna, DPM_UNK3); // carries Link (and enemies), turning him with it
    CollisionHeader_GetVirtual(this->corridor ? HT_CORRIDOR_COL : HT_PLATFORM_COL, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
}

void HtPlatform_Destroy(Actor* thisx, PlayState* play) {
    HtPlatform* this = (HtPlatform*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

void HtPlatform_Update(Actor* thisx, PlayState* play) {
    HtPlatform* this = (HtPlatform*)thisx;
    Actor* a = &this->dyna.actor;
    f32 fwdX = Math_SinS(a->home.rot.y);
    f32 fwdZ = Math_CosS(a->home.rot.y);

    if (this->carryEye && !this->eyeSearched) {
        // first frame: find the eye switch placed against the slab (it was spawned with the room)
        Actor* it = play->actorCtx.actorLists[ACTORCAT_SWITCH].head;
        f32 best = 150.0f;

        this->eyeSearched = true;
        for (; it != NULL; it = it->next) {
            if ((it->id == ACTOR_OBJ_SWITCH) && ((it->params & 7) == OBJSWITCH_TYPE_EYE) &&
                (Actor_WorldDistXYZToActor(a, it) < best)) {
                best = Actor_WorldDistXYZToActor(a, it);
                this->eye = it;
            }
        }
    }
    if (((this->gateFlag >= 0) && !Flags_GetSwitch(play, this->gateFlag)) ||
        (this->gateClear && !Flags_GetClear(play, a->room) && !Flags_GetTempClear(play, a->room))) {
        // waiting for its switch / for the room to be cleared: hold still where its phase puts it
        if ((this->mode == PLATFORM_SHUTTLE) || (this->mode == PLATFORM_ELEVATOR)) {
            f32 t = (1.0f - Math_CosS(this->angle)) * 0.5f;

            if (this->mode == PLATFORM_ELEVATOR) {
                a->world.pos.y = a->home.pos.y + this->distance * t;
            } else {
                a->world.pos.x = a->home.pos.x + fwdX * this->distance * t;
                a->world.pos.z = a->home.pos.z + fwdZ * this->distance * t;
            }
        }
        this->prevPos = a->world.pos;
        return;
    }

    if (this->oneWay) {
        // a plug: gone once its flag is set (reached only then: the gate check above holds it still until)
        Actor_Kill(a);
        return;
    }
    if ((this->mode == PLATFORM_ELEVATOR) && (this->extendFlag >= 0) && Flags_GetSwitch(play, this->extendFlag)) {
        // three stops: top, home, bottom (four, for the great lift once B3 opens), up and down them in turn. Each
        // leg eases from one stop to the next.
        f32 stopY[4];
        s32 last = ((this->extendFlag == HT_LIFT_FLAG) && Flags_GetSwitch(play, HT_LIFT_DEEP_FLAG)) ? 3 : 2;
        s16 step = 0x80 + this->speed * 0x60;
        f32 e;

        stopY[0] = this->distance;
        stopY[1] = 0.0f;
        stopY[2] = -this->lowerDistance;
        stopY[3] = -this->lowerDistance - HT_LIFT_DEEP_DROP;
        if (!this->extendStarted || (this->stopsLast != last)) {
            // the way down has just opened (or Link comes into the well with it open): the lift is at the stop on
            // Link's floor, waiting for him to step on, and then heads on down - he never has to chase it
            Player* player = GET_PLAYER(play);
            f32 rel = player->actor.world.pos.y - a->home.pos.y;
            s32 best = 1;
            f32 bestD = 1.0e9f;

            for (s32 k = 0; k <= last; k++) {
                if (fabsf(stopY[k] - rel) < bestD) {
                    bestD = fabsf(stopY[k] - rel);
                    best = k;
                }
            }
            this->extendStarted = true;
            this->stopsLast = last;
            this->stopFrom = best;
            this->stopTo = (best < last) ? (best + 1) : (best - 1);
            this->legAngle = 0;
            this->pause = 0;
            this->waitRide = 160; // (8 s; then on its way, back for him later)
        }
        if (this->waitRide > 0) {
            if (DynaPolyActor_IsPlayerOnTop(&this->dyna)) {
                this->waitRide = 0;
                this->pause = 25; // (a moment once he's on)
            } else {
                this->waitRide--;
            }
        } else if (this->pause > 0) {
            this->pause--;
        } else {
            if ((u16)this->legAngle + step >= 0x8000) {
                s8 dir = this->stopTo - this->stopFrom;

                this->legAngle = 0;
                this->stopFrom = this->stopTo;
                if ((this->stopFrom + dir < 0) || (this->stopFrom + dir > last)) {
                    dir = -dir; // the end of the line: back the other way
                }
                this->stopTo = this->stopFrom + dir;
                this->pause = this->longPause ? 120 : 60;
            } else {
                this->legAngle += step;
                Actor_PlaySfx_Flagged(a, NA_SE_EV_ELEVATOR_MOVE3 - SFX_FLAG);
            }
        }
        e = (1.0f - Math_CosS(this->legAngle)) * 0.5f;
        a->world.pos.y = a->home.pos.y + stopY[this->stopFrom] + (stopY[this->stopTo] - stopY[this->stopFrom]) * e;
        this->prevPos = a->world.pos;
        return;
    }

    switch (this->mode) {
        case PLATFORM_SHUTTLE:
        case PLATFORM_ELEVATOR: {
            // eased back-and-forth: 0 at home, 1 at the far end (the elevator goes up instead of forward)
            f32 t;

            if (this->pause > 0) {
                this->pause--;
                break;
            }
            this->angle += 0x80 + this->speed * 0x60;
            t = (1.0f - Math_CosS(this->angle)) * 0.5f;
            if (((u16)this->angle < (u16)(0x80 + this->speed * 0x60)) ||
                (((u16)(this->angle - 0x8000)) < (u16)(0x80 + this->speed * 0x60))) {
                this->pause = this->longPause ? 120 : (this->mode == PLATFORM_ELEVATOR) ? 60 : SHUTTLE_PAUSE; // reached an end
            }
            if (this->mode == PLATFORM_ELEVATOR) {
                a->world.pos.y = a->home.pos.y + this->distance * t;
            } else {
                a->world.pos.x = a->home.pos.x + fwdX * this->distance * t;
                a->world.pos.z = a->home.pos.z + fwdZ * this->distance * t;
            }
            Actor_PlaySfx_Flagged(a, NA_SE_EV_ELEVATOR_MOVE3 - SFX_FLAG);
            break;
        }
        case PLATFORM_SPINNER:
            this->angle += 0x40 + this->speed * 0x30;
            a->shape.rot.y = a->world.rot.y = a->home.rot.y + this->angle; // silent: the loop got grating
            break;
        case PLATFORM_WHEEL: {
            // level platform on a vertical circle; the circle's plane faces rotY (it moves forward/back and up/down)
            f32 c, s;

            this->angle += 0x40 + this->speed * 0x30;
            c = Math_CosS(this->angle);
            s = Math_SinS(this->angle);
            a->world.pos.x = a->home.pos.x + fwdX * this->distance * c;
            a->world.pos.z = a->home.pos.z + fwdZ * this->distance * c;
            a->world.pos.y = a->home.pos.y + this->distance * s;
            Actor_PlaySfx_Flagged(a, NA_SE_EV_WINDMILL_LEVEL - SFX_FLAG);
            break;
        }
    }
    if (this->eye != NULL) {
        if (this->eye->update == NULL) {
            this->eye = NULL; // gone
        } else {
            // carry the eye switch along: its position and its hit triangles (set once at its init)
            ObjSwitch* sw = (ObjSwitch*)this->eye;
            Vec3f d;

            Math_Vec3f_Diff(&a->world.pos, &this->prevPos, &d);
            Math_Vec3f_Sum(&sw->dyna.actor.world.pos, &d, &sw->dyna.actor.world.pos);
            Math_Vec3f_Sum(&sw->dyna.actor.home.pos, &d, &sw->dyna.actor.home.pos);
            for (s32 i = 0; i < 2; i++) {
                Vec3f v[3];

                for (s32 k = 0; k < 3; k++) {
                    Math_Vec3f_Sum(&sw->tris.items[i].dim.vtx[k], &d, &v[k]);
                }
                Collider_SetTrisVertices(&sw->tris.col, i, &v[0], &v[1], &v[2]);
            }
        }
    }
    this->prevPos = a->world.pos;
}

void HtPlatform_Draw(Actor* thisx, PlayState* play) {
    HtPlatform* this = (HtPlatform*)thisx;

    Gfx_DrawDListOpa(play, (Gfx*)(this->corridor ? HT_CORRIDOR_DL : HT_PLATFORM_DL));
}
