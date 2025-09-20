#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	void InitializeFallingBlock(short itemNumber);
	void FallingBlockCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void FallingBlockControl(short itemNumber);
}
