#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeMovingLaser(short itemNumber);
	void ControlMovingLaser(short itemNumber);
	void CollideMovingLaser(short itemNumber, ItemInfo* item, CollisionInfo* coll);
}
