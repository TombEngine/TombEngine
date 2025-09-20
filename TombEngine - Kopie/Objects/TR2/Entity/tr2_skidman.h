#pragma once
#include "Game/collision/collide_room.h"
#include "Game/items.h"

namespace TEN::Entities::Creatures::TR2
{
	void InitializeSkidooMan(short itemNumber);
	void SkidooManCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void SkidooManControl(short riderNumber);
}
