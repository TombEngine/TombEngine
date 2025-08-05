#pragma once
#include "Game/control/box.h"

struct CreatureBiteInfo;

constexpr auto MAX_VISIBILITY_DISTANCE = BLOCK(8);

constexpr auto FINAL_SHOT_CONE_ANGLE = ANGLE(25.0f);
constexpr auto FINAL_SHOT_COUNT = 4;
constexpr auto FINAL_SHOT_FLAG_INDEX = 7;

bool ShotLara(ItemInfo* item, AI_INFO* AI, const CreatureBiteInfo& gun, short extraRotation, int damage);
short GunMiss(int x, int y, int z, short velocity, short yRot, short roomNumber);
short GunHit(int x, int y, int z, short velocity, short yRot, short roomNumber);
short GunShot(int x, int y, int z, short velocity, short yRot, short roomNumber);
bool Targetable(ItemInfo* item, AI_INFO* ai);
bool TargetVisible(ItemInfo* item, AI_INFO* ai, float maxAngleInDegrees = 45.0f);
void PerformFinalAttack(ItemInfo& item, const CreatureBiteInfo& bite, int headBoneNumber, int deathAnimNumber, int damage, SOUND_EFFECTS soundID);