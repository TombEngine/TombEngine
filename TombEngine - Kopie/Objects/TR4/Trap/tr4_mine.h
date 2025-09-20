#pragma once

struct ItemInfo;
struct CollisionInfo;

namespace TEN::Entities::Traps
{
	void InitializeMine(short itemNumber);
	void ControlMine(short itemNumber);
	void CollideMine(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
