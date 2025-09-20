#pragma once
#include "Objects/Utils/VehicleHelpers.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Vehicles
{
	void InitializeMotorbike(short itemNumber);

	void MotorbikePlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void DoMotorbikeMount(ItemInfo* motorbikeItem, ItemInfo* laraItem, VehicleMountType mountType);

	bool MotorbikeControl(ItemInfo* laraItem, CollisionInfo* coll);
}
