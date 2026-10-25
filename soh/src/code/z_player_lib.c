#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "overlays/actors/ovl_Demo_Effect/z_demo_effect.h"

#include <libultraship/bridge/resourcebridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/draw.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/HereticsTomb/Katana.h"

#include <stdlib.h>
#include <libultraship/bridge/consolevariablebridge.h>

typedef struct {
    /* 0x00 */ u8 flag;
    /* 0x02 */ u16 textId;
} TextTriggerEntry; // size = 0x04

typedef struct {
    /* 0x00 */ void* dList;
    /* 0x04 */ Vec3f pos;
} BowStringData; // size = 0x10

FlexSkeletonHeader* gPlayerSkelHeaders[] = { &gLinkAdultSkel, &gLinkChildSkel };

s16 sBootData[PLAYER_BOOTS_MAX][17] = {
    { 200, 1000, 300, 700, 550, 270, 600, 350, 800, 600, -100, 600, 590, 750, 125, 200, 130 },
    { 200, 1000, 300, 700, 550, 270, 1000, 0, 800, 300, -160, 600, 590, 750, 125, 200, 130 },
    { 200, 1000, 300, 700, 550, 270, 600, 600, 800, 550, -100, 600, 540, 270, 25, 0, 130 },
    { 200, 1000, 300, 700, 380, 400, 0, 300, 800, 500, -100, 600, 590, 750, 125, 200, 130 },
    { 80, 800, 150, 700, 480, 270, 600, 50, 800, 550, -40, 400, 540, 270, 25, 0, 80 },
    { 200, 1000, 300, 800, 500, 400, 800, 400, 800, 550, -100, 600, 540, 750, 125, 400, 200 },
};

// Used to map action params to model groups
u8 sActionModelGroups[] = {
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_NONE
    PLAYER_MODELGROUP_SWORD,            // PLAYER_IA_SWORD_CS
    PLAYER_MODELGROUP_10,               // PLAYER_IA_FISHING_POLE
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_IA_SWORD_MASTER
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_IA_SWORD_KOKIRI
    PLAYER_MODELGROUP_BGS,              // PLAYER_IA_SWORD_BIGGORON
    PLAYER_MODELGROUP_10,               // PLAYER_IA_DEKU_STICK
    PLAYER_MODELGROUP_HAMMER,           // PLAYER_IA_HAMMER
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_FIRE
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_ICE
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_LIGHT
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0C
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0D
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0E
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_SLINGSHOT
    PLAYER_MODELGROUP_HOOKSHOT,         // PLAYER_IA_HOOKSHOT
    PLAYER_MODELGROUP_HOOKSHOT,         // PLAYER_IA_LONGSHOT
    PLAYER_MODELGROUP_EXPLOSIVES,       // PLAYER_IA_BOMB
    PLAYER_MODELGROUP_EXPLOSIVES,       // PLAYER_IA_BOMBCHU
    PLAYER_MODELGROUP_BOOMERANG,        // PLAYER_IA_BOOMERANG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_SPELL_15
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_SPELL_16
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_MAGIC_SPELL_17
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_FARORES_WIND
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_NAYRUS_LOVE
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_DINS_FIRE
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_DEKU_NUT
    PLAYER_MODELGROUP_OCARINA,          // PLAYER_IA_OCARINA_FAIRY
    PLAYER_MODELGROUP_OOT,              // PLAYER_IA_OCARINA_OF_TIME
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FISH
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FIRE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_BUG
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_BIG_POE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_RUTOS_LETTER
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_RED
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_BLUE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_GREEN
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_MILK_FULL
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_MILK_HALF
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FAIRY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ZELDAS_LETTER
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_WEIRD_EGG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_CHICKEN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_BEAN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POCKET_EGG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POCKET_CUCCO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_COJIRO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ODD_MUSHROOM
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ODD_POTION
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POACHERS_SAW
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_BROKEN_GORONS_SWORD
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_PRESCRIPTION
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_FROG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_EYEDROPS
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_CLAIM_CHECK
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_KEATON
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_SKULL
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_SPOOKY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_BUNNY_HOOD
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_GORON
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_ZORA
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_GERUDO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_TRUTH
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_LENS_OF_TRUTH
};

TextTriggerEntry sTextTriggers[] = {
    { 1, 0x3040 },
    { 2, 0x401D },
    { 0, 0x0000 },
    { 2, 0x401D },
};

// Used to map model groups to model types for [animation, left hand, right hand, sheath, waist]
u8 gPlayerModelTypes[PLAYER_MODELGROUP_MAX][PLAYER_MODELGROUPENTRY_MAX] = {
    /* PLAYER_MODELGROUP_0 */
    { PLAYER_ANIMTYPE_2, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_SHIELD, PLAYER_MODELTYPE_SHEATH_16,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD */
    { PLAYER_ANIMTYPE_1, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_SWORD_AND_SHIELD */
    { PLAYER_ANIMTYPE_1, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_SHIELD, PLAYER_MODELTYPE_SHEATH_17,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_DEFAULT */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_4 */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BGS */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_BGS, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOW_SLINGSHOT */
    { PLAYER_ANIMTYPE_4, PLAYER_MODELTYPE_LH_CLOSED, PLAYER_MODELTYPE_RH_BOW_SLINGSHOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_EXPLOSIVES */
    { PLAYER_ANIMTYPE_5, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOOMERANG */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_BOOMERANG, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_HOOKSHOT */
    { PLAYER_ANIMTYPE_4, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_HOOKSHOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_10 */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_CLOSED, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_HAMMER */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_HAMMER, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_OCARINA */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OCARINA, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_OOT */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOTTLE */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_BOTTLE, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_SWORD */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
};

Gfx* sPlayerRightHandShieldDLs[PLAYER_SHIELD_MAX * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightHandClosedFarDL,
    // PLAYER_SHIELD_DEKU
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightFistAndDekuShieldNearDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightFistAndDekuShieldFarDL,
    // PLAYER_SHIELD_HYLIAN
    gLinkAdultRightHandHoldingHylianShieldNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandHoldingHylianShieldFarDL,
    gLinkChildRightHandClosedFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultRightHandHoldingMirrorShieldNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandHoldingMirrorShieldFarDL,
    gLinkChildRightHandClosedFarDL,
};

Gfx* sSheathWithSwordDLs[(PLAYER_SHIELD_MAX + 2) * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
    // PLAYER_SHIELD_DEKU
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildDekuShieldSwordAndSheathNearDL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildDekuShieldSwordAndSheathFarDL,
    // PLAYER_SHIELD_HYLIAN
    gLinkAdultHylianShieldSwordAndSheathNearDL,
    gLinkChildHylianShieldSwordAndSheathNearDL,
    gLinkAdultHylianShieldSwordAndSheathFarDL,
    gLinkChildHylianShieldSwordAndSheathFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultMirrorShieldSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMirrorShieldSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
    // PLAYER_SHIELD_NONE (child, no sword)
    NULL,
    NULL,
    NULL,
    NULL,
    // PLAYER_SHIELD_DEKU (child, no sword)
    NULL,
    gLinkChildDekuShieldWithMatrixDL,
    NULL,
    gLinkChildDekuShieldWithMatrixDL,
};

Gfx* sSheathWithoutSwordDLs[(PLAYER_SHIELD_MAX + 2) * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultSheathFarDL,
    gLinkChildSheathFarDL,
    // PLAYER_SHIELD_DEKU
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldAndSheathNearDL,
    gLinkAdultSheathFarDL,
    gLinkChildDekuShieldAndSheathFarDL,
    // PLAYER_SHIELD_HYLIAN
    gLinkAdultHylianShieldAndSheathNearDL,
    gLinkChildHylianShieldAndSheathNearDL,
    gLinkAdultHylianShieldAndSheathFarDL,
    gLinkChildHylianShieldAndSheathFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultMirrorShieldAndSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultMirrorShieldAndSheathFarDL,
    gLinkChildSheathFarDL,
    // PLAYER_SHIELD_NONE (child, no sword)
    NULL,
    NULL,
    NULL,
    NULL,
    // PLAYER_SHIELD_DEKU (child, no sword)
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldWithMatrixDL,
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldWithMatrixDL,
};

Gfx* gPlayerLeftHandBgsDLs[] = {
    // Biggoron Sword
    gLinkAdultLeftHandHoldingBgsNearDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    gLinkAdultLeftHandHoldingBgsFarDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    // Broken Giant's Knife
    gLinkAdultHandHoldingBrokenGiantsKnifeDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    gLinkAdultHandHoldingBrokenGiantsKnifeFarDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
};

Gfx* gPlayerLeftHandOpenDLs[] = {
    gLinkAdultLeftHandNearDL,
    gLinkChildLeftHandNearDL,
    gLinkAdultLeftHandFarDL,
    gLinkChildLeftHandFarDL,
};

Gfx* gPlayerLeftHandClosedDLs[] = {
    gLinkAdultLeftHandClosedNearDL,
    gLinkChildLeftFistNearDL,
    gLinkAdultLeftHandClosedFarDL,
    gLinkChildLeftFistFarDL,
};

Gfx* sPlayerLeftHandSwordDLs2[] = {
    gLinkAdultLeftHandHoldingMasterSwordNearDL,
    gLinkChildLeftFistAndKokiriSwordNearDL,
    gLinkAdultLeftHandHoldingMasterSwordFarDL,
    gLinkChildLeftFistAndKokiriSwordFarDL,
};

Gfx* sPlayerLeftHandSwordDLs[] = {
    gLinkAdultLeftHandHoldingMasterSwordNearDL,
    gLinkChildLeftFistAndKokiriSwordNearDL,
    gLinkAdultLeftHandHoldingMasterSwordFarDL,
    gLinkChildLeftFistAndKokiriSwordFarDL,
};

Gfx* sPlayerRightHandOpenDLs[] = {
    gLinkAdultRightHandNearDL,
    gLinkChildRightHandNearDL,
    gLinkAdultRightHandFarDL,
    gLinkChildRightHandFarDL,
};

Gfx* sPlayerRightHandClosedDLs[] = {
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightHandClosedFarDL,
};

Gfx* sPlayerRightHandBowSlingshotDLs[] = {
    gLinkAdultRightHandHoldingBowNearDL,
    gLinkChildRightHandHoldingSlingshotNearDL,
    gLinkAdultRightHandHoldingBowFarDL,
    gLinkChildRightHandHoldingSlingshotFarDL,
};

Gfx* sSwordAndSheathDLs[] = {
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
};

Gfx* sSheathDLs[] = {
    gLinkAdultSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultSheathFarDL,
    gLinkChildSheathFarDL,
};

Gfx* sPlayerWaistDLs[] = {
    gLinkAdultWaistNearDL,
    gLinkChildWaistNearDL,
    gLinkAdultWaistFarDL,
    gLinkChildWaistFarDL,
};

Gfx* sPlayerRightHandBowSlingshotDLs2[] = {
    gLinkAdultRightHandHoldingBowNearDL,
    gLinkChildRightHandHoldingSlingshotNearDL,
    gLinkAdultRightHandHoldingBowFarDL,
    gLinkChildRightHandHoldingSlingshotFarDL,
};

Gfx* sPlayerRightHandOcarinaDLs[] = {
    gLinkAdultRightHandHoldingOotNearDL,
    gLinkChildRightHandHoldingFairyOcarinaNearDL,
    gLinkAdultRightHandHoldingOotFarDL,
    gLinkChildRightHandHoldingFairyOcarinaFarDL,
};

Gfx* sPlayerRightHandOotDLs[] = {
    gLinkAdultRightHandHoldingOotNearDL,
    gLinkChildRightHandAndOotNearDL,
    gLinkAdultRightHandHoldingOotFarDL,
    gLinkChildRightHandHoldingOOTFarDL,
};

Gfx* sPlayerRightHandHookshotDLs[] = {
    gLinkAdultRightHandHoldingHookshotNearDL,
    gLinkChildRightHandNearDL,
    gLinkAdultRightHandHoldingHookshotNearDL, // The 'far' display list exists but is not used
    gLinkChildRightHandFarDL,
};

Gfx* sPlayerLeftHandHammerDLs[] = {
    gLinkAdultLeftHandHoldingHammerNearDL,
    gLinkChildLeftHandNearDL,
    gLinkAdultLeftHandHoldingHammerFarDL,
    gLinkChildLeftHandFarDL,
};

Gfx* gPlayerLeftHandBoomerangDLs[] = {
    gLinkAdultLeftHandNearDL,
    gLinkChildLeftFistAndBoomerangNearDL,
    gLinkAdultLeftHandFarDL,
    gLinkChildLeftFistAndBoomerangFarDL,
};

Gfx* sPlayerLeftHandBottleDLs[] = {
    gLinkAdultLeftHandOutNearDL,
    gLinkChildLeftHandUpNearDL,
    gLinkAdultLeftHandOutNearDL,
    gLinkChildLeftHandUpNearDL,
};

Gfx* sFirstPersonLeftForearmDLs[] = {
    gLinkAdultRightArmOutNearDL,
    NULL,
};

Gfx* sFirstPersonLeftHandDLs[] = {
    gLinkAdultRightHandOutNearDL,
    NULL,
};

Gfx* sFirstPersonRightShoulderDLs[] = {
    gLinkAdultRightShoulderNearDL,
    gLinkChildRightShoulderNearDL,
};

Gfx* sFirstPersonForearmDLs[] = {
    gLinkAdultLeftArmOutNearDL,
    NULL,
};

Gfx* sFirstPersonRightHandHoldingWeaponDLs[] = {
    gLinkAdultRightHandHoldingBowFirstPersonDL,
    gLinkChildRightArmStretchedSlingshotDL,
};

// Indexed by model types (left hand, right hand, sheath or waist)
Gfx** sPlayerDListGroups[PLAYER_MODELTYPE_MAX] = {
    gPlayerLeftHandOpenDLs,           // PLAYER_MODELTYPE_LH_OPEN
    gPlayerLeftHandClosedDLs,         // PLAYER_MODELTYPE_LH_CLOSED
    sPlayerLeftHandSwordDLs,          // PLAYER_MODELTYPE_LH_SWORD
    sPlayerLeftHandSwordDLs2,         // PLAYER_MODELTYPE_LH_SWORD_2
    gPlayerLeftHandBgsDLs,            // PLAYER_MODELTYPE_LH_BGS
    sPlayerLeftHandHammerDLs,         // PLAYER_MODELTYPE_LH_HAMMER
    gPlayerLeftHandBoomerangDLs,      // PLAYER_MODELTYPE_LH_BOOMERANG
    sPlayerLeftHandBottleDLs,         // PLAYER_MODELTYPE_LH_BOTTLE
    sPlayerRightHandOpenDLs,          // PLAYER_MODELTYPE_RH_OPEN
    sPlayerRightHandClosedDLs,        // PLAYER_MODELTYPE_RH_CLOSED
    sPlayerRightHandShieldDLs,        // PLAYER_MODELTYPE_RH_SHIELD
    sPlayerRightHandBowSlingshotDLs,  // PLAYER_MODELTYPE_RH_BOW_SLINGSHOT
    sPlayerRightHandBowSlingshotDLs2, // PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2
    sPlayerRightHandOcarinaDLs,       // PLAYER_MODELTYPE_RH_OCARINA
    sPlayerRightHandOotDLs,           // PLAYER_MODELTYPE_RH_OOT
    sPlayerRightHandHookshotDLs,      // PLAYER_MODELTYPE_RH_HOOKSHOT
    sSwordAndSheathDLs,               // PLAYER_MODELTYPE_SHEATH_16
    sSheathDLs,                       // PLAYER_MODELTYPE_SHEATH_17
    sSheathWithSwordDLs,              // PLAYER_MODELTYPE_SHEATH_18
    sSheathWithoutSwordDLs,           // PLAYER_MODELTYPE_SHEATH_19
    sPlayerWaistDLs,                  // PLAYER_MODELTYPE_WAIST
};

Gfx gCullBackDList[] = {
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};

Gfx gCullFrontDList[] = {
    gsSPSetGeometryMode(G_CULL_FRONT),
    gsSPEndDisplayList(),
};

Vec3f* D_80160000;
s32 sDListsLodOffset;
Vec3f sGetItemRefPos;
s32 sLeftHandType;
s32 sRightHandType;

void Player_SetBootData(PlayState* play, Player* this) {
    s32 currentBoots;
    s16* bootRegs;

    REG(27) = 2000;
    REG(48) = 370;

    currentBoots = this->currentBoots;
    if (currentBoots == PLAYER_BOOTS_KOKIRI) {
        if (!LINK_IS_ADULT) {
            currentBoots = PLAYER_BOOTS_KOKIRI_CHILD;
        }
    } else if (currentBoots == PLAYER_BOOTS_IRON) {
        if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
            currentBoots = PLAYER_BOOTS_IRON_UNDERWATER;
        }
        REG(27) = 500;
        REG(48) = 100;
    }

    bootRegs = sBootData[currentBoots];
    REG(19) = bootRegs[0];
    REG(30) = bootRegs[1];
    REG(32) = bootRegs[2];
    REG(34) = bootRegs[3];
    REG(35) = bootRegs[4];
    REG(36) = bootRegs[5];
    REG(37) = bootRegs[6];
    REG(38) = bootRegs[7];
    REG(43) = bootRegs[8];
    REG(45) = bootRegs[9];
    REG(68) = bootRegs[10];
    REG(69) = bootRegs[11];
    IREG(66) = bootRegs[12];
    IREG(67) = bootRegs[13];
    IREG(68) = bootRegs[14];
    IREG(69) = bootRegs[15];
    MREG(95) = bootRegs[16];

    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        REG(45) = 500;
    }
}

// SoH: Custom method used to determine if we're using a custom model for link
uint8_t Player_IsCustomLinkModel() {
    return (LINK_IS_ADULT && ResourceGetIsCustomByName(gLinkAdultSkel)) ||
           (LINK_IS_CHILD && ResourceGetIsCustomByName(gLinkChildSkel));
}

s32 Player_InBlockingCsMode(PlayState* play, Player* this) {
    return (this->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE)) || (this->csAction != 0) ||
           (play->transitionTrigger == TRANS_TRIGGER_START) || (this->stateFlags1 & PLAYER_STATE1_LOADING) ||
           (this->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT) ||
           ((gSaveContext.magicState != MAGIC_STATE_IDLE) && (Player_ActionToMagicSpell(this, this->itemAction) >= 0));
}

s32 Player_InCsMode(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return Player_InBlockingCsMode(play, this) || (this->unk_6AD == 4);
}

/**
 * Checks if Player is currently locked onto a hostile actor.
 * `PLAYER_STATE1_HOSTILE_LOCK_ON` controls Player's "battle" response to hostile actors.
 *
 * Note that within Player, `Player_UpdateHostileLockOn` exists, which updates the flag and also returns the check.
 * Player can use this function instead if the flag should be checked, but not updated.
 */
s32 Player_CheckHostileLockOn(Player* this) {
    return (this->stateFlags1 & PLAYER_STATE1_HOSTILE_LOCK_ON);
}

s32 Player_IsChildWithHylianShield(Player* this) {
    return gSaveContext.linkAge != 0 && (this->currentShield == PLAYER_SHIELD_HYLIAN);
}

s32 Player_ActionToModelGroup(Player* this, s32 itemAction) {
    s32 modelGroup = sActionModelGroups[itemAction];

    // Heretics Tomb: the katana is held two-handed (two-handed stance and animations, shield on the back)
    if ((itemAction == PLAYER_IA_SWORD_MASTER) && Katana_IsEquipped()) {
        return PLAYER_MODELGROUP_BGS;
    }

    if ((modelGroup == PLAYER_MODELGROUP_SWORD_AND_SHIELD) && Player_IsChildWithHylianShield(this)) {
        // child, using kokiri sword with hylian shield equipped
        return PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD;
    } else {
        return modelGroup;
    }
}

void Player_SetModelsForHoldingShield(Player* this) {
    if ((this->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
        ((this->itemAction < 0) || (this->itemAction == this->heldItemAction))) {
        if ((CVarGetInteger(CVAR_CHEAT("ShieldTwoHanded"), 0) && (this->heldItemAction != PLAYER_IA_DEKU_STICK) ||
             !Player_HoldsTwoHandedWeapon(this)) &&
            !Player_IsChildWithHylianShield(this)) {
            this->rightHandType = PLAYER_MODELTYPE_RH_SHIELD;
            if (LINK_IS_CHILD && (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
                (this->currentShield == PLAYER_SHIELD_MIRROR)) {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][0];
            } else if (LINK_IS_ADULT && (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
                       (this->currentShield == PLAYER_SHIELD_DEKU)) {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][1];
            } else {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][gSaveContext.linkAge];
            }
            if (this->sheathType == PLAYER_MODELTYPE_SHEATH_18) {
                this->sheathType = PLAYER_MODELTYPE_SHEATH_16;
            } else if (this->sheathType == PLAYER_MODELTYPE_SHEATH_19) {
                this->sheathType = PLAYER_MODELTYPE_SHEATH_17;
            }
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][gSaveContext.linkAge];
            if ((CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) && LINK_IS_CHILD &&
                gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI) {
                this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
            } else if ((CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) && LINK_IS_ADULT &&
                       gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI) {
                this->sheathDLists = &sPlayerDListGroups[this->sheathType][1];
            }
            this->modelAnimType = PLAYER_ANIMTYPE_2;
            this->itemAction = -1;
        }
    }
}

void Player_SetModels(Player* this, s32 modelGroup) {
    // Left hand
    this->leftHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_LEFT_HAND];
    this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][gSaveContext.linkAge];

    // Heretics Tomb: two-handed katana stance; the hand is drawn as a plain fist and the katana itself is drawn
    // into it after the limb (Player_PostLimbDrawGameplay)
    if ((this->leftHandType == PLAYER_MODELTYPE_LH_BGS) && Katana_IsEquipped()) {
        this->leftHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_LH_CLOSED][gSaveContext.linkAge];
    }

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
        if (LINK_IS_CHILD &&
            (this->leftHandType == PLAYER_MODELTYPE_LH_HAMMER ||
             ((this->leftHandType == PLAYER_MODELTYPE_LH_SWORD || this->leftHandType == PLAYER_MODELTYPE_LH_BGS) &&
              (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)))) {
            this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][0];
        }

        if (LINK_IS_ADULT && (this->leftHandType == PLAYER_MODELTYPE_LH_BOOMERANG ||
                              (this->leftHandType == PLAYER_MODELTYPE_LH_SWORD &&
                               gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI))) {
            this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][1];
        }
    }

    // Right hand
    this->rightHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
    this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][gSaveContext.linkAge];

    this->rightHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
    this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][gSaveContext.linkAge];

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
        if (LINK_IS_CHILD &&
            (this->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT ||
             (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && this->currentShield == PLAYER_SHIELD_MIRROR))) {
            this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][0];
        }
        if (LINK_IS_ADULT &&
            (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && this->currentShield == PLAYER_SHIELD_DEKU)) {
            this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][1];
        }
    }
    if ((CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
         CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
        this->rightHandType == 11) { // If holding Bow/Slingshot
        this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][Player_HoldsSlingshot(this)];
    }

    // Sheath
    this->sheathType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_SHEATH];
    this->sheathDLists = &sPlayerDListGroups[this->sheathType][gSaveContext.linkAge];

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
        if (LINK_IS_CHILD && (this->currentShield == PLAYER_SHIELD_HYLIAN &&
                                  ((gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER) ||
                                   (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS)) ||
                              (this->currentShield == PLAYER_SHIELD_MIRROR) &&
                                  (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI))) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        } else if (LINK_IS_CHILD && this->currentShield == PLAYER_SHIELD_MIRROR &&
                   gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI &&
                   this->sheathType == PLAYER_MODELTYPE_SHEATH_18) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        } else if (LINK_IS_ADULT && (this->currentShield == PLAYER_SHIELD_DEKU &&
                                     gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER) ||
                   (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER &&
                    this->sheathType == PLAYER_MODELTYPE_SHEATH_18 && this->currentShield == PLAYER_SHIELD_DEKU)) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][1];
        } else if (LINK_IS_CHILD && this->sheathType == PLAYER_MODELTYPE_SHEATH_17 &&
                   ((gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER) ||
                    (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS))) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        }
    }

    // Waist
    this->waistDLists = &sPlayerDListGroups[gPlayerModelTypes[modelGroup][4]][gSaveContext.linkAge];

    Player_SetModelsForHoldingShield(this);
    GameInteractor_ExecuteOnPlayerSetModels(this, modelGroup);
}

