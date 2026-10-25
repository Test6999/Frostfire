#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

typedef struct KatanaIceSlash {
    Actor actor;
    ColliderQuad collider;
    s16 timer;
    s16 isIai;     // params == 1: the bigger Iai wave
    s16 delay;     // Iai: frames until the cut sends it off (it waits at the blade)
    s16 age;       // Iai: frames since it was sent off (it grows in, then fades out)
    f32 halfWidth;
    Vec3f prevLeft;
    Vec3f prevRight;
} KatanaIceSlash;

void KatanaIceSlash_Init(Actor* thisx, PlayState* play);
void KatanaIceSlash_Destroy(Actor* thisx, PlayState* play);
void KatanaIceSlash_Update(Actor* thisx, PlayState* play);
void KatanaIceSlash_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif
