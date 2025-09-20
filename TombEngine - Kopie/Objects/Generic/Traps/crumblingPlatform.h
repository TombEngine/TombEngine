#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeCrumblingPlatform(short itemNumber);
	void ControlCrumblingPlatform(short itemNumber);
	void CollideCrumblingPlatform(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
