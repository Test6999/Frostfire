// Heretics Tomb's dungeon map and compass. See HtMap.h.

#include "global.h"
#include "HtMap.h"
#include "HtMapData.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "textures/parameter_static/parameter_static.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <math.h>

#define HT_SUB 8.0f // vertices in eighths of a pixel / page unit (the map's matrix scales them back)

static s16 sCurPiece = -1;
static u32 sLastFrame = 0;
// where Link came into the room he's in, and which way he faced (vanilla's red arrow)
static f32 sEntryX = 0.0f;
static f32 sEntryZ = 0.0f;
static s16 sEntryDir = 0;

static s32 HtMap_PieceAt(s32 room, f32 y) {
    s32 i;

    for (i = 0; i < HT_MAP_PIECES; i++) {
        if ((sHtMapPieces[i].room == room) && (y > sHtMapPieces[i].yMin) && (y <= sHtMapPieces[i].yMax)) {
            return i;
        }
    }
    // (between the central well's bands: the nearest of its pieces)
    for (i = 0; i < HT_MAP_PIECES; i++) {
        if (sHtMapPieces[i].room == room) {
            return i;
        }
    }
    return -1;
}

s32 HtMap_FloorCount(void) {
    return HT_MAP_FLOORS;
}

s32 HtMap_BossFloor(void) {
    return HT_MAP_BOSS_FLOOR;
}

static bool HtMap_HasMap(void) {
    return CHECK_DUNGEON_ITEM(DUNGEON_MAP, gSaveContext.mapIndex);
}

static bool HtMap_HasCompass(void) {
    return CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, gSaveContext.mapIndex);
}

static bool HtMap_PieceShown(s32 i) {
    return HtMap_HasMap() || (HtMap_GetVisited() & (1u << i));
}

bool HtMap_FloorShown(s32 floor) {
    s32 i;

    if ((floor < 0) || (floor >= HT_MAP_FLOORS)) {
        return false;
    }
    if (HtMap_HasMap()) {
        return true;
    }
    for (i = 0; i < HT_MAP_PIECES; i++) {
        if ((sHtMapPieces[i].floor == floor) && (HtMap_GetVisited() & (1u << i))) {
            return true;
        }
    }
    return false;
}

void HtMap_Update(PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 piece;
    bool fresh = (play->gameplayFrames != sLastFrame + 1) && (play->gameplayFrames != sLastFrame);

    sLastFrame = play->gameplayFrames;
    if (player == NULL) {
        return;
    }
    piece = HtMap_PieceAt(play->roomCtx.curRoom.num, player->actor.world.pos.y);
    if (piece < 0) {
        return;
    }
    if ((piece != sCurPiece) || fresh) {
        // (into a room, or the tomb: where he came in)
        sCurPiece = piece;
        sEntryX = player->actor.world.pos.x;
        sEntryZ = player->actor.world.pos.z;
        sEntryDir = (0x7FFF - player->actor.shape.rot.y) / 0x400;
    }
    if (!(HtMap_GetVisited() & (1u << piece))) {
        HtMap_SetVisited(HtMap_GetVisited() | (1u << piece));
    }
    VREG(30) = HT_MAP_FLOOR_SLOT0 + sHtMapPieces[piece].floor;
}

// --- drawing: triangles batched into the vertex buffer ---

typedef struct {
    Gfx** gfx;
    Vtx* vtx; // the next free vertex
    s32 n;    // in the pending batch
} HtTriBatch;

static void HtBatch_Flush(HtTriBatch* b) {
    s32 i;

    if (b->n == 0) {
        return;
    }
    gSPVertex((*b->gfx)++, b->vtx, b->n, 0);
    for (i = 0; i < b->n; i += 3) {
        gSP1Triangle((*b->gfx)++, i, i + 1, i + 2, 0);
    }
    b->vtx += b->n;
    b->n = 0;
}

