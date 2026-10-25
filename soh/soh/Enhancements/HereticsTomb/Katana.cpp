#include "Katana.h"

#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ActorDB.h"
#include "soh/ResourceManagerHelpers.h"
#include <spdlog/spdlog.h>
#include "KatanaIceSlash.h"
#include "HtFlameJet.h"
#include "HtChainBarrier.h"
#include "HtWaterSpout.h"
#include "HtFrostShell.h"
#include "HtTorch.h"
#include "HtPlatform.h"
#include "HtFakeWall.h"
#include "HtRonin.h"
#include "HtKatanaPickup.h"
#include "HtOverlook.h"
#include "HtSpikes.h"
#include "HtFlameEye.h"
#include "HtBoat.h"
#include "HtGate.h"
#include "HtTentacle.h"
#include "HtArmorpha.h"
#include "HtLynel.h"
#include "HtPedestal.h"
#include "HtShadowPad.h"
#include "HtEpitaph.h"
#include "HtMoSpikes.h"
#include "HtMap.h"
#include "HtFireRing.h"
#include "HtHorse.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "sfx.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
extern PlayState* gPlayState;
}

static bool sKatanaOwned = false;
static bool sKatanaEquipped = false;
static bool sKatanaGetTextPending = false;

extern "C" bool Katana_IsOwned(void) {
    return sKatanaOwned;
}

extern "C" void Katana_SetOwned(bool owned) {
    sKatanaOwned = owned;
    if (!owned) {
        sKatanaEquipped = false;
    }
}

extern "C" bool Katana_IsEquipped(void) {
    return sKatanaOwned && sKatanaEquipped && LINK_IS_ADULT &&
           (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_MASTER) &&
           (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER);
}

extern "C" void Katana_SetEquipped(bool equipped) {
    sKatanaEquipped = equipped;
}

static bool sKatanaLocked = false;

extern "C" bool Katana_SetLocked(bool locked) {
    bool was = Katana_IsEquipped();

    sKatanaLocked = locked;
    if (locked && sKatanaEquipped) {
        sKatanaEquipped = false; // (the Master Sword it rides on stays equipped)
        if (gPlayState != nullptr) {
            Interface_LoadItemIcon1(gPlayState, 0);
        }
    }
    return was;
}

extern "C" bool Katana_IsLocked(void) {
    return sKatanaLocked;
}

// Same rule as every vanilla magic item (Magic_RequestChange): the meter must be free - idle, or only feeding the
// Lens of Truth. While Nayru's Love (or any other spell) holds it, the katana can't be swung.
static bool Katana_MagicFree(void) {
    // (Nayru's Love leaves the meter idle while its barrier lasts: it counts as holding it)
    return ((gSaveContext.magicState == MAGIC_STATE_IDLE) || (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS)) &&
           (gSaveContext.nayrusLoveTimer == 0);
}

extern "C" bool Katana_CanSwing(void) {
    if (!Katana_IsEquipped()) {
        return true;
    }
    return gSaveContext.isMagicAcquired && Katana_MagicFree() && (gSaveContext.magic >= KATANA_SLASH_MAGIC_COST);
}

extern "C" bool Katana_CanCharge(void) {
    if (!Katana_IsEquipped()) {
        return true;
    }
    return gSaveContext.isMagicAcquired && Katana_MagicFree() && (gSaveContext.magic >= KATANA_IAI_MAGIC_COST);
}

extern "C" void Katana_PaySwingCost(void) {
    if (!Katana_IsEquipped()) {
        return;
    }
    // Direct deduction rather than Magic_RequestChange: that call refuses while another
    // magic drain is in progress, which would block fast combos.
    s16 magic = gSaveContext.magic - KATANA_SLASH_MAGIC_COST;
    gSaveContext.magic = (magic < 0) ? 0 : magic;
}

// --- Ice slash projectile ---

#define KATANA_ICE_SLASH_ACTOR_NAME "Katana_IceSlash"

// --- Iai draw ---

static bool sIaiReleasePending = false; // the next swing is a charged release
static bool sIaiStrikeActive = false;   // the current swing is a paid Iai draw

extern "C" void Katana_BeginIaiRelease(void) {
    sIaiReleasePending = true;
}

extern "C" bool Katana_IsIaiStrikeActive(void) {
    return sIaiStrikeActive;
}

static u32 sSwingFrame = 0;

extern "C" bool Katana_IsIaiBladeHit(Actor* attacker) {
    Player* player = (Player*)attacker;

    if ((attacker == nullptr) || (attacker->id != ACTOR_PLAYER) || (gPlayState == nullptr)) {
        return false;
    }
    return Katana_IsEquipped() && sIaiStrikeActive && (player->heldItemAction == PLAYER_IA_SWORD_MASTER) &&
           (player->meleeWeaponState != 0) && ((gPlayState->gameplayFrames - sSwingFrame) < 60);
}

extern "C" unsigned int Katana_GetSpinDmgFlags(unsigned int vanillaFlags) {
    if (!Katana_IsEquipped()) {
        return vanillaFlags;
    }
    return sIaiStrikeActive ? 0x00800000 /* Biggoron spin */ : 0x00400000 /* Kokiri spin */;
}

extern "C" unsigned int Katana_GetGreatSpinDmgFlags(unsigned int vanillaFlags) {
    if (!Katana_IsEquipped()) {
        return vanillaFlags;
    }
    return sIaiStrikeActive ? 0x04000000 /* Biggoron jump */ : 0x02000000 /* Kokiri jump */;
}

extern "C" u32 Katana_FramesSinceSwing(PlayState* play) {
    return play->gameplayFrames - sSwingFrame;
}