void Player_SetModelGroup(Player* this, s32 modelGroup) {
    this->modelGroup = modelGroup;

    if (modelGroup == PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD) {
        this->modelAnimType = PLAYER_ANIMTYPE_0;
    } else {
        this->modelAnimType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_ANIM];
    }

    if ((this->modelAnimType < PLAYER_ANIMTYPE_3) && (this->currentShield == PLAYER_SHIELD_NONE)) {
        this->modelAnimType = PLAYER_ANIMTYPE_0;
    }

    Player_SetModels(this, modelGroup);
}

void func_8008EC70(Player* this) {
    this->itemAction = this->heldItemAction;
    Player_SetModelGroup(this, Player_ActionToModelGroup(this, this->heldItemAction));
    this->unk_6AD = 0;
}

void Player_SetEquipmentData(PlayState* play, Player* this) {
    if (this->csAction != 0x56) {
        this->currentShield = SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
        this->currentTunic = TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC));
        this->currentBoots = BOOTS_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS));
        this->currentSwordItemId = B_BTN_ITEM;
        Player_SetModelGroup(this, Player_ActionToModelGroup(this, this->heldItemAction));
        Player_SetBootData(play, this);
    }
}

void Player_UpdateBottleHeld(PlayState* play, Player* this, s32 item, s32 itemAction) {
    Inventory_UpdateBottleItem(play, item, this->heldItemButton);

    if (item != ITEM_BOTTLE) {
        this->heldItemId = item;
        this->heldItemAction = itemAction;
    }

    if (GameInteractor_Should(VB_PLAYER_UPDATE_BOTTLE_HELD, true, this)) {
        this->itemAction = itemAction;
    }
}

void Player_ReleaseLockOn(Player* this) {
    this->focusActor = NULL;
    this->stateFlags2 &= ~PLAYER_STATE2_LOCK_ON_WITH_SWITCH;
}

/**
 * This function aims to clear Z-Target related state when it isn't in use.
 * It also handles setting a specific free fall related state that is interntwined with Z-Targeting.
 * TODO: Learn more about this and give a name to PLAYER_STATE1_19
 */
void Player_ClearZTargeting(Player* this) {
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (this->stateFlags1 & (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER)) ||
        (!(this->stateFlags1 & (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL)) &&
         ((this->actor.world.pos.y - this->actor.floorHeight) < 100.0f))) {
        this->stateFlags1 &=
            ~(PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS | PLAYER_STATE1_PARALLEL |
              PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    } else if (!(this->stateFlags1 &
                 (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_CLIMBING_LADDER))) {
        this->stateFlags1 |= PLAYER_STATE1_FREEFALL;
    }

    Player_ReleaseLockOn(this);
}

/**
 * Sets the "auto lock-on actor" to lock onto an actor without Player's input.
 * This function will first release any existing lock-on or (try to) release parallel.
 *
 * When using Switch Targeting, it is not possible to carry an auto lock-on actor into a normal
 * lock-on when the auto lock-on is finished.
 * This is because the `PLAYER_STATE2_LOCK_ON_WITH_SWITCH` flag is never set with an auto lock-on.
 * With Hold Targeting it is possible to keep the auto lock-on going by keeping the Z button held down.
 *
 * The auto lock-on is considered "friendly" even if the actor is actually hostile. If the auto lock-on is hostile,
 * Player's battle response will not occur (if he is actionable) and the camera behaves differently.
 * When transitioning from auto lock-on to normal lock-on (with Hold Targeting) there will be a noticeable change
 * when it switches from "friendly" mode to "hostile" mode.
 */
void Player_SetAutoLockOnActor(PlayState* play, Actor* actor) {
    Player* this = GET_PLAYER(play);

    Player_ClearZTargeting(this);
    this->focusActor = actor;
    this->autoLockOnActor = actor;
    this->stateFlags1 |= PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS;
    Camera_SetParam(Play_GetCamera(play, CAM_ID_MAIN), 8, actor);
    Camera_RequestMode(Play_GetCamera(play, CAM_ID_MAIN), 2);
}

s32 func_8008EF30(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->stateFlags1 & PLAYER_STATE1_ON_HORSE);
}

s32 func_8008EF44(PlayState* play, s32 ammo) {
    play->shootingGalleryStatus = ammo + 1;
    return 1;
}

s32 Player_IsBurningStickInRange(PlayState* play, Vec3f* pos, f32 xzRange, f32 yRange) {
    Player* this = GET_PLAYER(play);
    Vec3f diff;
    s32 pad;

    if ((this->heldItemAction == PLAYER_IA_DEKU_STICK) && (this->unk_860 != 0)) {
        Math_Vec3f_Diff(&this->meleeWeaponInfo[0].tip, pos, &diff);
        return ((SQ(diff.x) + SQ(diff.z)) <= SQ(xzRange)) && (0.0f <= diff.y) && (diff.y <= yRange);
    } else {
        return false;
    }
}

s32 Player_GetStrength(void) {
    s32 strengthUpgrade = CUR_UPG_VALUE(UPG_STRENGTH);

    if (CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0) &&
        CVarGetInteger(CVAR_ENHANCEMENT("StrengthDisabled"), 0)) {
        return PLAYER_STR_NONE;
    }

    if (GameInteractor_Should(VB_PLAYER_MEETS_AGE_REQ, LINK_IS_ADULT, LINK_AGE_ADULT)) {
        return strengthUpgrade;
    } else if (strengthUpgrade != 0) {
        return PLAYER_STR_BRACELET;
    } else {
        return PLAYER_STR_NONE;
    }
}

u8 Player_GetMask(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return this->currentMask;
}

Player* Player_UnsetMask(PlayState* play) {
    Player* this = GET_PLAYER(play);

    this->currentMask = PLAYER_MASK_NONE;

    return this;
}

s32 Player_HasMirrorShieldEquipped(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->currentShield == PLAYER_SHIELD_MIRROR);
}

s32 Player_HasMirrorShieldSetToDraw(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) && (this->currentShield == PLAYER_SHIELD_MIRROR);
}

s32 Player_ActionToMagicSpell(Player* this, s32 itemAction) {
    s32 magicSpell = itemAction - PLAYER_IA_MAGIC_SPELL_15;

    if ((magicSpell >= 0) && (magicSpell < 6)) {
        return magicSpell;
    } else {
        return -1;
    }
}

s32 Player_HoldsHookshot(Player* this) {
    return (this->heldItemAction == PLAYER_IA_HOOKSHOT) || (this->heldItemAction == PLAYER_IA_LONGSHOT);
}

// SoH
s32 Player_HoldsBow(Player* this) {
    switch (this->heldItemAction) {
        case PLAYER_IA_BOW:
        case PLAYER_IA_BOW_FIRE:
        case PLAYER_IA_BOW_ICE:
        case PLAYER_IA_BOW_LIGHT:
            return true;
        default:
            return false;
    }
}

// SoH
s32 Player_HoldsSlingshot(Player* this) {
    return this->heldItemAction == PLAYER_IA_SLINGSHOT;
}

s32 func_8008F128(Player* this) {
    return Player_HoldsHookshot(this) && (this->heldActor == NULL);
}

s32 Player_ActionToMeleeWeapon(s32 itemAction) {
    s32 sword = itemAction - PLAYER_IA_FISHING_POLE;

    if ((sword > 0) && (sword < 6)) {
        return sword;
    } else {
        return 0;
    }
}

s32 Player_GetMeleeWeaponHeld(Player* this) {
    return Player_ActionToMeleeWeapon(this->heldItemAction);
}

s32 Player_HoldsTwoHandedWeapon(Player* this) {
    if ((this->heldItemAction >= PLAYER_IA_SWORD_BIGGORON) && (this->heldItemAction <= PLAYER_IA_HAMMER)) {
        return true;
    } else if ((this->heldItemAction == PLAYER_IA_SWORD_MASTER) && Katana_IsEquipped()) {
        return true; // Heretics Tomb: the katana is a two-handed sword
    } else {
        return false;
    }
}

s32 Player_HoldsBrokenKnife(Player* this) {
    return (this->heldItemAction == PLAYER_IA_SWORD_BIGGORON) && (gSaveContext.swordHealth <= 0.0f);
}

s32 Player_ActionToBottle(Player* this, s32 itemAction) {
    s32 bottle = itemAction - PLAYER_IA_BOTTLE;

    if ((bottle >= 0) && (bottle < 13)) {
        return bottle;
    } else {
        return -1;
    }
}

s32 Player_GetBottleHeld(Player* this) {
    return Player_ActionToBottle(this, this->heldItemAction);
}

s32 Player_ActionToExplosive(Player* this, s32 itemAction) {
    s32 explosive = itemAction - PLAYER_IA_BOMB;

    if ((explosive >= 0) && (explosive < 2)) {
        return explosive;
    } else {
        return -1;
    }
}

s32 Player_GetExplosiveHeld(Player* this) {
    return Player_ActionToExplosive(this, this->heldItemAction);
}

s32 func_8008F2BC(Player* this, s32 itemAction) {
    s32 sword = 0;

    if (itemAction != PLAYER_IA_SWORD_CS) {
        sword = itemAction - PLAYER_IA_SWORD_MASTER;
        if ((sword < 0) || (sword >= 3)) {
            goto return_neg;
        }
    }

    return sword;

return_neg:
    return -1;
}

s32 Player_GetEnvironmentalHazard(PlayState* play) {
    Player* this = GET_PLAYER(play);
    TextTriggerEntry* triggerEntry;
    s32 envHazard;

    if (play->roomCtx.curRoom.behaviorType2 == ROOM_BEHAVIOR_TYPE2_3) { // Room is hot
        envHazard = PLAYER_ENV_HAZARD_HOTROOM - 1;
    } else if ((this->underwaterTimer > 80) &&
               ((this->currentBoots == PLAYER_BOOTS_IRON) || (this->underwaterTimer >= 300))) { // Deep underwater
        envHazard = ((this->currentBoots == PLAYER_BOOTS_IRON) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))
                        ? (PLAYER_ENV_HAZARD_UNDERWATER_FLOOR - 1)
                        : (PLAYER_ENV_HAZARD_UNDERWATER_FREE - 1);
    } else if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) { // Swimming
        envHazard = PLAYER_ENV_HAZARD_SWIMMING - 1;
    } else {
        return PLAYER_ENV_HAZARD_NONE;
    }

    // Trigger general textboxes under certain conditions, like "It's so hot in here!"
    if (!Player_InCsMode(play)) {
        triggerEntry = &sTextTriggers[envHazard];

        if ((triggerEntry->flag != 0) && !(gSaveContext.textTriggerFlags & triggerEntry->flag) &&
            (((envHazard == (PLAYER_ENV_HAZARD_HOTROOM - 1)) &&
              (this->currentTunic != PLAYER_TUNIC_GORON && CVarGetInteger(CVAR_CHEAT("SuperTunic"), 0) == 0 &&
               CVarGetInteger(CVAR_ENHANCEMENT("DisableTunicWarningText"), 0) == 0)) ||
             (((envHazard == (PLAYER_ENV_HAZARD_UNDERWATER_FLOOR - 1)) ||
               (envHazard == (PLAYER_ENV_HAZARD_UNDERWATER_FREE - 1))) &&
              (this->currentBoots == PLAYER_BOOTS_IRON) &&
              (this->currentTunic != PLAYER_TUNIC_ZORA && CVarGetInteger(CVAR_CHEAT("SuperTunic"), 0) == 0 &&
               CVarGetInteger(CVAR_ENHANCEMENT("DisableTunicWarningText"), 0) == 0)))) {
            Message_StartTextbox(play, triggerEntry->textId, NULL);
            gSaveContext.textTriggerFlags |= triggerEntry->flag;
        }
    }

    return envHazard + 1;
}

