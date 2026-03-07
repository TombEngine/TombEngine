#pragma once

#include "Game/Debug/Debug.h"
#include "Scripting/Internal/ReservedScriptNames.h"

/// Constants for LogLevel IDs.
// To be used with @{Util.PrintLog} function.
// @enum Util.LogLevel
// @pragma nostrip

static const auto LOG_LEVEL_IDS = std::unordered_map<std::string, LogLevel>
{
	/// Only information messages will be shown.
	// @mem INFO
	{ ScriptReserved_LogLevelInfo, LogLevel::Info },

	/// Non-critical warnings and information messages will be shown.
	// @mem WARNING
	{ ScriptReserved_LogLevelWarning, LogLevel::Warning },

	/// Critical errors, non-critical warnings and information messages will be shown.
	// @mem ERROR
	{ ScriptReserved_LogLevelError, LogLevel::Error }
};