extern "C" void Katana_OnSwingStart(PlayState* play, Player* player) {
    bool iai = sIaiReleasePending;
    sSwingFrame = play->gameplayFrames;
    sIaiReleasePending = false;
    sIaiStrikeActive = false;

    if (iai) {
        if (gSaveContext.isMagicAcquired && (gSaveContext.magic >= KATANA_IAI_MAGIC_COST)) {
            gSaveContext.magic -= KATANA_IAI_MAGIC_COST;
            sIaiStrikeActive = true;
            Katana_SpawnIceSlash(play, player, true);
            Audio_PlaySfxGeneral(NA_SE_IT_MAGIC_ARROW_SHOT, &player->actor.projectedPos, 4,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        } else {
            // Not enough magic: the release fizzles into a plain spin with no wave.
            Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        }
        return;
    }

    Katana_PaySwingCost();
    Katana_SpawnIceSlash(play, player, false);
}

extern "C" void Katana_SpawnIceSlash(PlayState* play, Player* player, bool iai) {
    static int sIceSlashId = -1;
    if (sIceSlashId < 0) {
        sIceSlashId = ActorDB::Instance->RetrieveId(KATANA_ICE_SLASH_ACTOR_NAME);
        if (sIceSlashId < 0) {
            return;
        }
    }

    // Start at the blade: a little ahead of Link, at waist height, travelling where he faces.
    s16 yaw = player->actor.shape.rot.y;
    Actor_Spawn(&play->actorCtx, play, sIceSlashId, player->actor.world.pos.x + Math_SinS(yaw) * 45.0f,
                player->actor.world.pos.y + 30.0f, player->actor.world.pos.z + Math_CosS(yaw) * 45.0f, 0, yaw, 0,
                iai ? 1 : 0);
}

// Registers a Heretics Tomb actor at its fixed id, refusing ids already in use: overwriting a vanilla entry
// silently replaces that actor everywhere in the game (ids 0x1C0-0x1C6 did this to the Goron pot lid, cows...).
static void HtAddActor(const ActorDBInit& init) {
    if (ActorDB::Instance->RetrieveEntry(init.id).entry.valid) {
        SPDLOG_ERROR("Heretics Tomb: actor id 0x{:X} ({}) is already taken, not registered", init.id, init.name);
        return;
    }
    ActorDB::Instance->AddEntry(init);
}

static void Katana_RegisterIceSlashActor() {
    ActorDBInit entry;
    entry.name = KATANA_ICE_SLASH_ACTOR_NAME;
    entry.desc = "Ice Katana slash";
    entry.id = HT_ACTOR_ICE_SLASH;
    entry.category = ACTORCAT_MISC;
    entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = sizeof(KatanaIceSlash);
    entry.init = KatanaIceSlash_Init;
    entry.destroy = KatanaIceSlash_Destroy;
    entry.update = KatanaIceSlash_Update;
    entry.draw = KatanaIceSlash_Draw; // the Iai's chevron of ice (a plain slash is only its sparkles)
    HtAddActor(entry);

    ActorDBInit jet;
    jet.name = "HT_FlameJet";
    jet.desc = "Heretics Tomb freezable flamethrower";
    jet.id = HT_ACTOR_FLAME_JET;
    jet.category = ACTORCAT_PROP;
    jet.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    jet.objectId = OBJECT_DODONGO; // fire sprites
    jet.instanceSize = sizeof(HtFlameJet);
    jet.init = HtFlameJet_Init;
    jet.destroy = HtFlameJet_Destroy;
    jet.update = HtFlameJet_Update;
    jet.draw = HtFlameJet_Draw;
    HtAddActor(jet);

    ActorDBInit chains;
    chains.name = "HT_ChainBarrier";
    chains.desc = "Heretics Tomb katana-only chain barrier";
    chains.id = HT_ACTOR_CHAIN_BARRIER;
    chains.category = ACTORCAT_BG;
    chains.flags = ACTOR_FLAG_ATTENTION_ENABLED; // Z-targetable
    chains.objectId = OBJECT_GAMEPLAY_DANGEON_KEEP; // the door chains
    chains.instanceSize = sizeof(HtChainBarrier);
    chains.init = HtChainBarrier_Init;
    chains.destroy = HtChainBarrier_Destroy;
    chains.update = HtChainBarrier_Update;
    chains.draw = HtChainBarrier_Draw;
    HtAddActor(chains);

    ActorDBInit spout;
    spout.name = "HT_WaterSpout";
    spout.desc = "Heretics Tomb freezable water spout";
    spout.id = HT_ACTOR_WATER_SPOUT;
    spout.category = ACTORCAT_BG;
    spout.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    spout.objectId = OBJECT_GAMEPLAY_KEEP;
    spout.instanceSize = sizeof(HtWaterSpout);
    spout.init = HtWaterSpout_Init;
    spout.destroy = HtWaterSpout_Destroy;
    spout.update = HtWaterSpout_Update;
    spout.draw = HtWaterSpout_Draw;
    HtAddActor(spout);

    ActorDBInit frost;
    frost.name = "HT_FrostShell";
    frost.desc = "Heretics Tomb frost over katana-frozen enemies";
    frost.id = HT_ACTOR_FROST_SHELL;
    frost.category = ACTORCAT_MISC;
    frost.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    frost.objectId = OBJECT_GAMEPLAY_KEEP;
    frost.instanceSize = sizeof(HtFrostShell);
    frost.init = HtFrostShell_Init;
    frost.destroy = HtFrostShell_Destroy;
    frost.update = HtFrostShell_Update;
    frost.draw = HtFrostShell_Draw;
    HtAddActor(frost);

    ActorDBInit torch;
    torch.name = "HT_Torch";
    torch.desc = "Heretics Tomb torch the katana blows out";
    torch.id = HT_ACTOR_TORCH;
    torch.category = ACTORCAT_PROP;
    torch.flags = 0;
    torch.objectId = OBJECT_SYOKUDAI;
    torch.instanceSize = sizeof(HtTorch);
    torch.init = HtTorch_Init;
    torch.destroy = HtTorch_Destroy;
    torch.update = HtTorch_Update;
    torch.draw = HtTorch_Draw;
    HtAddActor(torch);

    ActorDBInit platform;
    platform.name = "HT_Platform";
    platform.desc = "Heretics Tomb moving / spinning / wheel platform";
    platform.id = HT_ACTOR_PLATFORM;
    platform.category = ACTORCAT_BG;
    platform.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    platform.objectId = OBJECT_GAMEPLAY_KEEP;
    platform.instanceSize = sizeof(HtPlatform);
    platform.init = HtPlatform_Init;
    platform.destroy = HtPlatform_Destroy;
    platform.update = HtPlatform_Update;
    platform.draw = HtPlatform_Draw;
    HtAddActor(platform);

    ActorDBInit fakeWall;
    fakeWall.name = "HT_FakeWall";
    fakeWall.desc = "Heretics Tomb illusory wall (vanishes through the Lens of Truth)";
    fakeWall.id = HT_ACTOR_FAKE_WALL;
    fakeWall.category = ACTORCAT_BG;
    fakeWall.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    fakeWall.objectId = OBJECT_GAMEPLAY_KEEP;
    fakeWall.instanceSize = sizeof(HtFakeWall);
    fakeWall.init = HtFakeWall_Init;
    fakeWall.destroy = HtFakeWall_Destroy;
    fakeWall.update = HtFakeWall_Update;
    fakeWall.draw = HtFakeWall_Draw;
    HtAddActor(fakeWall);

    ActorDBInit ronin;
    ronin.name = "HT_Ronin";
    ronin.desc = "Heretics Tomb mini-boss: the Frozen Ronin";
    ronin.id = HT_ACTOR_RONIN;
    ronin.category = ACTORCAT_ENEMY;
    ronin.flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                  ACTOR_FLAG_DRAW_CULLING_DISABLED;
    ronin.objectId = OBJECT_SK2; // the Stalfos skeleton and animations
    ronin.instanceSize = sizeof(HtRonin);
    ronin.init = HtRonin_Init;
    ronin.destroy = HtRonin_Destroy;
    ronin.update = HtRonin_Update;
    ronin.draw = HtRonin_Draw;
    HtAddActor(ronin);

    ActorDBInit pickup;
    pickup.name = "HT_KatanaPickup";
    pickup.desc = "Heretics Tomb: the Ice Katana left on the floor by the Frozen Ronin";
    pickup.id = HT_ACTOR_KATANA_PICKUP;
    pickup.category = ACTORCAT_MISC;
    pickup.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    pickup.objectId = OBJECT_GAMEPLAY_KEEP;
    pickup.instanceSize = sizeof(HtKatanaPickup);
    pickup.init = HtKatanaPickup_Init;
    pickup.destroy = HtKatanaPickup_Destroy;
    pickup.update = HtKatanaPickup_Update;
    pickup.draw = HtKatanaPickup_Draw;
    HtAddActor(pickup);

    ActorDBInit overlook;
    overlook.name = "HT_Overlook";
    overlook.desc = "Heretics Tomb: dais showing a torch-drawing room from above";
    overlook.id = HT_ACTOR_OVERLOOK;
    overlook.category = ACTORCAT_PROP;
    overlook.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    overlook.objectId = OBJECT_GAMEPLAY_KEEP;
    overlook.instanceSize = sizeof(HtOverlook);
    overlook.init = HtOverlook_Init;
    overlook.destroy = HtOverlook_Destroy;
    overlook.update = HtOverlook_Update;
    overlook.draw = nullptr; // the dais is part of the room
    HtAddActor(overlook);

    ActorDBInit spikes;
    spikes.name = "HT_Spikes";
    spikes.desc = "Heretics Tomb: a bed of spikes lining a wall";
    spikes.id = HT_ACTOR_SPIKES;
    spikes.category = ACTORCAT_PROP;
    spikes.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    spikes.objectId = OBJECT_GAMEPLAY_KEEP;
    spikes.instanceSize = sizeof(HtSpikes);
    spikes.init = HtSpikes_Init;
    spikes.destroy = HtSpikes_Destroy;
    spikes.update = HtSpikes_Update;
    spikes.draw = HtSpikes_Draw;
    HtAddActor(spikes);

    ActorDBInit eye;
    eye.name = "HT_FlameEye";
    eye.desc = "Heretics Tomb: silver eye switch that shoots single flames at Link";
    eye.id = HT_ACTOR_FLAME_EYE;
    eye.category = ACTORCAT_PROP;
    eye.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    eye.objectId = OBJECT_GAMEPLAY_DANGEON_KEEP;
    eye.instanceSize = sizeof(HtFlameEye);
    eye.init = HtFlameEye_Init;
    eye.destroy = HtFlameEye_Destroy;
    eye.update = HtFlameEye_Update;
    eye.draw = HtFlameEye_Draw;
    HtAddActor(eye);

    ActorDBInit boat;
    boat.name = "HT_Boat";
    boat.desc = "Heretics Tomb: the Shadow Temple's boat, sailing the B2 ring";
    boat.id = HT_ACTOR_BOAT;
    boat.category = ACTORCAT_BG;
    boat.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    boat.objectId = OBJECT_HAKA_OBJECTS;
    boat.instanceSize = sizeof(HtBoat);
    boat.init = HtBoat_Init;
    boat.destroy = HtBoat_Destroy;
    boat.update = HtBoat_Update;
    boat.draw = HtBoat_Draw;
    HtAddActor(boat);

    ActorDBInit gate;
    gate.name = "HT_Gate";
    gate.desc = "Heretics Tomb: an iron portcullis that rises on its switch flag";
    gate.id = HT_ACTOR_GATE;
    gate.category = ACTORCAT_BG;
    gate.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    gate.objectId = OBJECT_GAMEPLAY_KEEP;
    gate.instanceSize = sizeof(HtGate);
    gate.init = HtGate_Init;
    gate.destroy = HtGate_Destroy;
    gate.update = HtGate_Update;
    gate.draw = HtGate_Draw;
    HtAddActor(gate);

    ActorDBInit tent;
    tent.name = "HT_Tentacle";
    tent.desc = "Heretics Tomb: a lone Morpha water tentacle - freeze it, then smash it with the hammer";
    tent.id = HT_ACTOR_TENTACLE;
    // (the boss category, not the enemy one: Actor_Spawn refuses enemies in a room flagged clear, and an enemy's
    // death can flag it - Armorpha and its tentacles stopped appearing)
    tent.category = ACTORCAT_BOSS;
    tent.flags = ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | // (never Z-targeted)
                 ACTOR_FLAG_DRAW_CULLING_DISABLED;
    tent.objectId = OBJECT_MO;
    tent.instanceSize = sizeof(HtTentacle);
    tent.init = HtTentacle_Init;
    tent.destroy = HtTentacle_Destroy;
    tent.update = HtTentacle_Update;
    tent.draw = HtTentacle_Draw;
    HtAddActor(tent);

    ActorDBInit arm;
    arm.name = "HT_Armorpha";
    arm.desc = "Heretics Tomb: Armorpha, Morpha's core cased in iron - the B2 mini-boss";
    arm.id = HT_ACTOR_ARMORPHA;
    arm.category = ACTORCAT_BOSS;
    arm.flags = ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    arm.objectId = OBJECT_MO;
    arm.instanceSize = sizeof(HtArmorpha);
    arm.init = HtArmorpha_Init;
    arm.destroy = HtArmorpha_Destroy;
    arm.update = HtArmorpha_Update;
    arm.draw = HtArmorpha_Draw;
    HtAddActor(arm);

    ActorDBInit lyn;
    lyn.name = "HT_Lynel";
    lyn.desc = "Heretics Tomb: the Lynel - Ganondorf's black horse fused with a horned brute (the boss)";
    lyn.id = HT_ACTOR_LYNEL;
    lyn.category = ACTORCAT_BOSS;
    lyn.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    lyn.objectId = OBJECT_HORSE_GANON;
    lyn.instanceSize = sizeof(HtLynel);
    lyn.init = HtLynel_Init;
    lyn.destroy = HtLynel_Destroy;
    lyn.update = HtLynel_Update;
    lyn.draw = HtLynel_Draw;
    HtAddActor(lyn);

    ActorDBInit ped;
    ped.name = "HT_Pedestal";
    ped.desc = "Heretics Tomb: a tier of the boss arena's Triforce pedestal - sinks flush with the floor as the fight begins";
    ped.id = HT_ACTOR_PEDESTAL;
    ped.category = ACTORCAT_BG;
    ped.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    ped.objectId = OBJECT_GAMEPLAY_KEEP;
    ped.instanceSize = sizeof(HtPedestal);
    ped.init = HtPedestal_Init;
    ped.destroy = HtPedestal_Destroy;
    ped.update = HtPedestal_Update;
    ped.draw = HtPedestal_Draw;
    HtAddActor(ped);

    ActorDBInit ring;
    ring.name = "HT_FireRing";
    ring.desc = "Heretics Tomb: the Lynel's fire cage (phase 2) - a wall of fire and rings creeping in, cut with the katana";
    ring.id = HT_ACTOR_FIRE_RING;
    ring.category = ACTORCAT_PROP;
    ring.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    ring.objectId = OBJECT_GAMEPLAY_KEEP;
    ring.instanceSize = sizeof(HtFireRing);
    ring.init = HtFireRing_Init;
    ring.destroy = HtFireRing_Destroy;
    ring.update = HtFireRing_Update;
    ring.draw = HtFireRing_Draw;
    HtAddActor(ring);

    ActorDBInit pad;
    pad.name = "HT_ShadowPad";
    pad.desc = "Heretics Tomb: the shadow pad in Hyrule Field - the Nocturne of Shadow raises the culvert's bars";
    pad.id = HT_ACTOR_SHADOW_PAD;
    pad.category = ACTORCAT_PROP;
    pad.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA;
    pad.objectId = OBJECT_GAMEPLAY_KEEP;
    pad.instanceSize = sizeof(HtShadowPad);
    pad.init = HtShadowPad_Init;
    pad.destroy = HtShadowPad_Destroy;
    pad.update = HtShadowPad_Update;
    pad.draw = HtShadowPad_Draw;
    HtAddActor(pad);

    ActorDBInit ep;
    ep.name = "HT_Epitaph";
    ep.desc = "Frostfire Mausoleum: the Ronin's grave post / Ganondorf's plaque by the horse (readable)";
    ep.id = HT_ACTOR_EPITAPH;
    ep.category = ACTORCAT_PROP;
    ep.flags = ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    ep.objectId = OBJECT_GAMEPLAY_KEEP;
    ep.instanceSize = sizeof(HtEpitaph);
    ep.init = HtEpitaph_Init;
    ep.destroy = HtEpitaph_Destroy;
    ep.update = HtEpitaph_Update;
    ep.draw = HtEpitaph_Draw;
    HtAddActor(ep);

    ActorDBInit mo;
    mo.name = "HT_MoSpikes";
    mo.desc = "Frostfire Mausoleum: Morpha's lair's spikes and wedges in Armorpha's arena (built from the player's game data)";
    mo.id = HT_ACTOR_MO_SPIKES;
    mo.category = ACTORCAT_BG;
    mo.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    mo.objectId = OBJECT_GAMEPLAY_KEEP;
    mo.instanceSize = sizeof(HtMoSpikes);
    mo.init = HtMoSpikes_Init;
    mo.destroy = HtMoSpikes_Destroy;
    mo.update = HtMoSpikes_Update;
    mo.draw = HtMoSpikes_Draw;
    HtAddActor(mo);
}

// --- Timed continuous sounds (see HtSfx_PlayFor in Katana.h) ---

struct HtTimedSfx {
    Vec3f worldPos;
    Vec3f projectedPos; // what the audio engine reads, refreshed every frame
    u16 sfxId;
    s16 frames;
};
static HtTimedSfx sTimedSfx[8];

// The central room (6) brightens once the green torch burns (switch 0x24): its B1 landing (and the well on down,
// below y -400) has its baked vertex colours scaled up by a quarter, in place in the loaded room mesh. Vtx.flag of a
// buffer's first vertex marks it done (0 / 1), so a reloaded buffer, or a save without the flag, is set right again.
#define HT_ROOM6_PATH "__OTR__scenes/shared/heretics_tomb_scene/heretics_tomb_room_6"
#define HT_GREEN_FLAG 0x24
static void HtRoom_UpdateBrightness(void) {
    if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB) || (gPlayState->gameplayFrames & 7)) {
        return;
    }
    if (!ResourceMgr_FileExists(HT_ROOM6_PATH "_vtxcounts")) {
        return;
    }
    Vtx* counts = ResourceMgr_LoadVtxByName((char*)HT_ROOM6_PATH "_vtxcounts");
    if (counts == nullptr) {
        return;
    }
    u16 want = Flags_GetSwitch(gPlayState, HT_GREEN_FLAG) ? 1 : 0;
    s32 groups = counts[0].v.ob[0];

    for (s32 g = 0; g < groups; g++) {
        char name[160];
        snprintf(name, sizeof(name), HT_ROOM6_PATH "_vtx_%d", g);
        Vtx* v = ResourceMgr_LoadVtxByName(name);
        s32 n = counts[g + 1].v.ob[0];

        if ((v == nullptr) || (n <= 0) || (v[0].v.flag == want)) {
            continue;
        }
        for (s32 i = 0; i < n; i++) {
            if (v[i].v.ob[1] > -400) {
                continue;
            }
            for (s32 c = 0; c < 3; c++) {
                s32 x = v[i].v.cn[c];
                v[i].v.cn[c] = want ? (u8)MIN(255, x * 5 / 4) : (u8)(x * 4 / 5);
            }
        }
        v[0].v.flag = want;
    }
}

