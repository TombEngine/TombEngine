#pragma once

#include "Specific/configuration.h"

namespace TEN::Scripting
{
	/// Constants for control modes.
	// To be used with @{Flow.GetControlMode} and @{Flow.SetControlMode} functions, and with the `controlMode` fields of
	// @{Flow.Settings} and @{Flow.Level}.
	// @enum Flow.ControlMode
	// @pragma nostrip

	static const auto CONTROL_MODES = std::unordered_map<std::string, ScriptControlMode>
	{
		/// No mode is forced: the player chooses it in the options.
		// @mem DEFAULT
		{ "DEFAULT", ScriptControlMode::Default },

		/// Classic control mode.
		// @mem CLASSIC
		{ "CLASSIC", ScriptControlMode::Classic },

		/// Remastered control mode.
		// @mem REMASTERED
		{ "REMASTERED", ScriptControlMode::Remastered }
	};
}
