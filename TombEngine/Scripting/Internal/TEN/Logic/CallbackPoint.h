#pragma once
#include "Scripting/Internal/ReservedScriptNames.h"

namespace TEN::Scripting
{
	enum class EntityCallbackPoint
	{
		Killed,
		Hit,
		PreLoop,
		PostLoop,
		ObjectCollided,
		RoomCollided,

		Count
	};

	enum class LevelFuncCallbackPoint
	{
		Start,
		Load,
		Loop,
		Save,
		End,
		UseItem,
		Pickup,
		VehicleEnter,
		VehicleLeave,
		Freeze,

		Count
	};

	enum class CallbackPoint
	{
		PreStart,
		PostStart,
		PreLoad,
		PostLoad,
		PreLoop,
		PostLoop,
		PreSave,
		PostSave,
		PreEnd,
		PostEnd,
		PreUseItem,
		PostUseItem,
		PrePickup,
		PostPickup,
		PreVehicleEnter,
		PostVehicleEnter,
		PreVehicleLeave,
		PostVehicleLeave,
		PreFreeze,
		PostFreeze,

		Count
	};

	static auto LEVELFUNC_CALLBACK_POINTS = std::unordered_map<LevelFuncCallbackPoint, std::string>
	{
		{ LevelFuncCallbackPoint::Start, ScriptReserved_OnStart },
		{ LevelFuncCallbackPoint::Load, ScriptReserved_OnLoad },
		{ LevelFuncCallbackPoint::Loop, ScriptReserved_OnLoop },
		{ LevelFuncCallbackPoint::Save, ScriptReserved_OnSave },
		{ LevelFuncCallbackPoint::End, ScriptReserved_OnEnd },
		{ LevelFuncCallbackPoint::UseItem, ScriptReserved_OnUseItem },
		{ LevelFuncCallbackPoint::Pickup, ScriptReserved_OnPickup },
		{ LevelFuncCallbackPoint::VehicleEnter, ScriptReserved_OnVehicleEnter },
		{ LevelFuncCallbackPoint::VehicleLeave, ScriptReserved_OnVehicleLeave },
		{ LevelFuncCallbackPoint::Freeze, ScriptReserved_OnFreeze }
	};