static void HtBatch_Tri(HtTriBatch* b, f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, Color_RGBA8 c) {
    f32 xs[3] = { x0, x1, x2 };
    f32 ys[3] = { y0, y1, y2 };
    s32 i;

    for (i = 0; i < 3; i++) {
        Vtx* v = &b->vtx[b->n++];

        v->v.ob[0] = (s16)roundf(xs[i]);
        v->v.ob[1] = (s16)roundf(ys[i]);
        v->v.ob[2] = 0;
        v->v.flag = 0;
        v->v.tc[0] = v->v.tc[1] = 0;
        v->v.cn[0] = c.r;
        v->v.cn[1] = c.g;
        v->v.cn[2] = c.b;
        v->v.cn[3] = c.a;
    }
    if (b->n >= 30) {
        HtBatch_Flush(b);
    }
}

// a line hw (in local units) either side of a..b, its ends carried on by hw (so the corners close)
static void HtBatch_Line(HtTriBatch* b, f32 ax, f32 ay, f32 bx, f32 by, f32 hw, Color_RGBA8 c) {
    f32 dx = bx - ax;
    f32 dy = by - ay;
    f32 len = sqrtf(SQ(dx) + SQ(dy));
    f32 ux, uy;

    if (len < 0.01f) {
        return;
    }
    ux = dx / len * hw;
    uy = dy / len * hw;
    ax -= ux;
    ay -= uy;
    bx += ux;
    by += uy;
    HtBatch_Tri(b, ax - uy, ay + ux, bx - uy, by + ux, bx + uy, by - ux, c);
    HtBatch_Tri(b, ax - uy, ay + ux, bx + uy, by - ux, ax + uy, ay - ux, c);
}

// a piece's shape: its fill, then its outline (k: local units per world unit; the local origin at world bx, bz; y up)
static void HtMap_DrawPiece(HtTriBatch* b, s32 i, f32 bx, f32 bz, f32 k, f32 hw, Color_RGBA8 fill, Color_RGBA8 line) {
    const HtMapPiece* p = &sHtMapPieces[i];
    s32 j;

    for (j = p->triStart; j < p->triStart + p->triCount; j++) {
        const s16* t = sHtMapTris[j];
        HtBatch_Tri(b, (t[0] - bx) * k, -(t[1] - bz) * k, (t[2] - bx) * k, -(t[3] - bz) * k, (t[4] - bx) * k,
                    -(t[5] - bz) * k, fill);
    }
    if (line.a != 0) {
        for (j = p->edgeStart; j < p->edgeStart + p->edgeCount; j++) {
            const s16* e = sHtMapEdges[j];
            HtBatch_Line(b, (e[0] - bx) * k, -(e[1] - bz) * k, (e[2] - bx) * k, -(e[3] - bz) * k, hw, line);
        }
    }
}

static s32 HtMap_PieceVtxCount(s32 i) {
    return (sHtMapPieces[i].triCount + 2 * sHtMapPieces[i].edgeCount) * 3;
}

