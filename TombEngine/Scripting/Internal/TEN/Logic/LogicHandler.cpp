#include "framework.h"
#include "LogicHandler.h"

#include "Game/control/volume.h"
#include "Game/effects/Electricity.h"
#include "Game/Lara/lara.h"
#include "Game/savegame.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Logic/LevelFunc.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Time/Time.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"

using namespace TEN::Effects::Electricity;
using namespace TEN::Scripting::Types;

/***
Saving data, triggering functions, and callbacks for level-specific scripts.
@tentable Logic 
@pragma nostrip
*/

enum class CallbackPoint
{
	PreStart,
	PostStart,
	PreLoad,
	PostLoad,
	PreLoop,
	PostLoop,
	PreSave,
	PostSave,
	PreEnd,
	PostEnd,
	PreUseItem,
	PostUseItem,
	PreFreeze,
	PostFreeze
};

enum class LevelEndReason
{
	LevelComplete,
	LoadGame,
	ExitToTitle,
	Death,
	Other
};

static const auto CALLBACK_POINTS = std::unordered_map<std::string, CallbackPoint>
{
	{ ScriptReserved_PreStart, CallbackPoint::PreStart },
	{ ScriptReserved_PostStart, CallbackPoint::PostStart },
	{ ScriptReserved_PreLoad, CallbackPoint::PreLoad },
	{ ScriptReserved_PostLoad, CallbackPoint::PostLoad },
	{ ScriptReserved_PreLoop, CallbackPoint::PreLoop },
	{ ScriptReserved_PostLoop, CallbackPoint::PostLoop },
	{ ScriptReserved_PreSave, CallbackPoint::PreSave },
	{ ScriptReserved_PostSave, CallbackPoint::PostSave },
	{ ScriptReserved_PreEnd, CallbackPoint::PreEnd },
	{ ScriptReserved_PostEnd, CallbackPoint::PostEnd },
	{ ScriptReserved_PreUseItem, CallbackPoint::PreUseItem },
	{ ScriptReserved_PostUseItem, CallbackPoint::PostUseItem },
	{ ScriptReserved_PreFreeze, CallbackPoint::PreFreeze },
	{ ScriptReserved_PostFreeze, CallbackPoint::PostFreeze },

	// COMPATIBILITY
	{ "POSTSTART", CallbackPoint::PostStart },
	{ "PRELOAD", CallbackPoint::PreLoad },
	{ "POSTLOAD", CallbackPoint::PostLoad },
	{ "PRELOOP", CallbackPoint::PreLoop },
	{ "PRECONTROLPHASE", CallbackPoint::PreLoop },
	{ "POSTLOOP", CallbackPoint::PostLoop },
	{ "POSTCONTROLPHASE", CallbackPoint::PostLoop },
	{ "PRESAVE", CallbackPoint::PreSave },
	{ "POSTSAVE", CallbackPoint::PostSave },
	{ "PREEND", CallbackPoint::PreEnd },
	{ "POSTEND", CallbackPoint::PostEnd },
	{ "PREUSEITEM", CallbackPoint::PreUseItem },
	{ "POSTUSEITEM", CallbackPoint::PostUseItem },
	{ "PREFREEZE", CallbackPoint::PreFreeze },
	{ "POSTFREEZE", CallbackPoint::PostFreeze }
};

static const auto EVENT_TYPES = std::unordered_map<std::string, EventType>
{
	{ ScriptReserved_EventOnEnter, EventType::Enter },
	{ ScriptReserved_EventOnInside, EventType::Inside },
	{ ScriptReserved_EventOnLeave, EventType::Leave },
	{ ScriptReserved_EventOnLoop, EventType::Loop },
	{ ScriptReserved_EventOnLoad, EventType::Load },
	{ ScriptReserved_EventOnSave, EventType::Save },
	{ ScriptReserved_EventOnStart, EventType::Start },
	{ ScriptReserved_EventOnEnd, EventType::End },
	{ ScriptReserved_EventOnUseItem, EventType::UseItem },
	{ ScriptReserved_EventOnFreeze, EventType::Freeze },

	// COMPATIBILITY
	{ "USEITEM", EventType::UseItem }
};

static const auto LEVEL_END_REASONS = std::unordered_map<std::string, LevelEndReason>
{
	{ ScriptReserved_EndReasonLevelComplete, LevelEndReason::LevelComplete },
	{ ScriptReserved_EndReasonLoadGame, LevelEndReason::LoadGame },
	{ ScriptReserved_EndReasonExitToTitle, LevelEndReason::ExitToTitle },
	{ ScriptReserved_EndReasonDeath, LevelEndReason::Death },
	{ ScriptReserved_EndReasonOther, LevelEndReason::Other },

	// COMPATIBILITY
	{ "LEVELCOMPLETE", LevelEndReason::LevelComplete },
	{ "LOADGAME", LevelEndReason::LoadGame },
	{ "EXITTOTITLE", LevelEndReason::ExitToTitle }
};

static constexpr char const* strKey = "__internal_name";

