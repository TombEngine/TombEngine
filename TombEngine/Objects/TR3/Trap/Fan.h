#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeFan(short itemNumber);
	void ControlFan(short itemNumber);
	void CollideFan(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
