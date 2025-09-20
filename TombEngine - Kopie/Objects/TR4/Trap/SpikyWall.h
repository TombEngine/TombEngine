#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeSpikyWall(short itemNumber);
	void ControlSpikyWall(short itemNumber);
	void CollideSpikyWall(short itemNumber, ItemInfo* item, CollisionInfo* coll);
}