void SetVariable(sol::table tab, sol::object key, sol::object value)
{
	auto PutVar = [](sol::table tab, sol::object key, sol::object value)
	{
		switch (key.get_type())
		{
		case sol::type::number:
		case sol::type::string:
			tab.raw_set(key, value);
			break;

		default:
			ScriptAssert(false, "Unsupported key type used for special table. Valid types are string and number.", ErrorMode::Terminate);
			break;
		}
	};

	auto UnsupportedValue = [](sol::table tab, sol::object key)
	{
		key.push();

		size_t stringLength = 0;
		auto string = std::string(luaL_tolstring(tab.lua_state(), -1, &stringLength));

		if (!string.empty())
		{
			ScriptAssert(false, "Variable " + string + " has an unsupported type.", ErrorMode::Terminate);
			lua_pop(tab.lua_state(), 1);
		}
		else
		{
			ScriptAssert(false, "Variable has an unsupported type.", ErrorMode::Terminate);
		}

		key.pop();
	};

	switch (value.get_type())
	{
	case sol::type::lua_nil:
	case sol::type::boolean:
	case sol::type::number:
	case sol::type::string:
	case sol::type::table:
		PutVar(tab, key, value);
		break;

	case sol::type::userdata:
	{
		if (value.is<Vec2>() ||
			value.is<Vec3>() ||
			value.is<Rotation>() ||
			value.is<Time>() ||
			value.is<ScriptColor>())
		{
			PutVar(tab, key, value);
		}
		else
		{
			UnsupportedValue(tab, key);
		}
	}
		break;

	default:
		UnsupportedValue(tab, key);
	}
}

sol::object GetVariable(sol::table tab, sol::object key)
{
	return tab.raw_get<sol::object>(key);
}

LogicHandler::LogicHandler(sol::state* lua, sol::table& parent) : _handler{ lua }
{
	_handler.GetState()->set_function("print", &LogicHandler::LogPrint, this);

	auto tableLogic = sol::table(_handler.GetState()->lua_state(), sol::create);

	parent.set(ScriptReserved_Logic, tableLogic);

	tableLogic.set_function(ScriptReserved_AddCallback, &LogicHandler::AddCallback, this);
	tableLogic.set_function(ScriptReserved_RemoveCallback, &LogicHandler::RemoveCallback, this);
	tableLogic.set_function(ScriptReserved_HandleEvent, &LogicHandler::HandleEvent, this);
	tableLogic.set_function(ScriptReserved_EnableEvent, &LogicHandler::EnableEvent, this);
	tableLogic.set_function(ScriptReserved_DisableEvent, &LogicHandler::DisableEvent, this);

	_handler.MakeReadOnlyTable(tableLogic, ScriptReserved_EndReason, LEVEL_END_REASONS);
	_handler.MakeReadOnlyTable(tableLogic, ScriptReserved_CallbackPoint, CALLBACK_POINTS);
	_handler.MakeReadOnlyTable(tableLogic, ScriptReserved_EventType, EVENT_TYPES);

	_callbacks.insert(std::make_pair(CallbackPoint::PreStart, &_callbacksPreStart));
	_callbacks.insert(std::make_pair(CallbackPoint::PostStart, &_callbacksPostStart));
	_callbacks.insert(std::make_pair(CallbackPoint::PreLoad, &_callbacksPreLoad));
	_callbacks.insert(std::make_pair(CallbackPoint::PostLoad, &_callbacksPostLoad));
	_callbacks.insert(std::make_pair(CallbackPoint::PreLoop, &_callbacksPreLoop));
	_callbacks.insert(std::make_pair(CallbackPoint::PostLoop, &_callbacksPostLoop));
	_callbacks.insert(std::make_pair(CallbackPoint::PreSave, &_callbacksPreSave));
	_callbacks.insert(std::make_pair(CallbackPoint::PostSave, &_callbacksPostSave));
	_callbacks.insert(std::make_pair(CallbackPoint::PreEnd, &_callbacksPreEnd));
	_callbacks.insert(std::make_pair(CallbackPoint::PostEnd, &_callbacksPostEnd));
	_callbacks.insert(std::make_pair(CallbackPoint::PreUseItem, &_callbacksPreUseItem));
	_callbacks.insert(std::make_pair(CallbackPoint::PostUseItem, &_callbacksPostUseItem));
	_callbacks.insert(std::make_pair(CallbackPoint::PreFreeze, &_callbacksPreFreeze));
	_callbacks.insert(std::make_pair(CallbackPoint::PostFreeze, &_callbacksPostFreeze));

	LevelFunc::Register(tableLogic);

	ResetScripts(true);
}

void LogicHandler::ResetGameTables() 
{
	auto state = _handler.GetState();
	MakeSpecialTable(state, ScriptReserved_GameVars, &GetVariable, &SetVariable);

	(*state)[ScriptReserved_GameVars][ScriptReserved_Engine] = sol::table(*state, sol::create);
}

