#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeZipLine(short itemNumber);
	void CollideZipLine(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void ControlZipLine(short itemNumber);
}
