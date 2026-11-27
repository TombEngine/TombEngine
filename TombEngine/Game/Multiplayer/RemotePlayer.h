#pragma once

#include "Game/Lara/lara_struct.h"
#include "Game/items.h"

namespace TEN::Multiplayer
{
	// Returns the player ID encoded in an item name ("remote_player_<id>" or "lara_double_<id>"), or NO_VALUE.
	int GetRemotePlayerId(const std::string& name);

	void InitializeRemotePlayerObject();
	void InitializeRemotePlayer(short slotIndex);

	// Allocates and initializes a remote player and returns its pool slot, or NO_VALUE.
	short CreateRemotePlayer(const std::string& name, const Vector3i& pos, const EulerAngles& orient, int roomNumber);
	void KillRemotePlayer(short slotIndex);
	void ClearAllRemoteItems();

	// Returns the player info of the remote player behind a renderer index. Throws if there is none.
	LaraInfo& GetRemotePlayerInfo(short rendererIndex);

	// Spawns the dynamic lights of the remote players. Called once per game tick, after the scripts have set the remote states.
	void UpdateRemotePlayerLights();
}
