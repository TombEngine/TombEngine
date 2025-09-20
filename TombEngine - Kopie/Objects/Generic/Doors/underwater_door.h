#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Doors
{
	void UnderwaterDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
