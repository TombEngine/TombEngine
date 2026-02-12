#pragma once
#include <string>
#include <unordered_map>

#include "Game/room.h"

/// Constants for room flag IDs.
// Corresponds to room flags in Tomb Editor. To be used with @{Objects.Room.SetFlag} and @{Objects.Room.GetFlag} functions.
// @enum Objects.RoomFlagID
// @pragma nostrip

static const std::unordered_map<std::string, RoomEnvFlags> ROOM_FLAG_IDS
{
	/// Water room flag.
	// @mem WATER
	{ "WATER", RoomEnvFlags::ENV_FLAG_WATER },

	/// Quicksand room flag.
	// @mem QUICKSAND
	{ "QUICKSAND", RoomEnvFlags::ENV_FLAG_SWAMP },

	/// Skybox room flag.
	// @mem SKYBOX
	{ "SKYBOX", RoomEnvFlags::ENV_FLAG_SKYBOX },

	/// Wind room flag.
	// @mem WIND
	{ "WIND", RoomEnvFlags::ENV_FLAG_WIND },

	/// Cold room flag.
	// @mem COLD
	{ "COLD", RoomEnvFlags::ENV_FLAG_COLD },

	/// Damage room flag.
	// @mem DAMAGE
	{ "DAMAGE", RoomEnvFlags::ENV_FLAG_DAMAGE },

	/// No lens flare room flag.
	// @mem NOLENSFLARE
	{ "NOLENSFLARE", RoomEnvFlags::ENV_FLAG_NO_LENSFLARE }
};
