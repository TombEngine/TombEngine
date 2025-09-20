#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	void AirlockSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
