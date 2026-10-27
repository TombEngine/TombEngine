#pragma once
#include <unordered_set>

#include "Game/items.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Logic/CallbackPoint.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"

using CallbackSet = std::unordered_set<std::string>;
using CallbackRegistry = std::array<CallbackSet, (int)TEN::Scripting::CallbackPoint::Count>;
using LevelFuncCallbackRegistry = std::array<sol::protected_function, (int)LevelFuncCallbackPoint::Count>;

class LevelFunc;

class LogicHandler : public ScriptInterfaceGame
{
private:
	// Hierarchy of tables.
	//
	// For example:
	// LevelFuncs
	// |-	Engine
	//		|-	Timer
	//		|-	Util
	// |-	External
	//		|-	EnemiesBySteve
	//		|-	BetterEnemiesByChris
	//			|-	SubTable
	//
	// Each of these tables can only contain other tables as well as a string with their "path".
	// For example, the SubTable table will have a path of "LevelFuncs.Ext.MySecondLib.SubTable".
	// It uses this to construct the full path name of any functions that end up in m_levelFuncs_luaFunctions.
	//
	// Each of these has a metatable whose __index metamethod looks in m_levelFuncsTables, using the path
	// as the key, for the full name of the function. It then gets the FuncNameHolder from m_levelFuncsFakeFuncs,
	// and that FuncNameHolder's __call metamethod looks in m_levelFuncs_luaFunctions for the real function.
	sol::table _levelFuncs = {};

	// Maps full function paths into Lua functions.
	std::unordered_map<std::string, sol::protected_function> _levelFuncs_luaFunctions = {};

	// Maps full function paths to LevelFunc objects.
	// This is a table instead of a C++ container to more easily interface with Sol.
	sol::table _levelFuncs_levelFuncObjects = {};

	// Contains tables; each table refers to a table in the LevelFuncs hierarchy, and contains the full names
	// of the functions to index in m_levelFuncs_luaFunctions.
	// Tables are non-nested, so the following are all at the base level of m_levelFuncsTables.
	// "LevelFuncs"
	// "LevelFuncs.Engine"
	// "LevelFuncs.Engine.Util"
	// "LevelFuncs.MyLevel"
	// "LevelFuncs.MyLevel.CoolFuncs"
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> _levelFuncs_tablesOfNames = {};

	// Lists of TEN.Logic callbacks and LevelFuncs callbacks.
	CallbackRegistry _callbackSets = {};
	LevelFuncCallbackRegistry _levelFuncCallbacks = {};

	std::optional<CallbackPoint> _lastCallbackPoint = std::nullopt;

	std::vector<std::variant<std::string, unsigned int>> _savedVarPath;

	LuaHandler _handler;
	std::string _consoleInput = {};
	bool _shortenedCalls = false;
	bool _insideFunction = false;
	unsigned int _functionCallCount = 0;

	void PerformConsoleInput();
	void PerformMoveableCallbacks(CallbackPoint point, short itemNumber);
	void PerformMoveableCallbacks(LevelFuncCallbackPoint callback, CallbackPoint prePoint, CallbackPoint postPoint, short itemNumber, bool postLoop);

	std::string GetRequestedPath() const;

	void ResetLevelTables();
	void ResetGameTables();
	void ResetGlobalTables();

	void SerializeScriptTable(const sol::table& tab, std::vector<SavedVar>& vars);
	std::unordered_map<unsigned int, sol::table> DeserializeScriptVars(const std::vector<SavedVar>& vars);

public:	
	LogicHandler(sol::state* lua, sol::table& parent);

	template <typename ... Ts> sol::protected_function_result CallLevelFuncBase(const sol::protected_function& func, Ts&& ... vs)
	{
		bool insideFunction = _insideFunction;
		_insideFunction = true;
		_functionCallCount++;

		auto funcResult = func.call(std::forward<Ts>(vs)...);
		
		_insideFunction = insideFunction;
		return funcResult;
	}

