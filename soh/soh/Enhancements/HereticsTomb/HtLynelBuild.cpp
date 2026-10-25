// Heretics Tomb: the Lynel built at run time from the player's own game data. See HtLynelBuild.h.
//
// (Also compiled on its own, with HT_LB_HARNESS defined, by the tools: the same code reading the game data from an
// unpacked archive, to check what it builds against the bake it replaces.)

#include <vector>
#include <array>
#include <memory>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>

#ifdef HT_LB_HARNESS
#include "HtLynelHarness.h"
#else
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/Texture.h>
#include "HtLynelBuild.h"
extern "C" {
#include "macros.h"
#include "functions.h"
void SkelAnime_GetFrameData(AnimationHeader* animation, s32 frame, s32 limbCount, Vec3s* frameTable); // z_skelanime.c
}
#endif

#include "HtLynelRecipe.h"
#include "HtLynelAnimRecipe.h"

namespace {

// ---- matrices (double, as the bake's numpy) ----
struct M4 {
    double m[4][4];
};
M4 I4() {
    M4 r{};
    for (int i = 0; i < 4; i++) r.m[i][i] = 1.0;
    return r;
}
M4 Mul(const M4& a, const M4& b) {
    M4 r{};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0.0;
            for (int k = 0; k < 4; k++) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}
M4 Tr(double x, double y, double z) {
    M4 r = I4();
    r.m[0][3] = x; r.m[1][3] = y; r.m[2][3] = z;
    return r;
}
M4 Sc(double s) {
    M4 r = I4();
    r.m[0][0] = r.m[1][1] = r.m[2][2] = s;
    return r;
}
M4 Rot(char axis, double a) {
    double c = std::cos(a), s = std::sin(a);
    M4 m = I4();
    if (axis == 'x') { m.m[1][1] = c; m.m[1][2] = -s; m.m[2][1] = s; m.m[2][2] = c; }
    if (axis == 'y') { m.m[0][0] = c; m.m[0][2] = s; m.m[2][0] = -s; m.m[2][2] = c; }
    if (axis == 'z') { m.m[0][0] = c; m.m[0][1] = -s; m.m[1][0] = s; m.m[1][1] = c; }
    return m;
}
M4 RxDeg(double deg) { return Rot('x', deg * M_PI / 180.0); }
M4 Inv(const M4& in) {
    double a[4][8];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++) a[i][j] = (j < 4) ? in.m[i][j] : ((j - 4) == i ? 1.0 : 0.0);
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++)
            if (std::fabs(a[r][c]) > std::fabs(a[p][c])) p = r;
        if (p != c)
            for (int j = 0; j < 8; j++) std::swap(a[p][j], a[c][j]);
        double d = a[c][c];
        for (int j = 0; j < 8; j++) a[c][j] /= d;
        for (int r = 0; r < 4; r++) {
            if (r == c) continue;
            double f = a[r][c];
            if (f != 0.0)
                for (int j = 0; j < 8; j++) a[r][j] -= f * a[c][j];
        }
    }
    M4 o;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) o.m[i][j] = a[i][j + 4];
    return o;
}
void Apply(const M4& m, const double v[3], double out[3]) {
    for (int i = 0; i < 3; i++) out[i] = m.m[i][0] * v[0] + m.m[i][1] * v[1] + m.m[i][2] * v[2] + m.m[i][3];
}
M4 TrV(const double p[3], double k = 1.0) { return Tr(k * p[0], k * p[1], k * p[2]); }
void Pos(const M4& m, double p[3]) { p[0] = m.m[0][3]; p[1] = m.m[1][3]; p[2] = m.m[2][3]; }

// ---- the game data ----
struct Limb {
    double pos[3];
    int child, sibling;
};

#ifndef HT_LB_HARNESS
std::shared_ptr<Ship::IResource> Load(const char* path) {
    // (exact: the vanilla asset, never an alternate one's)
    return Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(std::string(path), true);
}
bool LoadSkeleton(const char* path, std::vector<Limb>& out) {
    auto res = Load(path);
    if (res == nullptr) return false;
    SkeletonHeader* sk = (SkeletonHeader*)res->GetRawPointer();
    if (sk == nullptr) return false;
    out.clear();
    for (int i = 0; i < sk->limbCount; i++) {
        StandardLimb* l = (StandardLimb*)sk->segment[i];
        out.push_back({ { (double)l->jointPos.x, (double)l->jointPos.y, (double)l->jointPos.z }, l->child, l->sibling });
    }
    return true;
}
// the joint table of one frame: [0] the root's translation, [1 + i] limb i's rotation
bool LoadFrame(const char* path, int frame, int joints, std::vector<Vec3s>& out) {
    auto res = Load(path);
    if (res == nullptr) return false;
    AnimationHeader* a = (AnimationHeader*)res->GetRawPointer();
    if ((a == nullptr) || (frame >= a->common.frameCount)) return false;
    out.resize(joints);
    SkelAnime_GetFrameData(a, frame, joints, out.data());
    return true;
}
int FrameCount(const char* path) {
    auto res = Load(path);
    if (res == nullptr) return 0;
    AnimationHeader* a = (AnimationHeader*)res->GetRawPointer();
    return (a != nullptr) ? a->common.frameCount : 0;
}
const Vtx* LoadVtx(const char* path, size_t* count) {
    auto res = Load(path);
    if (res == nullptr) return nullptr;
    *count = res->GetPointerSize() / sizeof(Vtx);
    return (const Vtx*)res->GetRawPointer();
}
struct TexImg {
    int type, w, h;
    const uint8_t* data;
    size_t size;
};
bool LoadTex(const char* path, TexImg& t) {
    auto res = Load(path);
    if (res == nullptr) return false;
    auto tex = std::static_pointer_cast<Fast::Texture>(res);
    t.type = (int)tex->Type;
    t.w = tex->Width;
    t.h = tex->Height;
    t.data = tex->ImageData;
    t.size = tex->ImageDataSize;
    return (t.data != nullptr) && (t.w > 0) && (t.h > 0);
}
#else
#include "HtLynelHarnessData.inc"
#endif

