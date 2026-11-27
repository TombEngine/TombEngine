#include "framework.h"
#include "Game/Multiplayer/Multiplayer.h"

#include "Game/debug/debug.h"
#include "Game/Multiplayer/RemotePlayer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Specific/configuration.h"

namespace TEN::Multiplayer
{
	// The choice of the player while a script has called Flow.EnableMultiplayer(false). It is not saved: the game starts with multiplayer off every time it is opened. -- divezzi, 24.11.26
	static bool s_isSessionOptionOn = false;

	bool GetMultiplayerOption()
	{
		// With true the player uses the saved option.
		if (g_GameFlow->IsMultiplayerEnabled())
			return g_Configuration.EnableMultiplayer;

		return s_isSessionOptionOn;
	}

	void ApplyMultiplayerOption(bool previousSavedOption)
	{
		if (g_GameFlow->IsMultiplayerEnabled())
			return;

		// The Options page has copied the switch into the saved configuration. With false it is a choice for this session only, so the saved value is put back.
		s_isSessionOptionOn = g_Configuration.EnableMultiplayer;
		g_Configuration.EnableMultiplayer = previousSavedOption;
	}

	bool IsRemotePlayerEnabled()
	{
		return (g_GameFlow->IsMultiplayerDeclared() && GetMultiplayerOption());
	}

	void OnMultiplayerSwitchedOff()
	{
		ClearAllRemoteItems();

		// Clearing the loaded modules makes the next require of LuaSocket go through the gate again.
		if (g_GameScript != nullptr)
			g_GameScript->ExecuteString("package.loaded['socket'] = nil package.loaded['socket.core'] = nil");
	}

	static int SocketGateLoader(lua_State* luaStatePtr)
	{
		return luaL_error(luaStatePtr, "[MP-GATE] LuaSocket is blocked: multiplayer is not in effect. A script must declare it with Flow.EnableMultiplayer() and the player must have it switched on in Options > Multiplayer.");
	}

	static int SocketGateSearcher(lua_State* luaStatePtr)
	{
		auto moduleName = std::string(luaL_checkstring(luaStatePtr, 1));
		bool isSocketModule = (moduleName == "socket" || moduleName.rfind("socket.", 0) == 0);

		// FAILSAFE: g_GameFlow is not set while the script state is created, and it is cleared on shutdown.
		bool isInEffect = (g_GameFlow != nullptr) && IsRemotePlayerEnabled();

		if (!isSocketModule || isInEffect)
		{
			// Returning nil lets the next searcher look for the module.
			lua_pushnil(luaStatePtr);
			return 1;
		}

		TENLog("[MP-GATE] Blocked require of '" + moduleName + "': multiplayer is not in effect.", LogLevel::Warning);
		lua_pushcfunction(luaStatePtr, SocketGateLoader);
		lua_pushstring(luaStatePtr, moduleName.c_str());
		return 2;
	}

	void InstallSocketGate(lua_State* luaStatePtr)
	{
		lua_getglobal(luaStatePtr, "package");
		lua_getfield(luaStatePtr, -1, "searchers");

		int searcherCount = (int)luaL_len(luaStatePtr, -1);
		for (int i = searcherCount; i >= 1; i--)
		{
			lua_rawgeti(luaStatePtr, -1, i);
			lua_rawseti(luaStatePtr, -2, i + 1);
		}

		lua_pushcfunction(luaStatePtr, SocketGateSearcher);
		lua_rawseti(luaStatePtr, -2, 1);
		lua_pop(luaStatePtr, 2);
	}
}
