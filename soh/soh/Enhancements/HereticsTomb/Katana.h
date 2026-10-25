#pragma once

// Heretics Tomb: the Ice Katana.
//
// Internally the katana rides on the Master Sword's equip value, so all of Link's
// sword code (reach, animations, collision) works unchanged. A separate "katana
// equipped" flag, kept in its own save section, switches on the katana behaviour.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct Actor;

// Placeholder ice tint for the katana icon (until it gets its own art)
#define KATANA_TINT_R 140
#define KATANA_TINT_G 210
#define KATANA_TINT_B 255

bool Katana_IsOwned(void);
void Katana_SetOwned(bool owned);

// True only when the katana flag is set AND Link is an adult holding the
// Master Sword equip value on B, so time travel or scripted sword swaps can
// never leave a stale katana state behind.
bool Katana_IsEquipped(void);
void Katana_SetEquipped(bool equipped);
// Locked (the Lynel's last round): the katana can't be equipped, and locking it puts it away (the Master Sword
// stays on B). Returns whether it was equipped.
bool Katana_SetLocked(bool locked);
bool Katana_IsLocked(void);

// Pause-menu name texture for the katana, shipped in the Heretics Tomb mod archive at
// textures/heretics_tomb/gKatanaItemNameTex. The "__OTR__" prefix tells the renderer this is
// a resource path; without it the path string itself gets drawn as pixels.
// the katana's item icon (32 x 32 RGBA32, generated with the tomb: katana_tex.py), on the B button and in the pause
// screen's equipment cell
#define KATANA_ICON_TEX_PATH "__OTR__textures/heretics_tomb/gKatanaIconTex"
#define KATANA_NAME_TEX_PATH "__OTR__textures/heretics_tomb/gKatanaItemNameTex"

// True while the equipment-screen cursor sits on the katana cell
// (defined in z_kaleido_equipment.c).
bool KaleidoScope_IsKatanaCursorActive(void);

// Magic cost of one katana slash (the meter holds 48, or 96 with double magic).
// Tuned in playtesting.
#define KATANA_SLASH_MAGIC_COST 2

// Magic cost of an Iai draw (the charged spin release). Tuned in playtesting.
#define KATANA_IAI_MAGIC_COST 12

// The Iai draw's pacing: the slash animation's play speed through the draw (a gather, the cut, the finish), the step
// in with the cut, and the follow-through's play speed (the finish is held through its first half), and how many frames
// after the release the frost wave leaves the blade (the moment of the cut)
#define KATANA_IAI_DRAW_SPEED 0.3f
#define KATANA_IAI_STEP_SPEED 14.0f
#define KATANA_IAI_FINISH_SPEED 0.25f
#define KATANA_IAI_HOLD_FRAMES 6.0f // the finish holds (and Link can't act) until this frame of the follow-through
// frames after the last cut before the katana goes back in its scabbard (at once if Link leaves the ground)
#define KATANA_SHEATHE_DELAY 100
#define KATANA_IAI_WAVE_DELAY 3

// False when the katana is equipped and the magic meter can't pay for a slash
// (or Link has no magic meter at all). Always true for every other sword.
bool Katana_CanSwing(void);

// False when the katana is equipped and the magic meter can't pay for an Iai draw:
// holding B then never starts the charge. Always true for every other sword.
bool Katana_CanCharge(void);

// Spends the magic for one katana slash. No-op when the katana isn't equipped.
void Katana_PaySwingCost(void);

struct PlayState;
struct Player;
struct Actor;

// Launches the freezing slash wave from Link's blade (see KatanaIceSlash.c).
// iai = true launches the bigger Iai wave that also deals Biggoron spin damage.
void Katana_SpawnIceSlash(struct PlayState* play, struct Player* player, bool iai);

// Called by the player code right before a charged spin is released, so the next
// swing is treated as an Iai draw.
void Katana_BeginIaiRelease(void);

// The katana's blade connected with `target` (may be NULL): fire enemies get the katana's cold.
void Katana_OnBladeHit(struct PlayState* play, struct Actor* target);

