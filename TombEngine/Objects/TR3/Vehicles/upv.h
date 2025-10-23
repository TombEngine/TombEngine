#pragma once
#include "Objects/Utils/VehicleHelpers.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Vehicles
{
	enum UPVFlags
	{
		UPV_FLAG_CONTROL = (1 << 0),
		UPV_FLAG_SURFACE = (1 << 1),
		UPV_FLAG_DIVE = (1 << 2),
		UPV_FLAG_DEAD = (1 << 3)
	};

	void InitializeUPV(short itemNumber);
	UPVInfo* GetUPVInfo(ItemInfo* UPVItem);

	void UPVPlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void DoUPVMount(ItemInfo* UPVItem, ItemInfo* laraItem, VehicleMountType mountType);

	void UPVEffects(short itemNumber);
	bool UPVControl(ItemInfo* laraItem, CollisionInfo* coll);
}