std::vector<M4> Pose(const std::vector<Limb>& L, const std::vector<Vec3s>& fv) {
    std::vector<M4> mats(L.size());
    struct W {
        static void walk(const std::vector<Limb>& L, const std::vector<Vec3s>& fv, std::vector<M4>& mats, int i,
                         const M4& parent) {
            double p[3];
            if (i == 0) { p[0] = fv[0].x; p[1] = fv[0].y; p[2] = fv[0].z; }
            else { p[0] = L[i].pos[0]; p[1] = L[i].pos[1]; p[2] = L[i].pos[2]; }
            const Vec3s& r = fv[1 + i];
            const double k = M_PI / 0x8000;
            M4 m = Mul(Mul(Mul(Mul(parent, TrV(p)), Rot('z', r.z * k)), Rot('y', r.y * k)), Rot('x', r.x * k));
            mats[i] = m;
            if (L[i].child != 0xFF) walk(L, fv, mats, L[i].child, m);
            if (L[i].sibling != 0xFF) walk(L, fv, mats, L[i].sibling, parent);
        }
    };
    W::walk(L, fv, mats, 0, I4());
    return mats;
}

bool PoseOf(const char* skel, const char* anim, int frame, std::vector<M4>& out, std::vector<Limb>* limbs = nullptr) {
    std::vector<Limb> L;
    std::vector<Vec3s> fv;
    if (!LoadSkeleton(skel, L)) return false;
    if (!LoadFrame(anim, frame, (int)L.size() + 1, fv)) return false;
    out = Pose(L, fv);
    if (limbs != nullptr) *limbs = L;
    return true;
}

// ---- the rig (lynel.py, lynel2.py, lynel_export.py): where the borrowed limbs sit on the Lynel ----
const double ATTACH[3] = { 0.0, 10260.0, 6870.0 };
const double LEAN = 30.0, SCALE = 1.55, ARM_K = 0.55, HEAD_K = 0.72, SCIM = 3.0;
const char* const DORF_SKEL = "objects/object_ganon/gGanondorfSkel";
const char* const DORF_ANIM = "objects/object_ganon_anime1/gGanondorfBlockAnim";
const char* const GANON_SKEL = "objects/object_ganon2/gGanonSkel";
const char* const GANON_WALK = "objects/object_ganon2/gGanonWalkAnim";

struct Rig {
    std::vector<M4> base[6];   // the matrices placing each base's limbs (0 Ganondorf, 1/2 the arms, 3 the head, 4/5 the
                               // scimitars)
    M4 rest[HT_LYNEL_PARTS];   // each part's frame in the modelled pose
    M4 W, M[2], H;
    std::vector<M4> gm;        // Ganon's modelled pose
};

bool BuildRig(Rig& r) {
    std::vector<M4> fkd;
    if (!PoseOf(DORF_SKEL, DORF_ANIM, 0, fkd) || !PoseOf(GANON_SKEL, GANON_WALK, 0, r.gm)) return false;
    if (fkd.size() < 7 || r.gm.size() < 15) return false;
    double waist[3];
    Pos(fkd[1], waist);
    r.W = Mul(Mul(Mul(Tr(ATTACH[0], ATTACH[1], ATTACH[2]), RxDeg(LEAN)), Sc(SCALE)), TrV(waist, -1.0));
    const int dsh[2] = { 2, 6 }, gsh[2] = { 2, 8 };
    for (int s = 0; s < 2; s++) {
        double a[3], b[3];
        Pos(Mul(r.W, fkd[dsh[s]]), a);
        Pos(r.gm[gsh[s]], b);
        r.M[s] = Mul(Mul(TrV(a), Sc(SCALE * ARM_K)), TrV(b, -1.0));
    }
    double hj[3], neck[3];
    const double nk[3] = { 0.0, 2750.0, -700.0 + 150.0 };
    Apply(r.W, nk, hj);
    Pos(r.gm[14], neck);
    r.H = Mul(Mul(Mul(TrV(hj), RxDeg(LEAN * 0.35)), Sc(SCALE * HEAD_K)), TrV(neck, -1.0));
    M4 Rs = I4();
    // (columns (0,1,0), (0,0,-1), (-1,0,0))
    Rs.m[0][0] = 0; Rs.m[1][0] = 1; Rs.m[2][0] = 0;
    Rs.m[0][1] = 0; Rs.m[1][1] = 0; Rs.m[2][1] = -1;
    Rs.m[0][2] = -1; Rs.m[1][2] = 0; Rs.m[2][2] = 0;
    for (auto& m : fkd) r.base[0].push_back(Mul(r.W, m));
    for (auto& m : r.gm) {
        r.base[1].push_back(Mul(r.M[0], m));
        r.base[2].push_back(Mul(r.M[1], m));
        r.base[3].push_back(Mul(r.H, m));
    }
    const int swl[2] = { 6, 12 };
    for (int s = 0; s < 2; s++) {
        M4 Ms = Mul(Mul(Mul(r.base[1 + s][swl[s]], Rs), Sc(SCIM)), Tr(-400, 0, -110));
        r.base[4 + s].push_back(Mul(Mul(Mul(Ms, Tr(400, 0, 110)), Rot('y', M_PI)), Tr(-400, 0, -110)));
    }
    r.rest[0] = r.W;
    r.rest[1] = r.base[3][14];
    const int limb[8] = { 2, 3, 4, 5, 8, 9, 10, 11 };
    for (int k = 0; k < 8; k++) r.rest[2 + k] = r.base[1 + (k / 4)][limb[k]];
    return true;
}

