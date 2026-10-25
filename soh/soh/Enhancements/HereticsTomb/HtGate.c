// Heretics Tomb: iron portcullis across a channel of the B2 ring. See HtGate.h.

#include "HtGate.h"
#include "Katana.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"

void HtGate_Init(Actor* thisx, PlayState* play) {
    HtGate* this = (HtGate*)thisx;
    CollisionHeader* colHeader = NULL;

    this->flag = thisx->params & 0x3F;
    this->rise = (thisx->params & HT_GATE_FENCE) ? HT_FENCE_RISE : HT_GATE_RISE;
    Actor_SetScale(thisx, 1.0f);
    thisx->uncullZoneForward = 4000.0f;
    thisx->uncullZoneScale = 1200.0f;
    thisx->uncullZoneDownward = 1200.0f;
    this->lift = Flags_GetSwitch(play, this->flag) ? this->rise : 0.0f;
    thisx->world.pos.y = thisx->home.pos.y + this->lift;

    DynaPolyActor_Init(&this->dyna, 0);
    CollisionHeader_GetVirtual((thisx->params & HT_GATE_FENCE) ? HT_FENCE_COL : HT_GATE_COL, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
}

void HtGate_Destroy(Actor* thisx, PlayState* play) {
    HtGate* this = (HtGate*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

void HtGate_Update(Actor* thisx, PlayState* play) {
    HtGate* this = (HtGate*)thisx;

    if ((this->lift < this->rise) && Flags_GetSwitch(play, this->flag)) {
        if (this->timer == 0) {
            Audio_PlayActorSound2(thisx, NA_SE_EV_IRON_DOOR_OPEN);
        }
        this->timer++;
        // a slow grind up, then faster
        Math_StepToF(&this->lift, this->rise, (this->timer < 30) ? 2.0f : 10.0f);
        Audio_PlayActorSound2(thisx, NA_SE_EV_METALDOOR_SLIDE - SFX_FLAG);
        if (this->lift >= this->rise) {
            Audio_PlayActorSound2(thisx, NA_SE_EV_METALDOOR_STOP);
        }
        thisx->world.pos.y = thisx->home.pos.y + this->lift;
    }
}

void HtGate_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)((thisx->params & HT_GATE_FENCE) ? HT_FENCE_DL : HT_GATE_DL));
}
