#pragma once
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Logic/LogicHandler.h"

// Why do we need this class?
// We need a way to save and load functions in a way that remembers exactly what "path" they have in the LevelFuncs table hierarchy.
// Thus, even if we use Lua to put a LevelFuncs function in a variable with another, shorter name, we can still pass it into
// LevelVars and have it remember the right function name when loaded.
// This is needed for things like Timers, which call a certain function after a certain amount of time. If we save and then load,
// the FuncNameHolder will be able to use its path as the key to find the actual Lua function to call at the end of the timer.

// The alternative would be to pass in a full string, but then we would need to split the string at runtime to find
// the exact tables to look in, which seems like it would take longer.

class LevelFunc
{
public:
	std::string m_funcName;
	LogicHandler* m_handler;

	template <typename ... Ts> sol::protected_function_result CallCallback(Ts ... vs)
	{
		return m_handler->CallLevelFuncByName(m_funcName, vs...);
	}

	sol::protected_function_result Call(sol::variadic_args args)
	{
		return m_handler->CallLevelFuncByName(m_funcName, args);
	}

	static void Register(sol::table& parent)
	{
		parent.new_usertype<LevelFunc>(ScriptReserved_LevelFunc, sol::no_constructor, sol::meta_function::call, &Call);
	}
};