	/// Points in the game flow where level scripts can hook into. Used with @{Logic.AddCallback} and @{Logic.RemoveCallback} methods.
	// @enum Logic.CallbackPoint
	// @pragma nostrip
	static const auto CALLBACK_POINTS = std::unordered_map<std::string, CallbackPoint>
	{
		/// Will be called immediately before LevelFuncs.OnStart.
		// @mem PRE_START
		{ ScriptReserved_PreStart, CallbackPoint::PreStart },

		/// Will be called immediately after LevelFuncs.OnStart.
		// @mem POST_START
		{ ScriptReserved_PostStart, CallbackPoint::PostStart },

		/// Will be called immediately before LevelFuncs.OnLoad.
		// @mem PRE_LOAD
		{ ScriptReserved_PreLoad, CallbackPoint::PreLoad },

		/// Will be called immediately after LevelFuncs.OnLoad.
		// @mem POST_LOAD
		{ ScriptReserved_PostLoad, CallbackPoint::PostLoad },

		/// Will be called in the beginning of game loop (LevelFuncs.OnLoop).
		// @mem PRE_LOOP
		{ ScriptReserved_PreLoop, CallbackPoint::PreLoop },

		/// Will be called at the end of game loop (LevelFuncs.OnLoop).
		// @mem POST_LOOP
		{ ScriptReserved_PostLoop, CallbackPoint::PostLoop },

		/// Will be called immediately before LevelFuncs.OnSave.
		// @mem PRE_SAVE
		{ ScriptReserved_PreSave, CallbackPoint::PreSave },

		/// Will be called immediately after LevelFuncs.OnSave.
		// @mem POST_SAVE
		{ ScriptReserved_PostSave, CallbackPoint::PostSave },

		/// Will be called immediately before LevelFuncs.OnEnd.
		// @mem PRE_END
		{ ScriptReserved_PreEnd, CallbackPoint::PreEnd },

		/// Will be called immediately after LevelFuncs.OnEnd.
		// @mem POST_END
		{ ScriptReserved_PostEnd, CallbackPoint::PostEnd },

		/// Will be called immediately before LevelFuncs.OnUseItem.
		// @mem PRE_USE_ITEM
		{ ScriptReserved_PreUseItem, CallbackPoint::PreUseItem },

		/// Will be called immediately after LevelFuncs.OnUseItem.
		// @mem POST_USE_ITEM
		{ ScriptReserved_PostUseItem, CallbackPoint::PostUseItem },

		/// Will be called when a pickup animation starts, before the item is added to inventory.
		// @mem PRE_PICKUP
		{ ScriptReserved_PrePickup, CallbackPoint::PrePickup },

		/// Will be called after a pickup is added to inventory.
		// @mem POST_PICKUP
		{ ScriptReserved_PostPickup, CallbackPoint::PostPickup },

		/// Will be called immediately before a vehicle mount is committed.
		// @mem PRE_VEHICLE_ENTER
		{ ScriptReserved_PreVehicleEnter, CallbackPoint::PreVehicleEnter },

		/// Will be called immediately after player is assigned to a vehicle.
		// @mem POST_VEHICLE_ENTER
		{ ScriptReserved_PostVehicleEnter, CallbackPoint::PostVehicleEnter },

		/// Will be called immediately before a vehicle dismount is committed.
		// @mem PRE_VEHICLE_LEAVE
		{ ScriptReserved_PreVehicleLeave, CallbackPoint::PreVehicleLeave },

		/// Will be called immediately after player leaves a vehicle.
		// @mem POST_VEHICLE_LEAVE
		{ ScriptReserved_PostVehicleLeave, CallbackPoint::PostVehicleLeave },

		/// Will be called immediately before LevelFuncs.OnFreeze.
		// @mem PRE_FREEZE
		{ ScriptReserved_PreFreeze, CallbackPoint::PreFreeze },

		/// Will be called immediately after LevelFuncs.OnFreeze.
		// @mem POST_FREEZE
		{ ScriptReserved_PostFreeze, CallbackPoint::PostFreeze },

		// COMPATIBILITY
		{ "POSTSTART", CallbackPoint::PostStart },
		{ "PRELOAD", CallbackPoint::PreLoad },
		{ "POSTLOAD", CallbackPoint::PostLoad },
		{ "PRELOOP", CallbackPoint::PreLoop },
		{ "PRECONTROLPHASE", CallbackPoint::PreLoop },
		{ "POSTLOOP", CallbackPoint::PostLoop },
		{ "POSTCONTROLPHASE", CallbackPoint::PostLoop },
		{ "PRESAVE", CallbackPoint::PreSave },
		{ "POSTSAVE", CallbackPoint::PostSave },
		{ "PREEND", CallbackPoint::PreEnd },
		{ "POSTEND", CallbackPoint::PostEnd },
		{ "PREUSEITEM", CallbackPoint::PreUseItem },
		{ "POSTUSEITEM", CallbackPoint::PostUseItem },
		{ "PREPICKUP", CallbackPoint::PrePickup },
		{ "POSTPICKUP", CallbackPoint::PostPickup },
		{ "PREVEHICLEENTER", CallbackPoint::PreVehicleEnter },
		{ "POSTVEHICLEENTER", CallbackPoint::PostVehicleEnter },
		{ "PREVEHICLELEAVE", CallbackPoint::PreVehicleLeave },
		{ "POSTVEHICLELEAVE", CallbackPoint::PostVehicleLeave },
		{ "PREFREEZE", CallbackPoint::PreFreeze },
		{ "POSTFREEZE", CallbackPoint::PostFreeze }
	};
}
