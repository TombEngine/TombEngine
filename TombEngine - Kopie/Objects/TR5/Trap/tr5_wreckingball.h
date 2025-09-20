#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeWreckingBall(short itemNumber);
	void CollideWreckingBall(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void ControlWreckingBall(short itemNumber);
}
