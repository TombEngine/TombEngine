#pragma once
#include "framework.h"

#include "Scripting/Internal/ScriptAssert.h"

class LuaHandler
{
protected:
	sol::state*	_lua;
	sol::table _globals;

public:
	LuaHandler(sol::state* lua);
	LuaHandler(const LuaHandler&) = delete;
	LuaHandler& operator=(const LuaHandler&) = delete;
	~LuaHandler() = default;

	void ExecuteScript(const std::string& luaFilename, bool isOptional = false);
	void ExecuteString(const std::string& command);

	void ResetGlobals();

	sol::state* GetState()
	{
		return _lua;
	};

	template <typename T>void MakeReadOnlyTable(sol::table parent, const std::string& tableName, const T& container)
	{
		// Put all data into metatable.
		auto metatable = tableName + "Meta";
		_lua->set(metatable, sol::as_table(container));

		auto mtmt = tableName + "MetaMeta";
		auto mtmtTable = _lua->create_named_table(mtmt);

		// Make metatable's metatable's __index fail an assert to generate warning/error when trying to use missing variable.
		auto lam = [tableName](sol::table tab, std::string const& key)
		{
			ScriptAssertF(false, tableName + " has no member \"" + key +"\"");
		};

		mtmtTable[sol::meta_method::index] = lam;
		_lua->safe_script("setmetatable(" + metatable + ", " + mtmt + ")");

		// Make metatable's __index refer to itself so that requests to main table will go through to metatable
		// (and thus container's members).
		_lua->safe_script(metatable + ".__index = " + metatable);
		
		_lua->safe_script(metatable + ".__type = \"readonly\"");

		// Don't allow table to have new elements put into it.
		_lua->safe_script(metatable + ".__newindex = function() error('" + tableName + " is read-only') end");

		// Protect metatable.
		_lua->safe_script(metatable + ".__metatable = 'metatable is protected'");

		auto tab = _lua->create_named_table(tableName);

		_lua->safe_script("setmetatable(" + tableName + ", " + metatable + ")");

		// Point initial metatable variable away from its contents. This is just for cleanliness.
		parent.set(tableName, tab);

		_lua->safe_script(tableName + " = nil");
		_lua->safe_script(metatable + " = nil");
		_lua->safe_script(mtmt + " = nil");
	}
};
