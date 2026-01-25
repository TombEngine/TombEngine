#pragma once
#include "Game/effects/effects.h"

void ShootAtEnemy(Vector3i target, ItemInfo* item, int fxNumber);
void ControlMissile(short fxNumber);
void ControlNatlaGun(short fxNumber);

short ShardGun(int x, int y, int z, short velocity, short yRot, short roomNumber);
short BombGun(int x, int y, int z, short velocity, short yRot, short roomNumber);
