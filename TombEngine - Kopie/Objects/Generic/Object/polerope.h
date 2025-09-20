#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	void PoleCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