/*** Register a function as a callback.
@advancedDesc
This is intended for module/library developers who want their modules to do
stuff during level start/load/end/save/control phase, but don't want the level
designer to add calls to `OnStart`, `OnLoad`, etc. in their level script.

Possible values for `point`:
	-- These take functions which accept no arguments
	PRE_START -- will be called immediately before OnStart
	POST_START -- will be called immediately after OnStart

	PRE_SAVE -- will be called immediately before OnSave
	POST_SAVE -- will be called immediately after OnSave

	PRE_LOAD -- will be called immediately before OnLoad
	POST_LOAD -- will be called immediately after OnLoad

	PRE_FREEZE -- will be called before entering freeze mode
	POST_FREEZE -- will be called immediately after exiting freeze mode

	-- These take a LevelEndReason arg, like OnEnd
	PRE_END -- will be called immediately before OnEnd
	POST_END -- will be called immediately after OnEnd

	-- These take functions which accepts a deltaTime argument
	PRE_LOOP -- will be called in the beginning of game loop
	POST_LOOP -- will be called at the end of game loop

	-- These take functions which accepts an objectNumber argument, like OnUseItem
	PRE_USE_ITEM -- will be called immediately before OnUseItem
	POST_USE_ITEM -- will be called immediately after OnUseItem

The order in which two functions with the same CallbackPoint are called is undefined.
i.e. if you register `MyFunc` and `MyFunc2` with `PRELOOP`, both will be called in the beginning of game loop, but there is no guarantee that `MyFunc` will be called before `MyFunc2`, or vice-versa.

Any returned value will be discarded.

@function AddCallback
@tparam CallbackPoint point When should the callback be called?
@tparam LevelFunc func The function to be called (must be in the `LevelFuncs` hierarchy). Will receive, as an argument, the time in seconds since the last frame.
@usage
	LevelFuncs.MyFunc = function(dt) print(dt) end
	TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRELOOP, LevelFuncs.MyFunc)
*/
void LogicHandler::AddCallback(CallbackPoint point, const LevelFunc& levelFunc)
{
	auto it = _callbacks.find(point);
	if (it == _callbacks.end()) 
	{
		TENLog("Error: callback point not found. Attempted to access missing value.", LogLevel::Error, LogConfig::All, false);
		return;
	}
	
	if (it->second->find(levelFunc.m_funcName) != it->second->end())
	{
		TENLog("Warning: function " + levelFunc.m_funcName + " already registered in callbacks list.", LogLevel::Warning, LogConfig::All, true);
	}
	else
	{
		it->second->insert(levelFunc.m_funcName);
	}
}

/*** Deregister a function as a callback.
Will have no effect if the function was not registered as a callback

@function RemoveCallback
@tparam CallbackPoint point The callback point the function was registered with. See @{AddCallback}
@tparam LevelFunc func The function to remove; must be in the LevelFuncs hierarchy.
@usage
	TEN.Logic.RemoveCallback(TEN.Logic.CallbackPoint.PRELOOP, LevelFuncs.MyFunc)
*/
void LogicHandler::RemoveCallback(CallbackPoint point, const LevelFunc& levelFunc)
{
	auto it = _callbacks.find(point);
	if (it == _callbacks.end())
	{
		TENLog("Error: callback point not found. Attempted to access missing value.", LogLevel::Error, LogConfig::All, false);
		return;
	}

	it->second->erase(levelFunc.m_funcName);
}

/*** Attempt to find an event set and execute a particular event from it.
@advancedDesc

Possible event type values:
	ENTER
	INSIDE
	LEAVE
	LOAD
	SAVE
	START
	END
	LOOP
	USE_ITEM
	MENU

@function HandleEvent
@tparam string name Name of the event set to find.
@tparam EventType type Event to execute.
@tparam[opt=Lara] Objects.Moveable activator Optional activator.
*/
void LogicHandler::HandleEvent(const std::string& name, EventType type, sol::optional<Moveable&> activator)
{
	TEN::Control::Volumes::HandleEvent(name, type, activator.has_value() ? (Activator)(short)activator->GetIndex() : (Activator)(short)LaraItem->Index);
}

/*** Attempt to find an event set and enable specified event in it.

@function EnableEvent
@tparam string name Name of the event set to find.
@tparam EventType type Event to enable.
*/
void LogicHandler::EnableEvent(const std::string& name, EventType type)
{
	TEN::Control::Volumes::SetEventState(name, type, true);
}

/*** Attempt to find an event set and disable specified event in it.

@function DisableEvent
@tparam string name Name of the event set to find.
@tparam EventType type Event to disable.
*/
void LogicHandler::DisableEvent(const std::string& name, EventType type)
{
	TEN::Control::Volumes::SetEventState(name, type, false);
}

void LogicHandler::ResetLevelTables()
{
	auto state = _handler.GetState();
	MakeSpecialTable(state, ScriptReserved_LevelVars, &GetVariable, &SetVariable);

	(*state)[ScriptReserved_LevelVars][ScriptReserved_Engine] = sol::table{ *state, sol::create };
}

sol::object LogicHandler::GetLevelFuncsMember(sol::table tab, const std::string& name)
{
	auto partName = tab.raw_get<std::string>(strKey);
	auto& map = _levelFuncs_tablesOfNames[partName];

	auto fullNameIt = map.find(name);
	if (fullNameIt != std::cend(map))
	{
		std::string_view key = fullNameIt->second;
		if (_levelFuncs_levelFuncObjects[key].valid())
			return _levelFuncs_levelFuncObjects[key];
	}

	return sol::nil;
}

