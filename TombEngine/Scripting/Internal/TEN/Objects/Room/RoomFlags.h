#pragma once
#include <string>
#include <unordered_map>

#include "Game/room.h"

/***
Constants for room flag IDs.
@enum Objects.RoomFlagID
@pragma nostrip
*/

/*** Table of Objects.RoomFlagID constants.
<br>
Corresponds to room flags in Tomb Editor. To be used with @{Objects.Room.SetFlag} and @{Objects.Room.GetFlag} functions.

The following constants are inside RoomFlagID.

 - `WATER`
 - `QUICKSAND`
 - `SKYBOX`
 - `WIND`
 - `COLD`
 - `DAMAGE`
 - `NOLENSFLARE`

@table Objects.RoomFlagID
*/

static const std::unordered_map<std::string, RoomEnvFlags> ROOM_FLAG_IDS
{
	{ "WATER", RoomEnvFlags::ENV_FLAG_WATER },
	{ "QUICKSAND", RoomEnvFlags::ENV_FLAG_SWAMP },
	{ "SKYBOX", RoomEnvFlags::ENV_FLAG_SKYBOX },
	{ "WIND", RoomEnvFlags::ENV_FLAG_WIND },
	{ "COLD", RoomEnvFlags::ENV_FLAG_COLD },
	{ "DAMAGE", RoomEnvFlags::ENV_FLAG_DAMAGE },
	{ "NOLENSFLARE", RoomEnvFlags::ENV_FLAG_NO_LENSFLARE }
};
