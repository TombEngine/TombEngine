#pragma once

#include "Scripting/Include/ScriptInterfaceLevel.h"

namespace TEN::Scripting
{
	/// Constants for player types.
	// @enum Flow.LaraType
	// @pragma nostrip

	static const std::unordered_map<std::string, LaraType> PLAYER_TYPES
	{
		/// Normal Lara.
		// @mem Normal
		{ "Normal", LaraType::Normal },

		/// Young Lara.
		// @mem Young
		{ "Young", LaraType::Young },

		/// Bunhead Lara.
		// @mem Bunhead
		{ "Bunhead", LaraType::Bunhead },

		/// Catsuit Lara.
		// @mem Catsuit
		{ "Catsuit", LaraType::Catsuit },

		/// Divesuit Lara.
		// @mem Divesuit
		{ "Divesuit", LaraType::Divesuit },

		/// Invisible Lara.
		// @mem Invisible
		{ "Invisible", LaraType::Invisible }
	};
}
