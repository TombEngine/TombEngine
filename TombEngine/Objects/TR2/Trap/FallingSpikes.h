#pragma once

struct CollisionInfo;
struct ItemInfo;
struct ObjectInfo;

namespace TEN::Entities::Traps
{
	void InitializeFallingSpikes(short itemNumber);

	void ControlFallingSpikes(short itemNumber);
	void CollideFallingSpikes(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