u8 sEyeMouthIndexes[][2] = {
    { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 }, { 4, 0 }, { 5, 1 },
    { 7, 2 }, { 0, 2 }, { 3, 0 }, { 4, 0 }, { 2, 2 }, { 1, 1 }, { 0, 2 }, { 0, 0 },
};

/**
 * Link's eye and mouth textures are placed at the exact same place in adult and child Link's respective object files.
 * This allows the array to only contain the symbols for one file and have it apply to both. This is a problem for
 * shiftability, and changes will need to be made in the code to account for this in a modding scenario. The symbols
 * from adult Link's object are used here.
 */

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
// TODO: Formatting
void* sEyeTextures[2][8] = {
    { gLinkAdultEyesOpenTex, gLinkAdultEyesHalfTex, gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
      gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex, gLinkAdultEyesUnk2Tex },
    { gLinkChildEyesOpenTex, gLinkChildEyesHalfTex, gLinkChildEyesClosedfTex, gLinkChildEyesRollLeftTex,
      gLinkChildEyesRollRightTex, gLinkChildEyesShockTex, gLinkChildEyesUnk1Tex, gLinkChildEyesUnk2Tex },
};

#else
void* sEyeTextures[] = {
    gLinkAdultEyesOpenTex,      gLinkAdultEyesHalfTex,  gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
    gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex,    gLinkAdultEyesUnk2Tex,
};
#endif

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
void* sMouthTextures[2][4] = {
    {
        gLinkAdultMouth1Tex,
        gLinkAdultMouth2Tex,
        gLinkAdultMouth3Tex,
        gLinkAdultMouth4Tex,
    },
    {
        gLinkChildMouth1Tex,
        gLinkChildMouth2Tex,
        gLinkChildMouth3Tex,
        gLinkChildMouth4Tex,
    },
};
#else
void* sMouthTextures[] = {
    gLinkAdultMouth1Tex,
    gLinkAdultMouth2Tex,
    gLinkAdultMouth3Tex,
    gLinkAdultMouth4Tex,
};
#endif

Color_RGB8 sTunicColors[] = {
    { 30, 105, 27 },
    { 100, 20, 0 },
    { 0, 60, 100 },
};

Color_RGB8 sGauntletColors[] = {
    { 255, 255, 255 },
    { 254, 207, 15 },
    // #region SOH [RBA] values matching OOB reads on N64
    { 0, 0, 6 },
    { 2, 89, 24 },
    { 6, 2, 90 },
    { 96, 6, 2 },
};

Gfx* sBootDListGroups[][2] = {
    { gLinkAdultLeftIronBootDL, gLinkAdultRightIronBootDL },   // PLAYER_BOOTS_IRON
    { gLinkAdultLeftHoverBootDL, gLinkAdultRightHoverBootDL }, // PLAYER_BOOTS_HOVER
};

