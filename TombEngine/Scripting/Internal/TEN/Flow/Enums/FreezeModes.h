#pragma once

#include "Game/control/control.h"

namespace TEN::Scripting
{
	/// Constants for freeze modes.
	// To be used with @{Flow.GetFreezeMode} and @{Flow.SetFreezeMode} functions.
	// @enum Flow.FreezeMode
	// @pragma nostrip

	static const auto FREEZE_MODES = std::unordered_map<std::string, FreezeMode>
	{
		/// Normal in-game operation.
		// @mem NONE
		{ "NONE", FreezeMode::None },

		/// Game is completely frozen, as in pause or inventory menus.
		// @mem FULL
		{ "FULL", FreezeMode::Full },

		/// Game is completely frozen, but with ability to control camera.
		// @mem SPECTATOR
		{ "SPECTATOR", FreezeMode::Spectator },

		/// Game is completely frozen, but with ability to control player. Experimental.
		// @mem PLAYER
		{ "PLAYER", FreezeMode::Player }
	};
}
