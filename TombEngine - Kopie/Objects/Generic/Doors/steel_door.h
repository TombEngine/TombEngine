#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Doors
{
	void InitializeSteelDoor(short itemNumber);
	void SteelDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