bool LogicHandler::SetLevelFuncsMember(sol::table tab, const std::string& name, sol::object value)
{
	if (sol::type::lua_nil == value.get_type())
	{
		auto error = std::string("Tried to set " + std::string{ScriptReserved_LevelFuncs} + " member ");
		error += name + " to nil; this not permitted at this time.";
		return ScriptAssert(false, error);
	}
	else if (sol::type::function == value.get_type())
	{
		// Add name to table of names.
		auto partName = tab.raw_get<std::string>(strKey);
		auto fullName = partName + "." + name;
		auto& parentNameTab = _levelFuncs_tablesOfNames[partName];
		parentNameTab.insert_or_assign(name, fullName);

		// Create LevelFunc userdata and add that too.
		LevelFunc levelFuncObject;
		levelFuncObject.m_funcName = fullName;
		levelFuncObject.m_handler = this;
		_levelFuncs_levelFuncObjects[fullName] = levelFuncObject;

		// Add function itself.
		_levelFuncs_luaFunctions[fullName] = value;
	}
	else if (sol::type::table == value.get_type())
	{
		// Create and add new name map.
		auto newNameMap = std::unordered_map<std::string, std::string>{};
		auto fullName = tab.raw_get<std::string>(strKey) + "." + name;
		_levelFuncs_tablesOfNames.insert_or_assign(fullName, newNameMap);

		// Create new table to put in the LevelFuncs hierarchy.
		auto newLevelFuncsTab = MakeSpecialTable(_handler.GetState(), name, &LogicHandler::GetLevelFuncsMember, &LogicHandler::SetLevelFuncsMember, this);
		newLevelFuncsTab.raw_set(strKey, fullName);
		tab.raw_set(name, newLevelFuncsTab);

		// "Populate" new table. This will trigger the __newindex metafunction and will
		// thus call this function recursively, handling all subtables and functions.
		for (auto& [key, val] : value.as<sol::table>())
			newLevelFuncsTab[key] = val;
	}
	else
	{
		auto error = std::string("Failed to add ");
		error += name + " to " + ScriptReserved_LevelFuncs + " or one of its tables; it must be a function or a table of functions.";
		return ScriptAssert(false, error);
	}

	return true;
}

void LogicHandler::LogPrint(sol::variadic_args args)
{
	auto str = std::string();
	for (const sol::object& o : args)
	{
		auto strPart = (*_handler.GetState())["tostring"](o).get<std::string>();
		str += strPart;
		str += "\t";
	}

	TENLog(str, LogLevel::Info, LogConfig::All, true);
}

void LogicHandler::ResetScripts(bool clearGameVars)
{
	FreeLevelScripts();

	for (auto& [first, second] : _callbacks)
		second->clear();

	auto currentPackage = _handler.GetState()->get<sol::table>("package");
	auto currentLoaded = currentPackage.get<sol::table>("loaded");

	for (auto& [first, second] : currentLoaded)
		currentLoaded[first] = sol::nil;

	if (clearGameVars)
		ResetGameTables();

	_handler.ResetGlobals();

	_shortenedCalls = false;

	_handler.GetState()->collect_garbage();
}

void LogicHandler::FreeLevelScripts()
{
	_levelFuncs = MakeSpecialTable(_handler.GetState(), ScriptReserved_LevelFuncs, &LogicHandler::GetLevelFuncsMember, &LogicHandler::SetLevelFuncsMember, this);
	_levelFuncs.raw_set(strKey, ScriptReserved_LevelFuncs);

	_levelFuncs[ScriptReserved_Engine] = sol::table(*_handler.GetState(), sol::create);

	_levelFuncs_tablesOfNames.clear();
	_levelFuncs_luaFunctions.clear();
	_levelFuncs_levelFuncObjects = sol::table(*_handler.GetState(), sol::create);

	_levelFuncs_tablesOfNames.emplace(std::make_pair(ScriptReserved_LevelFuncs, std::unordered_map<std::string, std::string>{}));

	ResetLevelTables();
	_onStart = sol::nil;
	_onLoad = sol::nil;
	_onLoop = sol::nil;
	_onSave = sol::nil;
	_onEnd = sol::nil;
	_onUseItem = sol::nil;
	_onFreeze = sol::nil;
	_handler.GetState()->collect_garbage();
}

// Used when loading.
void LogicHandler::SetVariables(const std::vector<SavedVar>& vars, bool onlyLevelVars)
{
	if (!onlyLevelVars)
		ResetGameTables();

	ResetLevelTables();

	auto solTables = std::unordered_map<unsigned int, sol::table>{};

	for(int i = 0; i < vars.size(); ++i)
	{
		if (std::holds_alternative<IndexTable>(vars[i]))
		{
			solTables.try_emplace(i, *_handler.GetState(), sol::create);
			auto indexTab = std::get<IndexTable>(vars[i]);
			for (auto& [first, second] : indexTab)
			{
				// if we're wanting to reference a table, make sure that table exists
				// create it if need be
				if (std::holds_alternative<IndexTable>(vars[second]))
				{
					solTables.try_emplace(second, *_handler.GetState(), sol::create);
					solTables[i][vars[first]] = solTables[second];
				}
				else if (std::holds_alternative<double>(vars[second]))
				{
					double theNum = std::get<double>(vars[second]);
					// If this is representable as an integer use an integer.
					// This is to ensure something saved as 1 is not loaded as 1.0
					// which would be confusing for the user.
					// todo: should we throw a warning if the user tries to save or load a value
					// outside of these bounds? - squidshire 30/04/2022
					if (std::trunc(theNum) == theNum && theNum <= INT64_MAX && theNum >= INT64_MIN)
					{
						solTables[i][vars[first]] = (int64_t)theNum;
					}
					else
					{
						solTables[i][vars[first]] = vars[second];
					}
				}
				else if (vars[second].index() == (int)SavedVarType::Vec2)
				{
					auto vec2 = Vec2(std::get<(int)SavedVarType::Vec2>(vars[second]));
					solTables[i][vars[first]] = vec2;
				}
				else if (vars[second].index() == int(SavedVarType::Vec3))
				{
					auto vec3 = Vec3(std::get<int(SavedVarType::Vec3)>(vars[second]));
					solTables[i][vars[first]] = vec3;
				}
				else if (vars[second].index() == int(SavedVarType::Rotation))
				{
					auto vec3 = Rotation(std::get<int(SavedVarType::Rotation)>(vars[second]));
					solTables[i][vars[first]] = vec3;
				}
				else if (vars[second].index() == int(SavedVarType::Time))
				{
					auto time = Time(std::get<int(SavedVarType::Time)>(vars[second]));
					solTables[i][vars[first]] = time;
				}
				else if (vars[second].index() == int(SavedVarType::Color))
				{
					auto color = D3DCOLOR(std::get<int(SavedVarType::Color)>(vars[second]));
					solTables[i][vars[first]] = ScriptColor{color};
				}
				else if (std::holds_alternative<FuncName>(vars[second]))
				{
					LevelFunc levelFunc;
					levelFunc.m_funcName = std::get<FuncName>(vars[second]).name;
					levelFunc.m_handler = this;
					solTables[i][vars[first]] = levelFunc;
				}
				else
				{
					solTables[i][vars[first]] = vars[second];
				}
			}
		}
	}
	
	auto rootTable = solTables[0];

	sol::table levelVars = rootTable[ScriptReserved_LevelVars];
	for (auto& [first, second] : levelVars)
		(*_handler.GetState())[ScriptReserved_LevelVars][first] = second;

	if (onlyLevelVars)
		return;

	sol::table gameVars = rootTable[ScriptReserved_GameVars];
	for (auto& [first, second] : gameVars)
		(*_handler.GetState())[ScriptReserved_GameVars][first] = second;
}

