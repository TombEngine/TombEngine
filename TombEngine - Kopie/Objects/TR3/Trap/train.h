#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void ControlTrain(short itemNumber);
	void CollideTrain(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
