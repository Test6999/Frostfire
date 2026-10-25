#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: a burning torch the Ice Katana blows out (slash wave, Iai wave or ice arrow).
//
// Puzzle use: torches sharing a switch flag form a group; when every torch of the group is out at the same time the
// flag is set (with the puzzle-solved chime) and the group stays dark for good. A relight time makes them flare back
// up after a while, so a group may have to be put out quickly.
// Params:
//   bits 0-5   group switch flag (63 = not part of a puzzle)
//   bits 6-9   relight after this many 2-second steps (0 = stays out)
//   bits 10-12 flame colour: 0 orange, 1 red, 2 blue, 3 green, 4 purple, 5 yellow
//   bit  13    wooden stand (else golden)
//   bit  14    SWITCH-LIT: the torch starts dark and lights (for good, the katana can't put it out) once the
//              switch flag in bits 0-5 is set. rotZ = 0x100 | flag: when every switch-lit torch sharing that rotZ
//              is burning, that flag is set (with the chime) - e.g. the colour torches that open a door.
//   bit  15    ETERNAL: always burning, the katana can't put it out (room lighting)
// rotZ 0x200: FIRE GROUP - dark, lit by fire arrows / Din's Fire, burns for the relight time (bits 6-9, 0 = 8 s)
//              unless every torch of the group (flag in bits 0-5) burns at once: then the flag is set for good.
// rotZ 0x400: PATTERN torch (the torch-drawing rooms): starts burning; the katana puts it out and it stays out (fire
//              arrows / Din's Fire light it again). rotZ 0x800 = burning in the solution; rotZ bits 0-5 = the room's
//              reset switch flag: when it is set every pattern torch sharing it lights again (and the flag is cleared,
//              so the floor switch pops back up). When every pattern torch of the group (switch flag in bits 0-5 of
//              the params) matches the solution, that flag is set (with the chime) and the drawing stays as it is.
// None of them can be hookshot.
#define HT_TORCH_PARAMS(switchFlag, relightSteps, wooden) \
    (((switchFlag) & 0x3F) | (((relightSteps) & 0xF) << 6) | ((wooden) ? 0x2000 : 0))

#define HT_TORCH_SWITCH_LIT 0x4000
#define HT_TORCH_ETERNAL 0x8000 // always burning, nothing puts it out (the tomb's plain lighting torches)

typedef struct HtTorch {
    Actor actor;
    s16 doneFlag;    // switch-lit torches: flag set once every torch of the set burns (-1 none)
    u8 color;
    u8 fireGroup;    // rotZ 0x200: lit by fire, gutters out unless its whole group burns at once
    u8 pattern;      // rotZ 0x400: a dot of a torch drawing
    u8 want;         // pattern: burning in the solution
    s16 resetFlag;   // pattern: the room's reset switch
    ColliderCylinder collider;
    LightNode* lightNode;
    LightInfo lightInfo;
    s16 lit;         // 1 = burning
    s16 relightTimer;
    s16 flameScroll;
    s16 flameGrow;   // 0..10 while relighting
} HtTorch;

void HtTorch_Init(Actor* thisx, PlayState* play);
void HtTorch_Destroy(Actor* thisx, PlayState* play);
void HtTorch_Update(Actor* thisx, PlayState* play);
void HtTorch_Draw(Actor* thisx, PlayState* play);

// Light a torch that's out (a flamethrower's fire reached it). Torches of a solved group stay dark.
void HtTorch_TryLight(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