// ---- the model ----
struct OutGroup {
    std::vector<Vtx> vtx;
    std::vector<HtLynelOp> ops;
};

struct Corner {
    double p[3];
    double uv[2];
    bool hasUv;
};

int RoundI(double v) { return (int)std::nearbyint(v); }

// lynel_export.build_ops: each batch of ten triangles loads its corners part by part (each part's matrix), then
// draws them
void BuildOps(const std::vector<std::array<int, 9>>& vs, OutGroup& g) {
    size_t ntri = vs.size() / 3;
    for (size_t b = 0; b < ntri; b += 10) {
        size_t nb = std::min<size_t>(10, ntri - b);
        std::vector<std::array<int, 3>> ents;   // (part, tri, corner)
        for (size_t k = 0; k < nb; k++)
            for (int c = 0; c < 3; c++) ents.push_back({ vs[(b + k) * 3 + c][8], (int)k, c });
        std::sort(ents.begin(), ents.end());
        std::map<std::pair<int, int>, int> slot;
        size_t i = 0;
        while (i < ents.size()) {
            int part = ents[i][0];
            size_t j = i;
            while (j < ents.size() && ents[j][0] == part) j++;
            g.ops.push_back({ 0, (u16)part, 0, 0 });
            g.ops.push_back({ 1, (u16)g.vtx.size(), (u16)(j - i), (u16)i });
            for (size_t e = i; e < j; e++) {
                const auto& v = vs[(b + ents[e][1]) * 3 + ents[e][2]];
                Vtx out;
                memset(&out, 0, sizeof(out));
                out.v.ob[0] = v[0]; out.v.ob[1] = v[1]; out.v.ob[2] = v[2];
                out.v.tc[0] = v[3]; out.v.tc[1] = v[4];
                out.v.cn[0] = v[5]; out.v.cn[1] = v[6]; out.v.cn[2] = v[7]; out.v.cn[3] = 255;
                g.vtx.push_back(out);
                slot[{ ents[e][1], ents[e][2] }] = (int)e;
            }
            i = j;
        }
        for (size_t k = 0; k < nb; k++)
            g.ops.push_back({ 2, (u16)slot[{ (int)k, 0 }], (u16)slot[{ (int)k, 1 }], (u16)slot[{ (int)k, 2 }] });
    }
    g.ops.push_back({ 3, 0, 0, 0 });
}

bool BuildModel(const Rig& rig, std::vector<OutGroup>& groups, OutGroup& halo) {
    // the vanilla vertex files
    std::vector<const Vtx*> files(HT_LR_FILES);
    std::vector<size_t> counts(HT_LR_FILES);
    for (int f = 0; f < HT_LR_FILES; f++) {
        files[f] = LoadVtx(sHtLrFiles[f], &counts[f]);
        if (files[f] == nullptr) return false;
    }
    auto refWorld = [&](const HtLrRef& r, double out[3]) -> bool {
        if (r.file >= HT_LR_FILES || r.index >= counts[r.file] || r.base > 5 || r.limb >= rig.base[r.base].size())
            return false;
        const Vtx& v = files[r.file][r.index];
        double p[3] = { (double)v.v.ob[0], (double)v.v.ob[1], (double)v.v.ob[2] };
        Apply(rig.base[r.base][r.limb], p, out);
        return true;
    };
    // the snout's squash: about its corners' lowest z
    double z0 = 1e300;
    for (int i = 0; i < HT_LR_REFS; i++) {
        if (!sHtLrRefs[i].squash) continue;
        double p[3];
        if (!refWorld(sHtLrRefs[i], p)) return false;
        z0 = std::min(z0, p[2]);
    }
    M4 restInv[HT_LYNEL_PARTS];
    for (int i = 0; i < HT_LYNEL_PARTS; i++) restInv[i] = Inv(rig.rest[i]);
    double light[3] = { 0.4, 0.8, 0.6 };
    double ln = std::sqrt(light[0] * light[0] + light[1] * light[1] + light[2] * light[2]);
    for (double& x : light) x /= ln;

    std::vector<std::vector<std::array<int, 9>>> gv(HT_LR_GROUPS);
    std::vector<std::array<int, 9>> hv;
    for (int t = 0; t < HT_LR_TRIS; t++) {
        const HtLrTri& tr = sHtLrTris[t];
        Corner c[3];
        for (int k = 0; k < 3; k++) {
            u16 ci = tr.corner[k];
            if (ci & 0x8000) {
                const f64* p = sHtLrPts[ci & 0x7FFF];
                c[k].p[0] = p[0]; c[k].p[1] = p[1]; c[k].p[2] = p[2];
                c[k].uv[0] = tr.uv[k][0]; c[k].uv[1] = tr.uv[k][1];
                c[k].hasUv = tr.hasUv;
            } else {
                const HtLrRef& r = sHtLrRefs[ci];
                if (!refWorld(r, c[k].p)) return false;
                if (r.squash) c[k].p[2] = z0 + (c[k].p[2] - z0) * 0.35;
                const Vtx& v = files[r.file][r.index];
                c[k].uv[0] = v.v.tc[0] / 32.0 * (r.sclS / 65536.0);
                c[k].uv[1] = v.v.tc[1] / 32.0 * (r.sclT / 65536.0);
                c[k].hasUv = true;
            }
        }
        double col[3][3];
        if (tr.mode == 0) {
            double a[3], b[3], n[3];
            for (int i = 0; i < 3; i++) { a[i] = c[1].p[i] - c[0].p[i]; b[i] = c[2].p[i] - c[0].p[i]; }
            n[0] = a[1] * b[2] - a[2] * b[1]; n[1] = a[2] * b[0] - a[0] * b[2]; n[2] = a[0] * b[1] - a[1] * b[0];
            double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            double shade = (nl > 0) ? 0.35 + 0.65 * std::fabs((n[0] / nl) * light[0] + (n[1] / nl) * light[1] +
                                                              (n[2] / nl) * light[2])
                                    : 0.7;
            for (int k = 0; k < 3; k++)
                for (int i = 0; i < 3; i++) col[k][i] = tr.rgb[i] * shade;
        } else if (tr.mode == 1) {
            for (int k = 0; k < 3; k++)
                for (int i = 0; i < 3; i++) col[k][i] = tr.cols[k][i];
        } else {
            for (int k = 0; k < 3; k++)
                for (int i = 0; i < 3; i++) col[k][i] = 1.0;
        }
        bool textured = (tr.group == 255) || (sHtLrGroupTex[tr.group] >= 0);
        for (int k = 0; k < 3; k++) {
            int part = tr.parts[k];
            double lp[3];
            Apply(restInv[part], c[k].p, lp);
            int s_ = 0, t_ = 0;
            if (textured) {
                s_ = std::max(-32768, std::min(32767, RoundI(c[k].uv[0] * 32)));
                t_ = std::max(-32768, std::min(32767, RoundI(c[k].uv[1] * 32)));
            }
            std::array<int, 9> v = { RoundI(lp[0]), RoundI(lp[1]), RoundI(lp[2]), s_, t_, 0, 0, 0, part };
            for (int i = 0; i < 3; i++) v[5 + i] = std::max(0, std::min(255, RoundI(col[k][i] * 255)));
            if (tr.group == 255) hv.push_back(v);
            else gv[tr.group].push_back(v);
        }
    }
    groups.assign(HT_LR_GROUPS, OutGroup());
    for (int g = 0; g < HT_LR_GROUPS; g++) BuildOps(gv[g], groups[g]);
    BuildOps(hv, halo);
    return true;
}

