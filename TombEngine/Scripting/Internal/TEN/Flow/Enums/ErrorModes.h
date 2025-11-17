#pragma once

#include "Game/control/control.h"
#include "Scripting/Internal/ScriptAssert.h"

namespace TEN::Scripting
{
	/// Constants for error modes.
	// To be used in @{Flow.Settings.System.errorMode} setting.
	// <br>
	// In all of these modes, an *unrecoverable* error will boot you to the title level. If the title level itself
	// has an unrecoverable error, the game will close.
	// @enum Flow.ErrorMode
	// @pragma nostrip

	static const auto ERROR_MODES = std::unordered_map<std::string, ErrorMode>
	{
		/// Do nothing when a recoverable script error is hit.
		// <br>
		// Think _very_ carefully before using this setting.
		// These error modes are here to help you to keep your scripts working properly,
		// but if you opt to ignore errors, you won't be alerted if you've misused a function or passed an invalid argument.
		// @mem SILENT
		{ "SILENT", ErrorMode::Silent },

		/// Print to the log file and continue running the application when a recoverable script error is hit.
		// Choose this one if booting to the title level is too much for you.
		// @mem WARN
		{ "WARN", ErrorMode::Warn },

		/// Print to the log file and return to the title level when any script error is hit.
		// This is the one you will want to go for if you want to know _immediately_ if something has gone wrong.
		// @mem TERMINATE
		{ "TERMINATE", ErrorMode::Terminate }
	};
}