void Player_DrawImpl(PlayState* play, void** skeleton, Vec3s* jointTable, s32 dListCount, s32 lod, s32 tunic, s32 boots,
                     s32 face, OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* data) {
    Color_RGB8* color;
    s32 eyeIndex = (jointTable[22].x & 0xF) - 1;
    s32 mouthIndex = (jointTable[22].x >> 4) - 1;

    OPEN_DISPS(play->state.gfxCtx);

    if (eyeIndex < 0) {
        eyeIndex = sEyeMouthIndexes[face][0];
    }

    if (eyeIndex > 7)
        eyeIndex = 7;

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[gSaveContext.linkAge][eyeIndex]));
#else
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[eyeIndex]));
#endif
    if (mouthIndex < 0) {
        mouthIndex = sEyeMouthIndexes[face][1];
    }

    if (mouthIndex > 3)
        mouthIndex = 3;

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[gSaveContext.linkAge][mouthIndex]));
#else
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[eyeIndex]));
#endif

    Color_RGB8 sTemp;
    color = &sTunicColors[tunic];
    if (tunic == PLAYER_TUNIC_KOKIRI && CVarGetInteger(CVAR_COSMETIC("Link.KokiriTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.KokiriTunic.Value"), sTunicColors[PLAYER_TUNIC_KOKIRI]);
        color = &sTemp;
    } else if (tunic == PLAYER_TUNIC_GORON && CVarGetInteger(CVAR_COSMETIC("Link.GoronTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.GoronTunic.Value"), sTunicColors[PLAYER_TUNIC_GORON]);
        color = &sTemp;
    } else if (tunic == PLAYER_TUNIC_ZORA && CVarGetInteger(CVAR_COSMETIC("Link.ZoraTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.ZoraTunic.Value"), sTunicColors[PLAYER_TUNIC_ZORA]);
        color = &sTemp;
    }

    if (GameInteractor_Should(VB_APPLY_TUNIC_COLOR, true, data, color)) {
        gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);
    }

    // If we have a custom link model, always use the most detailed LOD
    if (Player_IsCustomLinkModel()) {
        lod = 0;
    }

    sDListsLodOffset = lod * 2;

    SkelAnime_DrawFlexLod(play, skeleton, jointTable, dListCount, overrideLimbDraw, postLimbDraw, data, lod);

    if (!GameInteractor_InvisibleLinkActive() &&
        ((CVarGetInteger(CVAR_ENHANCEMENT("FirstPersonGauntlets"), 0) && LINK_IS_ADULT) ||
         (overrideLimbDraw != Player_OverrideLimbDrawGameplayFirstPerson)) &&
        (overrideLimbDraw != Player_OverrideLimbDrawGameplayCrawling) &&
        (gSaveContext.gameMode != GAMEMODE_END_CREDITS)) {
        if (LINK_IS_ADULT) {
            s32 strengthUpgrade = CUR_UPG_VALUE(UPG_STRENGTH);

            if (strengthUpgrade >= 2) { // silver or gold gauntlets
                gDPPipeSync(POLY_OPA_DISP++);

                color = &sGauntletColors[strengthUpgrade - 2];
                if (strengthUpgrade == PLAYER_STR_SILVER_G &&
                    CVarGetInteger(CVAR_COSMETIC("Gloves.SilverGauntlets.Changed"), 0)) {
                    sTemp = CVarGetColor24(CVAR_COSMETIC("Gloves.SilverGauntlets.Value"), *color);
                    color = &sTemp;
                } else if (strengthUpgrade == PLAYER_STR_GOLD_G &&
                           CVarGetInteger(CVAR_COSMETIC("Gloves.GoldenGauntlets.Changed"), 0)) {
                    sTemp = CVarGetColor24(CVAR_COSMETIC("Gloves.GoldenGauntlets.Value"), *color);
                    color = &sTemp;
                }
                gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);

                gSPDisplayList(POLY_OPA_DISP++, gLinkAdultLeftGauntletPlate1DL);
                gSPDisplayList(POLY_OPA_DISP++, gLinkAdultRightGauntletPlate1DL);
                gSPDisplayList(POLY_OPA_DISP++, (sLeftHandType == PLAYER_MODELTYPE_LH_OPEN)
                                                    ? gLinkAdultLeftGauntletPlate2DL
                                                    : gLinkAdultLeftGauntletPlate3DL);
                gSPDisplayList(POLY_OPA_DISP++, (sRightHandType == PLAYER_MODELTYPE_RH_OPEN)
                                                    ? gLinkAdultRightGauntletPlate2DL
                                                    : gLinkAdultRightGauntletPlate3DL);
            }

            if (boots != 0) {
                Gfx** bootDLists = sBootDListGroups[boots - 1];

                gSPDisplayList(POLY_OPA_DISP++, bootDLists[0]);
                gSPDisplayList(POLY_OPA_DISP++, bootDLists[1]);
            }
        } else {
            if (Player_GetStrength() > PLAYER_STR_NONE) {
                gSPDisplayList(POLY_OPA_DISP++, gLinkChildGoronBraceletDL);
            }
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

Vec3f D_80126038[] = {
    { 1304.0f, 0.0f, 0.0f },
    { 695.0f, 0.0f, 0.0f },
};

f32 D_80126050[] = { 1265.0f, 826.0f };
f32 D_80126058[] = { SQ(13.04f), SQ(6.95f) };
f32 D_80126060[] = { 10.019104f, -19.925102f };
f32 D_80126068[] = { 5.0f, 3.0f };

Vec3f D_80126070 = { 0.0f, -300.0f, 0.0f };

void func_8008F87C(PlayState* play, Player* this, SkelAnime* skelAnime, Vec3f* pos, Vec3s* rot, s32 thighLimbIndex,
                   s32 shinLimbIndex, s32 footLimbIndex) {
    Vec3f spA4;
    Vec3f sp98;
    Vec3f footprintPos;
    CollisionPoly* sp88;
    s32 sp84;
    f32 sp80;
    f32 sp7C;
    f32 sp78;
    f32 sp74;
    f32 sp70;
    f32 sp6C;
    f32 sp68;
    f32 sp64;
    f32 sp60;
    f32 sp5C;
    f32 sp58;
    f32 sp54;
    f32 sp50;
    s16 temp1;
    s16 temp2;
    s32 temp3;

    if ((this->actor.scale.y >= 0.0f) && !(this->stateFlags1 & PLAYER_STATE1_DEAD) &&
        (Player_ActionToMagicSpell(this, this->itemAction) < 0)) {
        s32 pad;

        sp7C = D_80126058[gSaveContext.linkAge];
        sp78 = D_80126060[gSaveContext.linkAge];
        sp74 = D_80126068[gSaveContext.linkAge] - this->unk_6C4;

        Matrix_Push();
        Matrix_TranslateRotateZYX(pos, rot);
        Matrix_MultVec3f(&sZeroVec, &spA4);
        Matrix_TranslateRotateZYX(&D_80126038[gSaveContext.linkAge], &skelAnime->jointTable[shinLimbIndex]);
        Matrix_Translate(D_80126050[gSaveContext.linkAge], 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_MultVec3f(&sZeroVec, &sp98);
        Matrix_MultVec3f(&D_80126070, &footprintPos);
        Matrix_Pop();

        footprintPos.y += 15.0f;

        sp80 = BgCheck_EntityRaycastFloor4(&play->colCtx, &sp88, &sp84, &this->actor, &footprintPos) + sp74;

        if (sp98.y < sp80) {
            sp70 = sp98.x - spA4.x;
            sp6C = sp98.y - spA4.y;
            sp68 = sp98.z - spA4.z;

            sp64 = sqrtf(SQ(sp70) + SQ(sp6C) + SQ(sp68));
            sp60 = (SQ(sp64) + sp78) / (2.0f * sp64);

            sp58 = sp7C - SQ(sp60);
            sp58 = (sp7C < SQ(sp60)) ? 0.0f : sqrtf(sp58);

            sp54 = Math_FAtan2F(sp58, sp60);

            sp6C = sp80 - spA4.y;

            sp64 = sqrtf(SQ(sp70) + SQ(sp6C) + SQ(sp68));
            sp60 = (SQ(sp64) + sp78) / (2.0f * sp64);
            sp5C = sp64 - sp60;

            sp58 = sp7C - SQ(sp60);
            sp58 = (sp7C < SQ(sp60)) ? 0.0f : sqrtf(sp58);

            sp50 = Math_FAtan2F(sp58, sp60);

            temp1 = (M_PI - (Math_FAtan2F(sp5C, sp58) + ((M_PI / 2) - sp50))) * (0x8000 / M_PI);
            temp1 = temp1 - skelAnime->jointTable[shinLimbIndex].z;

            if ((s16)(ABS(skelAnime->jointTable[shinLimbIndex].x) + ABS(skelAnime->jointTable[shinLimbIndex].y)) < 0) {
                temp1 += 0x8000;
            }

            temp2 = (sp50 - sp54) * (0x8000 / M_PI);
            rot->z -= temp2;

            skelAnime->jointTable[thighLimbIndex].z = skelAnime->jointTable[thighLimbIndex].z - temp2;
            skelAnime->jointTable[shinLimbIndex].z = skelAnime->jointTable[shinLimbIndex].z + temp1;
            skelAnime->jointTable[footLimbIndex].z = skelAnime->jointTable[footLimbIndex].z + temp2 - temp1;

            temp3 = SurfaceType_GetFloorType(&play->colCtx, sp88, sp84);

            if ((temp3 >= 2) && (temp3 < 4) && !SurfaceType_IsWallDamage(&play->colCtx, sp88, sp84)) {
                footprintPos.y = sp80;
                EffectSsGFire_Spawn(play, &footprintPos);
            }
        }
    }
}

// Heretics Tomb: the katana's arms. While the Iai charges and through the Iai draw, both arms are placed by two-bone
// IK instead of the animation. Charging: crouched (the one-handed charge crouch), the left fist (Link's sword hand) at
// his right hip in front of him, the blade drawn back along his right side, the right hand over it as if holding the
// scabbard. The draw: the fist dips, then sweeps out in front of him from low on his right to high on his left, the
// blade following the cut; the right hand stays at the hip; the finish is held a moment before the arms ease back.
// Tuned in an offline preview with Link's skeleton and these animations.
#define KAT_UPPER_ARM 919.0f
#define KAT_FOREARM 754.0f
#define KAT_GRIP_Y 380.0f         // the fist's grip line in hand space (the vanilla sword trail runs at 400)
#define KAT_POMMEL_SHIFT 1100.0f  // carried between cuts: the katana sits forward in the fist, Link holds the end of the hilt
#define KAT_CUT_SHIFT (-420.0f)   // cutting and the Iai: the left fist up the hilt against the guard (model x 420)
#define KAT_SWORD_LENGTH 6000.0f  // hitbox / trail length in hand space (the point is at about 6080 with the cutting grip)
// a cut keeps both hands on the hilt: the grip is held this far in front of / above the waist and out along the blade
// (vanilla's two-handed sweep parts the hands beyond arm's reach), turned with the animation's blade
#define KAT_SWING_FWD 600.0f
#define KAT_SWING_UP 900.0f
#define KAT_SWING_REACH 800.0f
#define KAT_SWING_FRAMES 30      // frames from a cut's start that the arms are held to it
#define KAT_IAI_EXTEND 1620.0f   // the Iai: shoulder to wrist through the cut (the arm's full reach is 1673)
enum { KAT_IK_NONE, KAT_IK_CHARGE, KAT_IK_IAI };
static s32 sKatIkMode = KAT_IK_NONE;
static f32 sKatIkWeight = 0.0f;
static u32 sKatIkFrame = 0xFFFFFFFF;
static s32 sKatArmForced[2] = { false, false }; // left, right
static MtxF sKatForeMtx[2];
static Vec3f sKatWaist;
static Vec3f sKatRightTarget;
static Vec3f sKatRightPole;
static Vec3f sKatWant; // the blade's direction and edge while the IK holds the arm
static Vec3f sKatOut;
static s32 sKatAimOverride = false;
static MtxF sKatHandEq; // the fist as if the katana ran straight along it (hitbox, trail, charge glow)
static s32 sKatHandEqValid = false;
// The scabbard (saya), worn on Link's right hip whenever the katana is equipped: the hilt forward in front of the
// hip, the scabbard back and down past it (under the shield). The katana sits in it whenever it isn't in his hand.
#define KAT_SAYA_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_saya_dl"
#define KAT_SAYA_MOUTH_X 1040.0f // where the right hand holds it, in katana model space (the guard is at 790)
static MtxF sKatWorn; // the sheathed katana / the scabbard on the hip, in katana model space
static s32 sKatWornValid = false;
static MtxF sKatSaya; // the scabbard as drawn this frame
static s32 sKatSayaValid = false;
static f32 sKatRestWeight = 0.0f; // the right hand on the hilt while the blade is out
static f32 sKatTwist = 0.0f;      // the torso turned into the Iai charge (0..1)
static f32 sKatCarry = 0.0f;      // blade out between cuts: carried in both hands beside the scabbard (0..1)
static MtxF sKatCarryMtx;         // ...here (katana model space)
static s32 sKatIaiArms = false;   // this frame the arms are placed for the Iai (charge / draw), not the carry
static s32 sKatIaiRelease = false; // this frame the Iai's finish is easing into the carry (sKatIkWeight 1 -> 0)
static s32 sKatHandForce = false;  // this frame the left fist is closed on the hilt (its frame set from the blade)
#define KAT_RIGHT_GRIP_X 100.0f   // carried: the right fist on the hilt, katana model space (the left holds its end, near -1100)
#define KAT_RIGHT_CUT_X (-380.0f) // cutting: the right fist behind the left one, toward the pommel
static f32 sKatSwing = 0.0f;      // the arms held to a cut (0..1)
static Vec3f sKatSwingD = { 1.0f, 0.0f, 0.0f }; // the blade and edge where the cut left them
static Vec3f sKatSwingE = { 0.0f, 1.0f, 0.0f };
static u32 sKatSwingFrame = 0xFFFFFFFF;
#define KAT_TWIST_YAW (-35.0f * M_PI / 180.0f) // the charge: chest turned to his right...
#define KAT_TWIST_PITCH (25.0f * M_PI / 180.0f) // ...and straightened up out of the crouch's lean

static Vec3f Kat_V(f32 x, f32 y, f32 z) {
    Vec3f v = { x, y, z };
    return v;
}
static Vec3f Kat_Add(Vec3f a, Vec3f b) {
    return Kat_V(a.x + b.x, a.y + b.y, a.z + b.z);
}
static Vec3f Kat_Sub(Vec3f a, Vec3f b) {
    return Kat_V(a.x - b.x, a.y - b.y, a.z - b.z);
}
static Vec3f Kat_Scale(Vec3f a, f32 k) {
    return Kat_V(a.x * k, a.y * k, a.z * k);
}
static f32 Kat_Dot(Vec3f a, Vec3f b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static Vec3f Kat_Cross(Vec3f a, Vec3f b) {
    return Kat_V(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static Vec3f Kat_Unit(Vec3f a) {
    f32 n = sqrtf(Kat_Dot(a, a));
    return (n > 0.0001f) ? Kat_Scale(a, 1.0f / n) : a;
}
static Vec3f Kat_Slerp(Vec3f a, Vec3f b, f32 t) { // unit vectors
    f32 w = acosf(CLAMP(a.x * b.x + a.y * b.y + a.z * b.z, -1.0f, 1.0f));
    f32 sw = sinf(w);

    if (sw < 0.0001f) {
        return a;
    }
    return (Vec3f){ (sinf((1.0f - t) * w) * a.x + sinf(t * w) * b.x) / sw, (sinf((1.0f - t) * w) * a.y + sinf(t * w) * b.y) / sw,
                    (sinf((1.0f - t) * w) * a.z + sinf(t * w) * b.z) / sw };
}
static Vec3f Kat_Lerp(Vec3f a, Vec3f b, f32 t) {
    return Kat_Add(a, Kat_Scale(Kat_Sub(b, a), t));
}
// r, f, u: Link's right, forward and up
static Vec3f Kat_Body(Vec3f r, Vec3f f, Vec3f u, f32 kr, f32 kf, f32 ku) {
    return Kat_Add(Kat_Add(Kat_Scale(r, kr), Kat_Scale(f, kf)), Kat_Scale(u, ku));
}
static f32 Kat_Smooth(f32 t) {
    t = CLAMP(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
static void Kat_Frame(MtxF* m, Vec3f o, Vec3f x, Vec3f pole, f32 s) {
    Vec3f z = Kat_Unit(Kat_Cross(x, pole));
    Vec3f y = Kat_Cross(z, x);

    m->xx = x.x * s; m->yx = x.y * s; m->zx = x.z * s; m->wx = 0.0f;
    m->xy = y.x * s; m->yy = y.y * s; m->zy = y.z * s; m->wy = 0.0f;
    m->xz = z.x * s; m->yz = z.y * s; m->zz = z.z * s; m->wz = 0.0f;
    m->xw = o.x;     m->yw = o.y;     m->zw = o.z;     m->ww = 1.0f;
}

static s32 Kat_AnimIs(Player* this, const char* anim) {
    const char* cur = (const char*)this->skelAnime.animation;
    return (cur != NULL) && (strcmp(cur, anim) == 0);
}

// One arm, from its shoulder override (the upper body's world matrix on top of the stack): the wrist goes to
// `target`, blended with where the animation puts it by sKatIkWeight; the elbow bends toward `pole`.
static void Kat_SolveArm(Player* this, Vec3f* pos, Vec3s* rot, s32 foreLimb, Vec3f target, Vec3f pole, s32 side,
                         f32 weight) {
    void** limbs = (void**)this->skelAnime.skeleton;
    StandardLimb* foreL = SEGMENTED_TO_VIRTUAL(limbs[foreLimb - 1]);
    StandardLimb* handL = SEGMENTED_TO_VIRTUAL(limbs[foreLimb]);
    Vec3f forePos = { foreL->jointPos.x, foreL->jointPos.y, foreL->jointPos.z };
    Vec3f handPos = { handL->jointPos.x, handL->jointPos.y, handL->jointPos.z };
    f32 s = this->actor.scale.x;
    f32 a = KAT_UPPER_ARM * s, b = KAT_FOREARM * s, len, c;
    Vec3f shoulder, natural, hand, d, v, elbow;
    MtxF m;

    Matrix_Push();
    Matrix_TranslateRotateZYX(pos, rot);
    Matrix_Get(&m);
    shoulder = Kat_V(m.xw, m.yw, m.zw);
    Matrix_TranslateRotateZYX(&forePos, &this->skelAnime.jointTable[foreLimb]);
    Matrix_MultVec3f(&handPos, &natural);
    Matrix_Pop();

    hand = Kat_Lerp(natural, target, weight);
    d = Kat_Sub(hand, shoulder);
    len = CLAMP(sqrtf(Kat_Dot(d, d)), 0.01f, a + b - 0.01f * s);
    d = Kat_Unit(d);
    c = CLAMP((a * a + len * len - b * b) / (2.0f * a * len), -1.0f, 1.0f);
    v = Kat_Unit(Kat_Sub(pole, Kat_Scale(d, Kat_Dot(pole, d))));
    elbow = Kat_Add(shoulder, Kat_Scale(Kat_Add(Kat_Scale(d, c), Kat_Scale(v, sqrtf(1.0f - c * c))), a));

    Kat_Frame(&m, shoulder, Kat_Unit(Kat_Sub(elbow, shoulder)), pole, s);
    Kat_Frame(&sKatForeMtx[side], elbow, Kat_Unit(Kat_Sub(hand, elbow)), pole, s);
    Matrix_Put(&m);
    pos->x = pos->y = pos->z = 0.0f;
    rot->x = rot->y = rot->z = 0;
    sKatArmForced[side] = true;
}

// The hand as the animation alone would place it (from the shoulder's override: pos / rot are the shoulder's)
static void Kat_NaturalHand(Player* this, Vec3f* pos, Vec3s* rot, s32 foreLimb, MtxF* out) {
    void** limbs = (void**)this->skelAnime.skeleton;
    StandardLimb* foreL = SEGMENTED_TO_VIRTUAL(limbs[foreLimb - 1]);
    StandardLimb* handL = SEGMENTED_TO_VIRTUAL(limbs[foreLimb]);
    Vec3f forePos = { foreL->jointPos.x, foreL->jointPos.y, foreL->jointPos.z };
    Vec3f handPos = { handL->jointPos.x, handL->jointPos.y, handL->jointPos.z };

    Matrix_Push();
    Matrix_TranslateRotateZYX(pos, rot);
    Matrix_TranslateRotateZYX(&forePos, &this->skelAnime.jointTable[foreLimb]);
    Matrix_TranslateRotateZYX(&handPos, &this->skelAnime.jointTable[foreLimb + 1]);
    Matrix_Get(out);
    Matrix_Pop();
}

static void Kat_Frame3(MtxF* m, Vec3f o, Vec3f x, Vec3f y, Vec3f z, f32 s) {
    m->xx = x.x * s; m->yx = x.y * s; m->zx = x.z * s; m->wx = 0.0f;
    m->xy = y.x * s; m->yy = y.y * s; m->zy = y.z * s; m->wy = 0.0f;
    m->xz = z.x * s; m->yz = z.y * s; m->zz = z.z * s; m->wz = 0.0f;
    m->xw = o.x;     m->yw = o.y;     m->zw = o.z;     m->ww = 1.0f;
}

// a matrix between two of the same scale s: positions and axes blended, the axes made square again
static void Kat_BlendMtx(MtxF* out, MtxF* a, MtxF* b, f32 t, f32 s) {
    Vec3f o = Kat_Lerp(Kat_V(a->xw, a->yw, a->zw), Kat_V(b->xw, b->yw, b->zw), t);
    Vec3f x = Kat_Unit(Kat_Lerp(Kat_V(a->xx, a->yx, a->zx), Kat_V(b->xx, b->yx, b->zx), t));
    Vec3f y = Kat_Lerp(Kat_V(a->xy, a->yy, a->zy), Kat_V(b->xy, b->yy, b->zy), t);
    Vec3f z;

    y = Kat_Unit(Kat_Sub(y, Kat_Scale(x, Kat_Dot(x, y))));
    z = Kat_Cross(x, y);
    Kat_Frame3(out, o, x, y, z, s);
}

static s32 Kat_OnLink(Player* this) {
    return (this->actor.id == ACTOR_PLAYER) && LINK_IS_ADULT && Katana_IsEquipped() && (this->actor.scale.y >= 0.0f);
}

// The scabbard on the hip (from the waist and Link's facing), and how much the right hand rests on it.
static void Kat_UpdateWorn(PlayState* play, Player* this) {
    f32 s = this->actor.scale.x;
    f32 fx = Math_SinS(this->actor.shape.rot.y), fz = Math_CosS(this->actor.shape.rot.y);
    Vec3f f = Kat_V(fx, 0.0f, fz), r = Kat_V(-fz, 0.0f, fx), u = Kat_V(0.0f, 1.0f, 0.0f);
    // worn as a samurai wears it (mirrored for a left-handed swordsman): the scabbard's mouth at his right hip, the
    // scabbard going back, out and down from it, the hilt crossing in front of his belly toward his left - in reach of
    // both hands
    Vec3f x = Kat_Unit(Kat_Body(r, f, u, 0.6f, -0.7f, -0.35f)); // from the guard to the point
    Vec3f y = Kat_Body(r, f, u, 0.3f, 0.0f, 1.0f);              // the edge, up
    Vec3f mouth = Kat_Add(sKatWaist, Kat_Scale(Kat_Body(r, f, u, 450.0f, 150.0f, -80.0f), s));
    Vec3f origin = Kat_Sub(mouth, Kat_Scale(x, KAT_SAYA_MOUTH_X * s)); // the katana model's origin
    s32 rest;

    sKatWornValid = Kat_OnLink(this);
    if (!sKatWornValid) {
        sKatRestWeight = 0.0f;
        return;
    }
    y = Kat_Unit(Kat_Sub(y, Kat_Scale(x, Kat_Dot(x, y))));
    Kat_Frame3(&sKatWorn, origin, x, y, Kat_Cross(x, y), s);
    // carried between cuts: the drawn blade held beside the scabbard, a little out from it and higher, in both hands
    // (the left at the hilt's end in front of his belly, the right below the guard by his right hip)
    // (edge down: the blade turned over from the way it's worn)
    // (low beside the scabbard and back along it, so the hilt's end is in front of his belly, where the left fist
    // holds it with the wrist straight: tuned in the offline preview)
    Kat_Frame3(&sKatCarryMtx,
               Kat_Add(origin, Kat_Scale(Kat_Add(Kat_Body(r, f, u, 270.0f, 0.0f, -350.0f), Kat_Scale(x, 600.0f)), s)), x,
               Kat_Scale(y, -1.0f), Kat_Cross(x, Kat_Scale(y, -1.0f)), s);

    // blade out: the katana is held in both hands (the right one on the hilt below the guard: its target is set where
    // the katana is drawn); the torso turns into the Iai charge
    rest = (this->leftHandType == PLAYER_MODELTYPE_LH_BGS) && !(this->stateFlags1 & PLAYER_STATE1_SHIELDING);
    {
        static u32 sRestFrame = 0xFFFFFFFF;

        if (sRestFrame != play->gameplayFrames) {
            sRestFrame = play->gameplayFrames;
            Math_StepToF(&sKatRestWeight, rest ? 1.0f : 0.0f, rest ? 0.34f : 0.5f);
            Math_StepToF(&sKatTwist, ((sKatIkMode == KAT_IK_CHARGE) && (this->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK))
                                         ? 1.0f : 0.0f, 0.2f);
            {
                s32 carry = rest && (this->meleeWeaponState == 0) && (sKatIkMode == KAT_IK_NONE) &&
                            !(this->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) &&
                            (Katana_FramesSinceSwing(play) > 12);

                Math_StepToF(&sKatCarry, carry ? 1.0f : 0.0f, carry ? 0.2f : 0.5f);
            }
        }
    }
    sKatRightPole = Kat_Unit(Kat_Body(r, f, u, 0.6f, -0.1f, -0.5f));
    {
        Vec3f mouth = { KAT_SAYA_MOUTH_X, 0.0f, 0.0f };
        MtxF w = sKatWorn;

        SkinMatrix_Vec3fMtxFMultXYZ(&w, &mouth, &sKatRightTarget);
    }
}

// From the L_SHOULDER override: works out both arms' targets, solves the left arm (the right one follows from its
// own override).
static void Player_KatanaArmIK(PlayState* play, Player* this, Vec3f* pos, Vec3s* rot) {
    s32 charging = (this->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) != 0;
    s32 drawing = Katana_IsIaiStrikeActive() && Kat_AnimIs(this, gPlayerAnim_link_fighter_Lside_kiru);
    s32 recovering = Katana_IsIaiStrikeActive() && Kat_AnimIs(this, gPlayerAnim_link_fighter_Lside_kiru_end);
    s32 want;
    f32 s = this->actor.scale.x;
    f32 fx = Math_SinS(this->actor.shape.rot.y), fz = Math_CosS(this->actor.shape.rot.y);
    Vec3f f = Kat_V(fx, 0.0f, fz), r = Kat_V(-fz, 0.0f, fx), u = Kat_V(0.0f, 1.0f, 0.0f);
    Vec3f chest, chargeHand, chargePole, chargeWant, chargeOut, target, pole;
    MtxF upper;

    sKatArmForced[0] = sKatArmForced[1] = false;
    sKatAimOverride = false;
    sKatIaiArms = false;
    sKatIaiRelease = false;
    sKatHandForce = false;
    if ((this->actor.id != ACTOR_PLAYER) || !LINK_IS_ADULT || !Katana_IsEquipped() ||
        (this->leftHandType != PLAYER_MODELTYPE_LH_BGS) || (this->actor.scale.y < 0.0f)) {
        sKatIkMode = KAT_IK_NONE;
        sKatIkWeight = 0.0f;
        return;
    }

    if (charging) {
        sKatIkMode = KAT_IK_CHARGE;
    } else if (drawing) {
        sKatIkMode = KAT_IK_IAI;
    }
    // the finish is held through the first half of the follow-through
    want = charging || drawing ||
           (recovering && (sKatIkMode == KAT_IK_IAI) && (this->skelAnime.curFrame < KATANA_IAI_HOLD_FRAMES));
    if (sKatIkFrame != play->gameplayFrames) {
        sKatIkFrame = play->gameplayFrames;
        Math_StepToF(&sKatIkWeight, want ? 1.0f : 0.0f, want ? 0.34f : ((sKatIkMode == KAT_IK_IAI) ? 0.12f : 0.15f));
    }
    if (!want && (sKatIkWeight <= 0.0f)) {
        if (sKatIkMode == KAT_IK_IAI) {
            sKatCarry = 1.0f; // the Iai's finish has eased into the carry: carry on from there
        }
        sKatIkMode = KAT_IK_NONE;
    }
    if (sKatIkMode == KAT_IK_NONE) {
        s32 swingOn = (Katana_FramesSinceSwing(play) <= KAT_SWING_FRAMES) && !charging;

        if (sKatSwingFrame != play->gameplayFrames) {
            sKatSwingFrame = play->gameplayFrames;
            Math_StepToF(&sKatSwing, swingOn ? 1.0f : 0.0f, swingOn ? 0.5f : 0.2f);
        }
        if ((sKatCarry > 0.0f) || (sKatSwing > 0.0f)) {
            // the carry: the left wrist to the hilt's end of the carried katana (its fist sits 1100 back along the
            // blade from the model's origin, the wrist 380 below the grip line)
            Vec3f wrist = { -KAT_POMMEL_SHIFT, -KAT_GRIP_Y, 0.0f };
            Vec3f pole = Kat_Unit(Kat_Body(r, f, u, -0.5f, -0.3f, 0.3f)); // the elbow out to his left, a little back
            Vec3f target, carryWant, carryOut;
            f32 c = sKatCarry, w = sKatCarry;

            SkinMatrix_Vec3fMtxFMultXYZ(&sKatCarryMtx, &wrist, &target);
            // the katana stays in the fist (turned about the grip to the carry's line), so the hilt runs through it
            carryWant = Kat_Unit(Kat_V(sKatCarryMtx.xx, sKatCarryMtx.yx, sKatCarryMtx.zx));
            carryOut = Kat_Unit(Kat_V(sKatCarryMtx.xy, sKatCarryMtx.yy, sKatCarryMtx.zy));
            sKatWant = carryWant;
            sKatOut = carryOut;
            if (sKatSwing > 0.0f) {
                // a cut: the blade swept as the animation sweeps it (its edge leading), but with the grip held in
                // front of him so the right hand stays on the hilt behind the left; easing into the carry after
                MtxF n;
                Vec3f d, e, grip;

                Kat_NaturalHand(this, pos, rot, PLAYER_LIMB_L_FOREARM, &n);
                d = (this->meleeWeaponState != 0) ? Kat_Unit(Kat_V(n.xx, n.yx, n.zx)) : sKatSwingD;
                {
                    // the sweep's wind-up has the blade pointing back along his right side, through the right arm
                    // holding the hilt: it is held no further round than straight out to his right
                    f32 a = atan2f(Kat_Dot(d, r), Kat_Dot(d, f));
                    f32 h = sqrtf(MAX(0.0f, 1.0f - SQ(d.y)));

                    if (a < (-150.0f * M_PI / 180.0f)) {
                        a += 2.0f * M_PI;
                    }
                    a = CLAMP(a, -100.0f * M_PI / 180.0f, 90.0f * M_PI / 180.0f);
                    d = Kat_Add(Kat_Add(Kat_Scale(f, cosf(a) * h), Kat_Scale(r, sinf(a) * h)), Kat_Scale(u, d.y));
                }
                e = (this->meleeWeaponState != 0) ? Kat_V(n.xy, n.yy, n.zy) : sKatSwingE;
                e = Kat_Unit(Kat_Sub(e, Kat_Scale(d, Kat_Dot(e, d))));
                // past the cut the blade stays where the cut left it (vanilla's follow-through raises it upright in
                // front of his face) until the carry takes it, down in front of him and round to his right side
                sKatSwingD = d;
                sKatSwingE = e;
                grip = Kat_Add(Kat_Add(sKatWaist, Kat_Scale(Kat_Body(r, f, u, 0.0f, KAT_SWING_FWD, KAT_SWING_UP), s)),
                               Kat_Scale(d, KAT_SWING_REACH * s));
                target = Kat_Lerp(Kat_Sub(grip, Kat_Scale(e, KAT_GRIP_Y * s)), target, c);
                {
                    Vec3f via = Kat_Unit(Kat_Body(r, f, u, 0.55f, 0.65f, -0.5f));

                    sKatWant = (c < 0.5f) ? Kat_Slerp(d, via, c * 2.0f) : Kat_Slerp(via, carryWant, c * 2.0f - 1.0f);
                }
                sKatOut = Kat_Lerp(e, carryOut, c);
                w = MAX(sKatSwing, c);
            }
            Kat_SolveArm(this, pos, rot, PLAYER_LIMB_L_FOREARM, target, pole, 0, w);
            sKatAimOverride = (sKatSwing > 0.0f) || (c >= 0.5f);
            sKatHandForce = (c >= 0.5f);
        }
        return;
    }
    sKatIaiArms = true;
    sKatHandForce = (sKatIkMode == KAT_IK_IAI); // the draw: the fist closed on the hilt

    Matrix_Get(&upper);
    chest = Kat_V(upper.xw, upper.yw, upper.zw);
    chargeHand = Kat_Add(sKatWaist, Kat_Scale(Kat_Body(r, f, u, 480.0f, 650.0f, 250.0f), s));
    chargePole = Kat_Unit(Kat_Body(r, f, u, -0.3f, 0.8f, -0.5f));
    chargeWant = Kat_Unit(Kat_Body(r, f, u, 0.55f, -0.8f, -0.25f)); // clear of the shield on his back
    chargeOut = Kat_Body(r, f, u, 0.3f, 0.0f, 1.0f);

    if (sKatIkMode == KAT_IK_CHARGE) {
        target = chargeHand;
        pole = chargePole;
        sKatWant = chargeWant;
        sKatOut = chargeOut;
    } else {
        f32 p = 1.0f;
        Vec3f dip = Kat_Add(sKatWaist, Kat_Scale(Kat_Body(r, f, u, 560.0f, 360.0f, 60.0f), s));

        if (drawing && (this->skelAnime.endFrame > 0.0f)) {
            p = CLAMP(this->skelAnime.curFrame / this->skelAnime.endFrame, 0.0f, 1.0f);
        }
        if (p < 0.2f) {
            // gather: the fist sinks a little lower and back
            target = Kat_Lerp(chargeHand, dip, Kat_Smooth(p / 0.2f));
            pole = chargePole;
            sKatWant = chargeWant;
            sKatOut = chargeOut;
        } else {
            // the cut: the arm straightens out of the hip and sweeps, extended, from low on his right to high on
            // his left (an arc about the shoulder, bowed forward), the blade carrying on the arm's line, edge leading
            f32 c = CLAMP((p - 0.2f) / 0.35f, 0.0f, 1.0f);
            Vec3f shoulder, dA, dEnd, dd, mv, cut;
            f32 lenA;

            c = 1.0f - (1.0f - c) * (1.0f - c);
            Matrix_MultVec3f(pos, &shoulder);
            dA = Kat_Sub(dip, shoulder);
            lenA = sqrtf(Kat_Dot(dA, dA));
            dA = Kat_Unit(dA);
            dEnd = Kat_Unit(Kat_Body(r, f, u, -0.55f, 0.35f, 0.75f));
            dd = Kat_Unit(Kat_Add(Kat_Slerp(dA, dEnd, c), Kat_Scale(f, 0.35f * sinf(M_PI * c))));
            // the elbow rises and leads, the forearm unfolds after it (the reach eases in over the whole cut)
            target = Kat_Add(shoulder, Kat_Scale(dd, lenA + (KAT_IAI_EXTEND * s - lenA) * (c * c * (3.0f - 2.0f * c))));
            pole = Kat_Unit(Kat_Lerp(chargePole, Kat_Unit(Kat_Body(r, f, u, 0.0f, 0.5f, 1.0f)), CLAMP(c * 2.0f, 0.0f, 1.0f)));
            // the blade on from the arm, the wrist cocked as vanilla's cuts cock it (34 degrees off the forearm),
            // hanging below the arm's line: the knuckles - and so the edge - face up, leading the rising cut; at the
            // finish the arm is up to his left and the point up-left past the fist
            mv = Kat_Sub(u, Kat_Scale(dd, Kat_Dot(u, dd)));
            mv = Kat_Unit(mv);
            cut = Kat_Unit(Kat_Sub(Kat_Scale(dd, 0.83f), Kat_Scale(mv, 0.56f)));
            sKatWant = Kat_Unit(Kat_Slerp(chargeWant, cut, CLAMP(c * 2.5f, 0.0f, 1.0f)));
            sKatOut = Kat_Sub(mv, Kat_Scale(sKatWant, Kat_Dot(mv, sKatWant))); // the edge leads the cut
        }
    }
    if ((sKatIkMode == KAT_IK_IAI) && !want) {
        // after the finish: the left fist eases from the finish into the carry beside the scabbard (the right hand
        // comes off the scabbard's mouth onto the hilt), instead of dropping back to the animation's grip
        Vec3f wrist = { -KAT_POMMEL_SHIFT, -KAT_GRIP_Y, 0.0f };
        Vec3f carryTarget;
        f32 w = sKatIkWeight;

        SkinMatrix_Vec3fMtxFMultXYZ(&sKatCarryMtx, &wrist, &carryTarget);
        target = Kat_Lerp(carryTarget, target, w);
        pole = Kat_Unit(Kat_Lerp(Kat_Unit(Kat_Body(r, f, u, -0.5f, -0.3f, 0.3f)), pole, w));
        sKatWant = Kat_Unit(Kat_Lerp(Kat_Unit(Kat_V(sKatCarryMtx.xx, sKatCarryMtx.yx, sKatCarryMtx.zx)), sKatWant, w));
        sKatOut = Kat_Unit(Kat_Lerp(Kat_Unit(Kat_V(sKatCarryMtx.xy, sKatCarryMtx.yy, sKatCarryMtx.zy)), sKatOut, w));
        sKatAimOverride = true;
        sKatIaiRelease = true;
        Kat_SolveArm(this, pos, rot, PLAYER_LIMB_L_FOREARM, target, pole, 0, 1.0f);
        return;
    }
    sKatAimOverride = (sKatIkWeight >= 0.5f);
    // (the right hand holds the scabbard: its target is set where the katana is drawn, Player_PostLimbDrawGameplay)

    Kat_SolveArm(this, pos, rot, PLAYER_LIMB_L_FOREARM, target, pole, 0, sKatIkWeight);
}

void* Player_KatanaSayaMtx(void) {
    return sKatSayaValid ? &sKatSaya : NULL;
}

s32 Player_OverrideLimbDrawGameplayCommon(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                          void* thisx) {
    Player* this = (Player*)thisx;

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
        CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && LINK_IS_CHILD) {
        if (limbIndex == PLAYER_LIMB_L_HAND) {
            if ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI &&
                 sLeftHandType == PLAYER_MODELTYPE_LH_SWORD) ||
                (sLeftHandType == PLAYER_MODELTYPE_LH_BGS) || (sLeftHandType == PLAYER_MODELTYPE_LH_HAMMER)) {
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
        if (limbIndex == PLAYER_LIMB_R_HAND) {
            if ((this->currentShield == PLAYER_SHIELD_MIRROR && sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) ||
                sRightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT ||
                (sRightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT && Player_HoldsBow(this))) {
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
        if (limbIndex == PLAYER_LIMB_SHEATH) {
            if ((this->currentShield == PLAYER_SHIELD_MIRROR ||
                 (this->currentShield == PLAYER_SHIELD_HYLIAN &&
                  (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER ||
                   gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS))) &&
                ((this->sheathType == PLAYER_MODELTYPE_SHEATH_16) || (this->sheathType == PLAYER_MODELTYPE_SHEATH_17) ||
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_18) ||
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_19))) {
                Matrix_Translate(218, -100, 62, MTXMODE_APPLY);
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
            if ((this->currentShield == PLAYER_SHIELD_DEKU && gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI &&
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_16 || this->sheathType == PLAYER_MODELTYPE_SHEATH_17))) {
                Matrix_Translate(218, -100, 62, MTXMODE_APPLY);
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
    }

    if (limbIndex == PLAYER_LIMB_ROOT) {
        sLeftHandType = this->leftHandType;
        sRightHandType = this->rightHandType;
        D_80160000 = &this->meleeWeaponInfo[2].base;

        if (!LINK_IS_ADULT) {
            if (!(this->skelAnime.movementFlags & 4) || (this->skelAnime.movementFlags & 1)) {
                pos->x *= 0.64f;
                pos->z *= 0.64f;
            }

            if (!(this->skelAnime.movementFlags & 4) || (this->skelAnime.movementFlags & 2)) {
                pos->y *= 0.64f;
            }
        }

        pos->y -= this->unk_6C4;

        if (this->unk_6C2 != 0) {
            Matrix_Translate(pos->x, ((Math_CosS(this->unk_6C2) - 1.0f) * 200.0f) + pos->y, pos->z, MTXMODE_APPLY);
            Matrix_RotateX(this->unk_6C2 * (M_PI / 0x8000), MTXMODE_APPLY);
            Matrix_RotateZYX(rot->x, rot->y, rot->z, MTXMODE_APPLY);
            pos->x = pos->y = pos->z = 0.0f;
            rot->x = rot->y = rot->z = 0;
        }
    } else {
        if (*dList != NULL) {
            D_80160000++;
        }

        if (limbIndex == PLAYER_LIMB_HEAD) {
            if (CVarGetInteger(CVAR_COSMETIC("Link.HeadScale.Changed"), 0)) {
                f32 scale = CVarGetFloat(CVAR_COSMETIC("Link.HeadScale.Value"), 1.0f);
                if (scale != 1.0f) {
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    if (scale > 1.2f) {
                        Matrix_Translate(-((LINK_IS_ADULT ? 320.0f : 200.0f) * scale), 0.0f, 0.0f, MTXMODE_APPLY);
                    } else if (scale < 1.0f) {
                        Matrix_Translate((LINK_IS_ADULT ? 3600.0f : 2900.0f) * ABS(scale - 1.0f), 0.0f, 0.0f,
                                         MTXMODE_APPLY);
                    }
                }
            }
            rot->x += this->headLimbRot.z;
            rot->y -= this->headLimbRot.y;
            rot->z += this->headLimbRot.x;
        } else if (limbIndex == PLAYER_LIMB_L_HAND) {
            if (sKatHandForce && sKatArmForced[0]) {
                // Heretics Tomb: the fist closed on the hilt, as vanilla's hand holds a sword - the blade along hand x,
                // the knuckles (hand y) toward the forearm's line - instead of the animation's hand turned about it
                MtxF* fm = &sKatForeMtx[0];
                Vec3f fore = Kat_Unit(Kat_V(fm->xx, fm->yx, fm->zx));
                Vec3f hx = Kat_Unit(sKatWant);
                Vec3f hy = Kat_Sub(fore, Kat_Scale(hx, Kat_Dot(fore, hx)));

                if (Kat_Dot(hy, hy) > 0.0001f) {
                    MtxF hm;
                    Vec3f o;

                    hy = Kat_Unit(hy);
                    Matrix_MultVec3f(pos, &o);
                    Kat_Frame3(&hm, o, hx, hy, Kat_Cross(hx, hy), this->actor.scale.x);
                    Matrix_Put(&hm);
                    pos->x = pos->y = pos->z = 0.0f;
                    rot->x = rot->y = rot->z = 0;
                    sKatOut = hy; // the edge goes with the fist
                }
            }
            if (CVarGetInteger(CVAR_COSMETIC("Link.SwordScale.Changed"), 0)) {
                f32 scale = CVarGetFloat(CVAR_COSMETIC("Link.SwordScale.Value"), 1.0f);
                if (scale != 1.0f) {
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    Matrix_Translate(-((LINK_IS_ADULT ? 320.0f : 200.0f) * (scale - 1.0f)), 0.0f, 0.0f, MTXMODE_APPLY);
                }
            }
        } else if (limbIndex == PLAYER_LIMB_UPPER) {
            // Heretics Tomb: charging the Iai, the chest turns to his right and straightens, so his left arm reaches
            // across the front of his body to the right hip (a world turn about the waist)
            if ((sKatTwist > 0.0f) && (this->actor.id == ACTOR_PLAYER) && Katana_IsEquipped()) {
                f32 fx = Math_SinS(this->actor.shape.rot.y), fz = Math_CosS(this->actor.shape.rot.y);
                Vec3f r = Kat_V(-fz, 0.0f, fx), u = Kat_V(0.0f, 1.0f, 0.0f);
                MtxF cur;

                Matrix_Get(&cur);
                Matrix_Translate(sKatWaist.x, sKatWaist.y, sKatWaist.z, MTXMODE_NEW);
                Matrix_RotateAxis(KAT_TWIST_PITCH * sKatTwist, &r, MTXMODE_APPLY);
                Matrix_RotateAxis(KAT_TWIST_YAW * sKatTwist, &u, MTXMODE_APPLY);
                Matrix_Translate(-sKatWaist.x, -sKatWaist.y, -sKatWaist.z, MTXMODE_APPLY);
                Matrix_Mult(&cur, MTXMODE_APPLY);
            }
            if (this->upperLimbYawSecondary != 0) {
                Matrix_RotateZ(0x44C * (M_PI / 0x8000), MTXMODE_APPLY);
                Matrix_RotateY(this->upperLimbYawSecondary * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.y != 0) {
                Matrix_RotateY(this->upperLimbRot.y * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.x != 0) {
                Matrix_RotateX(this->upperLimbRot.x * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.z != 0) {
                Matrix_RotateZ(this->upperLimbRot.z * (M_PI / 0x8000), MTXMODE_APPLY);
            }
        } else if (limbIndex == PLAYER_LIMB_WAIST) {
            Matrix_MultVec3f(pos, &sKatWaist); // Heretics Tomb: the katana charge's fist sits at the hip
        } else if (limbIndex == PLAYER_LIMB_L_SHOULDER) {
            Kat_UpdateWorn(play, this);
            Player_KatanaArmIK(play, this, pos, rot);
        } else if ((limbIndex == PLAYER_LIMB_L_FOREARM) || (limbIndex == PLAYER_LIMB_R_FOREARM)) {
            s32 side = (limbIndex == PLAYER_LIMB_R_FOREARM);

            if (sKatArmForced[side]) {
                Matrix_Put(&sKatForeMtx[side]);
                pos->x = pos->y = pos->z = 0.0f;
                rot->x = rot->y = rot->z = 0;
            }
        } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
            if (sKatIaiArms) {
                Kat_SolveArm(this, pos, rot, PLAYER_LIMB_R_FOREARM, sKatRightTarget, sKatRightPole, 1,
                             sKatIaiRelease ? 1.0f : sKatIkWeight);
            } else if (sKatRestWeight > 0.0f) {
                Kat_SolveArm(this, pos, rot, PLAYER_LIMB_R_FOREARM, sKatRightTarget, sKatRightPole, 1, sKatRestWeight);
            }
        } else if (limbIndex == PLAYER_LIMB_L_THIGH) {
            func_8008F87C(play, this, &this->skelAnime, pos, rot, PLAYER_LIMB_L_THIGH, PLAYER_LIMB_L_SHIN,
                          PLAYER_LIMB_L_FOOT);
        } else if (limbIndex == PLAYER_LIMB_R_THIGH) {
            func_8008F87C(play, this, &this->skelAnime, pos, rot, PLAYER_LIMB_R_THIGH, PLAYER_LIMB_R_SHIN,
                          PLAYER_LIMB_R_FOOT);
            return false;
        } else {
            return false;
        }
    }

    return false;
}

s32 Player_OverrideLimbDrawGameplayDefault(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                           void* thisx) {
    Player* this = (Player*)thisx;

    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        if (limbIndex == PLAYER_LIMB_L_HAND) {
            Gfx** dLists = this->leftHandDLists;

            if ((sLeftHandType == PLAYER_MODELTYPE_LH_BGS) && (gSaveContext.swordHealth <= 0.0f) &&
                !Katana_IsEquipped()) {
                dLists += 4;
            } else if ((sLeftHandType == PLAYER_MODELTYPE_LH_BOOMERANG) &&
                       (this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN)) {
                dLists = &gPlayerLeftHandOpenDLs[gSaveContext.linkAge];
                sLeftHandType = PLAYER_MODELTYPE_LH_OPEN;
            } else if ((this->leftHandType == PLAYER_MODELTYPE_LH_OPEN) && (this->actor.speedXZ > 2.0f) &&
                       !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                dLists = &gPlayerLeftHandClosedDLs[gSaveContext.linkAge];
                sLeftHandType = PLAYER_MODELTYPE_LH_CLOSED;
            }

            *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
        } else if (limbIndex == PLAYER_LIMB_R_HAND) {
            Gfx** dLists = this->rightHandDLists;

            if (sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
                dLists += this->currentShield * 4;
            } else if ((this->rightHandType == PLAYER_MODELTYPE_RH_OPEN) && (this->actor.speedXZ > 2.0f) &&
                       !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                dLists = &sPlayerRightHandClosedDLs[gSaveContext.linkAge];
                sRightHandType = PLAYER_MODELTYPE_RH_CLOSED;
            }

            *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
        } else if (limbIndex == PLAYER_LIMB_SHEATH) {
            Gfx** dLists = this->sheathDLists;
            s32 sheathType = this->sheathType;

            // Heretics Tomb: with the katana equipped the Master Sword's sheath on his back stays empty (the katana
            // has its own scabbard, on his hip)
            if (Katana_IsEquipped() && (sheathType == PLAYER_MODELTYPE_SHEATH_18)) {
                sheathType = PLAYER_MODELTYPE_SHEATH_19;
                dLists = &sSheathWithoutSwordDLs[gSaveContext.linkAge];
            } else if (Katana_IsEquipped() && (sheathType == PLAYER_MODELTYPE_SHEATH_16)) {
                sheathType = PLAYER_MODELTYPE_SHEATH_17;
                dLists = &sSheathDLs[gSaveContext.linkAge];
            }

            if ((sheathType == PLAYER_MODELTYPE_SHEATH_18) || (sheathType == PLAYER_MODELTYPE_SHEATH_19)) {
                dLists += this->currentShield * 4;
                if (!LINK_IS_ADULT && (this->currentShield < PLAYER_SHIELD_HYLIAN) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)) {
                    dLists += PLAYER_SHIELD_MAX * 4;
                }
            } else if (!CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) ||
                       (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
                        ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER &&
                          gSaveContext.equips.buttonItems[0] != ITEM_SWORD_BGS) &&
                         this->currentShield == PLAYER_SHIELD_DEKU))) {
                if (!LINK_IS_ADULT &&
                    ((this->sheathType == PLAYER_MODELTYPE_SHEATH_16) ||
                     (this->sheathType == PLAYER_MODELTYPE_SHEATH_17)) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)) {
                    dLists = &sSheathWithSwordDLs[PLAYER_SHIELD_MAX * 4];
                }
            }

            if (dLists[sDListsLodOffset] != NULL) {
                *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
            } else {
                *dList = NULL;
            }

        } else if (limbIndex == PLAYER_LIMB_WAIST) {

            if (!Player_IsCustomLinkModel()) {
                *dList = ResourceMgr_LoadGfxByName(
                    this->waistDLists[sDListsLodOffset]); // NOTE: This needs to be disabled when using custom
                                                          // characters - they're not going to have LODs anyways...
            }
        }
    }

    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW, true, limbIndex, dList, thisx, play);

    if (GameInteractor_InvisibleLinkActive()) {
        this->actor.shape.shadowDraw = NULL;
        *dList = NULL;
    }

    return false;
}

s32 Player_OverrideLimbDrawGameplayFirstPerson(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                               void* thisx) {
    Player* this = (Player*)thisx;

    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        if (this->unk_6AD != 2) {
            *dList = NULL;
        } else if (limbIndex == PLAYER_LIMB_L_FOREARM) {
            *dList = sFirstPersonLeftForearmDLs[gSaveContext.linkAge];
        } else if (limbIndex == PLAYER_LIMB_L_HAND) {
            s32 handOutDlIndex = gSaveContext.linkAge;
            if ((CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                 CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
                LINK_IS_ADULT && Player_HoldsSlingshot(this)) {
                handOutDlIndex = 1;
            }
            *dList = sFirstPersonLeftHandDLs[handOutDlIndex];
        } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
            *dList = sFirstPersonRightShoulderDLs[gSaveContext.linkAge];
        } else if (limbIndex == PLAYER_LIMB_R_FOREARM) {
            *dList = sFirstPersonForearmDLs[gSaveContext.linkAge];
        } else if (limbIndex == PLAYER_LIMB_R_HAND) {
            s32 firstPersonWeaponIndex = gSaveContext.linkAge;
            if (CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
                if (Player_HoldsBow(this)) {
                    firstPersonWeaponIndex = 0;
                } else if (Player_HoldsSlingshot(this)) {
                    firstPersonWeaponIndex = 1;
                }
            }
            *dList = Player_HoldsHookshot(this) ? gLinkAdultRightHandHoldingHookshotFarDL
                                                : sFirstPersonRightHandHoldingWeaponDLs[firstPersonWeaponIndex];
        } else {
            *dList = NULL;
        }
    }

    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW, true, limbIndex, dList, thisx, play);

    return false;
}

s32 Player_OverrideLimbDrawGameplayCrawling(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* thisx) {
    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        *dList = NULL;
    }

    return false;
}

