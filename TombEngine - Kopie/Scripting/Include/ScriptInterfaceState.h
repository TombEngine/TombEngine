#pragma once
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Include/Strings/ScriptInterfaceStringsHandler.h"

class ScriptInterfaceState
{
public:
	static ScriptInterfaceGame* CreateGame();
	static ScriptInterfaceFlowHandler* CreateFlow();
	static ScriptInterfaceObjectsHandler* CreateObjectsHandler();
	static ScriptInterfaceStringsHandler* CreateStringsHandler();
	static void ScriptInterfaceState::Init(const std::string& assetsDir);
};