template<SavedVarType TypeEnum, typename TypeTo, typename TypeFrom, typename MapType>
int Handle(TypeFrom& var, MapType& varsMap, size_t& numVars, std::vector<SavedVar>& vars)
{
	auto [first, second] = varsMap.insert(std::make_pair(&var, (int)numVars));

	if (second)
	{
		SavedVar savedVar;
		TypeTo varTo = (TypeTo)var;
		savedVar.emplace<(int)TypeEnum>(varTo);
		vars.push_back(savedVar);
		++numVars;
	}

	return first->second;
}

void LogicHandler::AddConsoleInput(const std::string& input)
{
	_consoleInput = input;
}

void LogicHandler::PerformConsoleInput()
{
	if (!_consoleInput.empty())
	{
		try
		{
			ExecuteString(_consoleInput);
		}
		catch (const std::exception& ex)
		{
			TENLog("Error executing " + _consoleInput + ": " + ex.what(), LogLevel::Error);
		}

		_consoleInput.clear();
	}
}

std::string LogicHandler::GetRequestedPath() const
{
	auto path = std::string();
	for (unsigned int i = 0; i < _savedVarPath.size(); ++i)
	{
		auto key = _savedVarPath[i];
		if (std::holds_alternative<unsigned int>(key))
		{
			path += "[" + std::to_string(std::get<unsigned int>(key)) + "]";
		}
		else if (std::holds_alternative<std::string>(key))
		{
			auto part = std::get<std::string>(key);
			if (i > 0)
			{
				path += "." + part;
			}
			else
			{
				path += part;
			}
		}
	}

	return path;
}

