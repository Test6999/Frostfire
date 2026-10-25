// Frostfire Mausoleum: Morpha's lair's spikes and wedges in Armorpha's arena. See HtMoSpikes.h.

#include "HtMoSpikes.h"
#include "HtMoSpikesRecipe.h"
#include "HtLynelBuild.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the wedges' surfaces: plain stone (their low tops), and a wall that hurts (their sides) - the arena's own two
static SurfaceType sSurfaces[2] = { { { 0, 2 } }, { { 0, 2 | (1 << 27) } } };
static CamData sCamData = { CAM_SET_DUNGEON0, 0, NULL }; // (the arena's camera, as everywhere in the mausoleum)

// the lair's files (scenes/shared in the 9.x archives; scenes/nonmq in older ones)
static const void* LoadLair(const char* path, size_t* size) {
    char alt[256];
    const char* at;
    const void* p = HtRes_LoadExact(path, size);

    if ((p != NULL) || ((at = strstr(path, "/shared/")) == NULL)) {
        return p;
    }
    snprintf(alt, sizeof(alt), "%.*s/nonmq/%s", (int)(at - path), path, at + 8);
    return HtRes_LoadExact(alt, size);
}

static void HtMoSpikes_BuildMesh(HtMoSpikes* this) {
    const Vtx* files[HT_MO_SPIKE_FILES];
    size_t counts[HT_MO_SPIKE_FILES];
    s32 i;

    for (i = 0; i < HT_MO_SPIKE_FILES; i++) {
        size_t size = 0;

        files[i] = (const Vtx*)LoadLair(sHtMoSpikeFiles[i], &size);
        counts[i] = size / sizeof(Vtx);
        if (files[i] == NULL) {
            return;
        }
    }
    this->vtx = malloc(sizeof(Vtx) * HT_MO_SPIKE_TRIS * 3);
    if (this->vtx == NULL) {
        return;
    }
    for (i = 0; i < HT_MO_SPIKE_TRIS * 3; i++) {
        u16 f = sHtMoSpikeCorners[i][0];
        u16 k = sHtMoSpikeCorners[i][1];
        const Vtx_t* src;
        Vtx_t* dst = &this->vtx[i].v;

        if (k >= counts[f]) {
            free(this->vtx);
            this->vtx = NULL;
            return;
        }
        src = &files[f][k].v;
        // mirrored like the rest of the lair (x -> -x) and set in the arena
        dst->ob[0] = HT_MO_X - src->ob[0];
        dst->ob[1] = HT_MO_Y + src->ob[1];
        dst->ob[2] = HT_MO_Z + src->ob[2];
        dst->flag = 0;
        dst->tc[0] = src->tc[0];
        dst->tc[1] = src->tc[1];
        dst->cn[0] = src->cn[0];
        dst->cn[1] = src->cn[1];
        dst->cn[2] = src->cn[2];
        dst->cn[3] = 255;
    }
    this->vtxCount = HT_MO_SPIKE_TRIS * 3;
}

