#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	void InitializePendulum(short itemNumber);
	void ControlPendulum(short itemNumber);
	void CollidePendulum(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}