extern "C" void HtSfx_PlayFor(float x, float y, float z, unsigned short sfxId, short frames) {
    HtTimedSfx* slot = &sTimedSfx[0];
    for (auto& e : sTimedSfx) {
        if (e.frames <= 0) {
            slot = &e;
            break;
        }
    }
    slot->worldPos = { x, y, z };
    slot->sfxId = sfxId;
    slot->frames = frames;
}

// --- In Heretics Tomb the water burns like lava ---
// Shortly after he goes in, it hurts and makes Link recoil: it breaks a swim stroke, cuts a dive short and, in Iron Boots, flinches
// him every 2 seconds he stays under. It's the vanilla hit reaction (damage plus a small knockback), repeated.
#define TOMB_WATER_DAMAGE 8       // half a heart
#define TOMB_WATER_INTERVAL 40    // frames between burns while still in the water
#define TOMB_WATER_GRACE 30       // frames after going in before the first burn: Link sinks in first
#define HT_ENTRANCE_ROOM 0     // the entrance hall: its pool is the culvert's water, plain
#define HT_VOID_WATER_ROOM 8      // the still lake (room number in build_heretics_dungeon.py)
#define HT_WATER_SIGN_TEXT 0x03F0 // EnKanban params 0xF0

static s16 sTombWaterTimer = 0;
static bool sTombWasInWater = false;