// the lair's wedges: its wall-damage polygons and the low stone ones (tops up to 40 high), mirrored like the spikes
static s32 HtMoSpikes_BuildCollision(HtMoSpikes* this) {
    const CollisionHeader* lair = (const CollisionHeader*)LoadLair(HT_MO_COL, NULL);
    CollisionHeader* col = &this->col;
    s16* remap;
    s32 i, j, nv = 0, np = 0;

    if ((lair == NULL) || (lair->vtxList == NULL) || (lair->polyList == NULL)) {
        return false;
    }
    remap = malloc(sizeof(s16) * lair->numVertices);
    col->vtxList = malloc(sizeof(Vec3s) * lair->numVertices);
    col->polyList = malloc(sizeof(CollisionPoly) * lair->numPolygons);
    if ((remap == NULL) || (col->vtxList == NULL) || (col->polyList == NULL)) {
        free(remap);
        return false;
    }
    for (i = 0; i < lair->numVertices; i++) {
        remap[i] = -1;
    }
    for (i = 0; i < lair->numPolygons; i++) {
        const CollisionPoly* p = &lair->polyList[i];
        u16 vi[3] = { p->flags_vIA & 0x1FFF, p->flags_vIB & 0x1FFF, p->vIC };
        s32 maxY = -0x8000;
        Vec3s w[3];
        f32 ux, uy, uz, vx, vy, vz, nx, ny, nz, l;
        CollisionPoly* q;

        if ((vi[0] >= lair->numVertices) || (vi[1] >= lair->numVertices) || (vi[2] >= lair->numVertices)) {
            continue;
        }
        for (j = 0; j < 3; j++) {
            maxY = MAX(maxY, lair->vtxList[vi[j]].y);
        }
        if (!((p->type == 4) || ((p->type == 1) && (maxY > 0) && (maxY <= 40)))) {
            continue;
        }
        // mirrored: the winding turns too (corners c, b, a)
        for (j = 0; j < 3; j++) {
            const Vec3s* v = &lair->vtxList[vi[2 - j]];

            w[j].x = HT_MO_X - v->x;
            w[j].y = HT_MO_Y + v->y;
            w[j].z = HT_MO_Z + v->z;
        }
        ux = w[1].x - w[0].x, uy = w[1].y - w[0].y, uz = w[1].z - w[0].z;
        vx = w[2].x - w[0].x, vy = w[2].y - w[0].y, vz = w[2].z - w[0].z;
        nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        l = sqrtf(nx * nx + ny * ny + nz * nz);
        if (l == 0.0f) {
            continue;
        }
        nx /= l, ny /= l, nz /= l;
        q = &col->polyList[np++];
        q->type = (p->type == 4) ? 1 : 0;
        for (j = 0; j < 3; j++) {
            u16 src = vi[2 - j];

            if (remap[src] < 0) {
                remap[src] = nv;
                col->vtxList[nv++] = w[j];
            }
            q->vtxData[j] = remap[src];
        }
        q->normal.x = (s16)(nx * 0x7FFF);
        q->normal.y = (s16)(ny * 0x7FFF);
        q->normal.z = (s16)(nz * 0x7FFF);
        q->dist = (s16)lroundf(-(nx * w[0].x + ny * w[0].y + nz * w[0].z));
    }
    free(remap);
    if (np == 0) {
        return false;
    }
    col->minBounds = col->maxBounds = col->vtxList[0];
    for (i = 1; i < nv; i++) {
        col->minBounds.x = MIN(col->minBounds.x, col->vtxList[i].x);
        col->minBounds.y = MIN(col->minBounds.y, col->vtxList[i].y);
        col->minBounds.z = MIN(col->minBounds.z, col->vtxList[i].z);
        col->maxBounds.x = MAX(col->maxBounds.x, col->vtxList[i].x);
        col->maxBounds.y = MAX(col->maxBounds.y, col->vtxList[i].y);
        col->maxBounds.z = MAX(col->maxBounds.z, col->vtxList[i].z);
    }
    col->numVertices = nv;
    col->numPolygons = np;
    col->surfaceTypeList = sSurfaces;
    col->cameraDataList = &sCamData;
    col->cameraDataListLen = 1;
    col->numWaterBoxes = 0;
    col->waterBoxes = NULL;
    return true;
}

void HtMoSpikes_Init(Actor* thisx, PlayState* play) {
    HtMoSpikes* this = (HtMoSpikes*)thisx;

    this->vtx = NULL;
    this->vtxCount = 0;
    memset(&this->col, 0, sizeof(this->col));
    this->dyna.bgId = BG_ACTOR_MAX;
    Actor_SetScale(thisx, 1.0f); // (the collision is at its world positions: the actor sits at the origin, unscaled)
    HtMoSpikes_BuildMesh(this);
    DynaPolyActor_Init(&this->dyna, 0);
    if (HtMoSpikes_BuildCollision(this)) {
        this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, &this->col);
    }
}

void HtMoSpikes_Destroy(Actor* thisx, PlayState* play) {
    HtMoSpikes* this = (HtMoSpikes*)thisx;

    if (this->dyna.bgId != BG_ACTOR_MAX) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
    free(this->vtx);
    free(this->col.vtxList);
    free(this->col.polyList);
}

void HtMoSpikes_Update(Actor* thisx, PlayState* play) {
}

void HtMoSpikes_Draw(Actor* thisx, PlayState* play) {
    HtMoSpikes* this = (HtMoSpikes*)thisx;
    s32 i;

    if (this->vtx == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW); // (world positions, like the room's own model)
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)HT_MO_SPIKE_MAT);
    for (i = 0; i < this->vtxCount; i += 30) {
        s32 n = MIN(30, this->vtxCount - i);
        s32 k;

        gSPVertex(POLY_OPA_DISP++, (uintptr_t)&this->vtx[i], n, 0);
        for (k = 0; k + 6 <= n; k += 6) {
            gSP2Triangles(POLY_OPA_DISP++, k, k + 1, k + 2, 0, k + 3, k + 4, k + 5, 0);
        }
        if (k < n) {
            gSP1Triangle(POLY_OPA_DISP++, k, k + 1, k + 2, 0);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
