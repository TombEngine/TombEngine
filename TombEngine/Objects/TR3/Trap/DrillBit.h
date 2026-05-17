#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void ControlDrillBit(short itemNumber);
	void CollideDrillBit(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
