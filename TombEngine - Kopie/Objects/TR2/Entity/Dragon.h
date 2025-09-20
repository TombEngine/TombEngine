#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR2
{
	void InitializeDragon(short itemNumber);
	void ControlDragon(short itemNumber);
	void CollideDragonFront(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void CollideDragonBack (short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