static void HtMap_SetupShapes(Gfx** gfx) {
    gDPPipeSync((*gfx)++);
    gSPTexture((*gfx)++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode((*gfx)++, G_CC_SHADE, G_CC_SHADE);
    gSPClearGeometryMode((*gfx)++, G_CULL_BOTH | G_LIGHTING | G_ZBUFFER);
    gSPSetGeometryMode((*gfx)++, G_SHADE | G_SHADING_SMOOTH);
}

static Color_RGBA8 HtMap_Rgba(u8 r, u8 g, u8 b, u8 a) {
    Color_RGBA8 c = { r, g, b, a };
    return c;
}

// --- the minimap ---

void HtMap_DrawMinimap(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    const HtMapPiece* p;
    Color_RGB8 mc = { 0, 255, 255 };
    Color_RGB8 here = { 200, 255, 0 };
    Color_RGB8 entry = { 200, 0, 0 };
    s32 posType = CVarGetInteger(CVAR_COSMETIC("HUD.Minimap.PosType"), 0);
    s16 xMargin = 0;
    s16 yMargin = 0;
    s16 left, top;
    f32 w, h, k, mapLeft, mapTop;
    u8 alpha = interfaceCtx->minimapAlpha;
    s32 i;

    if ((sCurPiece < 0) || R_MINIMAP_DISABLED || (posType == HIDDEN) || (alpha == 0)) {
        return;
    }
    if (!HtMap_HasMap() && !HtMap_HasCompass()) {
        return;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Minimap.Changed"), 0)) {
        mc = CVarGetColor24(CVAR_COSMETIC("HUD.Minimap.Value"), mc);
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.MinimapPosition.Changed"), 0)) {
        here = CVarGetColor24(CVAR_COSMETIC("HUD.MinimapPosition.Value"), here);
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.MinimapEntrance.Changed"), 0)) {
        entry = CVarGetColor24(CVAR_COSMETIC("HUD.MinimapEntrance.Value"), entry);
    }

    // the minimap's box, where the vanilla dungeon minimap goes (its 96 x 85, its position settings)
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Minimap.UseMargins"), 0) != 0) {
        xMargin = (posType == ANCHOR_LEFT) ? CVarGetInteger(CVAR_COSMETIC("HUD.Margin.L"), 0)
                                           : CVarGetInteger(CVAR_COSMETIC("HUD.Margin.R"), 0);
        yMargin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.B"), 0);
    }
    left = OTRGetRectDimensionFromRightEdge(R_DGN_MINIMAP_X + xMargin);
    top = R_DGN_MINIMAP_Y + yMargin;
    if (posType != ORIGINAL_LOCATION) {
        s16 px = CVarGetInteger(CVAR_COSMETIC("HUD.Minimap.PosX"), 0);

        top = R_DGN_MINIMAP_Y + CVarGetInteger(CVAR_COSMETIC("HUD.Minimap.PosY"), 0) + yMargin;
        if (posType == ANCHOR_LEFT) {
            left = OTRGetRectDimensionFromLeftEdge(R_DGN_MINIMAP_X + px + xMargin);
        } else if (posType == ANCHOR_RIGHT) {
            left = OTRGetRectDimensionFromRightEdge(R_DGN_MINIMAP_X + px + xMargin);
        } else {
            left = px;
        }
    }

    // the room fitted in the box (never closer than a pixel to 8 units)
    p = &sHtMapPieces[sCurPiece];
    w = MAX(1, p->x1 - p->x0);
    h = MAX(1, p->z1 - p->z0);
    k = MIN(MIN(88.0f / w, 77.0f / h), 0.125f);
    mapLeft = left + (96.0f - w * k) / 2.0f;
    mapTop = top + (85.0f - h * k) / 2.0f;

    OPEN_DISPS(gfxCtx);

    if (HtMap_HasMap()) {
        HtTriBatch b;

        b.gfx = &OVERLAY_DISP;
        b.vtx = Graph_Alloc(gfxCtx, HtMap_PieceVtxCount(sCurPiece) * sizeof(Vtx));
        b.n = 0;
        Gfx_SetupDL_42Overlay(gfxCtx);
        HtMap_SetupShapes(&OVERLAY_DISP);
        Matrix_Translate(mapLeft - SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - mapTop, 0.0f, MTXMODE_NEW);
        Matrix_Scale(1.0f / HT_SUB, 1.0f / HT_SUB, 1.0f, MTXMODE_APPLY);
        gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        HtMap_DrawPiece(&b, sCurPiece, p->x0, p->z0, k * HT_SUB, 0.55f * HT_SUB,
                        HtMap_Rgba(mc.r * 2 / 5, mc.g * 2 / 5, mc.b * 2 / 5, alpha * 3 / 5),
                        HtMap_Rgba(mc.r, mc.g, mc.b, alpha));
        HtBatch_Flush(&b);
    }

    if (HtMap_HasCompass()) {
        // the room's unopened chests
        Gfx_SetupDL_39Overlay(gfxCtx);
        gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, alpha);
        gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, alpha);
        for (i = 0; i < HT_MAP_CHESTS; i++) {
            s16 cx, cy;

            if ((sHtMapChests[i][4] != sCurPiece) || Flags_GetTreasure(play, sHtMapChests[i][3])) {
                continue;
            }
            cx = (s16)(mapLeft + (sHtMapChests[i][0] - p->x0) * k) - 4;
            cy = (s16)(mapTop + (sHtMapChests[i][2] - p->z0) * k) - 4;
            gDPPipeSync(OVERLAY_DISP++);
            gDPLoadTextureBlock(OVERLAY_DISP++, gMapChestIconTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPWideTextureRectangle(OVERLAY_DISP++, cx << 2, cy << 2, (cx + 8) << 2, (cy + 8) << 2,
                                    G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }

        // his arrow, and the one where he came in (as Minimap_DrawCompassIcons)
        if (alpha >= 0xAA) {
            f32 ax = mapLeft + (player->actor.world.pos.x - p->x0) * k - SCREEN_WIDTH / 2;
            f32 ay = SCREEN_HEIGHT / 2 - (mapTop + (player->actor.world.pos.z - p->z0) * k);
            f32 ex = mapLeft + (sEntryX - p->x0) * k - SCREEN_WIDTH / 2;
            f32 ey = SCREEN_HEIGHT / 2 - (mapTop + (sEntryZ - p->z0) * k);
            s16 dir = (0x7FFF - player->actor.shape.rot.y) / 0x400;

            Gfx_SetupDL_42Overlay(gfxCtx);
            gSPMatrix(OVERLAY_DISP++, &gMtxClear, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);

            Matrix_Translate(ax, ay, 0.0f, MTXMODE_NEW);
            Matrix_Scale(0.4f, 0.4f, 0.4f, MTXMODE_APPLY);
            Matrix_RotateX(-1.6f, MTXMODE_APPLY);
            Matrix_RotateY(dir / 10.0f, MTXMODE_APPLY);
            gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0xFF, here.r, here.g, here.b, 255);
            gSPDisplayList(OVERLAY_DISP++, gCompassArrowDL);

            Matrix_Translate(ex, ey, 0.0f, MTXMODE_NEW);
            Matrix_Scale(0.4f, 0.4f, 0.4f, MTXMODE_APPLY);
            Matrix_RotateX(-1.6f, MTXMODE_APPLY);
            Matrix_RotateY(sEntryDir / 10.0f, MTXMODE_APPLY);
            gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0xFF, entry.r, entry.g, entry.b, 255);
            gSPDisplayList(OVERLAY_DISP++, gCompassArrowDL);
        }
    }

    CLOSE_DISPS(gfxCtx);
}

