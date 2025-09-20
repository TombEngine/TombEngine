#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeSpikyCeiling(short itemNumber);
	void ControlSpikyCeiling(short itemNumber);
	void CollideSpikyCeiling(short itemNumber, ItemInfo* item, CollisionInfo* coll);
}
