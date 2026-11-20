#pragma once

#include "Specific/configuration.h"

namespace TEN::Scripting
{
	/// Constants for control modes.
	// To be used with @{Flow.GetControlMode} and @{Flow.SetControlMode} functions, and with the `controlMode` fields of
	// @{Flow.Settings} and @{Flow.Level}. Each constant is a number; printing a value from @{Flow.GetControlMode}
	// shows that number, so compare it with these constants instead of the number itself.
	// @enum Flow.ControlMode
	// @pragma nostrip

	static const auto CONTROL_MODES = std::unordered_map<std::string, ScriptControlMode>
	{
		/// 0: no mode is forced: the player chooses it in the options.
		// @mem DEFAULT
		{ "DEFAULT", ScriptControlMode::Default },

		/// 1: Classic control mode.
		// @mem CLASSIC
		{ "CLASSIC", ScriptControlMode::Classic },

		/// 2: Remastered control mode.
		// @mem REMASTERED
		{ "REMASTERED", ScriptControlMode::Remastered }
	};
}
