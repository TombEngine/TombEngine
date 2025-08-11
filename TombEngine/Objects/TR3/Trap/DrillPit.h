#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void ControlDrillPit(short itemNumber);
	void CollideDrillPit(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