// ---- the animations ----
const char* const HORSE_SKEL = "objects/object_horse_ganon/gHorseGanonSkel";
const char* const HORSE_IDLE = "objects/object_horse_ganon/gHorseGanonIdleAnim";
const char* const GANON_GUARD = "objects/object_ganon2/gGanonGuardIdleAnim";
const char* const EPONA_SKEL = "objects/object_horse/gEponaSkel";

// the upper body's: P = T(-A) . Y . V (HtLynelAnimRecipe.h)
bool BuildAnims(const Rig& rig, std::vector<std::vector<f32>>& out) {
    std::vector<M4> hm;
    if (!PoseOf(HORSE_SKEL, HORSE_IDLE, 0, hm) || hm.size() < 9) return false;
    double a[3];
    Pos(hm[8], a);
    const M4 TA = Tr(-a[0] * 1.6 * 1.5, -a[1] * 1.6, -a[2] * 1.6);
    std::vector<std::vector<M4>> guard;
    int gfc = FrameCount(GANON_GUARD);
    for (int f = 0; f < gfc; f++) {
        std::vector<M4> g;
        if (!PoseOf(GANON_SKEL, GANON_GUARD, f, g)) return false;
        guard.push_back(g);
    }
    static const int partSide[HT_LYNEL_PARTS] = { -1, -1, 0, 0, 0, 0, 1, 1, 1, 1 };
    static const int partLimb[HT_LYNEL_PARTS] = { -1, -1, 2, 3, 4, 5, 8, 9, 10, 11 };
    out.assign(LYNEL_ANIM_MAX, {});
    for (int an = 0; an < LYNEL_ANIM_MAX; an++) {
        const HtLarAnim& A = sHtLarAnims[an];
        if (A.guard && A.frames > (int)guard.size()) return false;
        out[an].resize((size_t)A.frames * HT_LYNEL_PARTS * 12);
        for (int f = 0; f < A.frames; f++) {
            for (int p = 0; p < HT_LYNEL_PARTS; p++) {
                M4 V = rig.rest[p];
                if (A.guard && partSide[p] >= 0) {
                    const std::vector<M4>& G = guard[f];
                    V = Mul(Mul(Mul(rig.M[partSide[p]], rig.gm[1]), Inv(G[1])), G[partLimb[p]]);
                }
                M4 Y = I4();
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 4; c++) Y.m[r][c] = A.y[f][p][r * 4 + c];
                M4 P = Mul(Mul(TA, Y), V);
                f32* dst = &out[an][((size_t)f * HT_LYNEL_PARTS + p) * 12];
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 4; c++) dst[r * 4 + c] = (f32)P.m[r][c];
            }
        }
    }
    return true;
}

s16 Wrap(int v) { return (s16)(((v + 0x8000) & 0xFFFF) - 0x8000); }

