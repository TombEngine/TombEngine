#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	enum SwitchStatus
	{
		SWITCH_OFF,
		SWITCH_ON,
		SWITCH_WAIT,
		SWITCH_ANIMATE,
		SWITCH_ANIMATE_UNDERWATER
	};
	
	void SwitchControl(short itemNumber);
	void SwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
