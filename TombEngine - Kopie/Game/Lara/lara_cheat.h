#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Player
{
	void lara_as_fly_cheat(ItemInfo* item, CollisionInfo* coll);
	void lara_col_fly_cheat(ItemInfo* item, CollisionInfo* coll);
}