// the horse's own poses: its idle's first frame and our change of it
bool HorseIdle(std::vector<Vec3s>& idle) {
    std::vector<Limb> L;
    if (!LoadSkeleton(HORSE_SKEL, L) || ((int)L.size() + 1 != LYNEL_CROUCH_JOINTS)) return false;
    return LoadFrame(HORSE_IDLE, 0, LYNEL_CROUCH_JOINTS, idle);
}
void ApplyDelta(const std::vector<Vec3s>& idle, const Vec3s (*d)[LYNEL_CROUCH_JOINTS], int frames,
                std::vector<Vec3s>& out) {
    out.resize((size_t)frames * LYNEL_CROUCH_JOINTS);
    for (int f = 0; f < frames; f++)
        for (int j = 0; j < LYNEL_CROUCH_JOINTS; j++) {
            Vec3s& o = out[(size_t)f * LYNEL_CROUCH_JOINTS + j];
            o.x = Wrap(idle[j].x + d[f][j].x);
            o.y = Wrap(idle[j].y + d[f][j].y);
            o.z = Wrap(idle[j].z + d[f][j].z);
        }
}
// Epona's animation on Ganon's horse: her limbs on his (crouch.py jump, ht_horse_export.py retarget), the root
// raised as she rises
bool Retarget(const std::vector<Vec3s>& idle, const char* anim, std::vector<Vec3s>& out, int* frames) {
    const int ne = (int)sizeof(sHtLarEponaToGanon) + 1;
    int fc = FrameCount(anim);
    if (fc <= 0) return false;
    out.resize((size_t)fc * LYNEL_CROUCH_JOINTS);
    std::vector<Vec3s> e;
    for (int f = 0; f < fc; f++) {
        if (!LoadFrame(anim, f, ne, e)) return false;
        Vec3s* g = &out[(size_t)f * LYNEL_CROUCH_JOINTS];
        for (int j = 0; j < LYNEL_CROUCH_JOINTS; j++) g[j] = idle[j];
        g[0].x = 0;
        g[0].y = (s16)std::nearbyint(idle[0].y + (e[0].y - 4470) * 1.153);
        g[0].z = idle[0].z;
        for (int ei = 0; ei < (int)sizeof(sHtLarEponaToGanon); ei++) g[sHtLarEponaToGanon[ei] + 1] = e[ei + 1];
    }
    *frames = fc;
    return true;
}

