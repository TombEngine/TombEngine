#pragma once
#include "framework.h"

#include <filesystem>
#include "Scripting/Internal/LuaHandler.h"

LuaHandler::LuaHandler(sol::state* lua) : _lua{ lua }
{
}

void LuaHandler::ResetGlobals()
{
	auto mt = sol::table{ *_lua, sol::create };
	_globals = sol::table{ *_lua, sol::create };
	mt.set(sol::meta_function::new_index, _globals);
	mt.set(sol::meta_function::index, _globals);

	_lua->set(sol::metatable_key, mt);
}

void LuaHandler::ExecuteScript(const std::string& luaFilename, bool isOptional)
{
	if (isOptional && !std::filesystem::is_regular_file(luaFilename))
		return;

	auto result = _lua->safe_script_file(luaFilename, sol::script_pass_on_error);
	if (!result.valid())
	{
		sol::error error = result;
		throw TENScriptException{ error.what() };
	}
}

void LuaHandler::ExecuteString(const std::string& command)
{
	auto result = _lua->safe_script(command, sol::environment(_lua->lua_state(), sol::create, _lua->globals()), sol::script_pass_on_error);
	if (!result.valid())
	{
		sol::error error = result;
		throw TENScriptException{ error.what() };
	}
}
