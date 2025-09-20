#pragma once

class Vector3i;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	enum class PushableEnvironmentType
	{
		Air,
		FlatFloor,
		SlopedFloor,
		Water,
		WaterFloor
	};

	struct PushableCollisionData
	{
		PushableEnvironmentType EnvType = PushableEnvironmentType::Air;

		int FloorHeight	  = 0;
		int CeilingHeight = 0;
	};

	bool IsPushableValid(ItemInfo& pushableItem);
	bool IsPushableObjectMoveAllowed(ItemInfo& pushableItem, const Vector3i& targetPos, int targetRoomNumber);
	bool IsValidForPlayer(const ItemInfo& pushableItem);

	bool PushableIdleConditions(ItemInfo& pushableItem);
	bool TestPushableMovementConditions(ItemInfo& pushableItem, bool hasPushAction, bool hasPullAction);

	PushableCollisionData GetPushableCollision(ItemInfo& item);
}
