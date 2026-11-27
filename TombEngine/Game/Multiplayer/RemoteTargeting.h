#pragma once

#include <vector>

#include "Game/items.h"

namespace TEN::Multiplayer
{
	// Marks a remote player as a target for the auto-aim of the local player. The mark is kept by player ID. -- divezzi, 20.11.26
	void SetRemotePlayerTargetable(const ItemInfo& item, bool isTargetable);
	bool IsRemotePlayerTargetable(const ItemInfo& item);

	// Returns the remote players that the local player can aim at: occupied, active and marked.
	std::vector<ItemInfo*> GetTargetableRemotePlayers();

	// Gives the point that the local player aims at on a remote player: between its torso and head bones. Returns false, and leaves the point unchanged, when both bones are at the same point, which is what the bone lookup gives when it cannot calculate them.
	bool GetRemotePlayerAimPoint(const ItemInfo& item, Vector3& point);

	// Drops the mark and every reference the local player keeps to a remote player. Called when the remote player is removed.
	void ReleaseRemotePlayerTarget(const ItemInfo& item);
}
