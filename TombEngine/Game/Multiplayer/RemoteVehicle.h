#pragma once

#include <string>

#include "Game/items.h"

namespace TEN::Multiplayer
{
	bool IsRemoteVehicleObject(int objectNumber);

	// Returns the pool slot, or NO_VALUE on failure (not a vehicle, not loaded, or no free slot).
	short CreateRemoteVehicle(GAME_OBJECT_ID objectID, const std::string& name, const Vector3i& pos, const EulerAngles& orient, int roomNumber);
	void  KillRemoteVehicle(short slotIndex);

	// Sets the pose and finds the room again, since a pooled item is in no room list and does not update its room by itself.
	void  UpdateRemoteVehiclePose(ItemInfo& item, const Vector3i& pos, const EulerAngles& orient);
}
