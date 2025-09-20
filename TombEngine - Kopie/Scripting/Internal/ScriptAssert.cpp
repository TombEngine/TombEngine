#include "framework.h"
#include "Scripting/Internal/ScriptAssert.h"

static ErrorMode ScriptErrorMode = ErrorMode::Warn;

void ScriptWarn(const std::string& msg)
{
	switch (ScriptErrorMode)
	{
	case ErrorMode::Terminate:
	case ErrorMode::Warn:
		TENLog(msg, LogLevel::Warning, LogConfig::All);
		break;
	}
}

bool ScriptAssert(bool cond, const std::string& msg, std::optional<ErrorMode> forceMode)
{
	if (!cond)
	{
		auto mode = forceMode ? *forceMode : ScriptErrorMode;
		switch (mode)
		{
		case ErrorMode::Warn:
			TENLog(msg, LogLevel::Error, LogConfig::All);
			break;

		case ErrorMode::Terminate:
			TENLog(msg, LogLevel::Error, LogConfig::All);
			throw TENScriptException(msg);
			break;
		}
	}

	return cond;
}

void SetScriptErrorMode(ErrorMode mode)
{
	ScriptErrorMode = mode;
}

ErrorMode GetScriptErrorMode()
{
	return ScriptErrorMode;
}
