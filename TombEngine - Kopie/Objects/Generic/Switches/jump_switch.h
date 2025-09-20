#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	void JumpSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
