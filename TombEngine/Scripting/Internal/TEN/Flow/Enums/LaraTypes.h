#pragma once

#include "Scripting/Include/ScriptInterfaceLevel.h"

namespace TEN::Scripting
{
	/// Constants for player types.
	// @enum Flow.LaraType
	// @pragma nostrip

	static const std::unordered_map<std::string, LaraType> PLAYER_TYPES
	{
		/// Normal Lara with a single braid.
		// @mem Normal
		{ "Normal", LaraType::Normal },

		/// Young Lara with two ponytails. This setting does not affect ability to use flares and weapons, which can be removed from inventory via scripting.
		// @mem Young
		{ "Young", LaraType::Young },

		/// Divesuit Lara. As seen in the underwater levels of TR5.
		// @mem Divesuit
		{ "Divesuit", LaraType::Divesuit }
	};
}
