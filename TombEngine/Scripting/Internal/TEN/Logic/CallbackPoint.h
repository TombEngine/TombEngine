#pragma once
#include "Scripting/Internal/ReservedScriptNames.h"

namespace TEN::Scripting
{
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
		PreFreeze,
		PostFreeze
	};

	/// Points in the game flow where level scripts can hook into.
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
		{ "PREFREEZE", CallbackPoint::PreFreeze },
		{ "POSTFREEZE", CallbackPoint::PostFreeze }
	};
}
