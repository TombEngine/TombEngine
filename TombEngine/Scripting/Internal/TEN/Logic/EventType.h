#pragma once
#include "Scripting/Internal/ReservedScriptNames.h"

namespace TEN::Scripting
{	/// Events that can be handled in level scripts.
	// @enum Logic.EventType
	// @pragma nostrip
	static const auto EVENT_TYPES = std::unordered_map<std::string, EventType>
	{
		/// Triggered when an activator enters the volume.
		// @mem ENTER
		{ ScriptReserved_EventOnEnter, EventType::Enter },

		/// Triggered while an activator is inside the volume.
		// @mem INSIDE
		{ ScriptReserved_EventOnInside, EventType::Inside },

		/// Triggered when an activator leaves the volume.
		// @mem LEAVE
		{ ScriptReserved_EventOnLeave, EventType::Leave },

		/// Triggered each game loop.
		// @mem LOOP
		{ ScriptReserved_EventOnLoop, EventType::Loop },

		/// Triggered when a saved game is loaded.
		// @mem LOAD
		{ ScriptReserved_EventOnLoad, EventType::Load },

		/// Triggered when the game is saved.
		// @mem SAVE
		{ ScriptReserved_EventOnSave, EventType::Save },

		/// Triggered when the level starts.
		// @mem START
		{ ScriptReserved_EventOnStart, EventType::Start },

		/// Triggered when the level ends.
		// @mem END
		{ ScriptReserved_EventOnEnd, EventType::End },

		/// Triggered when an item is used from inventory.
		// @mem USE_ITEM
		{ ScriptReserved_EventOnUseItem, EventType::UseItem },

		/// Triggered when any of the Freeze modes are activated.
		// @mem FREEZE
		{ ScriptReserved_EventOnFreeze, EventType::Freeze },

		// COMPATIBILITY
		{ "USEITEM", EventType::UseItem }
	};
}