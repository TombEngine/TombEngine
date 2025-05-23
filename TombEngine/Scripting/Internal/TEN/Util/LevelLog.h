#pragma once

#include "Game/Debug/Debug.h"
#include "Scripting/Internal/ReservedScriptNames.h"

/// Constants for LogLevel IDs.
// @enum Util.LogLevel
// @pragma nostrip

/// Table of Util.LogLevel constants. To be used with @{Util.PrintLog} function.
//
// - `INFO` - Only information messages will be shown.
// - `WARNING` - Non-critical warnings and information messages will be shown.
// - `ERROR` - Critical errors, non-critical warnings and information messages will be shown.
//
// @table Util.LogLevel

static const auto LOG_LEVEL_IDS = std::unordered_map<std::string, LogLevel>
{
	{ ScriptReserved_LogLevelInfo, LogLevel::Info },
	{ ScriptReserved_LogLevelWarning, LogLevel::Warning },
	{ ScriptReserved_LogLevelError, LogLevel::Error }
};
