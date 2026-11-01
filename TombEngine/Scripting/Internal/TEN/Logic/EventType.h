#pragma once
#include "Scripting/Internal/ReservedScriptNames.h"

namespace TEN::Scripting
{	/// Events that can be handled in level scripts.
	// @enum Logic.EventType
	// @pragma nostrip
	static const auto EVENT_TYPES = std::unordered_map<std::string, EventType>
	{
		/// Triggered when an activator enters the volume.
		// @mem VOLUME_ENTER
		{ ScriptReserved_EventOnVolumeEnter, EventType::VolumeEnter },

		/// Triggered while an activator is inside the volume.
		// @mem VOLUME_INSIDE
		{ ScriptReserved_EventOnVolumeInside, EventType::VolumeInside },

		/// Triggered when an activator leaves the volume.
		// @mem VOLUME_LEAVE
		{ ScriptReserved_EventOnVolumeLeave, EventType::VolumeLeave },

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

		/// Triggered when an item is picked up.
		// @mem PICKUP
		{ ScriptReserved_EventOnPickup, EventType::Pickup },

		/// Triggered when a vehicle is entered.
		// @mem VEHICLE_ENTER
		{ ScriptReserved_EventOnVehicleEnter, EventType::VehicleEnter },

		/// Triggered when a vehicle is left.
		// @mem VEHICLE_LEAVE
		{ ScriptReserved_EventOnVehicleLeave, EventType::VehicleLeave },

		/// Triggered when any of the @{Flow.FreezeMode} is activated.
		// @mem FREEZE
		{ ScriptReserved_EventOnFreeze, EventType::Freeze },

		// COMPATIBILITY
		{ "USEITEM", EventType::UseItem },
		{ "ENTER", EventType::VolumeEnter },
		{ "INSIDE", EventType::VolumeInside},
		{ "LEAVE", EventType::VolumeLeave},
	};
}