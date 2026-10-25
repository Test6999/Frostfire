#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb mini-boss: the Frozen Ronin. A tall samurai long dead - now a Stalfos - still clinging to his Ice
// Katana. Stalfos skeleton (no shield; head a little small on a tall frame), the katana held in both hands (the
// empty scabbard on his left hip); guarding, he holds it level across his front in the left hand.
//
// Moves:
//   Quick draw      - from the sheathed guard, a lightning cut (blade out for the cut only, then back in the scabbard).
//   Charged lunge   - hand on the hilt, frost gathering on the scabbard; then a leaping cut that closes the gap.
//                     Open for a moment after landing.
//   Icicle line     - cuts into the floor: small icicles burst up one after another in a line toward Link. One
//                     touch freezes him.
//   Guard           - blocks with his arm raised: swords glance off and he answers with a quick draw.
//                     A bomb knocks the guard aside and stuns him. Arrows (he cuts them out of the air), Deku Nuts,
//                     the hookshot and Din's Fire do nothing.
//   Parry           - swinging at him while he's poised (stalking, not in one of his openings) is often parried with a
//                     ringing clang and answered at once with a counter-cut: strike when he's open (after a cut, after
//                     the lunge's landing, stunned, reeling off Link's shield).
// Phase 2 (half health): faster, two cuts per quick draw, three icicle lines in a fan, and:
//   Flash step      - he bursts into frost mist and reappears beside or behind Link (a puff of mist marks the spot a
//                     moment before) with a quick draw. He does it on his own now and then, and always against a Link
//                     who hides behind his shield too long.
//
// Stands at attention in the middle of the arena and wakes (mini-boss music) when Link comes near or hits him.
// The arena's only enemy: when he shatters the room is clear and the katana drops where he stood.

#define HT_RONIN_LIMBS 0x3D // STALFOS_LIMB_MAX
#define HT_RONIN_NAVI_TEXT 0x06FA // Navi's C-up on the Ronin (Katana.cpp)
#define HT_RONIN_ICICLES 24
#define HT_RONIN_LINES 3
#define HT_RONIN_BODYPARTS 10

typedef struct HtRoninIcicle {
    Vec3f pos;
    s16 timer; // frames since it burst up; 0 = unused
    s16 yaw;
} HtRoninIcicle;

typedef struct HtRoninLine {
    Vec3f origin;
    s16 yaw;
    s16 count;    // icicles raised so far
    s16 cooldown; // frames to the next one
    u8 active;
} HtRoninLine;

typedef struct HtRonin {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[HT_RONIN_LIMBS];
    Vec3s morphTable[HT_RONIN_LIMBS];
    ColliderCylinder bodyCollider;  // takes hits, blocks Link
    ColliderQuad swordCollider;     // the blade's line (kept for its trail; it no longer hurts)
    ColliderCylinder cutCollider;   // the cut's hitbox: ahead of him while the swing cuts
    ColliderCylinder icicleColliders[HT_RONIN_ICICLES];
    HtRoninIcicle icicles[HT_RONIN_ICICLES];
    HtRoninLine lines[HT_RONIN_LINES];
    s16 action;
    s16 timer;
    s16 hurtTimer;
    s16 lastAttack;
    s16 strafeDir;
    s16 cuts;        // quick-draw cuts left in this flurry
    u8 phase;
    u8 bladeOut;     // katana in the right hand (else sheathed in the left)
    u8 swordActive;  // the blade hurts
    u8 trailOn;
    u8 airborne;
    s32 effectIndex;
    Vec3f handPos;   // the scabbard's middle: where the charge frost gathers
    Vec3f waistPos;  // the belt the scabbard hangs from
    Vec3f leftHandPos; // the katana's grip while guarding
    // two-handed grip (see HtRonin_SolveGrip): world matrices forced on the upper arm and forearm roots of each arm,
    // and the katana's own world matrix, worked out each frame from the animation's sword hand
    u8 gripOn;
    MtxF armMtx[4];   // left upper arm, left forearm, right upper arm, right forearm
    MtxF katanaMtx;
    Vec3f bladeBase;
    Vec3f bladeTip;
    // the blade's hitbox is swept, as Link's sword's is: from last frame's edge to this frame's (a flat blade-shaped
    // quad, a frame at a time, passed clean through Link between frames - the swings missed)
    Vec3f prevBladeBase;
    Vec3f prevBladeTip;
    u8 sweepValid;
    Vec3f bodyPartsPos[HT_RONIN_BODYPARTS];
    s32 bodyPartCount;
    // the flash step
    u8 hidden;       // gone into the mist: not drawn, can't be hit or touched
    Vec3f flashTarget;
    s16 shieldFrames; // how long Link has been hiding behind his shield
} HtRonin;

void HtRonin_Init(Actor* thisx, PlayState* play);
void HtRonin_Destroy(Actor* thisx, PlayState* play);
void HtRonin_Update(Actor* thisx, PlayState* play);
void HtRonin_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
