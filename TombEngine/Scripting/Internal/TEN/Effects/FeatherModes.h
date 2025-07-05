#pragma once

#include "Game/effects/Streamer.h"

/// Constants for feather modes.
// @enum Effects.StreamerFeatherMode
// @pragma nostrip

/// Table of Effects.StreamerFeatherMode constants.
// To be used with @{Effects.EmitStreamer} function.
//
// - `NONE`
// - `CENTER`
// - `LEFT`
// - `RIGHT`
//
// @table Effects.StreamerFeatherMode

using namespace TEN::Effects::Streamer;

namespace TEN::Scripting::Effects
{
	static const auto FEATHER_MODES = std::unordered_map<std::string, StreamerFeatherMode>
	{
		{ "NONE", StreamerFeatherMode::None },
		{ "CENTER", StreamerFeatherMode::Center },
		{ "LEFT", StreamerFeatherMode::Left },
		{ "RIGHT",StreamerFeatherMode::Right}
	};
}
