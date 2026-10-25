// Heretics Tomb: a wall of spikes. See HtSpikes.h.

#include "HtSpikes.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

static ColliderTrisElementInit sTrisElementsInit[2] = {
    {
        { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x10 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE,
          OCELEM_NONE },
        { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
    },
    {
        { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x10 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE,
          OCELEM_NONE },
        { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
    },
};

static ColliderTrisInit sTrisInit = {
    { COLTYPE_METAL, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_TRIS },
    2,
    sTrisElementsInit,
};

// a point on the bed's face, the points' tips: `side` along the wall, `up` above the base
static void HtSpikes_Point(HtSpikes* this, f32 side, f32 up, Vec3f* out) {
    f32 s = Math_SinS(this->actor.shape.rot.y), c = Math_CosS(this->actor.shape.rot.y);
    f32 out_ = HT_SPIKES_DEPTH * 0.7f; // a little short of the very tips

    // local x = along the wall (perpendicular to the facing), local z = the facing
    out->x = this->actor.world.pos.x + c * side + s * out_;
    out->y = this->actor.world.pos.y + up;
    out->z = this->actor.world.pos.z - s * side + c * out_;
}

void HtSpikes_Init(Actor* thisx, PlayState* play) {
    HtSpikes* this = (HtSpikes*)thisx;
    s32 w = thisx->params & 0x3F;
    s32 h = (thisx->params >> 6) & 0x3F;
    Vec3f a, b, c, d;

    this->width = (w == 0) ? 100.0f : w * 20.0f;
    this->height = (h == 0) ? 100.0f : h * 20.0f;
    // the model is one spike in a 40 x 40 cell, at full size: the bed is drawn as many of them as fit (never stretched)
    thisx->scale.x = thisx->scale.y = thisx->scale.z = 1.0f;

    Collider_InitTris(play, &this->collider);
    Collider_SetTris(play, &this->collider, thisx, &sTrisInit, this->colliderItems);
    HtSpikes_Point(this, -this->width * 0.5f, 0.0f, &a);
    HtSpikes_Point(this, this->width * 0.5f, 0.0f, &b);
    HtSpikes_Point(this, this->width * 0.5f, this->height, &c);
    HtSpikes_Point(this, -this->width * 0.5f, this->height, &d);
    Collider_SetTrisVertices(&this->collider, 0, &a, &b, &c);
    Collider_SetTrisVertices(&this->collider, 1, &a, &c, &d);
}

void HtSpikes_Destroy(Actor* thisx, PlayState* play) {
    HtSpikes* this = (HtSpikes*)thisx;

    Collider_DestroyTris(play, &this->collider);
}

void HtSpikes_Update(Actor* thisx, PlayState* play) {
    HtSpikes* this = (HtSpikes*)thisx;

    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
}

void HtSpikes_Draw(Actor* thisx, PlayState* play) {
    HtSpikes* this = (HtSpikes*)thisx;
    s32 nx = MAX(1, (s32)(this->width / HT_SPIKES_CELL + 0.5f));
    s32 ny = MAX(1, (s32)(this->height / HT_SPIKES_CELL + 0.5f));
    f32 step = this->width / nx;
    s32 i, j;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    for (i = 0; i < nx; i++) {
        for (j = 0; j < ny; j++) {
            Matrix_Push();
            Matrix_Translate(-this->width * 0.5f + step * (i + 0.5f), j * (this->height / ny), 0.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_SPIKES_DL);
            Matrix_Pop();
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