u8 func_80090480(PlayState* play, ColliderQuad* collider, WeaponInfo* weaponInfo, Vec3f* newTip, Vec3f* newBase) {
    if (weaponInfo->active == 0) {
        if (collider != NULL) {
            Collider_ResetQuadAT(play, &collider->base);
        }
        Math_Vec3f_Copy(&weaponInfo->tip, newTip);
        Math_Vec3f_Copy(&weaponInfo->base, newBase);
        weaponInfo->active = 1;
        return 1;
    } else if ((weaponInfo->tip.x == newTip->x) && (weaponInfo->tip.y == newTip->y) &&
               (weaponInfo->tip.z == newTip->z) && (weaponInfo->base.x == newBase->x) &&
               (weaponInfo->base.y == newBase->y) && (weaponInfo->base.z == newBase->z)) {
        if (collider != NULL) {
            Collider_ResetQuadAT(play, &collider->base);
        }
        return 0;
    } else {
        if (collider != NULL) {
            Collider_SetQuadVertices(collider, newBase, newTip, &weaponInfo->base, &weaponInfo->tip);
            CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
        }
        Math_Vec3f_Copy(&weaponInfo->base, newBase);
        Math_Vec3f_Copy(&weaponInfo->tip, newTip);
        weaponInfo->active = 1;
        return 1;
    }
}

