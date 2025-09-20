#pragma once

#include "Game/control/control.h"
#include "Game/items.h"

#define EffectFunction void(ItemInfo* item)

constexpr auto NUM_FLIPEFFECTS = 47;

extern int FlipEffect;

extern std::function<EffectFunction> effect_routines[];

void AddLeftFootprint(ItemInfo* item); // TODO: To anim command.
void AddRightFootprint(ItemInfo* item); // TODO: To anim command.
void VoidEffect(ItemInfo* item);
void FinishLevel(ItemInfo* item);
void Turn180(ItemInfo* item); // TODO: To anim command.
void FloorShake(ItemInfo* item);
void PlaySoundEffect(ItemInfo* item);
void RubbleFX(ItemInfo* item);
void PoseidonSFX(ItemInfo* item);
void ActivateCamera(ItemInfo* item);
void ActivateKey(ItemInfo* item);
void SwapCrowbar(ItemInfo* item); // TODO: To anim command.
void ExplosionFX(ItemInfo* item);
void LaraLocation(ItemInfo* item);
void LaraLocationPad(ItemInfo* item);
void KillActiveBaddys(ItemInfo* item);
void LaraHandsFree(ItemInfo* item); // TODO: To anim command.
void ShootRightGun(ItemInfo* item); // TODO: To anim command.
void ShootLeftGun(ItemInfo* item); // TODO: To anim command.
void SetFog(ItemInfo* item);
void InvisibilityOn(ItemInfo* item);
void InvisibilityOff(ItemInfo* item);
void ResetHair(ItemInfo* item);
void Pickup(ItemInfo* item); // TODO: To anim command.
void Puzzle(ItemInfo* item); // TODO: To anim command.
void DrawRightPistol(ItemInfo* item); // TODO: To anim command.
void DrawLeftPistol(ItemInfo* item); // TODO: To anim command.
void MeshSwapToPour(ItemInfo* item); // TODO: To anim command.
void MeshSwapFromPour(ItemInfo* item); // TODO: To anim command.
void FlashOrange(ItemInfo* item);
void ClearSwarmEnemies(ItemInfo* item);

void DoFlipEffect(int number, ItemInfo* item = nullptr);