// Armorpha's room: its own pool burn (HtArmorpha.c, damage only) is the water's - the generic burn's knockback
// threw Link back off the edge every time he tried to climb out
static bool HtTombWater_ArmorphaRoom() {
    for (s32 c = 0; c < ACTORCAT_MAX; c++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[c].head; a != nullptr; a = a->next) {
            if ((a->id == HT_ACTOR_ARMORPHA) && (a->room == gPlayState->roomCtx.curRoom.num)) {
                return true;
            }
        }
    }
    return false;
}

static void HtTombWater_Update() {
    if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
        sTombWasInWater = false;
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    bool inWater = (player->actor.bgCheckFlags & BGCHECKFLAG_WATER) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER);

    if (!inWater || (gSaveContext.health <= 0) || Player_InCsMode(gPlayState) ||
        (gPlayState->roomCtx.curRoom.num == HT_BOSS_ROOM) || // (the boss arena's water is plain water)
        (gPlayState->roomCtx.curRoom.num == HT_ENTRANCE_ROOM) || // (and the entrance pool's: the way in and out)
        HtTombWater_ArmorphaRoom()) {
        sTombWasInWater = false;
        return;
    }
    // The still lake's water is deep as an abyss: falling in sends Link back to the room's door, like a pit
    // (no swimming or iron-booting across)
    if ((gPlayState->roomCtx.curRoom.num == HT_VOID_WATER_ROOM) && (gPlayState->transitionTrigger == TRANS_TRIGGER_OFF)) {
        if (!sTombWasInWater) {
            sTombWasInWater = true;
            sTombWaterTimer = 30; // he splashes down and burns for a moment first
            Actor_SetPlayerKnockbackSmall(gPlayState, &player->actor, 0.0f, player->actor.shape.rot.y, 0.0f,
                                          TOMB_WATER_DAMAGE);
            Audio_PlayActorSound2(&player->actor, NA_SE_EN_EXTINCT);
            return;
        }
        if (--sTombWaterTimer <= 0) {
            Play_TriggerVoidOut(gPlayState);
            gPlayState->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
            Sfx_PlaySfxCentered(NA_SE_OC_ABYSS);
        }
        return;
    }
    if (!sTombWasInWater) {
        sTombWasInWater = true;
        sTombWaterTimer = TOMB_WATER_GRACE; // no burn at the surface: it starts once he's in
    }
    if (sTombWaterTimer > 0) {
        sTombWaterTimer--;
        return;
    }
    sTombWaterTimer = TOMB_WATER_INTERVAL;
    Actor_SetPlayerKnockbackSmall(gPlayState, &player->actor, 4.0f, player->actor.shape.rot.y + 0x8000, 4.0f,
                                  TOMB_WATER_DAMAGE);
    Audio_PlayActorSound2(&player->actor, NA_SE_EN_EXTINCT); // sizzle
}