// ---- the textures (tex.py): the vanilla image decoded as the bake read it, recoloured, written RGBA16 ----
struct Img {
    int w = 0, h = 0;
    std::vector<float> px;   // rgb, as the bake's float32 images
};
double Rgba16C(uint16_t v, int sh) { return ((v >> sh) & 31) / 31.0; }
bool Decode(const char* path, const char* tlut, Img& out) {
    TexImg t, p;
    if (!LoadTex(path, t)) return false;
    bool hasPal = (tlut != nullptr) && LoadTex(tlut, p);
    out.w = t.w; out.h = t.h; out.px.assign((size_t)t.w * t.h * 3, 0.0f);
    for (int v = 0; v < t.h; v++)
        for (int u = 0; u < t.w; u++) {
            size_t i = (size_t)v * t.w + u;
            double c[3] = { 0.5, 0.5, 0.5 };
            auto nib = [&](size_t k) -> int { return (t.data[k / 2] >> ((k % 2 == 0) ? 4 : 0)) & 15; };
            switch (t.type) {
                case 1: for (int k = 0; k < 3; k++) c[k] = t.data[4 * i + k] / 255.0; break;
                case 2: {
                    uint16_t x = (uint16_t)((t.data[2 * i] << 8) | t.data[2 * i + 1]);
                    c[0] = Rgba16C(x, 11); c[1] = Rgba16C(x, 6); c[2] = Rgba16C(x, 1);
                    break;
                }
                case 3: case 4: {
                    int ci = (t.type == 3) ? nib(i) : t.data[i];
                    if (!hasPal) { c[0] = c[1] = c[2] = ci / ((t.type == 3) ? 15.0 : 255.0); break; }
                    size_t j = (size_t)ci;
                    if (2 * j + 2 > p.size) break;
                    uint16_t x = (uint16_t)((p.data[2 * j] << 8) | p.data[2 * j + 1]);
                    c[0] = Rgba16C(x, 11); c[1] = Rgba16C(x, 6); c[2] = Rgba16C(x, 1);
                    break;
                }
                case 5: c[0] = c[1] = c[2] = nib(i) / 15.0; break;
                case 6: c[0] = c[1] = c[2] = t.data[i] / 255.0; break;
                case 7: c[0] = c[1] = c[2] = (nib(i) >> 1) / 7.0; break;
                case 8: c[0] = c[1] = c[2] = (t.data[i] >> 4) / 15.0; break;
                case 9: c[0] = c[1] = c[2] = t.data[2 * i] / 255.0; break;
                default: break;
            }
            for (int k = 0; k < 3; k++) out.px[i * 3 + k] = (float)c[k];
        }
    return true;
}
// matplotlib's rgb_to_hsv / hsv_to_rgb (float32), as the bake's recolours used them
void RgbToHsv(const float* in, float* o) {
    float r = std::min(1.0f, std::max(0.0f, in[0])), g = std::min(1.0f, std::max(0.0f, in[1])),
          b = std::min(1.0f, std::max(0.0f, in[2]));
    float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    float delta = mx - mn;
    float s = (mx > 0) ? delta / mx : 0.0f;
    float h = 0.0f;
    if (delta > 0) {
        if (r == mx) h = (g - b) / delta;
        if (g == mx) h = 2.0f + (b - r) / delta;
        if (b == mx) h = 4.0f + (r - g) / delta;
    }
    h = h / 6.0f;
    h = h - std::floor(h);
    o[0] = h; o[1] = s; o[2] = mx;
}
void HsvToRgb(const float* in, float* o) {
    float h = in[0], s = in[1], v = in[2];
    int i = (int)(h * 6.0f);
    float f = (h * 6.0f) - (float)i;
    float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
    float r = 0, g = 0, b = 0;
    switch (((i % 6) + 6) % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    if (i == 1) { r = q; g = v; b = p; }
    if (s == 0) { r = g = b = v; }
    o[0] = r; o[1] = g; o[2] = b;
}
// the recolours (lynel2.py): hue, saturation and value scale, and whether only the coloured parts change
void Recolour(Img& im, int rule) {
    float hue, sat, val;
    bool sel = false;
    switch (rule) {
        case 1: hue = 0.0f; sat = 0.35f; val = 0.45f; break;   // the hide: near-black with a red cast
        case 2: hue = 0.0f; sat = 0.6f; val = 0.35f; break;    // the mouth: dark
        case 3: hue = 0.78f; sat = 1.1f; val = 1.0f; break;    // purple
        case 4: hue = 0.99f; sat = 1.0f; val = 0.85f; sel = true; break;   // crimson (the coloured parts)
        default: return;
    }
    for (size_t i = 0; i < im.px.size(); i += 3) {
        float h[3];
        RgbToHsv(&im.px[i], h);
        if (!sel || (h[1] > 0.12f)) {
            h[0] = hue;
            h[1] = std::min(1.0f, std::max(0.0f, h[1] * sat));
            h[2] = std::min(1.0f, std::max(0.0f, h[2] * val));
        }
        HsvToRgb(h, &im.px[i]);
    }
}
// RGBA16 (big-endian, as the game's own), opaque
std::vector<uint8_t> ToRgba16(const Img& im) {
    std::vector<uint8_t> out((size_t)im.w * im.h * 2);
    for (size_t i = 0; i < (size_t)im.w * im.h; i++) {
        int c[3];
        for (int k = 0; k < 3; k++)
            c[k] = std::max(0, std::min(31, (int)std::nearbyint(im.px[i * 3 + k] * 31.0f)));
        uint16_t v = (uint16_t)((c[0] << 11) | (c[1] << 6) | (c[2] << 1) | 1);
        out[2 * i] = v >> 8;
        out[2 * i + 1] = v & 0xFF;
    }
    return out;
}
// the horse's eye shut (lynel_export.closed_eye): its socket's dark hide all over, a lid, the lashes' line
const char* const HORSE_EYE = "objects/object_horse_ganon/gHorseGanonEyeTex";
bool ClosedEye(Img& out) {
    Img img;
    if (!Decode(HORSE_EYE, nullptr, img) || img.w != 16 || img.h != 16) return false;
    std::vector<float> lum(256), sorted;
    for (int i = 0; i < 256; i++) lum[i] = (img.px[i * 3] + img.px[i * 3 + 1] + img.px[i * 3 + 2]) / 3.0f;
    sorted = lum;
    std::sort(sorted.begin(), sorted.end());
    double idx = 0.35 * 255;
    int lo = (int)std::floor(idx);
    double pct = sorted[lo] + (idx - lo) * (sorted[lo + 1] - sorted[lo]);
    double hide[3] = { 0, 0, 0 };
    int n = 0;
    for (int i = 0; i < 256; i++)
        if (lum[i] <= pct) {
            for (int k = 0; k < 3; k++) hide[k] += img.px[i * 3 + k];
            n++;
        }
    float hf[3];
    for (int k = 0; k < 3; k++) hf[k] = (float)(hide[k] / n);
    out.w = out.h = 16;
    out.px.assign(256 * 3, 0.0f);
    const double lidc[3] = { 0.10, 0.07, 0.08 };
    for (int x = 0; x < 16; x++) {
        double yl = 8.5 + 1.6 * std::sin(M_PI * x / 15);
        for (int y = 0; y < 16; y++) {
            float* o = &out.px[(y * 16 + x) * 3];
            for (int k = 0; k < 3; k++) o[k] = hf[k];
            double d = y - yl;
            if (-4.5 < d && d < 0) {
                for (int k = 0; k < 3; k++)
                    o[k] = (float)(hf[k] + lidc[k] * (0.6 + 0.6 * (1 + d / 4.5) * std::sin(M_PI * x / 15)));
            } else if (0 <= d && d < 1.0) {
                o[0] = 0.01f; o[1] = 0.0f; o[2] = 0.0f;
            }
            for (int k = 0; k < 3; k++) o[k] = std::min(1.0f, std::max(0.0f, o[k]));
        }
    }
    return true;
}
// half open (waking): the lid over its top half, the glow showing below
bool HalfEye(Img& out) {
    Img op, sh;
    if (!Decode(HORSE_EYE, nullptr, op) || !ClosedEye(sh) || op.w != 16 || op.h != 16) return false;
    out = op;
    for (int x = 0; x < 16; x++) {
        double yl = 8.5 + 1.6 * std::sin(M_PI * x / 15) - 2.0;
        for (int y = 0; y < 16; y++) {
            float* o = &out.px[(y * 16 + x) * 3];
            if (y < yl - 1.0) {
                int sy = std::max(0, std::min(15, (int)(y + 2.0)));
                for (int k = 0; k < 3; k++) o[k] = sh.px[(sy * 16 + x) * 3 + k];
            } else if (y < yl) {
                o[0] = 0.01f; o[1] = 0.0f; o[2] = 0.0f;
            }
            for (int k = 0; k < 3; k++) o[k] = std::min(1.0f, std::max(0.0f, o[k]));
        }
    }
    return true;
}
const char* const HORSE_HAIR = "objects/object_horse_ganon/gHorseGanonHeadHairTex";

} // namespace

