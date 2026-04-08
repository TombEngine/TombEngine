#pragma once

struct ItemInfo;
struct CollisionInfo;

namespace TEN::Entities::TR4
{
	void InitializeElementPuzzle(short itemNumber);
	void ElementPuzzleDoCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
