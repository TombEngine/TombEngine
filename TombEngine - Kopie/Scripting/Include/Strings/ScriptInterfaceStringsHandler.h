#pragma once
#include "Scripting/Include/Strings/StringsCommon.h"

class ScriptInterfaceStringsHandler
{
public:
	virtual ~ScriptInterfaceStringsHandler() = default;

	virtual void ProcessDisplayStrings(float deltaTime) = 0;
	virtual void ClearDisplayStrings() = 0;
	virtual void SetCallbackDrawString(CallbackDrawString) = 0;
};

extern ScriptInterfaceStringsHandler* g_GameStringsHandler;
