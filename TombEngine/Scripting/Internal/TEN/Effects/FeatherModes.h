#pragma once

#include "Game/effects/Streamer.h"

using namespace TEN::Effects::Streamer;

namespace TEN::Scripting::Effects
{
	/// Constants for feather modes.
	// To be used with @{Effects.EmitStreamer} function.
	// @enum Effects.StreamerFeatherMode
	// @pragma nostrip

	static const auto FEATHER_MODES = std::unordered_map<std::string, StreamerFeatherMode>
	{
		/// No feather effect.
		// @mem NONE
		{ "NONE", StreamerFeatherMode::None },

		/// Center feather effect.
		// @mem CENTER
		{ "CENTER", StreamerFeatherMode::Center },

		/// Left feather effect.
		// @mem LEFT
		{ "LEFT", StreamerFeatherMode::Left },

		/// Right feather effect.
		// @mem RIGHT
		{ "RIGHT",StreamerFeatherMode::Right}
	};
}
