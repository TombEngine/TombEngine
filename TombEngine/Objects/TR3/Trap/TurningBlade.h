#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeTurningBlade(short itemNumber);
	void ControlTurningBlade(short itemNumber);
	void CollideTurningBlade(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
