#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void ControlHeavyStamper(short itemNumber);
	void CollideHeavyStamper(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