void Player_UpdateShieldCollider(PlayState* play, Player* this, ColliderQuad* collider, Vec3f* quadSrc) {
    static u8 shieldColTypes[PLAYER_SHIELD_MAX] = {
        COLTYPE_METAL,
        COLTYPE_WOOD,
        COLTYPE_METAL,
        COLTYPE_METAL,
    };

    if (this->stateFlags1 & PLAYER_STATE1_SHIELDING) {
        Vec3f quadDest[4];

        this->shieldQuad.base.colType = shieldColTypes[this->currentShield];

        Matrix_MultVec3f(&quadSrc[0], &quadDest[0]);
        Matrix_MultVec3f(&quadSrc[1], &quadDest[1]);
        Matrix_MultVec3f(&quadSrc[2], &quadDest[2]);
        Matrix_MultVec3f(&quadSrc[3], &quadDest[3]);
        Collider_SetQuadVertices(collider, &quadDest[0], &quadDest[1], &quadDest[2], &quadDest[3]);

        CollisionCheck_SetAC(play, &play->colChkCtx, &collider->base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
    }
}

Vec3f D_80126080 = { 5000.0f, 400.0f, 0.0f };
Vec3f D_8012608C = { 5000.0f, -400.0f, 1000.0f };
Vec3f D_80126098 = { 5000.0f, 1400.0f, -1000.0f };

Vec3f D_801260A4[3] = {
    { 0.0f, 400.0f, 0.0f },
    { 0.0f, 1400.0f, -1000.0f },
    { 0.0f, -400.0f, 1000.0f },
};

void func_800906D4(PlayState* play, Player* this, Vec3f* newTipPos) {
    Vec3f newBasePos[3];

    Matrix_MultVec3f(&D_801260A4[0], &newBasePos[0]);
    Matrix_MultVec3f(&D_801260A4[1], &newBasePos[1]);
    Matrix_MultVec3f(&D_801260A4[2], &newBasePos[2]);

    if (func_80090480(play, NULL, &this->meleeWeaponInfo[0], &newTipPos[0], &newBasePos[0]) &&
        !(this->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
        !CVarGetInteger(CVAR_ENHANCEMENT("DisableLinkSwordTrail"), 0)) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->meleeWeaponEffectIndex), &this->meleeWeaponInfo[0].tip,
                              &this->meleeWeaponInfo[0].base);
    }

    if ((this->meleeWeaponState > 0) &&
        ((this->meleeWeaponAnimation < 0x18) || (this->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING))) {
        func_80090480(play, &this->meleeWeaponQuads[0], &this->meleeWeaponInfo[1], &newTipPos[1], &newBasePos[1]);
        func_80090480(play, &this->meleeWeaponQuads[1], &this->meleeWeaponInfo[2], &newTipPos[2], &newBasePos[2]);
    }
}