// The hub's pit (room 1) has no floor (B1's wind corridor lies right under it): Link falling into it is sent back to
// the door here, once he's well down in it
static void HtHubPit_Update() {
    Player* player;

    if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB) ||
        (gPlayState->roomCtx.curRoom.num != 1) || (gPlayState->transitionTrigger != TRANS_TRIGGER_OFF)) {
        return;
    }
    player = GET_PLAYER(gPlayState);
    if ((player != nullptr) && (player->actor.world.pos.y < -120.0f) &&
        !(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING))) {
        Play_TriggerVoidOut(gPlayState);
        gPlayState->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
        Sfx_PlaySfxCentered(NA_SE_OC_ABYSS);
        player->stateFlags1 |= PLAYER_STATE1_LOADING;
    }
}

// the mausoleum's switch flags 0x20-0x3F (the game drops them on leaving a scene), kept in the save - except the few
// meant to start over on each visit: the skull room's three reset switches (0x2B-0x2D), the B2 boat's voyage (0x30)
// and its waves (0x33-0x35)
#define HT_TOMB_KEPT_FLAGS (~((1u << (0x2B - 0x20)) | (1u << (0x2C - 0x20)) | (1u << (0x2D - 0x20)) | \
                             (1u << (0x30 - 0x20)) | (1u << (0x33 - 0x20)) | (1u << (0x34 - 0x20)) | (1u << (0x35 - 0x20))))
static u32 sTombTempSwch = 0;

extern "C" void HtTomb_RestoreFlags(PlayState* play) {
    if (play->sceneNum == SCENE_HERETICS_TOMB) {
        play->actorCtx.flags.tempSwch |= sTombTempSwch & HT_TOMB_KEPT_FLAGS;
    }
}

static void HtTomb_KeepFlags() {
    if ((gPlayState != nullptr) && (gPlayState->sceneNum == SCENE_HERETICS_TOMB)) {
        sTombTempSwch = gPlayState->actorCtx.flags.tempSwch;
    }
}

