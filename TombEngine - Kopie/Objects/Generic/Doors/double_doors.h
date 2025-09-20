#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Doors
{
	void DoubleDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
