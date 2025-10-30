#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::TR4
{
	void InitializeStatuePlinth(short itemNumber);
	void CollideStatuePlinth(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