	template <typename ... Ts> sol::protected_function_result CallLevelFuncByName(const std::string& name, Ts&& ... vs)
	{
		auto func = _levelFuncs_luaFunctions[name];

		if (!func.valid())
		{
			TENLog("Could not find script function " + name, LogLevel::Warning);
			return sol::protected_function_result();
		}

		auto funcResult = CallLevelFuncBase(func, std::forward<Ts>(vs)...);

		if (!funcResult.valid())
		{
			sol::error err = funcResult;
			ScriptAssertF(false, "Could not execute function {}: {}", name, err.what());
		}

		return funcResult;
	}

	template <typename ... Ts> sol::protected_function_result CallLevelFunc(const sol::protected_function& func, Ts&& ... vs)
	{
		auto funcResult = CallLevelFuncBase(func, std::forward<Ts>(vs)...);

		if (!funcResult.valid())
		{
			sol::error err = funcResult;
			ScriptAssertF(false, "Could not execute function: {}", err.what());
		}

		return funcResult;
	}

	template <typename ... Ts> void PerformCallbacks(CallbackPoint point, Ts&& ... vs)
	{
		auto& callbacks = _callbackSets[(int)point];
		if (callbacks.empty())
			return;

		_lastCallbackPoint = point;

		for (const auto& name : callbacks)
			CallLevelFuncByName(name, std::forward<Ts>(vs)...);

		_lastCallbackPoint = std::nullopt;
	}

	template <typename ... Ts> void PerformLevelFuncCallback(LevelFuncCallbackPoint callback, Ts&& ... vs)
	{
		auto& func = _levelFuncCallbacks[(int)callback];
		if (func.valid())
			CallLevelFunc(func, std::forward<Ts>(vs)...);
	}

	void FreeLevelScripts() override;

	void LogPrint(sol::variadic_args args);
	bool SetLevelFuncsMember(sol::table tab, const std::string& name, sol::object value);

	void AddCallback(CallbackPoint point, const LevelFunc& levelFunc);
	void RemoveCallback(CallbackPoint point, const LevelFunc& levelFunc);
	bool HasCallback(CallbackPoint point, const LevelFunc& levelFunc);
	void HandleEvent(const std::string& name, EventType type, sol::optional<Moveable&> activator);
	void EnableEvent(const std::string& name, EventType type);
	void DisableEvent(const std::string& name, EventType type);
	void AddConsoleInput(const std::string& input);

	void ResetScripts(bool clearGameVars) override;
	void ShortenTENCalls() override;

	sol::object GetLevelFuncsMember(sol::table tab, const std::string& name);

	void ExecuteScriptFile(const std::string& luaFilename) override;
	void ExecuteString(const std::string& command) override;
	void ExecuteFunction(const std::string& name, TEN::Control::Volumes::Activator, const std::string& arguments) override;
	void ExecuteFunction(const std::string& name, short idOne, short idTwo) override;

	unsigned int GetFunctionCallCount() override;

	void GetVariables(std::vector<SavedVar>& vars) override;
	void SetVariables(const std::vector<SavedVar>& vars, bool onlyLevelVars) override;
	void GetGlobalVariables(std::vector<SavedVar>& vars) override;
	void SetGlobalVariables(const std::vector<SavedVar>& vars) override;
	void ResetVariables();

	void SetCallbackStrings(const CallbackStringLists& callbackLists) override;
	void GetCallbackStrings(CallbackStringLists& callbackLists) const override;

	void InitCallbacks() override;
	void OnStart() override;
	void OnLoad() override;
	void OnLoop(float deltaTime, bool postLoop) override;
	void OnSave() override;
	void OnEnd(GameStatus reason) override;
	void OnUseItem(short itemNumber, GAME_OBJECT_ID item) override;
	void OnPickup(short itemNumber, bool postLoop) override;
	void OnVehicleEnter(short itemNumber, bool postLoop) override;
	void OnVehicleLeave(short itemNumber, bool postLoop) override;
	void OnFreeze() override;
};
