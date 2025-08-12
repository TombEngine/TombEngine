#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void ControlSPikedFrame(short itemNumber);
	void CollideSpikedFrame(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
