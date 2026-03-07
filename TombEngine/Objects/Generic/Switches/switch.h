#pragma once

struct CollisionInfo;
struct ItemInfo;

void ProcessShootSwitch(ItemInfo* item);
void InitializeShootSwitch(short itemNumber);
void ShootSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
