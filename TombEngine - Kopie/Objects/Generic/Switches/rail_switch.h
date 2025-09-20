#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	void RailSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
