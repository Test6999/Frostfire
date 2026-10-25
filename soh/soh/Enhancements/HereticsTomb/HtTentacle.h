#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: one of Morpha's water tentacles, on its own (no nucleus) - the B2 boat's last boarding wave, and a
// warm-up for the mini-boss. Spawned on the water's surface beside the boat (its base at the actor's position), it
// rises and writhes as Morpha's do, then lunges at Link; if it catches him it lifts him, wrings him a few seconds
// and flings him away (overboard: a void-out), then sinks and rises again.
// A katana wave (or an Iai wave / ice arrow) freezes it solid where it stands; frozen, the Megaton Hammer shatters it.
// Unbroken it thaws after a while. Navi's C-up hint (text 0x06F2) says so; she also says it the first time one
// rises (switch flag 0x37).
// Drawing and motion are Morpha's (z_boss_mo.c), trimmed of the nucleus, its water and its particle effects.
#define HT_TENTACLE_NAVI_TEXT 0x06F2
#define HT_TENTACLE_HINT_FLAG 0x37
// params bit 0: one of Armorpha's (Morpha's own height: its pool's floor is 60 over the water; if it's shattered with
// the core inside, Armorpha is told - HtArmorpha_OnHostShattered)
#define HT_TENTACLE_ARENA 1

typedef struct HtTentacle {
    Actor actor;
    s16 state;
    s16 timer;
    s16 varTimer;
    s16 sfxTimer;
    s16 invincTimer;
    s16 widthIndex;
    s16 pulsePhase;
    s16 xSwing;
    s16 zSwing;
    s16 linkToLeft;
    s16 linkHitTimer;
    s16 attackAngleMod;
    s16 mashCounter;
    s16 frozenTimer;
    s16 baseTex[4];
    f32 fwork[7];
    f32 baseAlpha;
    f32 tentMaxAngle;
    f32 tentSpeed;
    f32 tentPulse;
    f32 tentRippleSize;
    f32 ice;       // 0..1: how frozen it looks
    f32 heightMul; // how tall it stands (TENT_HEIGHT by the boat; 1.0, Morpha's own, in Armorpha's pool)
    s16 coreIndex; // Armorpha's core is inside, at this segment (it bulges there); -1 none
    f32 tentWidth[300];
    Vec3f tentStretch[41];
    Vec3f tentScale[41];
    Vec3f tentRipple[41];
    Vec3s tentRot[41];
    Vec3f tentPos[41];
    Vec3f tentTipPos;
    PosRot grabPosRot;
    Vec3f tossFrom;  // the throw back aboard
    Vec3f tossTo;
    ColliderJntSph collider;
    ColliderJntSphElement elements[19];
} HtTentacle;

// for Armorpha: is it lunging (its core slides up into it then), frozen, or up out of the water?
s32 HtTentacle_IsLunging(Actor* thisx);
s32 HtTentacle_IsFrozen(Actor* thisx);
s32 HtTentacle_IsUp(Actor* thisx);
s32 HtTentacle_IsLeaving(Actor* thisx);
void HtTentacle_RiseNow(Actor* thisx);

void HtTentacle_Init(Actor* thisx, PlayState* play);
void HtTentacle_Destroy(Actor* thisx, PlayState* play);
void HtTentacle_Update(Actor* thisx, PlayState* play);
void HtTentacle_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
