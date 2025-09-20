#pragma once
#include "Objects/Utils/VehicleHelpers.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Vehicles
{
	void InitializeJeep(short itemNumber);

	void JeepPlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void DoJeepMount(ItemInfo* jeepItem, ItemInfo* laraItem, VehicleMountType mountType);

	int JeepControl(ItemInfo* laraItem);
}