// --- the pause screen's map ---

// (x0, y0: the map area's top left on the page, 96 x 85; the page's matrix is the current one)
void HtMap_DrawPauseFloor(PlayState* play, GraphicsContext* gfxCtx, s32 floor, s16 x0, s16 y0, u8 alpha) {
    const s16* box;
    f32 w, h, k, ox, oy, pulse;
    s32 i, count;
    HtTriBatch b;

    if ((floor < 0) || (floor >= HT_MAP_FLOORS)) {
        return;
    }
    box = sHtMapFloorBox[floor];
    w = MAX(1, box[2] - box[0]);
    h = MAX(1, box[3] - box[1]);
    k = MIN(90.0f / w, 79.0f / h);
    ox = x0 + (96.0f - w * k) / 2.0f;
    oy = y0 - (85.0f - h * k) / 2.0f;
    // the room he's in pulses blue - green (as the vanilla map's)
    pulse = (Math_SinS(play->state.frames * 0x600) + 1.0f) / 2.0f;

    count = 0;
    for (i = 0; i < HT_MAP_PIECES; i++) {
        if (sHtMapPieces[i].floor == floor) {
            count += HtMap_PieceVtxCount(i);
        }
    }

    OPEN_DISPS(gfxCtx);

    Matrix_Push();
    Matrix_Translate(ox, oy, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(1.0f / HT_SUB, 1.0f / HT_SUB, 1.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    Gfx_SetupDL_42Opa(gfxCtx);
    HtMap_SetupShapes(&POLY_OPA_DISP);
    b.gfx = &POLY_OPA_DISP;
    b.vtx = Graph_Alloc(gfxCtx, MAX(3, count) * sizeof(Vtx));
    b.n = 0;
    for (i = 0; i < HT_MAP_PIECES; i++) {
        Color_RGBA8 fill, line;

        if ((sHtMapPieces[i].floor != floor) || !HtMap_PieceShown(i)) {
            continue;
        }
        if (i == sCurPiece) {
            fill = HtMap_Rgba(0, 80 + (s32)(120 * pulse), 255 - (s32)(115 * pulse), alpha);
            line = HtMap_Rgba(230, 255, 240, alpha);
        } else if (HtMap_GetVisited() & (1u << i)) {
            fill = HtMap_Rgba(45, 95, 210, alpha);
            line = HtMap_Rgba(200, 220, 255, alpha);
        } else {
            // (shown by the map, not yet been in)
            fill = HtMap_Rgba(22, 36, 96, alpha);
            line = HtMap_Rgba(90, 110, 170, alpha);
        }
        HtMap_DrawPiece(&b, i, box[0], box[1], k * HT_SUB, 0.35f * HT_SUB, fill, line);
    }
    HtBatch_Flush(&b);

    if (HtMap_HasCompass()) {
        // the compass: chests not yet opened, the boss's skull (beating, as the vanilla one in boss rooms)
        Vtx* v = Graph_Alloc(gfxCtx, (HT_MAP_CHESTS + 1) * 4 * sizeof(Vtx));
        s32 n = 0;
        f32 beat = 1.0f + 0.25f * (Math_SinS(play->state.frames * 0x800) + 1.0f);

        Gfx_SetupDL_42Opa(gfxCtx);
        gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, alpha);
        for (i = 0; i <= HT_MAP_CHESTS; i++) {
            f32 cx, cy, hs;
            s32 c;

            if (i < HT_MAP_CHESTS) {
                if ((sHtMapPieces[sHtMapChests[i][4]].floor != floor) || Flags_GetTreasure(play, sHtMapChests[i][3])) {
                    continue;
                }
                cx = (sHtMapChests[i][0] - box[0]) * k * HT_SUB;
                cy = -(sHtMapChests[i][2] - box[1]) * k * HT_SUB;
                hs = 3.0f * HT_SUB;
            } else {
                if ((HT_MAP_BOSS_FLOOR != floor) || (HT_MAP_BOSS_PIECE < 0)) {
                    continue;
                }
                cx = (HT_MAP_BOSS_X - box[0]) * k * HT_SUB;
                cy = -(HT_MAP_BOSS_Z - box[1]) * k * HT_SUB;
                hs = 3.5f * HT_SUB * beat;
            }
            for (c = 0; c < 4; c++) {
                Vtx* q = &v[n * 4 + c];

                q->v.ob[0] = (s16)(cx + ((c & 1) ? hs : -hs));
                q->v.ob[1] = (s16)(cy + ((c & 2) ? -hs : hs));
                q->v.ob[2] = 0;
                q->v.flag = 0;
                q->v.tc[0] = (c & 1) ? (8 << 5) : 0;
                q->v.tc[1] = (c & 2) ? (8 << 5) : 0;
                q->v.cn[0] = q->v.cn[1] = q->v.cn[2] = 255;
                q->v.cn[3] = alpha;
            }
            gDPPipeSync(POLY_OPA_DISP++);
            if (i < HT_MAP_CHESTS) {
                gDPLoadTextureBlock(POLY_OPA_DISP++, gMapChestIconTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
            } else {
                gDPLoadTextureBlock(POLY_OPA_DISP++, gMapBossIconTex, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 8, 0,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
            }
            gSPVertex(POLY_OPA_DISP++, &v[n * 4], 4, 0);
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
            n++;
        }
    }

    Matrix_Pop();
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Gfx_SetupDL_42Opa(gfxCtx);

    CLOSE_DISPS(gfxCtx);
}
