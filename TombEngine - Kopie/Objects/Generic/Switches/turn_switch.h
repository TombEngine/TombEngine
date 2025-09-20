#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	void TurnSwitchControl(short itemNumber);
	void TurnSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
