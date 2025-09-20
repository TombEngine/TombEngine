#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	void CrowbarSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
