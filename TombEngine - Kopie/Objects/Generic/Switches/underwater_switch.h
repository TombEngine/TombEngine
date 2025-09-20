#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	void CollideUnderwaterWallSwitch(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void CollideUnderwaterCeilingSwitch(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
