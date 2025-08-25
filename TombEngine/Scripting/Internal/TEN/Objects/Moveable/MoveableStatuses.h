#pragma once

#include "Game/items.h"

/// Constants for moveable statuses.
// To be used with @{Objects.Moveable.GetStatus} and @{Objects.Moveable.SetStatus} functions.
// @enum Objects.MoveableStatus
// @pragma nostrip

static const auto MOVEABLE_STATUSES = std::unordered_map<std::string, ItemStatus>
{
	/// Moveable is inactive (was never activated).
	// @mem INACTIVE
	{ "INACTIVE", ItemStatus::ITEM_NOT_ACTIVE },

	/// Moveable is active.
	// @mem ACTIVE
	{ "ACTIVE", ItemStatus::ITEM_ACTIVE },

	/// Moveable is deactivated (was previously active and later deactivated).
	// @mem DEACTIVATED
	{ "DEACTIVATED", ItemStatus::ITEM_DEACTIVATED },

	/// Moveable is invisible.
	// @mem INVISIBLE
	{ "INVISIBLE", ItemStatus::ITEM_INVISIBLE }
};