static void HtSfx_Update() {
    HtTombWater_Update();
    HtTomb_KeepFlags();
    HtHubPit_Update();
    if (gPlayState == nullptr) {
        return;
    }
    for (auto& e : sTimedSfx) {
        if (e.frames > 0) {
            f32 w;
            SkinMatrix_Vec3fMtxFMultXYZW(&gPlayState->viewProjectionMtxF, &e.worldPos, &e.projectedPos, &w);
            Audio_PlaySfxGeneral(e.sfxId - SFX_FLAG, &e.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            e.frames--;
        }
    }
}

// --- Getting the katana: the big chest in Heretics Tomb that holds the Biggoron's Sword get-item ---

extern "C" bool Katana_TryChestGet(PlayState* play, Actor* player, s16 getItemId, s16 modIndex) {
    if ((play->sceneNum != SCENE_HERETICS_TOMB) || (getItemId != GI_SWORD_BGS) || (modIndex != MOD_NONE)) {
        return false;
    }
    sKatanaOwned = true;
    sKatanaGetTextPending = true;
    Message_StartTextbox(play, KATANA_GET_TEXT_ID, player);
    Audio_PlayFanfare(NA_BGM_ITEM_GET | 0x900);
    return true;
}

// --- Save data: own "hereticsTomb" section, so the vanilla save layout is untouched ---

// --- The black horse (HtHorse.h) ---
static bool sBlackHorseOwned = false;
static bool sBlackHorseActive = false;
static bool sBlackHorsePlace = false; // (freed: set it down by Link when he next comes into Hyrule Field)
static bool sEponaWasObtained = false;
static bool sTombOpen = false; // (the Nocturne's been played on the shadow pad: the culvert's bars are up)

extern "C" bool HtTomb_IsOpen(void) {
    return sTombOpen;
}

extern "C" void HtTomb_SetOpen(bool open) {
    sTombOpen = open;
}

static u32 sMapVisited = 0; // (the map's rooms Link has been in: HtMap.c, a bit per piece)

extern "C" u32 HtMap_GetVisited(void) {
    return sMapVisited;
}

extern "C" void HtMap_SetVisited(u32 pieces) {
    sMapVisited = pieces;
}

extern "C" bool HtHorse_BlackOwned(void) {
    return sBlackHorseOwned;
}

extern "C" bool HtHorse_BlackActive(void) {
    return sBlackHorseOwned && sBlackHorseActive;
}

extern "C" void HtHorse_SetBlackActive(bool active) {
    sBlackHorseActive = active;
}

extern "C" bool HtHorse_EponaObtained(void) {
    return Flags_GetEventChkInf(EVENTCHKINF_EPONA_OBTAINED);
}

extern "C" void HtHorse_Free(void) {
    if (sBlackHorseOwned) {
        return;
    }
    sBlackHorseOwned = true;
    sBlackHorseActive = true; // (Epona, if Link had her, to the stable)
    sBlackHorsePlace = true;
    gSaveContext.horseData.scene = SCENE_HYRULE_FIELD;
}

extern "C" bool HtHorse_HasHorse(PlayState* play) {
    if (Flags_GetEventChkInf(EVENTCHKINF_EPONA_OBTAINED)) {
        return true;
    }
    // (the black horse only: Lon Lon Ranch keeps its own Epona story till she's won)
    return sBlackHorseOwned && ((play == nullptr) || (play->sceneNum != SCENE_LON_LON_RANCH));
}

// each frame: Epona just won (the black horse to the stable, as she's the newer); the freed horse set down by Link
static void HtHorse_Watch() {
    bool epona = Flags_GetEventChkInf(EVENTCHKINF_EPONA_OBTAINED);

    if (epona && !sEponaWasObtained && sBlackHorseOwned) {
        sBlackHorseActive = false;
    }
    sEponaWasObtained = epona;
    if (sBlackHorsePlace && (gPlayState != nullptr) && (gPlayState->sceneNum == SCENE_HYRULE_FIELD)) {
        Player* player = GET_PLAYER(gPlayState);
        Actor* a;

        bool warped = HtShadowPad_ArrivedByWarp(); // (the boss's blue warp: it waits across the stream)

        if ((player == nullptr) || (Player_InCsMode(gPlayState) && !warped)) {
            return;
        }
        for (a = gPlayState->actorCtx.actorLists[ACTORCAT_BG].head; a != nullptr; a = a->next) {
            if (a->id == ACTOR_EN_HORSE) {
                if (((EnHorse*)a)->stateFlags & ENHORSE_INACTIVE) {
                    Actor_Kill(a); // (the one waiting to be called: this one's here already)
                    continue;
                }
                break;
            }
        }
        if ((a == nullptr) && LINK_IS_ADULT) {
            // (none here yet - it comes)
            a = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_HORSE, player->actor.world.pos.x,
                            player->actor.world.pos.y + 50.0f, player->actor.world.pos.z, 0, 0, 0, 1);
        }
        if ((a != nullptr) && warped) {
            // on the stream's far bank, across from the shadow pad, looking over at him
            Vec3f pad = { HT_SHADOW_PAD_X, HT_SHADOW_PAD_Y, HT_SHADOW_PAD_Z };

            a->world.pos.x = HT_HORSE_WAIT_X;
            a->world.pos.z = HT_HORSE_WAIT_Z;
            a->world.pos.y = HT_SHADOW_PAD_Y + 50.0f;
            a->prevPos = a->world.pos;
            a->shape.rot.y = a->world.rot.y = Math_Vec3f_Yaw(&a->world.pos, &pad);
            gSaveContext.horseData.scene = SCENE_HYRULE_FIELD;
            gSaveContext.horseData.pos.x = a->world.pos.x;
            gSaveContext.horseData.pos.y = HT_SHADOW_PAD_Y;
            gSaveContext.horseData.pos.z = a->world.pos.z;
            gSaveContext.horseData.angle = a->shape.rot.y;
            sBlackHorsePlace = false;
        } else if (a != nullptr) {
            // just there, a little way ahead of him (off the bridge, out on the field), looking at him
            s16 yaw = player->actor.shape.rot.y;

            a->world.pos.x = player->actor.world.pos.x + Math_SinS(yaw) * 340.0f;
            a->world.pos.z = player->actor.world.pos.z + Math_CosS(yaw) * 340.0f;
            a->world.pos.y = player->actor.world.pos.y + 50.0f;
            a->prevPos = a->world.pos;
            a->shape.rot.y = a->world.rot.y = Math_Vec3f_Yaw(&a->world.pos, &player->actor.world.pos);
            gSaveContext.horseData.pos.x = a->world.pos.x;
            gSaveContext.horseData.pos.y = player->actor.world.pos.y;
            gSaveContext.horseData.pos.z = a->world.pos.z;
            gSaveContext.horseData.angle = a->shape.rot.y;
            sBlackHorsePlace = false;
        }
    }
}

static bool sFreshStartDone = false; // (Katana_FreshStart, below)

static void Katana_InitSave(bool isDebug) {
    sFreshStartDone = true; // (a new file: nothing to put back)
    sKatanaOwned = false;
    sKatanaEquipped = false;
    sBlackHorseOwned = sBlackHorseActive = sBlackHorsePlace = false;
    sTombOpen = false;
    sMapVisited = 0;
    sTombTempSwch = 0;
}

