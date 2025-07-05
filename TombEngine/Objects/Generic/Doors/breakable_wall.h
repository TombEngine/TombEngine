#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Doors
{
	void BreakableWallCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
