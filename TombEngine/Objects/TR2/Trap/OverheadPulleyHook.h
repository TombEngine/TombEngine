#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void ControlOverheadPulleyHook(short itemNumber);
	void CollideOverheadPulleyHook(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