// Used when saving.
void LogicHandler::GetVariables(std::vector<SavedVar>& vars)
{
	auto tab = sol::table(*_handler.GetState(), sol::create);
	tab[ScriptReserved_LevelVars] = (*_handler.GetState())[ScriptReserved_LevelVars];
	tab[ScriptReserved_GameVars] = (*_handler.GetState())[ScriptReserved_GameVars];

	auto varsMap = std::unordered_map<void const*, unsigned int>{};
	auto numMap = std::unordered_map<double, unsigned int>{};
	auto boolMap = std::unordered_map<bool, unsigned int>{};

	size_t varCount = 0;

	// The following functions will all try to put their values in a map. If it succeeds
	// then the value was not already in the map, so we can put it into the var vector.
	// If it fails, the value is in the map, and thus will also be in the var vector.
	// We then return the value's position in the var vector.

	// The purpose of this is to only store each value once, and to fill our tables with
	// indices to the values rather than copies of the values.

	auto handleNum = [&](auto num, auto map)
	{
		auto [first, second] = map.insert(std::make_pair(num, (int)varCount));

		if (second)
		{
			vars.push_back(num);
			++varCount;
		}

		return first->second;
	};

	auto handleStr = [&](const sol::object& obj)
	{
		auto str = obj.as<sol::string_view>();
		auto [first, second] = varsMap.insert(std::make_pair(str.data(), (int)varCount));

		if (second)
		{
			vars.push_back(std::string{ str.data() });
			++varCount;
		}

		return first->second;
	};

	auto handleFuncName = [&](const LevelFunc& fnh)
	{
		auto [first, second] = varsMap.insert(std::make_pair(&fnh, (int)varCount));

		if (second)
		{
			vars.push_back(FuncName{ std::string{ fnh.m_funcName } });
			++varCount;
		}

		return first->second;
	};

	std::function<unsigned int(const sol::table&)> populate = [&](const sol::table& obj)
	{
		auto [first, second] = varsMap.insert(std::make_pair(obj.pointer(), (int)varCount));

		if(second)
		{
			++varCount;
			auto id = first->second;

			vars.push_back(IndexTable{});

			for (auto& [first, second] : obj)
			{
				bool validKey = true;
				unsigned int keyIndex = 0;
				std::variant<std::string, unsigned int> key{unsigned int(0)};

				// Strings and numbers can be keys AND values.
				switch (first.get_type())
				{
				case sol::type::string:
				{
					keyIndex = handleStr(first);
					key = std::string{ first.as<sol::string_view>().data() };
					_savedVarPath.push_back(key);
				}
					break;

				case sol::type::number:
				{
					if (double data = first.as<double>(); std::floor(data) != data)
					{
						ScriptAssert(false, "Tried using a non-integer number " + std::to_string(data) + " as a key in table " + GetRequestedPath());
						validKey = false;
					}
					else
					{
						keyIndex = handleNum(data, numMap);
						key = static_cast<unsigned int>(data);
						_savedVarPath.push_back(key);
					}
				}
					break;

				default:
					validKey = false;
					ScriptAssert(false, "Tried using an unsupported type as a key in table " + GetRequestedPath());
				}

				if (!validKey)
					continue;

				auto putInVars = [&vars, id, keyIndex](unsigned int valIndex)
				{
					std::get<IndexTable>(vars[id]).push_back(std::make_pair(keyIndex, valIndex));
				};

				switch (second.get_type())
				{
				case sol::type::table:
					putInVars(populate(second.as<sol::table>()));
					break;

				case sol::type::string:
					putInVars(handleStr(second));
					break;

				case sol::type::number:
					putInVars(handleNum(second.as<double>(), numMap));
					break;

				case sol::type::boolean:
					putInVars(handleNum(second.as<bool>(), boolMap));
					break;

				case sol::type::userdata:
				{
					if (second.is<Vec2>())
					{
						putInVars(Handle<SavedVarType::Vec2, Vector2>(second.as<Vec2>(), varsMap, varCount, vars));
					}
					else if (second.is<Vec3>())
					{
						putInVars(Handle<SavedVarType::Vec3, Vector3>(second.as<Vec3>(), varsMap, varCount, vars));
					}
					else if (second.is<Rotation>())
					{
						putInVars(Handle<SavedVarType::Rotation, Vector3>(second.as<Rotation>(), varsMap, varCount, vars));
					}
					else if (second.is<Time>())
					{
						putInVars(Handle<SavedVarType::Time, int>(second.as<Time>(), varsMap, varCount, vars));
					}
					else if (second.is<ScriptColor>())
					{
						putInVars(Handle<SavedVarType::Color, D3DCOLOR>(second.as<ScriptColor>(), varsMap, varCount, vars));
					}
					else if (second.is<LevelFunc>())
					{
						putInVars(handleFuncName(second.as<LevelFunc>()));
					}
					else
					{
						ScriptAssert(false, "Tried saving an unsupported userdata as a value; variable is " + GetRequestedPath());
					}
				}
					break;

				default:
					ScriptAssert(false, "Tried saving an unsupported type as a value; variable is " + GetRequestedPath());
				}

				_savedVarPath.pop_back();
			}
		}

		return first->second;
	};

	populate(tab);
}

void LogicHandler::GetCallbackStrings(	
	std::vector<std::string>& preStart,
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
	std::vector<std::string>& postBreak) const
{
	auto populateWith = [](std::vector<std::string>& dest, const std::unordered_set<std::string>& src)
	{
		for (const auto& string : src)
			dest.push_back(string);
	};

	populateWith(preStart, _callbacksPreStart);
	populateWith(postStart, _callbacksPostStart);

	populateWith(preEnd, _callbacksPreEnd);
	populateWith(postEnd, _callbacksPostEnd);

	populateWith(preSave, _callbacksPreSave);
	populateWith(postSave, _callbacksPostSave);

	populateWith(preLoad, _callbacksPreLoad);
	populateWith(postLoad, _callbacksPostLoad);

	populateWith(preLoop, _callbacksPreLoop);
	populateWith(postLoop, _callbacksPostLoop);

	populateWith(preUseItem, _callbacksPreUseItem);
	populateWith(postUseItem, _callbacksPostUseItem);

	populateWith(preBreak, _callbacksPreFreeze);
	populateWith(postBreak, _callbacksPostFreeze);
}

void LogicHandler::SetCallbackStrings(	
	const std::vector<std::string>& preStart,
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
	const std::vector<std::string>& postBreak)
{
	auto populateWith = [](std::unordered_set<std::string>& dest, const std::vector<std::string>& src)
	{
		for (const auto& string : src)
			dest.insert(string);
	};

	populateWith(_callbacksPreStart, preStart);
	populateWith(_callbacksPostStart, postStart);

	populateWith(_callbacksPreEnd, preEnd);
	populateWith(_callbacksPostEnd, postEnd);

	populateWith(_callbacksPreSave, preSave);
	populateWith(_callbacksPostSave, postSave);

	populateWith(_callbacksPreLoad, preLoad);
	populateWith(_callbacksPostLoad, postLoad);

	populateWith(_callbacksPreLoop, preLoop);
	populateWith(_callbacksPostLoop, postLoop);

	populateWith(_callbacksPreUseItem, preUseItem);
	populateWith(_callbacksPostUseItem, postUseItem);

	populateWith(_callbacksPreFreeze, preBreak);
	populateWith(_callbacksPostFreeze, postBreak);
}

