#pragma once

struct ItemInfo;
struct CollisionInfo;

namespace TEN::Entities::Traps
{
	void CollideBlade(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
