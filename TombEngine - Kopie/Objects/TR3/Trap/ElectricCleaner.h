#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeElectricCleaner(short itemNumber);
	void ControlElectricCleaner(short itemNumber);
	void CollideElectricCleaner(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
