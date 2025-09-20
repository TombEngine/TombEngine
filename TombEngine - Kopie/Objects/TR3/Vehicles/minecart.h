#pragma once
#include "Objects/Utils/VehicleHelpers.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Vehicles
{
	void InitializeMinecart(short itemNumber);
	MinecartInfo* GetMinecartInfo(ItemInfo* minecartItem);

	void MinecartPlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void DoMinecartMount(ItemInfo* minecartItem, ItemInfo* laraItem, VehicleMountType mountType);

	bool MinecartControl(ItemInfo* laraItem);

	void MinecartWrenchTake(MinecartInfo* minecart, ItemInfo* laraItem);
	void MinecartWrenchPut(MinecartInfo* minecart, ItemInfo* laraItem);
}
