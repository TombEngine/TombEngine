#pragma once
#include "Game/collision/collide_room.h"
#include "Game/items.h"

namespace TEN::Entities::Switches
{
	void InitializeCrowDoveSwitch(short itemNumber);
	void CrowDoveSwitchControl(short itemNumber);
	void CrowDoveSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
