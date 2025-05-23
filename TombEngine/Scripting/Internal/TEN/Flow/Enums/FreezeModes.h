#pragma once

#include "Game/control/control.h"

namespace TEN::Scripting
{

	/// Constants for freeze modes.
	// @enum Flow.FreezeMode
	// @pragma nostrip

	/// Table of Flow.FreezeMode constants.
	// To be used with @{Flow.GetFreezeMode} and @{Flow.SetFreezeMode} functions.
	// @table FreezeMode
	// 
	// - `NONE` - Normal in-game operation.
	// - `FULL` - Game is completely frozen, as in pause or inventory menus.
	// - `SPECTATOR` - Game is completely frozen, but with ability to control camera.
	// - `PLAYER` - Game is completely frozen, but with ability to control player. Experimental.

	static const auto FREEZE_MODES = std::unordered_map<std::string, FreezeMode>
	{
		{ "NONE", FreezeMode::None },
		{ "FULL", FreezeMode::Full },
		{ "SPECTATOR", FreezeMode::Spectator },
		{ "PLAYER", FreezeMode::Player }
	};
}