// The full playthrough test: saves made while the testing shortcuts were in (the katana granted on entering the
// mausoleum, the black horse in Hyrule Field) are put back once, on loading, to a fresh start of the dungeon - no
// katana, no black horse, the culvert barred, the dungeon's own flags, chests, map, compass and keys cleared. Saved as done after that.
static void Katana_FreshStart() {
    SavedSceneFlags* f = &gSaveContext.sceneFlags[SCENE_HERETICS_TOMB];

    sKatanaOwned = false;
    sKatanaEquipped = false;
    sBlackHorseOwned = false;
    sBlackHorseActive = false;
    sBlackHorsePlace = false;
    sMapVisited = 0;
    sTombTempSwch = 0;
    sTombOpen = false; // (the culvert barred again: the Nocturne on the pad opens it)
    f->chest = f->swch = f->clear = f->collect = f->unk = f->rooms = f->floors = 0;
    gSaveContext.inventory.dungeonItems[HERETICS_TOMB_MAP_INDEX] = 0;
    gSaveContext.inventory.dungeonKeys[HERETICS_TOMB_MAP_INDEX] = 0;
    sFreshStartDone = true;
}

static void Katana_Save(const SaveContext& saveContext, int sectionID, bool fullSave) {
    SaveManager::Instance->SaveData("freshStart3", sFreshStartDone);
    SaveManager::Instance->SaveData("katanaOwned", sKatanaOwned);
    SaveManager::Instance->SaveData("katanaEquipped", sKatanaEquipped);
    SaveManager::Instance->SaveData("blackHorseOwned", sBlackHorseOwned);
    SaveManager::Instance->SaveData("blackHorseActive", sBlackHorseActive);
    SaveManager::Instance->SaveData("blackHorsePlace", sBlackHorsePlace);
    SaveManager::Instance->SaveData("tombOpen", sTombOpen);
    SaveManager::Instance->SaveData("mapVisited", sMapVisited);
    SaveManager::Instance->SaveData("tombFlags", sTombTempSwch);
}

static void Katana_LoadV1() {
    SaveManager::Instance->LoadData("katanaOwned", sKatanaOwned, false);
    SaveManager::Instance->LoadData("katanaEquipped", sKatanaEquipped, false);
    SaveManager::Instance->LoadData("blackHorseOwned", sBlackHorseOwned, false);
    SaveManager::Instance->LoadData("blackHorseActive", sBlackHorseActive, false);
    SaveManager::Instance->LoadData("blackHorsePlace", sBlackHorsePlace, false);
    SaveManager::Instance->LoadData("tombOpen", sTombOpen, false);
    SaveManager::Instance->LoadData("mapVisited", sMapVisited, (u32)0);
    SaveManager::Instance->LoadData("tombFlags", sTombTempSwch, (u32)0);
    SaveManager::Instance->LoadData("freshStart3", sFreshStartDone, false);
    if (!sFreshStartDone) {
        Katana_FreshStart();
    }
    sEponaWasObtained = Flags_GetEventChkInf(EVENTCHKINF_EPONA_OBTAINED);
}

