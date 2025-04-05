#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	void InitializePulleySwitch(short itemNumber);
	void CollisionPulleySwitch(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void ControlPulleySwitch(short itemNumber);
}
