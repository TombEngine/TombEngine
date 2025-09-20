#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeSquishyBlock(short itemNumber);
	void ControlSquishyBlock(short itemNumber);
	void ControlFallingSquishyBlock(short itemNumber);
	void CollideSquishyBlock(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void CollideFallingSquishyBlock(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
