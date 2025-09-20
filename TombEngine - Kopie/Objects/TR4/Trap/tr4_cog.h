#pragma once

struct ItemInfo;
struct CollisionInfo;

namespace TEN::Entities::Traps
{
	void ControlCog(short itemNumber);
	void CollideCog(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
