#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeFusebox(short itemNumber);
	void ControlFusebox(short itemNumber);
	void CollideFusebox(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
