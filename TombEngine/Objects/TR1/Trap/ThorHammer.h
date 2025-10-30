#pragma once

struct CollisionInfo;
struct ItemInfo;
struct ObjectInfo;

namespace TEN::Entities::Traps
{
	void InitializeThorHammer(short itemNumber);
	void ControlThorHammer(short itemNumber);
	void CollideThorHammer(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void CollideThorHammerHandle(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}
