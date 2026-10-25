#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

// Heretics Tomb: moving stone platform. Link rides it (it carries him along and turns him with it).
//
// Modes (rotY sets the direction / plane):
//   0 shuttle  - slides forward `distance` and back, pausing at each end
//   1 spinner  - turns in place around its centre (a bridge that lines up now and then)
//   2 wheel    - circles around its home point in the vertical plane facing rotY, staying level (Ferris wheel /
//                windmill style); several platforms with different phases make one wheel
//   3 elevator - rises `distance` and comes back down, pausing at each end
// Params:
//   bits 0-1   mode
//   bits 2-5   distance (shuttle) / radius (wheel) in 40-unit steps (0 = 160)
//   bits 6-8   speed 0..7
//   bits 9-11  phase in 45-degree steps (wheel / spinner start angle; shuttle starts that far along the cycle)
//   bits 12-13 width  (side to side): 80 / 120 / 160 / 200
//   bits 14-15 length (front to back): 80 / 120 / 160 / 200
// The platform's top surface is at the actor's position.
// rotZ (the slab is never tilted, so it's free for options):
//   0x0100 | flag  the platform waits at home until switch flag `flag` (0-63) is set
//   0x0200         carries the eye switch placed against it (within 150 units): it rides along, still shootable
//   0x0400         the platform waits until its room is cleared (all enemies beaten)
//   0x0800         shuttle / elevator rests 6 s at each end (instead of 1.5 / 3 s)
//   0x1000 | flag  elevator: once switch flag `flag` is set its run goes on below home by the same distance (a third
//                  stop: top - home - bottom - home - ...)
//                  rotX > 0: the bottom stop lies rotX units below home instead
//                  The tomb's great lift (its flag HT_LIFT_FLAG, the green torch) gets a fourth stop once the yellow
//                  torch burns (HT_LIFT_DEEP_FLAG): HT_LIFT_DEEP_DROP below its bottom one - B3.
//   0x2000         with 0x0100 | flag: a plug - it stands still, and is gone once the flag is set
//   0x4000         a deep slab (HT_PLATFORM_DEEP): Link catches its edge when he jumps short
//   0x8000         a wind hall corridor (HT_CORRIDOR_DL, full size, centred on the actor) instead of the slab: its
//                  size bits are ignored, and it never turns (rotY only sets which way it shuttles)
#define HT_PLATFORM_PARAMS(mode, distSteps, speed, phase, widthIdx, lengthIdx)                              \
    (((mode) & 3) | (((distSteps) & 0xF) << 2) | (((speed) & 7) << 6) | (((phase) & 7) << 9) | \
     (((widthIdx) & 3) << 12) | (((lengthIdx) & 3) << 14))

#define HT_LIFT_FLAG 0x24       // F_GREEN
#define HT_LIFT_DEEP_FLAG 0x36  // F_YELLOW
#define HT_LIFT_DEEP_DROP 1000.0f
#define HT_PLATFORM_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_platform_dl"
#define HT_PLATFORM_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_platform_col"
#define HT_CORRIDOR_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_corridor_dl"
#define HT_CORRIDOR_COL "__OTR__scenes/shared/heretics_tomb_scene/ht_corridor_col"
#define HT_PLATFORM_DEEP 200.0f
#define HT_PLATFORM_THICKNESS 60.0f // thick enough for Link to catch the edge (a thin slab slips past his ledge check)

typedef struct HtPlatform {
    DynaPolyActor dyna;
    s16 mode;
    s16 speed;
    s16 angle;      // wheel / spinner angle; shuttle cycle angle
    s16 pause;
    s16 gateFlag;   // -1: always moving
    s16 extendFlag; // elevator: -1, or the flag that opens its lower stop
    u8 oneWay;
    u8 leg;         // extended elevator: 0 up from home, 1 down to home, 2 down from home, 3 up to home
    s16 legAngle;
    u8 extendStarted;
    u8 gateClear;   // waits for the room clear flag
    u8 longPause;
    u8 carryEye;
    u8 eyeSearched;
    u8 corridor;    // rotZ 0x8000: the wind hall's moving corridor model
    Actor* eye;     // the eye switch riding on this platform
    Vec3f prevPos;
    f32 distance;
    f32 lowerDistance; // extended elevator: home to its third (bottom) stop
    s8 stopFrom;       // extended elevator: the stop it leaves (0 top, 1 home, 2 bottom, 3 deep)...
    s8 stopTo;         // ...and the one it's going to
    s8 stopsLast;      // extended elevator: its last stop when it last set off (a new stop opening restarts it)
    s16 waitRide;      // extended elevator: frames left waiting at Link's floor for him to step on
} HtPlatform;

void HtPlatform_Init(Actor* thisx, PlayState* play);
void HtPlatform_Destroy(Actor* thisx, PlayState* play);
void HtPlatform_Update(Actor* thisx, PlayState* play);
void HtPlatform_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
