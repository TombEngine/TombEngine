#pragma once
#include <unordered_set>

#include "Game/items.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"

enum class CallbackPoint;
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

	std::unordered_set<std::string> _callbacksPreStart	  = {};
	std::unordered_set<std::string> _callbacksPostStart	  = {};
	std::unordered_set<std::string> _callbacksPreLoop	  = {};
	std::unordered_set<std::string> _callbacksPostLoop	  = {};
	std::unordered_set<std::string> _callbacksPreLoad	  = {};
	std::unordered_set<std::string> _callbacksPostLoad	  = {};
	std::unordered_set<std::string> _callbacksPreSave	  = {};
	std::unordered_set<std::string> _callbacksPostSave	  = {};
	std::unordered_set<std::string> _callbacksPreEnd	  = {};
	std::unordered_set<std::string> _callbacksPostEnd	  = {};
	std::unordered_set<std::string> _callbacksPreUseItem  = {};
	std::unordered_set<std::string> _callbacksPostUseItem = {};
	std::unordered_set<std::string> _callbacksPreFreeze	  = {};
	std::unordered_set<std::string> _callbacksPostFreeze  = {};

	sol::protected_function	_onStart   = {};
	sol::protected_function	_onLoop	   = {};
	sol::protected_function	_onLoad	   = {};
	sol::protected_function	_onSave	   = {};
	sol::protected_function	_onEnd	   = {};
	sol::protected_function	_onUseItem = {};
	sol::protected_function	_onFreeze  = {};

	std::unordered_map<CallbackPoint, std::unordered_set<std::string>*> _callbacks;
	std::vector<std::variant<std::string, unsigned int>> _savedVarPath;

	bool _shortenedCalls = false;

	std::string _consoleInput = {};

	void PerformConsoleInput();

	std::string GetRequestedPath() const;

	void ResetLevelTables();
	void ResetGameTables();
	LuaHandler _handler;

public:	
	LogicHandler(sol::state* lua, sol::table& parent);

	template <typename ... Ts> sol::protected_function_result CallLevelFuncBase(const sol::protected_function& func, Ts ... vs)
	{
		auto funcResult = func.call(vs...);
		return funcResult;
	}

	template <typename ... Ts> sol::protected_function_result CallLevelFuncByName(const std::string& name, Ts ... vs)
	{
		auto func = _levelFuncs_luaFunctions[name];
		auto funcResult = CallLevelFuncBase(func, vs...);

		if (!funcResult.valid())
		{
			sol::error err = funcResult;
			ScriptAssertF(false, "Could not execute function {}: {}", name, err.what());
		}

		return funcResult;
	}

	template <typename ... Ts> sol::protected_function_result CallLevelFunc(const sol::protected_function& func, Ts ... vs)
	{
		auto funcResult = CallLevelFuncBase(func, vs...);

		if (!funcResult.valid())
		{
			sol::error err = funcResult;
			ScriptAssertF(false, "Could not execute function: {}", err.what());
		}

		return funcResult;
	}

	void FreeLevelScripts() override;

	void LogPrint(sol::variadic_args args);
	bool SetLevelFuncsMember(sol::table tab, const std::string& name, sol::object value);

	void AddCallback(CallbackPoint point, const LevelFunc& levelFunc);
	void RemoveCallback(CallbackPoint point, const LevelFunc& levelFunc);
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

	void GetVariables(std::vector<SavedVar>& vars) override;
	void SetVariables(const std::vector<SavedVar>& vars, bool onlyLevelVars) override;
	void ResetVariables();

	void SetCallbackStrings(const std::vector<std::string>& preStart,
							const std::vector<std::string>& postStart,
							const std::vector<std::string>& preEnd,
							const std::vector<std::string>& postEnd,
							const std::vector<std::string>& preSave,
							const std::vector<std::string>& postSave,
							const std::vector<std::string>& preLoad,
							const std::vector<std::string>& postLoad,
							const std::vector<std::string>& preLoop,
							const std::vector<std::string>& postLoop,
							const std::vector<std::string>& preUseItem,
							const std::vector<std::string>& postUseItem,
							const std::vector<std::string>& preBreak,
							const std::vector<std::string>& postBreak) override;

	void GetCallbackStrings(std::vector<std::string>& preStart,
							std::vector<std::string>& postStart,
							std::vector<std::string>& preEnd,
							std::vector<std::string>& postEnd,
							std::vector<std::string>& preSave,
							std::vector<std::string>& postSave,
							std::vector<std::string>& preLoad,
							std::vector<std::string>& postLoad,
							std::vector<std::string>& preLoop,
							std::vector<std::string>& postLoop,
							std::vector<std::string>& preUseItem,
							std::vector<std::string>& postUseItem,
							std::vector<std::string>& preBreak,
							std::vector<std::string>& postBreak) const override;

	void InitCallbacks() override;
	void OnStart() override;
	void OnLoad() override;
	void OnLoop(float deltaTime, bool postLoop) override;
	void OnSave() override;
	void OnEnd(GameStatus reason) override;
	void OnUseItem(GAME_OBJECT_ID item) override;
	void OnFreeze() override;
};
