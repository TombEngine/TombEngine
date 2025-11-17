#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	enum SwitchStatus
	{
		SWITCH_OFF,
		SWITCH_ON,
		SWITCH_ANIMATE // Used to animate switches based on different conditions, such as underwater animations for above ground switches.
	};
	
	void SwitchControl(short itemNumber);
	void SwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