void Player_DrawGetItemIceTrap(PlayState* play, Player* this, Vec3f* refPos, s32 drawIdPlusOne, f32 height) {
    OPEN_DISPS(play->state.gfxCtx);

    if (CVarGetInteger(CVAR_GENERAL("LetItSnow"), 0)) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);

        Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);

        gDPSetGrayscaleColor(POLY_OPA_DISP++, 75, 75, 75, 255);
        gSPGrayscale(POLY_OPA_DISP++, true);

        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSilverRockDL);

        gSPGrayscale(POLY_OPA_DISP++, false);
    } else {
        if (iceTrapScale < 0.01) {
            iceTrapScale += 0.001f;
        } else if (iceTrapScale < 0.8f) {
            iceTrapScale += 0.2f;
        }

        // Draw the ice only after a bit so it doesn't spoil the fact that it's a trap
        if (iceTrapScale >= 0.01) {
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, (0 - play->gameplayFrames) % 128, 32, 32, 1, 0,
                                          (play->gameplayFrames * -2) % 128, 32, 32, 0, -1, 0, -2));

            Matrix_Translate(0.0f, -40.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(iceTrapScale, iceTrapScale, iceTrapScale, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 50, 100, 255);
            gSPDisplayList(POLY_XLU_DISP++, gEffIceFragment3DL);

            // Reset matrix for the fake item model because we're animating the size of the ice block around it before
            // this.
            Matrix_Translate(refPos->x + (3.3f * Math_SinS(this->actor.shape.rot.y)), refPos->y + height,
                             refPos->z + ((3.3f + (IREG(90) / 10.0f)) * Math_CosS(this->actor.shape.rot.y)),
                             MTXMODE_NEW);
            Matrix_RotateZYX(0, play->gameplayFrames * 1000, 0, MTXMODE_APPLY);
            Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
        }

        // Draw fake item model.
        if (this->getItemEntry.drawFunc != NULL) {
            this->getItemEntry.drawFunc(play, &this->getItemEntry);
        } else {
            GetItem_Draw(play, drawIdPlusOne - 1);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawGetItemImpl(PlayState* play, Player* this, Vec3f* refPos, s32 drawIdPlusOne) {
    f32 height = (this->exchangeItemId != EXCH_ITEM_NONE) ? 6.0f : 14.0f;

    OPEN_DISPS(play->state.gfxCtx);

    gSegments[6] = VIRTUAL_TO_PHYSICAL(this->giObjectSegment);

    gSPSegment(POLY_OPA_DISP++, 0x06, this->giObjectSegment);
    gSPSegment(POLY_XLU_DISP++, 0x06, this->giObjectSegment);

    Matrix_Translate(refPos->x + (3.3f * Math_SinS(this->actor.shape.rot.y)), refPos->y + height,
                     refPos->z + ((3.3f + (IREG(90) / 10.0f)) * Math_CosS(this->actor.shape.rot.y)), MTXMODE_NEW);
    Matrix_RotateZYX(0, play->gameplayFrames * 1000, 0, MTXMODE_APPLY);
    Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);

    if ((play->sceneNum == SCENE_HERETICS_TOMB) && (drawIdPlusOne - 1 == GID_SWORD_BGS)) {
        // Heretics Tomb: the get-item there is the Ice Katana (it rides on the Biggoron's Sword get-item): held up
        // over Link's head lying almost level, the point a little raised (as most items are shown), turning, in its
        // own model
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
        Matrix_RotateZ(0.35f, MTXMODE_APPLY);        // (about 20 degrees up from level)
        Matrix_RotateX(-0.5f, MTXMODE_APPLY);        // (the blade's flat tipped toward the camera a little)
        Matrix_Translate(-2300.0f, 0.0f, 0.0f, MTXMODE_APPLY); // (its middle on the spot)
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)KATANA_HELD_DL);
    } else if (this->getItemEntry.modIndex == MOD_RANDOMIZER && this->getItemEntry.getItemId == RG_ICE_TRAP) {
        Player_DrawGetItemIceTrap(play, this, refPos, drawIdPlusOne, height);
    } else if (this->getItemEntry.modIndex == MOD_RANDOMIZER &&
               (this->getItemEntry.getItemId == RG_TRIFORCE_PIECE || this->getItemEntry.getItemId == RG_TRIFORCE)) {
        Randomizer_DrawTriforcePieceGI(play, this->getItemEntry);
    } else if (this->getItemEntry.drawFunc != NULL) {
        this->getItemEntry.drawFunc(play, &this->getItemEntry);
    } else {
        GetItem_Draw(play, drawIdPlusOne - 1);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawGetItem(PlayState* play, Player* this) {
    // if (!this->giObjectLoading || !osRecvMesg(&this->giObjectLoadQueue, NULL, OS_MESG_NOBLOCK)) // OTRTODO: Do
    // something about osRecvMesg here...
    {
        this->giObjectLoading = false;
        Player_DrawGetItemImpl(play, this, &sGetItemRefPos, ABS(this->unk_862));
    }
}

void func_80090A28(Player* this, Vec3f* vecs) {
    D_8012608C.x = D_80126080.x;

    if (this->unk_845 >= 3) {
        this->unk_845 += 1;
        D_8012608C.x *= 1.0f + ((9 - this->unk_845) * 0.1f);
    }

    D_8012608C.x += 1200.0f;
    D_80126098.x = D_8012608C.x;

    Matrix_MultVec3f(&D_80126080, &vecs[0]);
    Matrix_MultVec3f(&D_8012608C, &vecs[1]);
    Matrix_MultVec3f(&D_80126098, &vecs[2]);
}

void Player_DrawHookshotReticle(PlayState* play, Player* this, f32 hookshotRange) {
    static Vec3f D_801260C8 = { -500.0f, -100.0f, 0.0f };
    CollisionPoly* colPoly;
    s32 bgId;
    Vec3f hookshotStart;
    Vec3f hookshotEnd;
    Vec3f firstHit;
    Vec3f sp68;
    f32 sp64;

    D_801260C8.z = 0.0f;
    Matrix_MultVec3f(&D_801260C8, &hookshotStart);
    D_801260C8.z = hookshotRange;
    Matrix_MultVec3f(&D_801260C8, &hookshotEnd);

    if (BgCheck_AnyLineTest3(&play->colCtx, &hookshotStart, &hookshotEnd, &firstHit, &colPoly, 1, 1, 1, 1, &bgId)) {
        OPEN_DISPS(play->state.gfxCtx);

        OVERLAY_DISP = Gfx_SetupDL(OVERLAY_DISP, 0x07);

        SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &firstHit, &sp68, &sp64);

        const f32 sp60 = (sp64 < 200.0f) ? 0.08f : (sp64 / 200.0f) * 0.08f;

        Matrix_Translate(firstHit.x, firstHit.y, firstHit.z, MTXMODE_NEW);
        Matrix_Scale(sp60, sp60, sp60, MTXMODE_APPLY);

        gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        if (GameInteractor_Should(VB_TARGETABLE_HOOKSHOT_RETICLE, true, colPoly, bgId)) {
            gSPSegment(OVERLAY_DISP++, 0x06, play->objectCtx.status[this->actor.objBankIndex].segment);
            gSPDisplayList(OVERLAY_DISP++, gLinkAdultHookshotReticleDL);
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

// Coordinates of the player focus position, in the head limb's own model space.
Vec3f sPlayerFocusOffsetFromHead = { 1100.0f, -700.0f, 0.0f };

f32 sMeleeWeaponLengths[] = {
    0.0f, 4000.0f, 3000.0f, 5500.0f, 0.0f, 2500.0f,
};

f32 sSwordTypes[] = {
    TRAIL_TYPE_REST,           TRAIL_TYPE_MASTER_SWORD, TRAIL_TYPE_KOKIRI_SWORD,
    TRAIL_TYPE_BIGGORON_SWORD, TRAIL_TYPE_REST,         TRAIL_TYPE_HAMMER,
};

Gfx* sBottleDLists[] = { gLinkAdultBottleDL, gLinkChildBottleDL };

Color_RGB8 sBottleColors[] = {
    { 255, 255, 255 }, { 80, 80, 255 },   { 255, 100, 255 }, { 0, 0, 255 }, { 255, 0, 255 },
    { 255, 0, 255 },   { 200, 200, 100 }, { 255, 0, 0 },     { 0, 0, 255 }, { 0, 255, 0 },
    { 255, 255, 255 }, { 255, 255, 255 }, { 80, 80, 255 },
};

Vec3f sLeftHandArrowVec3 = { 398.0f, 1419.0f, 244.0f };

BowStringData sBowStringData[] = {
    { gLinkAdultBowStringDL, { 0.0f, -360.4f, 0.0f } },        // bow
    { gLinkChildSlingshotStringDL, { 606.0f, 236.0f, 0.0f } }, // slingshot
};

Vec3f sRightHandLimbModelShieldQuadVertices[] = {
    { -4500.0f, -3000.0f, -600.0f },
    { 1500.0f, -3000.0f, -600.0f },
    { -4500.0f, 3000.0f, -600.0f },
    { 1500.0f, 3000.0f, -600.0f },
};

Vec3f D_80126184 = { 100.0f, 1500.0f, 0.0f };
Vec3f D_80126190 = { 100.0f, 1640.0f, 0.0f };

Vec3f sSheathLimbModelShieldQuadVertices[] = {
    { -3000.0f, -3000.0f, -900.0f },
    { 3000.0f, -3000.0f, -900.0f },
    { -3000.0f, 3000.0f, -900.0f },
    { 3000.0f, 3000.0f, -900.0f },
};

Vec3f sSheathLimbModelShieldOnBackPos = { 630.0f, 100.0f, -30.0f };
Vec3s sSheathLimbModelShieldOnBackZyxRot = { 0, 0, 0x7FFF };

Vec3f sLeftRightFootLimbModelFootPos[] = {
    { 200.0f, 300.0f, 0.0f },
    { 200.0f, 200.0f, 0.0f },
};

void Player_PostLimbDrawGameplay(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    Player* this = (Player*)thisx;

    if (*dList != NULL) {
        Matrix_MultVec3f(&sZeroVec, D_80160000);
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        MtxF sp14C;
        Actor* hookedActor;

        Math_Vec3f_Copy(&this->leftHandPos, D_80160000);

        // Heretics Tomb: the katana, in the fist or in its scabbard on his hip
        sKatHandEqValid = false;
        sKatSayaValid = false;
        if (sKatWornValid && !((this->leftHandType == PLAYER_MODELTYPE_LH_BGS) &&
                               !(this->stateFlags1 & PLAYER_STATE1_SHIELDING))) {
            // sheathed (and while he crouches: no point holding it out)
            sKatSaya = sKatWorn;
            sKatSayaValid = true;
            Matrix_Push();
            Matrix_Put(&sKatWorn);
            OPEN_DISPS(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)KATANA_HELD_DL);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)KAT_SAYA_DL);
            CLOSE_DISPS(play->state.gfxCtx);
            Matrix_Pop();
        } else if (sKatWornValid) {
            // The hilt runs through the middle of the fist (KAT_GRIP_Y) and Link holds its end (KAT_POMMEL_SHIFT).
            // Out of a swing the katana is held low, the point down and behind him on his right (he is left-handed:
            // the blade crosses in front of him); charging and drawing the Iai, the arm IK sets its direction.
            // Whatever way the animation points the blade, it is turned in place about the grip.
            s32 charging = (this->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) != 0;
            s32 aim = (this->meleeWeaponState == 0) || charging || sKatAimOverride;

            Matrix_Push();
            if (aim) {
                static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
                static Vec3f sAlong = { 1000.0f, 0.0f, 0.0f };
                static Vec3f sEdge = { 0.0f, 1000.0f, 0.0f };
                static Vec3f sGrip = { 0.0f, KAT_GRIP_Y, 0.0f };
                f32 fx = Math_SinS(this->actor.shape.rot.y), fz = Math_CosS(this->actor.shape.rot.y);
                Vec3f f = Kat_V(fx, 0.0f, fz), r = Kat_V(-fz, 0.0f, fx), u = Kat_V(0.0f, 1.0f, 0.0f);
                Vec3f o, t, e, g, d, want, out, axis, ep, op;
                MtxF hand;
                f32 len, roll;

                if (sKatAimOverride) {
                    want = sKatWant;
                    out = sKatOut;
                } else if (charging) {
                    want = Kat_Unit(Kat_Body(r, f, u, 0.55f, -0.8f, -0.25f));
                    out = Kat_Body(r, f, u, 0.3f, 0.0f, 1.0f);
                } else {
                    want = Kat_Unit(Kat_Body(r, f, u, 0.3f, -0.85f, -0.45f)); // low on his right, the point behind
                    out = Kat_Body(r, f, u, 1.0f, 0.0f, -0.2f);
                }

                Matrix_MultVec3f(&sGrip, &g);
                Matrix_MultVec3f(&sOrigin, &o);
                Matrix_MultVec3f(&sAlong, &t);
                d = Kat_Unit(Kat_Sub(t, o));
                axis = Kat_Cross(d, want);
                len = sqrtf(Kat_Dot(axis, axis));
                if (len > 0.001f) {
                    // world-space turn about the grip: T(g) * R * T(-g) * hand
                    axis = Kat_Scale(axis, 1.0f / len);
                    Matrix_Get(&hand);
                    Matrix_Translate(g.x, g.y, g.z, MTXMODE_NEW);
                    Matrix_RotateAxis(acosf(CLAMP(Kat_Dot(d, want), -1.0f, 1.0f)), &axis, MTXMODE_APPLY);
                    Matrix_Translate(-g.x, -g.y, -g.z, MTXMODE_APPLY);
                    Matrix_Mult(&hand, MTXMODE_APPLY);
                }
                // then about the blade's own length, so the cutting edge (the model's +y) faces `out`
                Matrix_MultVec3f(&sOrigin, &o);
                Matrix_MultVec3f(&sEdge, &e);
                e = Kat_Sub(e, o);
                ep = Kat_Sub(e, Kat_Scale(want, Kat_Dot(e, want)));
                op = Kat_Sub(out, Kat_Scale(want, Kat_Dot(out, want)));
                roll = atan2f(Kat_Dot(Kat_Cross(ep, op), want), Kat_Dot(ep, op));
                Matrix_Get(&hand);
                Matrix_Translate(g.x, g.y, g.z, MTXMODE_NEW);
                Matrix_RotateAxis(roll, &want, MTXMODE_APPLY);
                Matrix_Translate(-g.x, -g.y, -g.z, MTXMODE_APPLY);
                Matrix_Mult(&hand, MTXMODE_APPLY);
            }
            Matrix_Get(&sKatHandEq);
            sKatHandEqValid = true;
            // carried, Link holds the end of the hilt (the right hand below the guard); cutting and drawing, the
            // left fist is up against the guard and the right one behind it
            Matrix_Translate(KAT_CUT_SHIFT + (KAT_POMMEL_SHIFT - KAT_CUT_SHIFT) * sKatCarry, KAT_GRIP_Y, 0.0f,
                             MTXMODE_APPLY);
            {
                MtxF k;
                Vec3f hilt = { KAT_RIGHT_CUT_X + (KAT_RIGHT_GRIP_X - KAT_RIGHT_CUT_X) * sKatCarry, 0.0f, 0.0f };
                Vec3f mouth = { KAT_SAYA_MOUTH_X, 0.0f, 0.0f };

                Matrix_Get(&k);
                if (charging) {
                    // charging the Iai the blade stays in the scabbard: both come off the hip with the fist
                    Kat_BlendMtx(&k, &sKatWorn, &k, sKatIkWeight, this->actor.scale.x);
                    Matrix_Put(&k);
                    sKatSaya = k;
                } else {
                    sKatSaya = sKatWorn;
                }
                sKatSayaValid = true;
                // the right hand: on the scabbard's mouth through the Iai (charge and draw), else on the hilt
                if (sKatIaiRelease) {
                    // easing out of the Iai: off the scabbard's mouth onto the hilt
                    Vec3f a, b;

                    SkinMatrix_Vec3fMtxFMultXYZ(&sKatSaya, &mouth, &a);
                    SkinMatrix_Vec3fMtxFMultXYZ(&k, &hilt, &b);
                    sKatRightTarget = Kat_Lerp(b, a, sKatIkWeight);
                } else if (charging || sKatIaiArms) {
                    SkinMatrix_Vec3fMtxFMultXYZ(&sKatSaya, &mouth, &sKatRightTarget);
                } else {
                    SkinMatrix_Vec3fMtxFMultXYZ(&k, &hilt, &sKatRightTarget);
                }
            }
            OPEN_DISPS(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)KATANA_HELD_DL);
            Matrix_Put(&sKatSaya);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)KAT_SAYA_DL);
            CLOSE_DISPS(play->state.gfxCtx);
            Matrix_Pop();
        }

        if (this->itemAction == PLAYER_IA_DEKU_STICK) {
            Vec3f sp124[3];

            OPEN_DISPS(play->state.gfxCtx);

            if (this->actor.scale.y >= 0.0f) {
                D_80126080.x = this->unk_85C * 5000.0f;
                func_80090A28(this, sp124);
                if (this->meleeWeaponState != 0) {
                    EffectBlureShip_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex), TRAIL_TYPE_STICK);
                    func_800906D4(play, this, sp124);
                } else {
                    Math_Vec3f_Copy(&this->meleeWeaponInfo[0].tip, &sp124[0]);
                }
            }

            Matrix_Translate(-428.26f, 267.2f, -33.82f, MTXMODE_APPLY);
            Matrix_RotateZYX(-0x8000, 0, 0x4000, MTXMODE_APPLY);
            Matrix_Scale(1.0f, this->unk_85C, 1.0f, MTXMODE_APPLY);

            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, gLinkChildLinkDekuStickDL);

            CLOSE_DISPS(play->state.gfxCtx);
        } else if ((this->actor.scale.y >= 0.0f) && (this->meleeWeaponState != 0)) {
            Vec3f spE4[3];

            if (Player_HoldsBrokenKnife(this)) {
                D_80126080.x = 1500.0f;
            } else {
                D_80126080.x = sMeleeWeaponLengths[Player_GetMeleeWeaponHeld(this)];
                EffectBlureShip_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex),
                                           sSwordTypes[Player_GetMeleeWeaponHeld(this)]);
            }

            // Heretics Tomb: the katana's hitbox and trail run along the katana as drawn (turned by the Iai)
            if (sKatHandEqValid) {
                D_80126080.x = KAT_SWORD_LENGTH;
                Matrix_Push();
                Matrix_Put(&sKatHandEq);
            }
            func_80090A28(this, spE4);
            func_800906D4(play, this, spE4);
            if (sKatHandEqValid) {
                Matrix_Pop();
            }
        } else if ((*dList != NULL) && (this->leftHandType == PLAYER_MODELTYPE_LH_BOTTLE)) {
            Color_RGB8* bottleColor = &sBottleColors[Player_ActionToBottle(this, this->itemAction)];

            OPEN_DISPS(play->state.gfxCtx);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            if (GameInteractor_Should(VB_PLAYER_DRAW_BOTTLE, true, this, play)) {
                gDPSetEnvColor(POLY_XLU_DISP++, bottleColor->r, bottleColor->g, bottleColor->b, 0);
                gSPDisplayList(POLY_XLU_DISP++, sBottleDLists[gSaveContext.linkAge]);
            }

            CLOSE_DISPS(play->state.gfxCtx);
        }

        if (this->actor.scale.y >= 0.0f) {
            if (!Player_HoldsHookshot(this) && ((hookedActor = this->heldActor) != NULL)) {
                if (this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) {
                    Matrix_MultVec3f(&sLeftHandArrowVec3, &hookedActor->world.pos);
                    Matrix_RotateZYX(0x69E8, -0x5708, 0x458E, MTXMODE_APPLY);
                    Matrix_Get(&sp14C);
                    Matrix_MtxFToYXZRotS(&sp14C, &hookedActor->world.rot, 0);
                    hookedActor->shape.rot = hookedActor->world.rot;
                } else if (this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
                    Vec3s spB8;

                    Matrix_Get(&sp14C);
                    Matrix_MtxFToYXZRotS(&sp14C, &spB8, 0);

                    if (hookedActor->flags & ACTOR_FLAG_CARRY_X_ROT_INFLUENCE) {
                        hookedActor->world.rot.x = hookedActor->shape.rot.x = spB8.x - this->unk_3BC.x;
                    } else {
                        hookedActor->world.rot.y = hookedActor->shape.rot.y = this->actor.shape.rot.y + this->unk_3BC.y;
                    }
                }
            } else {
                Matrix_Get(&this->mf_9E0);
                Matrix_MtxFToYXZRotS(&this->mf_9E0, &this->unk_3BC, 0);
                // Heretics Tomb: the charge glow and its sparks (En_M_Thunder, Eff_Dust) gather on the katana's
                // blade, from the guard out, wherever the Iai pose puts it
                if (sKatSayaValid && (this->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
                    // charging: on the scabbard, the blade inside it (Eff_Dust's sparks gather on its middle)
                    Matrix_Push();
                    Matrix_Put(&sKatSaya);
                    Matrix_Translate(3400.0f, -440.0f, 20.0f, MTXMODE_APPLY);
                    Matrix_Get(&this->mf_9E0);
                    Matrix_Pop();
                } else if (sKatHandEqValid) {
                    Matrix_Push();
                    Matrix_Put(&sKatHandEq);
                    Matrix_Translate(1500.0f, 0.0f, 0.0f, MTXMODE_APPLY);
                    Matrix_Get(&this->mf_9E0);
                    Matrix_Pop();
                }
            }
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        Actor* heldActor = this->heldActor;

        if (this->rightHandType == PLAYER_MODELTYPE_RH_FF) {
            Matrix_Get(&this->shieldMf);
        } else if ((this->rightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT) ||
                   (this->rightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2)) {
            s32 stringModelToUse = gSaveContext.linkAge;
            if (CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
                stringModelToUse = Player_HoldsSlingshot(this);
            }
            BowStringData* stringData = &sBowStringData[stringModelToUse];

            OPEN_DISPS(play->state.gfxCtx);

            Matrix_Push();
            Matrix_Translate(stringData->pos.x, stringData->pos.y, stringData->pos.z, MTXMODE_APPLY);

            if ((this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && (this->unk_860 >= 0) && (this->unk_834 <= 10)) {
                Vec3f sp90;
                f32 distXYZ;

                Matrix_MultVec3f(&sZeroVec, &sp90);
                distXYZ = Math_Vec3f_DistXYZ(D_80160000, &sp90);

                this->unk_858 = distXYZ - 3.0f;
                if (distXYZ < 3.0f) {
                    this->unk_858 = 0.0f;
                } else {
                    this->unk_858 *= 1.6f;
                    if (this->unk_858 > 1.0f) {
                        this->unk_858 = 1.0f;
                    }
                }

                this->unk_85C = -0.5f;
            }

            Matrix_Scale(1.0f, this->unk_858, 1.0f, MTXMODE_APPLY);

            if (!LINK_IS_ADULT) {
                Matrix_RotateZ(this->unk_858 * -0.2f, MTXMODE_APPLY);
            }

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, stringData->dList);

            Matrix_Pop();

            CLOSE_DISPS(play->state.gfxCtx);
        } else if ((this->actor.scale.y >= 0.0f) && (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD)) {
            Matrix_Get(&this->shieldMf);
            Player_UpdateShieldCollider(play, this, &this->shieldQuad, sRightHandLimbModelShieldQuadVertices);
        }

        if (this->actor.scale.y >= 0.0f) {
            if (GameInteractor_Should(VB_DRAW_ADDITIONAL_RETICLES,
                                      (this->heldItemAction == PLAYER_IA_HOOKSHOT) ||
                                          (this->heldItemAction == PLAYER_IA_LONGSHOT),
                                      this)) {
                Matrix_MultVec3f(&D_80126184, &this->unk_3C8);

                if (heldActor != NULL) {
                    MtxF sp44;
                    s32 pad;

                    Matrix_MultVec3f(&D_80126190, &heldActor->world.pos);
                    Matrix_RotateZYX(0, -0x4000, -0x4000, MTXMODE_APPLY);
                    Matrix_Get(&sp44);
                    Matrix_MtxFToYXZRotS(&sp44, &heldActor->world.rot, 0);
                    heldActor->shape.rot = heldActor->world.rot;

                    if (func_8002DD78(this) != 0) {
                        Matrix_Translate(500.0f, 300.0f, 0.0f, MTXMODE_APPLY);
                        Player_DrawHookshotReticle(
                            play, this,
                            ((this->heldItemAction == PLAYER_IA_HOOKSHOT) ? 38600.0f : 77600.0f) *
                                CVarGetFloat(CVAR_CHEAT("HookshotReachMultiplier"), 1.0f));
                    }
                }
            }

            if ((this->unk_862 != 0) || ((func_8002DD6C(this) == 0) && (heldActor != NULL))) {
                if (!(this->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) && (this->unk_862 != 0) &&
                    (this->exchangeItemId != EXCH_ITEM_NONE)) {
                    Math_Vec3f_Copy(&sGetItemRefPos, &this->leftHandPos);
                } else {
                    sGetItemRefPos.x = (this->bodyPartsPos[15].x + this->leftHandPos.x) * 0.5f;
                    sGetItemRefPos.y = (this->bodyPartsPos[15].y + this->leftHandPos.y) * 0.5f;
                    sGetItemRefPos.z = (this->bodyPartsPos[15].z + this->leftHandPos.z) * 0.5f;
                }

                if (this->unk_862 == 0) {
                    Math_Vec3f_Copy(&heldActor->world.pos, &sGetItemRefPos);
                }
            }
        }
    } else if (this->actor.scale.y >= 0.0f) {
        if (limbIndex == PLAYER_LIMB_SHEATH) {
            if ((this->rightHandType != PLAYER_MODELTYPE_RH_SHIELD) &&
                (this->rightHandType != PLAYER_MODELTYPE_RH_FF)) {
                if (Player_IsChildWithHylianShield(this)) {
                    Player_UpdateShieldCollider(play, this, &this->shieldQuad, sSheathLimbModelShieldQuadVertices);
                }

                Matrix_TranslateRotateZYX(&sSheathLimbModelShieldOnBackPos, &sSheathLimbModelShieldOnBackZyxRot);
                Matrix_Get(&this->shieldMf);
            }
        } else if (limbIndex == PLAYER_LIMB_HEAD) {
            Matrix_MultVec3f(&sPlayerFocusOffsetFromHead, &this->actor.focus.pos);
        } else {
            Vec3f* vec = &sLeftRightFootLimbModelFootPos[(gSaveContext.linkAge)];

            Actor_SetFeetPos(&this->actor, limbIndex, PLAYER_LIMB_L_FOOT, vec, PLAYER_LIMB_R_FOOT, vec);
        }
    }
}