// True while the current swing is a paid Iai draw.
bool Katana_IsIaiStrikeActive(void);
// True if this hit is the Iai draw's own blade: the attacker is Link, mid-swing with the katana, in the paid Iai draw
// (not a later swing of something else - the hammer, a stick - while the last draw's state lingers). What the chains,
// Armorpha and the Lynel check.
bool Katana_IsIaiBladeHit(struct Actor* attacker);
// The katana's scabbard as drawn this frame on Link's hip (katana model space: the guard at x 790, the point near
// x 6050, curving toward -y), or NULL. Defined in z_player_lib.c; the Iai charge glow (En_M_Thunder) follows it.
void* Player_KatanaSayaMtx(void); // MtxF*
// game frames since the last katana swing began (Katana_OnSwingStart)
unsigned int Katana_FramesSinceSwing(struct PlayState* play);

// Handles the katana side of a swing start: pays magic, launches the wave, plays sounds.
// Call only when the katana is equipped and the swing is a new attack (not a finisher).
void Katana_OnSwingStart(struct PlayState* play, struct Player* player);

// Spin-attack damage type for the spin ring while the katana is equipped: Biggoron spin
// after a paid Iai draw, Kokiri spin otherwise. Returns vanillaFlags for other swords.
unsigned int Katana_GetSpinDmgFlags(unsigned int vanillaFlags);

// Same for a fully charged (Great Spin) release, which vanilla scores with jump-attack
// damage types: Biggoron jump after a paid Iai draw, Kokiri jump otherwise.
unsigned int Katana_GetGreatSpinDmgFlags(unsigned int vanillaFlags);

// The katana chest: a big chest in Heretics Tomb holding GI_SWORD_BGS (its model is held up).
// Returns true when this get-item is the katana: grants it, shows its text and plays the fanfare.
#define KATANA_GET_TEXT_ID 0x00F9
bool Katana_TryChestGet(struct PlayState* play, struct Actor* player, short getItemId, short modIndex);

// Heretics Tomb actors get fixed ids (well past the ids Ship hands out dynamically), so rooms can place them.
// Plays a looping (continuous) sound effect at a world position for a number of frames, then lets it stop.
// The freeze sounds are continuous: started once they never end, so they have to be kept alive this way.
void HtSfx_PlayFor(float x, float y, float z, unsigned short sfxId, short frames);

// Custom actor ids: past the vanilla table (last vanilla id is 0x1D6), so no vanilla actor is replaced.
// the katana in Link's left hand (drawn in the hand limb's space, blade along +x)
#define KATANA_HELD_DL "__OTR__scenes/shared/heretics_tomb_scene/ht_katana_dl"

#define HT_ACTOR_ICE_SLASH 0x0300
#define HT_ACTOR_FLAME_JET 0x0301
#define HT_ACTOR_CHAIN_BARRIER 0x0302
#define HT_ACTOR_WATER_SPOUT 0x0303
#define HT_ACTOR_FROST_SHELL 0x0304
#define HT_ACTOR_TORCH 0x0305
#define HT_ACTOR_PLATFORM 0x0306
#define HT_ACTOR_FAKE_WALL 0x0307
#define HT_ACTOR_RONIN 0x0308
#define HT_ACTOR_KATANA_PICKUP 0x0309
#define HT_ACTOR_OVERLOOK 0x030A
#define HT_ACTOR_SPIKES 0x030B
#define HT_ACTOR_FLAME_EYE 0x030C
#define HT_ACTOR_BOAT 0x030D
#define HT_ACTOR_GATE 0x030E
#define HT_ACTOR_TENTACLE 0x030F
#define HT_ACTOR_ARMORPHA 0x0310
#define HT_ACTOR_LYNEL 0x0311
#define HT_ACTOR_PEDESTAL 0x0312
#define HT_ACTOR_FIRE_RING 0x0313
#define HT_ACTOR_SHADOW_PAD 0x0314
#define HT_ACTOR_EPITAPH 0x0315
#define HT_ACTOR_MO_SPIKES 0x0316

#ifdef __cplusplus
}
#endif
