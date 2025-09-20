#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void StargateControl(short itemNumber);
	void StargateCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