u32 func_80091738(PlayState* play, u8* segment, SkelAnime* skelAnime) {
    s16 linkObjectId = gLinkObjectIds[gSaveContext.linkAge];
    size_t size;
    void* ptr;

    size = gObjectTable[OBJECT_GAMEPLAY_KEEP].vromEnd - gObjectTable[OBJECT_GAMEPLAY_KEEP].vromStart;
    ptr = segment + 0x3800;
    DmaMgr_SendRequest1(ptr, gObjectTable[OBJECT_GAMEPLAY_KEEP].vromStart, size, __FILE__, __LINE__);

    size = gObjectTable[linkObjectId].vromEnd - gObjectTable[linkObjectId].vromStart;
    ptr = segment + 0x8800;
    DmaMgr_SendRequest1(ptr, gObjectTable[linkObjectId].vromStart, size, __FILE__, __LINE__);

    ptr = (void*)ALIGN16((intptr_t)ptr + size);

    gSegments[4] = VIRTUAL_TO_PHYSICAL(segment + 0x3800);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(segment + 0x8800);

    SkelAnime_InitLink(play, skelAnime, gPlayerSkelHeaders[gSaveContext.linkAge], &gPlayerAnim_link_normal_wait, 9, ptr,
                       ptr, PLAYER_LIMB_MAX);

    return size + 0x8800 + 0x90;
}

u8 sPauseModelGroupBySword[] = {
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_SWORD_KOKIRI
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_SWORD_MASTER
    PLAYER_MODELGROUP_BGS,              // PLAYER_SWORD_BIGGORON
};

// Heretics Tomb: the pause screen's Link with the katana: the Iai stance - his left hand on the hilt of the sheathed
// katana at his right hip, his right hand on the scabbard's mouth, the shield on his back (the arms solved offline
// from the vanilla shield pose to the katana's hilt and scabbard mouth, as Player_PostLimbDrawPauseKatana wears it):
// each joint's change from the vanilla shield pose (gLinkPauseAdultShieldJointTable, from the player's game data)
static Vec3s sKatanaPauseJointDelta[] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { -1079, 7482, -2896 }, { 12, 18, 5958 }, { -9138, -1420, -28780 }, { -15, -1523, 128 }, { 3645, -8943, -4684 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };

static s32 Player_PauseKatana(u8* playerSwordAndShield) {
    return LINK_IS_ADULT && Katana_IsEquipped() && (playerSwordAndShield[0] == PLAYER_SWORD_MASTER);
}

s32 Player_OverrideLimbDrawPause(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    u8* playerSwordAndShield = arg;
    // SOH: Ensure positive value from playerSwordAndShield[] to avoid OOB array access.
    //      This can occur in the case where playerSwordAndShield[0] is PLAYER_SWORD_NONE
    u8 modelGroup =
        sPauseModelGroupBySword[playerSwordAndShield[0] > 0 ? playerSwordAndShield[0] - PLAYER_SWORD_KOKIRI : 0];

    if (Player_PauseKatana(playerSwordAndShield)) {
        modelGroup = PLAYER_MODELGROUP_BGS; // (two-handed: a closed fist each side, the shield on his back)
    }
    s32 type;
    s32 dListOffset = 0;
    Gfx** dLists;
    size_t ptrSize = sizeof(uint32_t);

    if ((modelGroup == PLAYER_MODELGROUP_SWORD_AND_SHIELD) && !LINK_IS_ADULT &&
        (playerSwordAndShield[1] == PLAYER_SHIELD_HYLIAN)) {
        modelGroup = PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD;
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_LEFT_HAND];
        sLeftHandType = type;

        // SOH: Handle unexpected swordless case. Previously OOB array access is avoided, but we want the
        //      hand model-type to be set to open (otherwise it is set to holding sword model-type)
        if (playerSwordAndShield[0] == PLAYER_SWORD_NONE) {
            type = PLAYER_MODELTYPE_LH_OPEN;
        }

        if ((type == PLAYER_MODELTYPE_LH_BGS) && (gSaveContext.swordHealth <= 0.0f)) {
            dListOffset = 4;
        }
        if ((type == PLAYER_MODELTYPE_LH_BGS) && Player_PauseKatana(playerSwordAndShield)) {
            type = PLAYER_MODELTYPE_LH_CLOSED; // (the fist round the katana's hilt)
            dListOffset = 0;
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
        sRightHandType = type;
        if (type == PLAYER_MODELTYPE_RH_SHIELD) {
            dListOffset = playerSwordAndShield[1] * ptrSize;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_SHEATH];
        // Heretics Tomb: with the katana the sheath on his back is empty (the katana is on his hip, drawn after)
        if (LINK_IS_ADULT && Katana_IsEquipped() && (playerSwordAndShield[0] == PLAYER_SWORD_MASTER)) {
            if (type == PLAYER_MODELTYPE_SHEATH_18) {
                type = PLAYER_MODELTYPE_SHEATH_19;
            } else if (type == PLAYER_MODELTYPE_SHEATH_16) {
                type = PLAYER_MODELTYPE_SHEATH_17;
            }
        }
        if ((type == PLAYER_MODELTYPE_SHEATH_18) || (type == PLAYER_MODELTYPE_SHEATH_19)) {
            dListOffset = playerSwordAndShield[1] * ptrSize;
        }
    } else if (limbIndex == PLAYER_LIMB_WAIST) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_WAIST];

        if (Player_IsCustomLinkModel()) {
            return 0;
        }
    } else {
        return 0;
    }

    dLists = &sPlayerDListGroups[type][gSaveContext.linkAge];
    *dList = dLists[dListOffset];

    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW_PAUSE, true, limbIndex, dList, GET_PLAYER(play), play);

    return 0;
}

#include <overlays/actors/ovl_Demo_Effect/z_demo_effect.h>
void DemoEffect_DrawTriforceSpot(Actor* thisx, PlayState* play);

void Pause_DrawTriforceSpot(PlayState* play, s32 showLightColumn) {
    static DemoEffect triforce;
    static s16 rotation = 0;

    triforce.triforceSpot.crystalLightOpacity = 244;
    triforce.triforceSpot.triforceSpotOpacity = 249;
    triforce.triforceSpot.lightColumnOpacity = showLightColumn ? 244 : 0;
    triforce.triforceSpot.rotation = rotation;

    DemoEffect_DrawTriforceSpot(&triforce, play);

    rotation += 0x03E8;
}

// Heretics Tomb: the pause screen's Link wears the katana in its scabbard on his right hip, as in the game
static MtxF sKatPauseRoot;
static void Player_PostLimbDrawPauseKatana(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* arg) {
    u8* playerSwordAndShield = arg;
    Vec3f zero = { 0.0f, 0.0f, 0.0f }, waist, r, f, u, x, y, mouth, origin;
    f32 s;
    MtxF worn;

    if ((limbIndex != PLAYER_LIMB_WAIST) || !LINK_IS_ADULT || !Katana_IsEquipped() ||
        (playerSwordAndShield[0] != PLAYER_SWORD_MASTER)) {
        return;
    }
    Matrix_MultVec3f(&zero, &waist);
    s = sqrtf(SQ(sKatPauseRoot.xx) + SQ(sKatPauseRoot.yx) + SQ(sKatPauseRoot.zx));
    if (s < 0.00001f) {
        return;
    }
    // the model's own axes: right -x, forward +z, up +y
    r = Kat_Unit(Kat_V(-sKatPauseRoot.xx, -sKatPauseRoot.yx, -sKatPauseRoot.zx));
    f = Kat_Unit(Kat_V(sKatPauseRoot.xz, sKatPauseRoot.yz, sKatPauseRoot.zz));
    u = Kat_Unit(Kat_V(sKatPauseRoot.xy, sKatPauseRoot.yy, sKatPauseRoot.zy));
    x = Kat_Unit(Kat_Body(r, f, u, 0.6f, -0.7f, -0.35f));
    y = Kat_Body(r, f, u, 0.3f, 0.0f, 1.0f);
    y = Kat_Unit(Kat_Sub(y, Kat_Scale(x, Kat_Dot(x, y))));
    mouth = Kat_Add(waist, Kat_Scale(Kat_Body(r, f, u, 450.0f, 150.0f, -80.0f), s));
    origin = Kat_Sub(mouth, Kat_Scale(x, KAT_SAYA_MOUTH_X * s));
    Kat_Frame3(&worn, origin, x, y, Kat_Cross(x, y), s);

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Put(&worn);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)KATANA_HELD_DL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)KAT_SAYA_DL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawPauseImpl(PlayState* play, void* gameplayKeep, void* linkObject, SkelAnime* skelAnime, Vec3f* pos,
                          Vec3s* rot, f32 scale, s32 sword, s32 tunic, s32 shield, s32 boots, s32 width, s32 height,
                          Vec3f* eye, Vec3f* at, f32 fovy, void* colorFrameBuffer, void* depthFrameBuffer) {
    // Note: the viewport x and y values are overwritten below, before usage
    static Vp viewport = { (PAUSE_EQUIP_PLAYER_WIDTH / 2) << 2, (PAUSE_EQUIP_PLAYER_HEIGHT / 2) << 2, G_MAXZ / 2, 0,
                           (PAUSE_EQUIP_PLAYER_WIDTH / 2) << 2, (PAUSE_EQUIP_PLAYER_HEIGHT / 2) << 2, G_MAXZ / 2, 0 };
    static Lights1 lights1 = gdSPDefLights1(80, 80, 80, 255, 255, 255, 84, 84, -84);
    static Vec3f lightDir = { 89.8f, 0.0f, 89.8f };
    u8 playerSwordAndShield[2];
    Gfx* opaRef;
    Gfx* xluRef;
    u16 perspNorm;
    Mtx* perspMtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));
    Mtx* lookAtMtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));

    u8 mirrorWorldActive = CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0);

    OPEN_DISPS(play->state.gfxCtx);

    opaRef = POLY_OPA_DISP;
    POLY_OPA_DISP++;

    xluRef = POLY_XLU_DISP;
    POLY_XLU_DISP++;

    gSPDisplayList(WORK_DISP++, POLY_OPA_DISP);
    gSPDisplayList(WORK_DISP++, POLY_XLU_DISP);

    if (mirrorWorldActive) {
        gSPSetExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
        gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    }

    gSPSegment(POLY_OPA_DISP++, 0x00, NULL);

    gDPPipeSync(POLY_OPA_DISP++);

    gSPLoadGeometryMode(POLY_OPA_DISP++, 0);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
    gDPSetOtherMode(POLY_OPA_DISP++,
                    G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_PERSP | G_CYC_FILL | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_NOOP2);

    // Also matches if some of the previous graphics commands are moved inside this block too. Possible macro?
    if (1) {
        s32 pad[2];

        gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    }

    gDPSetScissor(POLY_OPA_DISP++, G_SC_NON_INTERLACE, 0, 0, width, height);
    gSPClipRatio(POLY_OPA_DISP++, FRUSTRATIO_1);

    gDPSetColorImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, depthFrameBuffer);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_FILL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(POLY_OPA_DISP++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));
    gDPFillRectangle(POLY_OPA_DISP++, 0, 0, width - 1, height - 1);

    gDPPipeSync(POLY_OPA_DISP++);

    gDPSetColorImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, colorFrameBuffer);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_FILL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(POLY_OPA_DISP++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(POLY_OPA_DISP++, 0, 0, width - 1, height - 1);

    gDPPipeSync(POLY_OPA_DISP++);

    gDPSetDepthImage(POLY_OPA_DISP++, depthFrameBuffer);

    viewport.vp.vscale[0] = viewport.vp.vtrans[0] = width * ((1 << 2) / 2);
    viewport.vp.vscale[1] = viewport.vp.vtrans[1] = height * ((1 << 2) / 2);
    gSPViewport(POLY_OPA_DISP++, &viewport);

    guPerspective(perspMtx, &perspNorm, fovy, (f32)width / (f32)height, 10.0f, 4000.0f, 1.0f);

    gSPPerspNormalize(POLY_OPA_DISP++, perspNorm);
    gSPMatrix(POLY_OPA_DISP++, perspMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    guLookAt(lookAtMtx, eye->x, eye->y, eye->z, at->x, at->y, at->z, 0.0f, 1.0f, 0.0f);

    gSPMatrix(POLY_OPA_DISP++, lookAtMtx, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);

    playerSwordAndShield[0] = sword;
    playerSwordAndShield[1] = shield;

    Matrix_SetTranslateRotateYXZ(
        pos->x - ((CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 0) && LINK_AGE_IN_YEARS == YEARS_ADULT)
                      ? 25
                      : 0),
        pos->y - (CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0) ? 16 : 0), pos->z, rot);
    Matrix_Scale(scale * (mirrorWorldActive ? -1 : 1), scale, scale, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 0x04, gameplayKeep);
    gSPSegment(POLY_OPA_DISP++, 0x06, linkObject);

    gSPSetLights1(POLY_OPA_DISP++, lights1);

    func_80093C80(play);

    POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP++, 0, 0, 0, 0, 997, 1000);

    func_8002EABC(pos, &play->view.eye, &lightDir, play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x0C, gCullBackDList);

    Matrix_Get(&sKatPauseRoot);
    Player_DrawImpl(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount, 0, tunic, boots, 0,
                    Player_OverrideLimbDrawPause, Player_PostLimbDrawPauseKatana, &playerSwordAndShield);

    if (CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        Matrix_SetTranslateRotateYXZ(pos->x - (LINK_AGE_IN_YEARS == YEARS_ADULT ? 25 : 0),
                                     pos->y + 280 + (LINK_AGE_IN_YEARS == YEARS_ADULT ? 48 : 0), pos->z, rot);
        Matrix_Scale(scale * (mirrorWorldActive ? -1 : 1), scale * 1, scale * 1, MTXMODE_APPLY);

        Pause_DrawTriforceSpot(play, 1);
    }

    if (mirrorWorldActive) {
        gSPClearExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
        gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    }

    gSPEndDisplayList(POLY_OPA_DISP++);
    gSPEndDisplayList(POLY_XLU_DISP++);

    gSPBranchList(opaRef, POLY_OPA_DISP);
    gSPBranchList(xluRef, POLY_XLU_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawPause(PlayState* play, u8* segment, SkelAnime* skelAnime, Vec3f* pos, Vec3s* rot, f32 scale, s32 sword,
                      s32 tunic, s32 shield, s32 boots) {
    Input* p1Input = &play->state.input[0];
    Vec3f eye = { 0.0f, 0.0f, -400.0f };
    Vec3f at = { 0.0f, 0.0f, 0.0f };
    Vec3s* destTable;
    Vec3s* srcTable;
    s32 i;
    bool canswitchrnd = false;

    gSegments[4] = VIRTUAL_TO_PHYSICAL(segment + 0x3800);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(segment + 0x8800);

    uintptr_t* PauseMenuAnimSet[4] = { // IDLE                       // Two Handed                       // No shield //
                                       // Kid Hylian Shield
                                       gPlayerAnim_link_normal_wait, gPlayerAnim_link_fighter_wait_long,
                                       gPlayerAnim_link_normal_wait_free, gPlayerAnim_link_normal_wait_free
    };

    if (CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 0) ||
        CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        uintptr_t anim = 0; // Initialise anim

        s16 EquipedStance;
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_BIGGORON) {
            EquipedStance = 1;
        } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SWORD_NONE) {
            EquipedStance = 2;
        } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SWORD_MASTER && LINK_AGE_IN_YEARS == YEARS_CHILD) {
            EquipedStance = 3;
        } else {
            // Link is idle so revert to 0
            EquipedStance = 0;
        }

        if (!CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
            anim = PauseMenuAnimSet[EquipedStance];
        } else {
            anim = gPlayerAnim_link_magic_kaze2;
            sword = 0;
            shield = 0;
        }

        if (skelAnime->animation != anim) {
            LinkAnimation_Change(play, skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP, -6.0f);
        }

        LinkAnimation_Update(play, skelAnime);

        if (!LINK_IS_ADULT) {
            // Link is placed too far up by default when animating
            at.y += 60;
        }
    } else {

        if (!LINK_IS_ADULT) {
            if (shield == PLAYER_SHIELD_DEKU) {
                srcTable = gLinkPauseChildDekuShieldJointTable;
            } else {
                srcTable = gLinkPauseChildJointTable;
            }
        } else {
            if (sword == PLAYER_SWORD_BIGGORON) {
                srcTable = gLinkPauseAdultBgsJointTable;
            } else if (shield != PLAYER_SHIELD_NONE) {
                srcTable = gLinkPauseAdultShieldJointTable;
            } else {
                srcTable = gLinkPauseAdultJointTable;
            }
        }

        srcTable = ResourceMgr_LoadArrayByNameAsVec3s(srcTable);
        Vec3s* ogSrcTable = srcTable;
        destTable = skelAnime->jointTable;
        for (i = 0; i < skelAnime->limbCount; i++) {
            *destTable++ = *srcTable++;
        }
        free(ogSrcTable);
    }

    {
        u8 ss[2] = { (u8)sword, (u8)shield };

        if (Player_PauseKatana(ss) && !CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
            // Heretics Tomb: the katana's Iai stance (held still, also with the animated pause Link)
            Vec3s* base = (Vec3s*)ResourceMgr_LoadArrayByNameAsVec3s(gLinkPauseAdultShieldJointTable);

            for (i = 0; (i < skelAnime->limbCount) && (i < ARRAY_COUNT(sKatanaPauseJointDelta)); i++) {
                skelAnime->jointTable[i].x = base[i].x + sKatanaPauseJointDelta[i].x;
                skelAnime->jointTable[i].y = base[i].y + sKatanaPauseJointDelta[i].y;
                skelAnime->jointTable[i].z = base[i].z + sKatanaPauseJointDelta[i].z;
            }
            free(base);
        }
    }

    Player_DrawPauseImpl(play, segment + 0x3800, segment + 0x8800, skelAnime, pos, rot, scale, sword, tunic, shield,
                         boots, PAUSE_EQUIP_PLAYER_WIDTH, PAUSE_EQUIP_PLAYER_HEIGHT, &eye, &at, 60.0f,
                         play->state.gfxCtx->curFrameBuffer,
                         play->state.gfxCtx->curFrameBuffer + (PAUSE_EQUIP_PLAYER_WIDTH * PAUSE_EQUIP_PLAYER_HEIGHT));
}