#ifndef HT_LB_HARNESS
// ---- the game's side ----
namespace {
std::vector<OutGroup> sGroups;
OutGroup sHaloGroup;
std::vector<HtLynelGroup> sGroupTab;
std::vector<std::vector<Gfx>> sMaterials;
std::vector<std::vector<uint8_t>> sTextures;
std::vector<std::vector<f32>> sAnims;
std::vector<Vec3s> sStandUp, sStuckIn, sStuckLoop, sJump, sPush, sKick, sCrouch;
std::vector<Vec3s> sHorseRefuse, sHorseJumpLow, sHorseJumpHigh;
std::vector<uint8_t> sHairTex, sEyeClosedTex, sEyeHalfTex;
s32 sBuilt = 0;   // 0 not yet, 1 built, -1 failed
s32 sHorseBuilt = 0;
const char* const HALO_MAT = "__OTR__scenes/shared/heretics_tomb_scene/ht_lynel_upper_halo_mat";
const char* const CHEST_TEX = "__OTR__scenes/shared/heretics_tomb_scene/ht_lynel_upper_tex_0";

// a group's material (lynel_export.mat_xml / mat_solid_xml): shaded, unlit; textured with the group's RGBA16 image
std::vector<Gfx> Material(const void* tex, int w, int h, bool clamp) {
    std::vector<Gfx> m(32);
    Gfx* g = m.data();
    gDPPipeSync(g++);
    if (tex != nullptr) {
        gDPSetCombineLERP(g++, TEXEL0, 0, SHADE, 0, 0, 0, 0, 1, 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);
    } else {
        gDPSetCombineLERP(g++, 0, 0, 0, SHADE, 0, 0, 0, 1, 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);
    }
    gSPSetGeometryMode(g++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    gSPClearGeometryMode(g++, G_CULL_BACK | G_CULL_FRONT | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetOtherMode(g++, G_SETOTHERMODE_H, 4, 20,
                    G_AD_NOISE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TL_TILE | G_TD_CLAMP |
                        G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE);
    gSPSetOtherMode(g++, G_SETOTHERMODE_L, 0, 32, G_AC_NONE | G_ZS_PIXEL | G_RM_FOG_SHADE_A | G_RM_AA_ZB_OPA_SURF2);
    if (tex != nullptr) {
        gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPSetTextureLUT(g++, G_TT_NONE);
        u32 cm = (clamp ? G_TX_CLAMP : G_TX_WRAP) | G_TX_NOMIRROR;
        int ms = (int)std::lround(std::log2((double)w)), mt = (int)std::lround(std::log2((double)h));
        gDPLoadTextureBlock(g++, tex, G_IM_FMT_RGBA, G_IM_SIZ_16b, w, h, 0, cm, cm, ms, mt, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    }
    gSPEndDisplayList(g++);
    m.resize(g - m.data());
    return m;
}

bool BuildAll() {
    Rig rig;
    if (!BuildRig(rig)) return false;
    if (!BuildModel(rig, sGroups, sHaloGroup)) return false;
    if (!BuildAnims(rig, sAnims)) return false;
    // the textures, and the groups' materials
    sTextures.assign(HT_LR_TEXTURES, {});
    std::vector<int> tw(HT_LR_TEXTURES), th(HT_LR_TEXTURES);
    for (int i = 0; i < HT_LR_TEXTURES; i++) {
        if (sHtLrTex[i].tex == nullptr) continue;   // (our own chest: in the tomb's archive)
        Img im;
        if (!Decode(sHtLrTex[i].tex, sHtLrTex[i].tlut, im)) return false;
        Recolour(im, sHtLrTex[i].rule);
        sTextures[i] = ToRgba16(im);
        tw[i] = im.w;
        th[i] = im.h;
    }
    sMaterials.clear();
    for (int gi = 0; gi < HT_LR_GROUPS; gi++) {
        int t = sHtLrGroupTex[gi];
        if (t < 0) sMaterials.push_back(Material(nullptr, 0, 0, false));
        else if (sHtLrTex[t].tex == nullptr) sMaterials.push_back(Material(CHEST_TEX, 64, 32, true));
        else sMaterials.push_back(Material(sTextures[t].data(), tw[t], th[t], false));
    }
    sGroupTab.clear();
    for (int gi = 0; gi < HT_LR_GROUPS; gi++)
        sGroupTab.push_back({ sMaterials[gi].data(), sGroups[gi].vtx.data(), sGroups[gi].ops.data() });
    gHtLynelGroups = sGroupTab.data();
    gHtLynelGroupCount = (s32)sGroupTab.size();
    gHtLynelHaloGroup = { (Gfx*)HALO_MAT, sHaloGroup.vtx.data(), sHaloGroup.ops.data() };
    for (int a = 0; a < LYNEL_ANIM_MAX; a++)
        gHtLynelAnims[a] = (const f32(*)[HT_LYNEL_PARTS][12])sAnims[a].data();
    // the horse's poses
    std::vector<Vec3s> idle;
    if (!HorseIdle(idle)) return false;
    ApplyDelta(idle, (const Vec3s(*)[LYNEL_CROUCH_JOINTS])sHtLarCrouchPose, 1, sCrouch);
    memcpy(gHtLynelCrouchPose, sCrouch.data(), sizeof(gHtLynelCrouchPose));
    ApplyDelta(idle, sHtLarStandUp, HT_LAR_STANDUP_FRAMES, sStandUp);
    ApplyDelta(idle, sHtLarStuckIn, HT_LAR_STUCKIN_FRAMES, sStuckIn);
    ApplyDelta(idle, sHtLarStuckLoop, HT_LAR_STUCKLOOP_FRAMES, sStuckLoop);
    ApplyDelta(idle, sHtLarPush, HT_LAR_PUSH_FRAMES, sPush);
    ApplyDelta(idle, sHtLarKick, HT_LAR_KICK_FRAMES, sKick);
    int fc;
    if (!Retarget(idle, "objects/object_horse/gEponaJumpingHighAnim", sJump, &fc) || (fc != LYNEL_JUMP_FRAMES))
        return false;
    gHtLynelStandUp = (Vec3s(*)[LYNEL_CROUCH_JOINTS])sStandUp.data();
    gHtLynelStuckIn = (Vec3s(*)[LYNEL_CROUCH_JOINTS])sStuckIn.data();
    gHtLynelStuckLoop = (Vec3s(*)[LYNEL_CROUCH_JOINTS])sStuckLoop.data();
    gHtLynelPush = (Vec3s(*)[LYNEL_CROUCH_JOINTS])sPush.data();
    gHtLynelKick = (Vec3s(*)[LYNEL_CROUCH_JOINTS])sKick.data();
    gHtLynelJump = (Vec3s(*)[LYNEL_CROUCH_JOINTS])sJump.data();
    // the horse's textures: its tail purple, its eye shut and half open
    Img im;
    if (!Decode(HORSE_HAIR, nullptr, im)) return false;
    Recolour(im, 3);
    sHairTex = ToRgba16(im);
    if (!ClosedEye(im)) return false;
    sEyeClosedTex = ToRgba16(im);
    if (!HalfEye(im)) return false;
    sEyeHalfTex = ToRgba16(im);
    gHtLynelHairTex = (u16*)sHairTex.data();
    gHtLynelEyeClosedTex = (u16*)sEyeClosedTex.data();
    gHtLynelEyeHalfTex = (u16*)sEyeHalfTex.data();
    return true;
}
} // namespace

extern "C" {
HtLynelGroup* gHtLynelGroups = nullptr;
s32 gHtLynelGroupCount = 0;
HtLynelGroup gHtLynelHaloGroup = { nullptr, nullptr, nullptr };
const f32 (*gHtLynelAnims[LYNEL_ANIM_MAX])[HT_LYNEL_PARTS][12] = {};
Vec3s gHtLynelCrouchPose[LYNEL_CROUCH_JOINTS];
Vec3s (*gHtLynelStandUp)[LYNEL_CROUCH_JOINTS] = nullptr;
Vec3s (*gHtLynelStuckIn)[LYNEL_CROUCH_JOINTS] = nullptr;
Vec3s (*gHtLynelStuckLoop)[LYNEL_CROUCH_JOINTS] = nullptr;
Vec3s (*gHtLynelJump)[LYNEL_CROUCH_JOINTS] = nullptr;
Vec3s (*gHtLynelPush)[LYNEL_CROUCH_JOINTS] = nullptr;
Vec3s (*gHtLynelKick)[LYNEL_CROUCH_JOINTS] = nullptr;
u16* gHtLynelHairTex = nullptr;
u16* gHtLynelEyeClosedTex = nullptr;
u16* gHtLynelEyeHalfTex = nullptr;
Vec3s* gHtHorseRefuse = nullptr;
Vec3s* gHtHorseJumpLow = nullptr;
Vec3s* gHtHorseJumpHigh = nullptr;
s32 gHtHorseRefuseFrames = 0, gHtHorseJumpLowFrames = 0, gHtHorseJumpHighFrames = 0;

s32 HtLynelBuild_Init(void) {
    if (sBuilt == 0) sBuilt = BuildAll() ? 1 : -1;
    return sBuilt == 1;
}

const void* HtRes_LoadExact(const char* path, size_t* size) {
    static std::vector<std::shared_ptr<Ship::IResource>> sKeep; // (held for the session: the pointers stay good)
    auto res = Load(path);
    if ((res == nullptr) || (res->GetRawPointer() == nullptr)) return nullptr;
    sKeep.push_back(res);
    if (size != nullptr) *size = res->GetPointerSize();
    return res->GetRawPointer();
}

s32 HtLynelBuild_HorseAnims(void) {
    if (sHorseBuilt == 0) {
        std::vector<Vec3s> idle;
        bool ok = HorseIdle(idle) &&
                  Retarget(idle, "objects/object_horse/gEponaRefuseAnim", sHorseRefuse, &gHtHorseRefuseFrames) &&
                  Retarget(idle, "objects/object_horse/gEponaJumpingAnim", sHorseJumpLow, &gHtHorseJumpLowFrames) &&
                  Retarget(idle, "objects/object_horse/gEponaJumpingHighAnim", sHorseJumpHigh, &gHtHorseJumpHighFrames);
        gHtHorseRefuse = sHorseRefuse.data();
        gHtHorseJumpLow = sHorseJumpLow.data();
        gHtHorseJumpHigh = sHorseJumpHigh.data();
        sHorseBuilt = ok ? 1 : -1;
    }
    return sHorseBuilt == 1;
}
}
#endif

#ifdef HT_LB_HARNESS
#include "HtLynelHarnessMain.inc"
#endif
