#include "framework.h"
#include "Scripting/Include/ScriptInterfaceState.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Collision/Probe.h"
#include "Scripting/Internal/TEN/Effects/EffectsFunctions.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"
#include "Scripting/Internal/TEN/Input/InputHandler.h"
#include "Scripting/Internal/TEN/Inventory/InventoryHandler.h"
#include "Scripting/Internal/TEN/Logic/LogicHandler.h"
#include "Scripting/Internal/TEN/Objects/ObjectsHandler.h"
#include "Scripting/Internal/TEN/Strings/StringsHandler.h"
#include "Scripting/Internal/TEN/Sound/SoundHandler.h"
#include "Scripting/Internal/TEN/Util/Util.h"
#include "Scripting/Internal/TEN/View/ViewHandler.h"

constexpr auto DEADLOCK_CHECK_INTERVAL = 500;
constexpr auto DEADLOCK_HOOK_INSTRUCTION_COUNT = 1000;

static sol::state SolState;
static sol::table RootTable;

int lua_exception_handler(lua_State* luaStatePtr, sol::optional<const std::exception&> exception, sol::string_view description)
{
	return luaL_error(luaStatePtr, description.data());
}

void lua_deadlock_handler(lua_State* luaStatePtr, lua_Debug* debug)
{
	static unsigned int funcCount = 0;
	static auto lastCheck = std::chrono::steady_clock::now();

	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCheck).count();

	// Don't process if script engine was unitialized or not initialized yet.
	if (g_GameScript == nullptr)
		return;

	// Check for deadlocks with specified interval to avoid high CPU usage.
	if (elapsed < DEADLOCK_CHECK_INTERVAL)
		return;
	else
		lastCheck = now;

	// If lua function call count isn't zero (which means we are outside of Lua call) and haven't changed,
	// it means that no new lua function was called since the last check, therefore we are in a deadlock.
	auto nextFuncCount = g_GameScript->GetFunctionCallCount();

	if (nextFuncCount > 0 && nextFuncCount == funcCount)
	{
		lua_Debug info = {};

		if (lua_getstack(luaStatePtr, 0, &info) && lua_getinfo(luaStatePtr, "nSl", &info))
		{
			luaL_error(luaStatePtr, "Script deadlock detected in '%s', line %d. Check if your script contains infinite loops.",
				info.short_src ? info.short_src : "unknown", info.currentline);
		}
		else
		{
			luaL_error(luaStatePtr, "General script deadlock detected. Check if your script contains infinite loops.");
		}
	}

	funcCount = nextFuncCount;
}

ScriptInterfaceGame* ScriptInterfaceState::CreateGame()
{
	return new LogicHandler(&SolState, RootTable);
}

ScriptInterfaceFlowHandler* ScriptInterfaceState::CreateFlow()
{
	return new FlowHandler(&SolState, RootTable);
}

ScriptInterfaceObjectsHandler* ScriptInterfaceState::CreateObjectsHandler()
{
	return new ObjectsHandler(&SolState, RootTable);
}

ScriptInterfaceStringsHandler* ScriptInterfaceState::CreateStringsHandler()
{
	return new StringsHandler(&SolState, RootTable);
}

void ScriptInterfaceState::Init(const std::string& assetsDir)
{
	SolState.open_libraries(
		sol::lib::base, sol::lib::math, sol::lib::package, sol::lib::coroutine,
		sol::lib::table, sol::lib::string, sol::lib::debug);

	SolState.script("package.path=\"" + assetsDir + "Scripts/?.lua\"");
	SolState.set_exception_handler(lua_exception_handler);
	lua_sethook(SolState, lua_deadlock_handler, LUA_MASKCOUNT, DEADLOCK_HOOK_INSTRUCTION_COUNT);

	RootTable = sol::table(SolState.lua_state(), sol::create);
	SolState.set(ScriptReserved_TEN, RootTable);

	// Misc. handlers not assigned above.
	TEN::Scripting::InventoryHandler::Register(&SolState, RootTable);
	TEN::Scripting::Collision::Register(&SolState, RootTable);
	TEN::Scripting::Effects::Register(&SolState, RootTable);
	TEN::Scripting::Input::Register(&SolState, RootTable);
	TEN::Scripting::Sound::Register(&SolState, RootTable);
	TEN::Scripting::Util::Register(&SolState, RootTable);
	TEN::Scripting::View::Register(&SolState, RootTable);
}
