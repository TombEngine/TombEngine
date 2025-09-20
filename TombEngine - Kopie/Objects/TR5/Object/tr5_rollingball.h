#pragma once
#include "Game/items.h"
#include "Game/collision/collide_room.h"

void RollingBallCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
void RollingBallControl(short itemNumber);
void InitializeClassicRollingBall(short itemNumber);
void ClassicRollingBallCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
void ClassicRollingBallControl(short itemNumber);