template <typename R, char const * S, typename mapType>
std::unique_ptr<R> GetByName(const std::string& type, const std::string& name, const mapType& map)
{
	ScriptAssert(map.find(name) != map.end(), std::string{ type + " name not found: " + name }, ErrorMode::Terminate);
	return std::make_unique<R>(map.at(name), false);
}

/*** Special objects
@section specialobjects
*/

/*** An @{Objects.Moveable} entry representing Lara herself.
@table Lara
*/
void LogicHandler::ResetVariables()
{
	(*_handler.GetState())["Lara"] = nullptr;
}

void LogicHandler::ShortenTENCalls()
{
	auto str = R"(local ShortenInner 

	ShortenInner = function(tab)
		for k, v in pairs(tab) do
			if _G[k] then
				print("WARNING! Key " .. k .. " already exists in global environment!")
			else
				_G[k] = v
				if "table" == type(v) then
					if nil == v.__type then
						ShortenInner(v)
					end
				end
			end
		end
	end
	ShortenInner(TEN))";

	ExecuteString(str);

	_shortenedCalls = true;
}

void LogicHandler::ExecuteScriptFile(const std::string& luaFilename)
{
	if (!_shortenedCalls)
		ShortenTENCalls();

	_handler.ExecuteScript(luaFilename);
}

void LogicHandler::ExecuteString(const std::string& command)
{
	_handler.ExecuteString(command);
}

// These wind up calling CallLevelFunc, which is where all error checking is.
void LogicHandler::ExecuteFunction(const std::string& name, short idOne, short idTwo) 
{
	auto func = _levelFuncs_luaFunctions[name];

	func(std::make_unique<Moveable>(idOne), std::make_unique<Moveable>(idTwo));
}

void LogicHandler::ExecuteFunction(const std::string& name, TEN::Control::Volumes::Activator activator, const std::string& arguments)
{
	sol::protected_function func = (*_handler.GetState())[ScriptReserved_LevelFuncs][name.c_str()];
	if (std::holds_alternative<int>(activator))
	{
		func(std::make_unique<Moveable>(std::get<int>(activator), true), arguments);
	}
	else
	{
		func(nullptr, arguments);
	}
}

void LogicHandler::OnStart()
{
	for (const auto& name : _callbacksPreStart)
		CallLevelFuncByName(name);

	if (_onStart.valid())
		CallLevelFunc(_onStart);

	for (const auto& name : _callbacksPostStart)
		CallLevelFuncByName(name);
}

void LogicHandler::OnLoad()
{
	for (const auto& name : _callbacksPreLoad)
		CallLevelFuncByName(name);

	if (_onLoad.valid())
		CallLevelFunc(_onLoad);

	for (const auto& name : _callbacksPostLoad)
		CallLevelFuncByName(name);
}

void LogicHandler::OnLoop(float deltaTime, bool postLoop)
{
	if (!postLoop)
	{
		for (const auto& name : _callbacksPreLoop)
			CallLevelFuncByName(name, deltaTime);

		PerformConsoleInput();

		lua_gc(_handler.GetState()->lua_state(), LUA_GCCOLLECT, 0);
		if (_onLoop.valid())
			CallLevelFunc(_onLoop, deltaTime);
	}
	else
	{
		for (const auto& name : _callbacksPostLoop)
			CallLevelFuncByName(name, deltaTime);
	}
}

void LogicHandler::OnSave()
{
	for (const auto& name : _callbacksPreSave)
		CallLevelFuncByName(name);

	if (_onSave.valid())
		CallLevelFunc(_onSave);

	for (const auto& name : _callbacksPostSave)
		CallLevelFuncByName(name);
}

void LogicHandler::OnEnd(GameStatus reason)
{
	auto endReason = LevelEndReason::Other;
	switch (reason)
	{
	case GameStatus::LaraDead:
		endReason = LevelEndReason::Death;
		break;

	case GameStatus::LevelComplete:
		endReason = LevelEndReason::LevelComplete;
		break;

	case GameStatus::ExitToTitle:
		endReason = LevelEndReason::ExitToTitle;
		break;

	case GameStatus::LoadGame:
		endReason = LevelEndReason::LoadGame;
		break;
	}

	for (const auto& name : _callbacksPreEnd)
		CallLevelFuncByName(name, endReason);

	if (_onEnd.valid())
		CallLevelFunc(_onEnd, endReason);

	for (const auto& name : _callbacksPostEnd)
		CallLevelFuncByName(name, endReason);
}

void LogicHandler::OnUseItem(GAME_OBJECT_ID objectNumber)
{
	for (const auto& name : _callbacksPreUseItem)
		CallLevelFuncByName(name, objectNumber);

	if (_onUseItem.valid())
		CallLevelFunc(_onUseItem, objectNumber);

	for (const auto& name : _callbacksPostUseItem)
		CallLevelFuncByName(name, objectNumber);
}

void LogicHandler::OnFreeze()
{
	for (const auto& name : _callbacksPreFreeze)
		CallLevelFuncByName(name);

	PerformConsoleInput();

	if (_onFreeze.valid())
		CallLevelFunc(_onFreeze);
		
	for (const auto& name : _callbacksPostFreeze)
		CallLevelFuncByName(name);
}

/*** Special tables

TombEngine uses the following tables for specific things.

@section levelandgametables
*/

