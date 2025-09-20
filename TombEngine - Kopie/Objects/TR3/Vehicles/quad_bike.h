#pragma once
#include "Objects/Utils/VehicleHelpers.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Vehicles
{
	void InitializeQuadBike(short itemNumber);

	void QuadBikePlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void DoQuadBikeMount(ItemInfo* quadBikeItem, ItemInfo* laraItem, VehicleMountType mountType);

	bool QuadBikeControl(ItemInfo* laraItem, CollisionInfo* coll);
}