static void Katana_Register() {
    // ShipInit::InitAll can run more than once (boot, presets); register only once.
    static bool sRegistered = false;
    if (sRegistered) {
        return;
    }
    sRegistered = true;

    Katana_RegisterIceSlashActor();

    SaveManager::Instance->AddInitFunction(Katana_InitSave);
    SaveManager::Instance->AddLoadFunction("hereticsTomb", 1, Katana_LoadV1);
    SaveManager::Instance->AddSaveFunction("hereticsTomb", 1, Katana_Save, true, SECTION_PARENT_NONE);

   GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(HtSfx_Update);
    // the Iai's wave: its model and ice texture loaded with each scene (once the katana is owned), not at the first
    // cut (loading them there hitched the game for a moment just as the Iai went off)
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        static const char* sIaiRes[] = {
            "scenes/shared/heretics_tomb_scene/ht_iai_wave_dl", "scenes/shared/heretics_tomb_scene/mat_ht_iai_wave_0",
            "scenes/shared/heretics_tomb_scene/ht_iai_wave_vtx_0", "scenes/shared/heretics_tomb_scene/ht_iai_wave_tri_0",
            "scenes/shared/heretics_tomb_scene/ht_ice_tex",
        };
        if (Katana_IsOwned()) {
            for (const char* path : sIaiRes) {
                ResourceMgr_LoadTexOrDListByName(path);
            }
        }
    });
    // the tomb's door in Hyrule Field (HtShadowPad.c)
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        if (gPlayState != nullptr) {
            HtShadowPad_OnSceneInit(gPlayState);
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>([]() {
        if (gPlayState != nullptr) {
            HtShadowPad_OnSpawnActors(gPlayState);
            HtHorse_OnSpawnActors(gPlayState);
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() { HtShadowPad_TombUpdate(gPlayState); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(HtHorse_Watch);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(HtRoom_UpdateBrightness);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>(
        [](void* actor) { HtFrostShell_OnActorDestroy((Actor*)actor); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        for (auto& e : sTimedSfx) {
            e.frames = 0;
        }
    });

    // Mini-boss music at full volume: in the tomb the "enemy nearby" ducking (meant for the dungeon's own BGM)
    // would otherwise turn the mini-boss theme down to a whisper while the enemy stands close
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnVanillaBehavior>(
        VB_DETECT_BGM_ENEMY, [](GIVanillaBehavior, bool* should, va_list) {
            if (*should && (gPlayState != nullptr) && (gPlayState->sceneNum == SCENE_HERETICS_TOMB) &&
                ((func_800FA0B4(SEQ_PLAYER_BGM_MAIN) & 0xFF) == NA_BGM_MINI_BOSS)) {
                *should = false;
            }
        });

    // Lens-only hookshot targets: in the tomb an Obj_Hsblock with params bit 0x40 (unused by the game) only shows
    // through the Lens of Truth (its room hides lens actors). It can still be hooked blind, like the Shadow Temple's.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](void* refActor) {
        Actor* a = (Actor*)refActor;
        if ((gPlayState != nullptr) && (gPlayState->sceneNum == SCENE_HERETICS_TOMB) && (a->id == ACTOR_OBJ_HSBLOCK) &&
            (a->params & 0x40)) {
            a->flags |= ACTOR_FLAG_REACT_TO_LENS;
        }
    });

    // Doors behind uncut chains don't offer to open.
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnVanillaBehavior>(
        VB_EN_DOOR_OFFER_OPEN, [](GIVanillaBehavior, bool* should, va_list) {
            if (*should && (gPlayState != nullptr) && (gPlayState->sceneNum == SCENE_HERETICS_TOMB) &&
                HtChainBarrier_BlocksPosition(&GET_PLAYER(gPlayState)->actor.world.pos)) {
                *should = false;
            }
        });

    // Navi's C-up hint for the chain barriers (naviEnemyId 0xF0 -> text 0x06F0)
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0x06F0, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            // Only what Link can tell by looking: the katana comes later, in part 2
            CustomMessage msg("Rusted war chains, as thick as your arm.&Swords just ring off the iron...",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // ...and the full hint, on the chains met once the katana is in hand (naviEnemyId 0xF1 -> text 0x06F1)
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0x06F1, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("Rusted war chains, as thick as your arm.^"
                              "An ordinary cut just rings off the iron... but an %rIai strike%w could cut "
                              "through! Hold B to charge Yukikaze, then let go to draw and cut in one stroke.",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // Navi on Morpha's water tentacles (their C-up hint, and said once when the first one rises)
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_TENTACLE_NAVI_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("A living tentacle of water! Your sword will just pass through it...^"
                              "Freeze it solid with %cYukikaze%w, then %rsmash%w the ice with the "
                              "%rMegaton Hammer%w!",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // the epitaphs (HtEpitaph.c): the Ronin's grave post, written by the Sheikah who sealed the mausoleum; and
    // Ganondorf's plaque over his chained horse
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_EPITAPH_RONIN_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("Here sleeps a swordsman from beyond the eastern sea, whose name is lost to time.^"
                              "He bowed to no throne and served no master. His blade alone was his oath.^"
                              "We, the %pSheikah%w, laid him to rest here in the deep, that none might disturb his "
                              "sleep.^"
                              "His blade, %cYukikaze%w, would not leave his hand even in death. If you hold it now, "
                              "you have earned it. Bear it with honor.",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_EPITAPH_HORSE_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("This beast carried me on the night Hyrule's king fell.^"
                              "And on that night it let a Sheikah woman and a little princess slip across the "
                              "drawbridge, from beneath my very hand.^"
                              "Let it lie chained here, in the ashes of an old war, until the fire of this place "
                              "burns the failure out of it - and leaves something fit to carry the King of Evil.^"
                              "%r- Ganondorf%w",
                              TEXTBOX_TYPE_BLACK);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // Navi on the Frozen Ronin (its C-up hint)
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_RONIN_NAVI_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("%rFrozen Ronin%w! A swordsman long dead, still bound to his blade... He reads "
                              "your swings - strike while he's poised, and he'll turn your sword aside and cut back!^"
                              "Wait for an %ropening%w: right after he cuts, or as he lands from his leap!",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // Navi on Armorpha (its C-up hint, and said once as the fight starts)
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_ARMORPHA_NAVI_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("%rArmorpha%w! Morpha's core, cased in iron... It rides in its tentacles - "
                              "freeze the one it's in just as it slides to the top, then %rsmash%w the ice!^"
                              "Thrown out, it rolls round the room in a bubble of water. Only an %rIai strike%w "
                              "can cut through!",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // Navi, as Link nears the black horse chained in the boss arena (said once: HtLynel)
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_LYNEL_NAVI_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("Wait, Link... this steed...", TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // Navi on the Lynel (its C-up hint)...
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_LYNEL_HINT_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("When it rears up neighing, it's about to %rcharge%w!^"
                              "Freeze a spout in its path at the %rlast moment%w - too soon, and it will see the "
                              "ice coming!",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // ...and while its horns are stuck in the ice
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_LYNEL_STUCK_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("Its horns are stuck in the ice! Now's your chance!^"
                              "Get beside it and strike its flank with an %cIai%w! Stay clear of its "
                              "%rhind legs%w!",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // Navi, as the sword round begins with the katana in hand (and when it glances off): it's spent
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_LYNEL_KATANA_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("Link, careful! Its %rcurse%w is laid bare now...^"
                              "That dark power swallows your katana's cold before the blade can bite - its frost "
                              "won't take hold of cursed flesh!^"
                              "Only the %rMaster Sword%w, the blade of evil's bane, can cut through a curse like "
                              "that. Face it blade to blade!",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // her C-up hint in the fire cage
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_LYNEL_CAGE_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("Cut through the %rrings of fire%w with your katana before they close in on you!^"
                              "When it rears up with its swords crossed, it's about to %rcharge%w you - "
                              "meet it with an %cIai%w as it comes!",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // ...and her C-up hint for the sword round
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_LYNEL_SWORD_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("This is it, Link - the %rfinal stretch%w! It's trapped down there, and it's "
                              "weakening!^"
                              "Its bared curse drinks up your katana's frost. Strike it with the %rMaster "
                              "Sword%w - the bane of evil!",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // The stable: the horse waiting there - switch?
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_HORSE_SWAP_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            // (Format: the colour codes and line breaks turned into the game's own; laid out by hand, as a choice
            // box needs its two lines last)
            CustomMessage msg(HtHorse_BlackActive() ? "%rEpona%w is resting in her stall.&Ride her instead?&" +
                                                          CustomMessage::TWO_WAY_CHOICE() + "%gSwitch&Leave her%w"
                                                    : "The %rblack horse%w rests in its stall.&Ride it instead?&" +
                                                          CustomMessage::TWO_WAY_CHOICE() + "%gSwitch&Leave it%w",
                              TEXTBOX_TYPE_BLACK);
            msg.Format();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_HORSE_SWAPPED_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            CustomMessage msg(HtHorse_BlackActive() ? "The %rblack horse%w is yours to ride again."
                                                    : "%rEpona%w is yours to ride again.",
                              TEXTBOX_TYPE_BLACK);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // The warning sign before the shaft's pool
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        HT_WATER_SIGN_TEXT, [](uint16_t* textId, bool* loadFromMessageTable) {
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_HERETICS_TOMB)) {
                return;
            }
            CustomMessage msg("%rDanger!%w The water of this tomb burns&like fire.^"
                              "Do not touch it.",
                              TEXTBOX_TYPE_WOODEN);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    // The katana's get-item text. Only replaces the message while a katana pickup is showing it.
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        KATANA_GET_TEXT_ID, [](uint16_t* textId, bool* loadFromMessageTable) {
            if (!sKatanaGetTextPending) {
                return;
            }
            sKatanaGetTextPending = false;
            CustomMessage msg("You got %cYukikaze%w, the katana of&the snow wind!&Equip it from the sword row of the&equipment screen.^"
                              "Each swing costs a little %gmagic%w&and sends out a freezing slash.&"
                              "It can't be swung with an empty&magic meter.^"
                              "Hold %bB%w to charge an %cIai draw%w: a&wide, powerful frost wave for a&hefty "
                              "share of your magic.",
                              TEXTBOX_TYPE_BLUE);
            msg.AutoFormat(ITEM_SWORD_MASTER);
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });
}

static RegisterShipInitFunc initFunc(Katana_Register);