/*** A table with level-specific data which will be saved and loaded.
This is for level-specific information that you want to store in saved games.

@advancedDesc
For example, you may have a level with a custom puzzle where Lara has
to kill exactly seven enemies to open a door to a secret. You could use
the following line each time an enemy is killed:

	LevelVars.enemiesKilled = LevelVars.enemiesKilled + 1

If the player saves the level after killing three, saves, and then reloads the save
some time later, the values `3` will be put back into `LevelVars.enemiesKilled.`

__This table is emptied when a level is finished.__ If the player needs to be able
to return to the level (like in the Karnak and Alexandria levels in *The Last Revelation*),
you will need to use the @{GameVars} table, below.

__LevelVars.Engine is a reserved table used internally by TombEngine's libs. Do not modify, overwrite, or add to it.__

@table LevelVars
*/

/*** A table with game data which will be saved and loaded.
This is for information not specific to any level, but which concerns your whole
levelset or game, that you want to store in saved games.

@advancedDesc
For example, you may wish to have a final boss say a specific voice line based on
a choice the player made in a previous level. In the level with the choice, you could
write:

	GameVars.playerSnoopedInDrawers = true

And in the script file for the level with the boss, you could write:

	if GameVars.playerSnoopedInDrawers then
		PlayAudioTrack("how_dare_you.wav")
	end

Unlike @{LevelVars}, this table will remain intact for the entirety of the game.

__GameVars.Engine is a reserved table used internally by TombEngine's libs. Do not modify, overwrite, or add to it.__

@table GameVars
*/

/*** A table nested table system for level-specific functions.

@advancedDesc
This serves a few purposes: it holds the level callbacks (listed below) as well as
any trigger functions you might have specified. For example, if you give a trigger
a Lua name of "my_trigger" in Tomb Editor, you will have to implement it as a member
of this table:

	LevelFuncs.my_trigger = function() 
		-- implementation goes here
	end

You can organise functions into tables within the hierarchy:

	LevelFuncs.enemyFuncs = {}

	LevelFuncs.enemyFuncs.makeBaddyRunAway = function() 
		-- implementation goes here
	end

	LevelFuncs.enemyFuncs.makeBaddyUseMedkit = function() 
		-- implementation goes here
	end

There are two special subtables which you should __not__ overwrite:

	LevelFuncs.Engine -- this is for 'first-party' functions, i.e. ones that come with TombEngine.
	LevelFuncs.External -- this is for 'third-party' functions. If you write a library providing LevelFuncs functions for other builders to use in their levels, put those functions in LevelFuncs.External.YourLibraryNameHere

The following are the level callbacks. They are optional; if your level has no special
behaviour for a particular scenario, you do not need to implement the function. For
example, if your level does not need any special initialisation when it is loaded,
you can just leave out `LevelFuncs.OnStart`.

__The order of loading is as follows:__

1. The level data itself is loaded.
2. The level script itself is run (i.e. any code you put outside the `LevelFuncs` callbacks is executed).
3. Save data is loaded, if saving from a saved game (will empty `LevelVars` and `GameVars` and repopulate them with what they contained when the game was saved).
4. If loading from a save, `OnLoaded` will be called. Otherwise, `OnStart` will be called.
5. The control loop, in which `OnLoop` will be called once per frame, begins.

@tfield function OnStart Will be called when a level is entered by completing a previous level or by selecting it in the menu. Will not be called when loaded from a saved game.
@tfield function OnLoad Will be called when a saved game is loaded, just *after* data is loaded
@tfield function(float) OnLoop Will be called during the game's update loop,
and provides the delta time (a float representing game time since last call) via its argument.
@tfield function OnSave Will be called when the player saves the game, just *before* data is saved
@tfield function OnEnd(EndReason) Will be called when leaving a level. This includes finishing it, exiting to the menu, or loading a save in a different level. It can take an `EndReason` arg:

	EXIT_TO_TITLE
	LEVEL_COMPLETE
	LOAD_GAME
	DEATH
	OTHER

For example:
	LevelFuncs.OnEnd = function(reason)
		if(reason == TEN.Logic.EndReason.DEATH) then
			print("death")
		end
	end
@tfield function OnUseItem Will be called when using an item from inventory.
@tfield function OnFreeze Will be called when any of the Freeze modes are activated.
@table LevelFuncs
*/

void LogicHandler::InitCallbacks()
{
	auto assignCB = [this](sol::protected_function& func, const std::string& luaFunc)
	{
		auto state = _handler.GetState();
		auto fullName = std::string(ScriptReserved_LevelFuncs) + "." + luaFunc;

		sol::object theData = (*state)[ScriptReserved_LevelFuncs][luaFunc];

		if (!theData.valid())
			return;

		LevelFunc fnh = (*state)[ScriptReserved_LevelFuncs][luaFunc];

		func = _levelFuncs_luaFunctions[fnh.m_funcName];

		if (!func.valid())
			TENLog("Level's script does not define callback " + fullName + ". Defaulting to no " + fullName + " behaviour.");
	};

	assignCB(_onStart, ScriptReserved_OnStart);
	assignCB(_onLoad, ScriptReserved_OnLoad);
	assignCB(_onLoop, ScriptReserved_OnLoop);
	assignCB(_onSave, ScriptReserved_OnSave);
	assignCB(_onEnd, ScriptReserved_OnEnd);
	assignCB(_onUseItem, ScriptReserved_OnUseItem);
	assignCB(_onFreeze, ScriptReserved_OnFreeze);

	// COMPATIBILITY
	assignCB(_onLoop, "OnControlPhase");
}
