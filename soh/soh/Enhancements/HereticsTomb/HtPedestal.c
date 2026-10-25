// Heretics Tomb: the boss arena's pedestal, sinking. See HtPedestal.h.

#include "HtPedestal.h"
#include "HtLynel.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include <math.h>

f32 HtPedestal_SinkCurve(s32 t) {
    f32 k = CLAMP((f32)t / HT_PEDESTAL_SINK_FRAMES, 0.0f, 1.0f);

    return 0.5f - 0.5f * cosf(k * (f32)M_PI);
}

static f32 HtPedestal_Depth(Actor* thisx) {
    return (thisx->params & HT_PEDESTAL_TOP) ? HT_PEDESTAL_TOP_DROP : HT_PEDESTAL_LOW_DROP;
}

void HtPedestal_MoveTo(Actor* thisx, f32 depth, s16 frames) {
    HtPedestal* this = (HtPedestal*)thisx;

    this->moveFrom = this->drop;
    this->moveTo = depth;
    this->moveT = 0;
    this->moveDur = MAX(frames, 1);
}

void HtPedestal_Init(Actor* thisx, PlayState* play) {
    HtPedestal* this = (HtPedestal*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_SetScale(thisx, 1.0f);
    thisx->uncullZoneForward = 4000.0f;
    thisx->uncullZoneScale = 600.0f;
    thisx->uncullZoneDownward = 600.0f;
    if (thisx->params >= HT_PEDESTAL_DIG_RING) {
        // the dig pieces: flush with the floor, waiting for their cue
        this->drop = this->moveFrom = this->moveTo = 0.0f;
        this->moveT = this->moveDur = 0;
        DynaPolyActor_Init(&this->dyna, DPM_UNK);
        CollisionHeader_GetVirtual((thisx->params == HT_PEDESTAL_DIG_PIT)    ? HT_PEDESTAL_DIG_PIT_COL
                                   : (thisx->params == HT_PEDESTAL_DIG_BAND)  ? HT_PEDESTAL_DIG_BAND_COL
                                   : (thisx->params == HT_PEDESTAL_DIG_RING2) ? HT_PEDESTAL_DIG_RING2_COL
                                                                              : HT_PEDESTAL_DIG_RING_COL,
                                   &colHeader);
        this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
        return;
    }
    if (Flags_GetSwitch(play, HT_PEDESTAL_SINK_FLAG) || Flags_GetSwitch(play, HT_LYNEL_CHAIN_FLAG)) {
        this->timer = HT_PEDESTAL_SINK_FRAMES; // already down (a retry: the fight starts with it down)
    }
    this->drop = HtPedestal_Depth(thisx) * HtPedestal_SinkCurve(this->timer);
    thisx->world.pos.y = thisx->home.pos.y - this->drop;

    DynaPolyActor_Init(&this->dyna, DPM_UNK); // (Link on it goes down with it)
    CollisionHeader_GetVirtual((thisx->params & HT_PEDESTAL_TOP) ? HT_PEDESTAL_TOP_COL : HT_PEDESTAL_LOW_COL,
                               &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
}

void HtPedestal_Destroy(Actor* thisx, PlayState* play) {
    HtPedestal* this = (HtPedestal*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

void HtPedestal_Update(Actor* thisx, PlayState* play) {
    HtPedestal* this = (HtPedestal*)thisx;

    if (thisx->params >= HT_PEDESTAL_DIG_RING) {
        if (this->moveT < this->moveDur) {
            f32 k = (f32)(++this->moveT) / this->moveDur;

            k = 0.5f - 0.5f * cosf(k * (f32)M_PI);
            this->drop = this->moveFrom + (this->moveTo - this->moveFrom) * k;
            thisx->world.pos.y = thisx->home.pos.y - this->drop;
            if (thisx->params == HT_PEDESTAL_DIG_PIT) {
                Actor_PlaySfx_Flagged(thisx, NA_SE_EV_ELEVATOR_MOVE - SFX_FLAG); // (refreshed each frame: stops when it does)
                if (this->moveT == this->moveDur) {
                    Audio_PlayActorSound2(thisx, NA_SE_EV_STONEDOOR_STOP);
                }
            }
        }
        return;
    }

    if ((this->timer < HT_PEDESTAL_SINK_FRAMES) && Flags_GetSwitch(play, HT_PEDESTAL_SINK_FLAG)) {
        this->timer++;
        this->drop = HtPedestal_Depth(thisx) * HtPedestal_SinkCurve(this->timer);
        thisx->world.pos.y = thisx->home.pos.y - this->drop;
        if (thisx->params & HT_PEDESTAL_TOP) { // (one grinding sound for the pair)
            Actor_PlaySfx_Flagged(thisx, NA_SE_EV_ELEVATOR_MOVE - SFX_FLAG); // (refreshed each frame: stops when it does)
            if (this->timer == HT_PEDESTAL_SINK_FRAMES) {
                Audio_PlayActorSound2(thisx, NA_SE_EV_STONEDOOR_STOP);
            }
        }
    }
}

void HtPedestal_Draw(Actor* thisx, PlayState* play) {
    if (thisx->params >= HT_PEDESTAL_DIG_RING) {
        Gfx_DrawDListOpa(play, (Gfx*)((thisx->params == HT_PEDESTAL_DIG_PIT)    ? HT_PEDESTAL_DIG_PIT_DL
                                      : (thisx->params == HT_PEDESTAL_DIG_BAND) ? HT_PEDESTAL_DIG_BAND_DL
                                                                                : HT_PEDESTAL_DIG_RING_DL));
        return;
    }
    Gfx_DrawDListOpa(play, (Gfx*)((thisx->params & HT_PEDESTAL_TOP) ? HT_PEDESTAL_TOP_DL : HT_PEDESTAL_LOW_DL));
}
