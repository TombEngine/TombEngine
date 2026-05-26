#include "framework.h"
#include "LogicHandler.h"

#include "Game/control/volume.h"
#include "Game/effects/Electricity.h"
#include "Game/Lara/lara.h"
#include "Game/savegame.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Logic/EventType.h"
#include "Scripting/Internal/TEN/Logic/LevelEndReason.h"
#include "Scripting/Internal/TEN/Logic/LevelFunc.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Time/Time.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/trutils.h"

using namespace TEN::Effects::Electricity;
using namespace TEN::Scripting::Types;
using namespace TEN::Utils;

/***
Saving data, triggering functions, and callbacks for level-specific scripts.
@tentable Logic 
@pragma nostrip
*/

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

		auto string = std::string(luaL_tolstring(tab.lua_state(), -1, nullptr));

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
	tableLogic.set_function(ScriptReserved_HasCallback, &LogicHandler::HasCallback, this);
	tableLogic.set_function(ScriptReserved_HandleEvent, &LogicHandler::HandleEvent, this);
	tableLogic.set_function(ScriptReserved_EnableEvent, &LogicHandler::EnableEvent, this);
	tableLogic.set_function(ScriptReserved_DisableEvent, &LogicHandler::DisableEvent, this);

	_handler.MakeReadOnlyTable(tableLogic, ScriptReserved_EndReason, LEVEL_END_REASONS);
	_handler.MakeReadOnlyTable(tableLogic, ScriptReserved_CallbackPoint, CALLBACK_POINTS);
	_handler.MakeReadOnlyTable(tableLogic, ScriptReserved_EventType, EVENT_TYPES);

	LevelFunc::Register(tableLogic);

	ResetScripts(true);
	ResetGlobalTables();
}

/*** Register a function as a callback.
@advancedDesc
This is intended for module/library developers who want their modules to do
stuff during level start/load/end/save/control phase, but don't want the level
designer to add calls to `OnStart`, `OnLoad`, etc. in their level script. Any returned value will be discarded.

Note: __the order in which two functions with the same CallbackPoint are called is undefined__.

i.e. if you register `MyFunc` and `MyFunc2` with `PRE_LOOP`, both will be called in the beginning of game loop, but there is no guarantee that `MyFunc` will be called before `MyFunc2`, or vice-versa.

Arguments:

- The callbacks `PRE_END` and `POST_END` receive a @{Logic.EndReason} argument.
- The callbacks `PRE_USE_ITEM` and `POST_USE_ITEM` receive an @{Objects.ObjID} argument.
- The callbacks `PRE_PICKUP`, `POST_PICKUP`, `PRE_VEHICLE_ENTER`, `POST_VEHICLE_ENTER`, `PRE_VEHICLE_LEAVE`, and `POST_VEHICLE_LEAVE` receive a @{Objects.Moveable} argument.
- The argument for `PRE_LOOP` and `POST_LOOP` is deprecated and should not be used.

@function AddCallback
@tparam Logic.CallbackPoint point When should the callback be called?
@tparam function func The function to be called (must be in the `LevelFuncs` hierarchy). Will receive, as an argument, the time in seconds since the last frame.
@usage
	LevelFuncs.MyFunc = function() 
		-- do stuff here
	end
	TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_START, LevelFuncs.MyFunc)

	-- Another example, with argument
	LevelFuncs.OnLevelEnd = function(reason)
		-- do stuff here
		print("Level ended because reason code: " .. reason)
	end
	TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_END, LevelFuncs.OnLevelEnd)

	-- Another example, two functions added in the same callback type
	LevelFuncs.FuncA = function()
		-- do stuff here
	end
	LevelFuncs.FuncB = function()
		-- do other stuff here
	end
	TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.POST_LOAD, LevelFuncs.FuncA)
	TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.POST_LOAD, LevelFuncs.FuncB)
	-- In this case, both FuncA and FuncB will be called after level load, but the order is undefined.
*/
void LogicHandler::AddCallback(CallbackPoint point, const LevelFunc& levelFunc)
{
	if (point == _lastCallbackPoint)
	{
		TENLog(fmt::format("Attempt to add callback function {} within the same callback type.", levelFunc.m_funcName), LogLevel::Error);
		return;
	}

	auto& callbacks = _callbackSets[(int)point];

	if (callbacks.find(levelFunc.m_funcName) != callbacks.end())
	{
		TENLog(fmt::format("Function {} already registered in the callback list.", levelFunc.m_funcName), LogLevel::Warning, LogConfig::All, true);
	}
	else
	{
		callbacks.insert(levelFunc.m_funcName);
	}
}

/*** Deregister a function as a callback.
Will have no effect if the function was not registered as a callback

@function RemoveCallback
@tparam Logic.CallbackPoint point The callback point the function was registered with. See @{AddCallback}
@tparam function func The function to remove; must be in the `LevelFuncs` hierarchy.
@usage
	TEN.Logic.RemoveCallback(TEN.Logic.CallbackPoint.PRELOOP, LevelFuncs.MyFunc)
*/
void LogicHandler::RemoveCallback(CallbackPoint point, const LevelFunc& levelFunc)
{
	if (point == _lastCallbackPoint)
	{
		TENLog("Attempt to remove callback function " + levelFunc.m_funcName + " within the same callback type.", LogLevel::Error, LogConfig::All);
		return;
	}

	_callbackSets[(int)point].erase(levelFunc.m_funcName);
}

/*** Check if a function is registered as a callback for a specific callback point.

@function HasCallback
@tparam Logic.CallbackPoint point The callback point to check.
@tparam function func The function to check; must be in the `LevelFuncs` hierarchy.
@treturn boolean True if the function is registered for the callback point, false otherwise.
@usage
	if TEN.Logic.HasCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.MyFunc) then
		print("MyFunc is registered for PRE_LOOP")
	end
*/
bool LogicHandler::HasCallback(CallbackPoint point, const LevelFunc& levelFunc)
{
	if (point == CallbackPoint::Count)
		return false;

	auto& callbacks = _callbackSets[(int)point];
	return callbacks.find(levelFunc.m_funcName) != callbacks.end();
}

/*** Attempt to find an event set and execute a particular event from it.

@function HandleEvent
@tparam string name Name of the event set to find.
@tparam Logic.EventType type Event to execute.
@tparam[opt=Lara] Objects.Moveable activator Optional activator.
@usage
	-- Executes the "ENTER" volume event of the event set named "MyVolumeEvent"
	TEN.Logic.HandleEvent("MyVolumeEvent", TEN.Logic.EventType.ENTER)

	-- Executes the "LOAD" global event of the event set named "MyGlobalEvent", with enemy as activator
	enemy = TEN.Objects.GetMoveableByName("MyEnemy")
	TEN.Logic.HandleEvent("MyGlobalEvent", TEN.Logic.EventType.LOAD, enemy)
*/
void LogicHandler::HandleEvent(const std::string& name, EventType type, sol::optional<Moveable&> activator)
{
	TEN::Control::Volumes::HandleEvent(name, type, activator.has_value() ? (Activator)(short)activator->GetIndex() : (Activator)(short)LaraItem->Index);
}

/*** Attempt to find an event set and enable specified event in it.

@function EnableEvent
@tparam string name Name of the event set to find.
@tparam Logic.EventType type Event to enable.
@usage
	-- Enables the "ENTER" volume event of the event set named "MyVolumeEvent"
	TEN.Logic.EnableEvent("MyVolumeEvent", TEN.Logic.EventType.ENTER)
*/
void LogicHandler::EnableEvent(const std::string& name, EventType type)
{
	TEN::Control::Volumes::SetEventState(name, type, true);
}

/*** Attempt to find an event set and disable specified event in it.

@function DisableEvent
@tparam string name Name of the event set to find.
@tparam Logic.EventType type Event to disable.
@usage
	-- Disables the "ENTER" volume event of the event set named "MyVolumeEvent"
	TEN.Logic.DisableEvent("MyVolumeEvent", TEN.Logic.EventType.ENTER)
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

void LogicHandler::ResetGameTables()
{
	auto state = _handler.GetState();
	MakeSpecialTable(state, ScriptReserved_GameVars, &GetVariable, &SetVariable);

	(*state)[ScriptReserved_GameVars][ScriptReserved_Engine] = sol::table(*state, sol::create);
}

void LogicHandler::ResetGlobalTables()
{
	auto state = _handler.GetState();
	MakeSpecialTable(state, ScriptReserved_GlobalVars, &GetVariable, &SetVariable);

	(*state)[ScriptReserved_GlobalVars][ScriptReserved_Engine] = sol::table(*state, sol::create);
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

		// Check if function is already defined.
		if (auto it = _levelFuncs_luaFunctions.find(fullName); it != _levelFuncs_luaFunctions.end())
			TENLog("Lua function " + fullName + " is being redefined. Check your level script.", LogLevel::Warning);

		// Define function.
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

	for (auto& callbacks : _callbackSets)
		callbacks.clear();

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
	_levelFuncs[ScriptReserved_External] = sol::table(*_handler.GetState(), sol::create);

	_levelFuncs_tablesOfNames.clear();
	_levelFuncs_luaFunctions.clear();
	_levelFuncs_levelFuncObjects = sol::table(*_handler.GetState(), sol::create);

	_levelFuncs_tablesOfNames.emplace(std::make_pair(ScriptReserved_LevelFuncs, std::unordered_map<std::string, std::string>{}));

	ResetLevelTables();

	for (auto& callback : _levelFuncCallbacks)
		callback = sol::nil;

	_functionCallCount = 0;
	_insideFunction = false;

	_handler.GetState()->collect_garbage();
}

template<SavedVarType TypeEnum, typename TypeTo, typename TypeFrom, typename MapType>
int Handle(TypeFrom& var, MapType& varsMap, int& numVars, std::vector<SavedVar>& vars)
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

void LogicHandler::SerializeScriptTable(const sol::table& tab, std::vector<SavedVar>& vars)
{
	auto varsMap = std::unordered_map<void const*, unsigned int>{};
	auto numMap = std::unordered_map<double, unsigned int>{};
	auto boolMap = std::unordered_map<bool, unsigned int>{};

	int varCount = 0;

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

		if (second)
		{
			++varCount;
			auto id = first->second;

			vars.push_back(IndexTable{});

			for (auto& [first, second] : obj)
			{
				bool validKey = true;
				unsigned int keyIndex = 0;
				std::variant<std::string, unsigned int> key{ unsigned int(0) };

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
						key = (unsigned int)data;
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

std::unordered_map<unsigned int, sol::table> LogicHandler::DeserializeScriptVars(const std::vector<SavedVar>& vars)
{
	auto solTables = std::unordered_map<unsigned int, sol::table>{};

	for (int i = 0; i < vars.size(); ++i)
	{
		if (std::holds_alternative<IndexTable>(vars[i]))
		{
			solTables.try_emplace(i, *_handler.GetState(), sol::create);
			auto indexTab = std::get<IndexTable>(vars[i]);

			for (auto& [first, second] : indexTab)
			{
				if (first >= vars.size() || second >= vars.size())
				{
					TENLog("Corrupted save data: variable index out of range. Skipping entry.", LogLevel::Warning);
					continue;
				}

				if (std::holds_alternative<IndexTable>(vars[second]))
				{
					solTables.try_emplace(second, *_handler.GetState(), sol::create);
					solTables[i][vars[first]] = solTables[second];
				}
				else if (std::holds_alternative<double>(vars[second]))
				{
					double theNum = std::get<double>(vars[second]);
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
					solTables[i][vars[first]] = ScriptColor{ color };
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

	return solTables;
}

// Used when saving.
void LogicHandler::GetVariables(std::vector<SavedVar>& vars)
{
	auto tab = sol::table(*_handler.GetState(), sol::create);
	tab[ScriptReserved_LevelVars] = (*_handler.GetState())[ScriptReserved_LevelVars];
	tab[ScriptReserved_GameVars] = (*_handler.GetState())[ScriptReserved_GameVars];

	SerializeScriptTable(tab, vars);
}

// Used when loading.
void LogicHandler::SetVariables(const std::vector<SavedVar>& vars, bool onlyLevelVars)
{
	if (!onlyLevelVars)
		ResetGameTables();

	ResetLevelTables();

	auto solTables = DeserializeScriptVars(vars);

	if (solTables.empty())
		return;

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

// Used when saving global vars to external file.
void LogicHandler::GetGlobalVariables(std::vector<SavedVar>& vars)
{
	auto tab = sol::table(*_handler.GetState(), sol::create);
	tab[ScriptReserved_GlobalVars] = (*_handler.GetState())[ScriptReserved_GlobalVars];

	SerializeScriptTable(tab, vars);
}

// Used when loading global vars from external file.
void LogicHandler::SetGlobalVariables(const std::vector<SavedVar>& vars)
{
	ResetGlobalTables();

	auto solTables = DeserializeScriptVars(vars);

	if (solTables.empty())
		return;

	auto rootTable = solTables[0];

	sol::object globalVarsObj = rootTable[ScriptReserved_GlobalVars];
	if (globalVarsObj.valid() && globalVarsObj.is<sol::table>())
	{
		sol::table globalVars = globalVarsObj;
		for (auto& [first, second] : globalVars)
			(*_handler.GetState())[ScriptReserved_GlobalVars][first] = second;
	}
}

void LogicHandler::GetCallbackStrings(CallbackStringLists& callbackLists) const
{
	for (int i = 0; i < (int)_callbackSets.size(); ++i)
	{
		for (const auto& string : _callbackSets[i])
			callbackLists[i].push_back(string);
	}
}

void LogicHandler::SetCallbackStrings(const CallbackStringLists& callbackLists)
{
	for (int i = 0; i < (int)callbackLists.size(); ++i)
	{
		for (const auto& string : callbackLists[i])
			_callbackSets[i].insert(string);
	}
}

template <typename R, char const * S, typename mapType>
std::unique_ptr<R> GetByName(const std::string& type, const std::string& name, const mapType& map)
{
	ScriptAssert(map.find(name) != map.end(), std::string{ type + " name not found: " + name }, ErrorMode::Terminate);
	return std::make_unique<R>(map.at(name), false);
}

void LogicHandler::ResetVariables()
{
	(*_handler.GetState())["Lara"] = nullptr;
}

void LogicHandler::ShortenTENCalls()
{
	auto str = R"(local ShortenInner 
	local exceptions = {
		DisplaySprite = true,
	}

	ShortenInner = function(tab)
		for k, v in pairs(tab) do
			if _G[k] and not exceptions[k] then
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

unsigned int LogicHandler::GetFunctionCallCount()
{
	// Only return the count if we're inside a lua function call.
	return _insideFunction ? _functionCallCount : 0;
}

void LogicHandler::PerformMoveableCallbacks(CallbackPoint point, short itemNumber)
{
	if (itemNumber == NO_VALUE)
		return;

	auto& callbacks = _callbackSets[(int)point];
	if (callbacks.empty())
		return;

	_lastCallbackPoint = point;

	for (const auto& name : callbacks)
		CallLevelFuncByName(name, std::make_unique<Moveable>(itemNumber));

	_lastCallbackPoint = std::nullopt;
}

void LogicHandler::PerformMoveableCallbacks(LevelFuncCallbackPoint callback, CallbackPoint prePoint, CallbackPoint postPoint, short itemNumber, bool postLoop)
{
	if (itemNumber == NO_VALUE)
		return;

	if (!postLoop)
	{
		PerformMoveableCallbacks(prePoint, itemNumber);
		return;
	}

	PerformLevelFuncCallback(callback, std::make_unique<Moveable>(itemNumber));
	PerformMoveableCallbacks(postPoint, itemNumber);
}


void LogicHandler::OnStart()
{
	PerformCallbacks(CallbackPoint::PreStart);
	PerformLevelFuncCallback(LevelFuncCallbackPoint::Start);
	PerformCallbacks(CallbackPoint::PostStart);
	HandleAllGlobalEvents(EventType::Start, (Activator)short(LaraItem->Index));
}

void LogicHandler::OnLoad()
{
	PerformCallbacks(CallbackPoint::PreLoad);
	PerformLevelFuncCallback(LevelFuncCallbackPoint::Load);
	PerformCallbacks(CallbackPoint::PostLoad);
	HandleAllGlobalEvents(EventType::Load, (Activator)short(LaraItem->Index));
}

void LogicHandler::OnLoop(float deltaTime, bool postLoop)
{
	if (!postLoop)
	{
		PerformCallbacks(CallbackPoint::PreLoop, deltaTime);

		PerformConsoleInput();

		lua_gc(_handler.GetState()->lua_state(), LUA_GCCOLLECT, 0);
		PerformLevelFuncCallback(LevelFuncCallbackPoint::Loop, deltaTime);
		HandleAllGlobalEvents(EventType::Loop, (Activator)short(LaraItem->Index));
	}
	else
	{
		PerformCallbacks(CallbackPoint::PostLoop, deltaTime);
	}
}

void LogicHandler::OnSave()
{
	PerformCallbacks(CallbackPoint::PreSave);
	PerformLevelFuncCallback(LevelFuncCallbackPoint::Save);
	PerformCallbacks(CallbackPoint::PostSave);
	HandleAllGlobalEvents(EventType::Save, (Activator)short(LaraItem->Index));
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

	PerformCallbacks(CallbackPoint::PreEnd, int(endReason));
	PerformLevelFuncCallback(LevelFuncCallbackPoint::End, endReason);
	PerformCallbacks(CallbackPoint::PostEnd, int(endReason));
	HandleAllGlobalEvents(EventType::End, (Activator)short(LaraItem->Index));
}

void LogicHandler::OnUseItem(short itemNumber, GAME_OBJECT_ID objectNumber)
{
	PerformCallbacks(CallbackPoint::PreUseItem, objectNumber);
	PerformLevelFuncCallback(LevelFuncCallbackPoint::UseItem, objectNumber);
	PerformCallbacks(CallbackPoint::PostUseItem, objectNumber);
	HandleAllGlobalEvents(EventType::UseItem, (Activator)itemNumber);
}

void LogicHandler::OnPickup(short itemNumber, bool postLoop)
{
	PerformMoveableCallbacks(LevelFuncCallbackPoint::Pickup, CallbackPoint::PrePickup, CallbackPoint::PostPickup, itemNumber, postLoop);
	if (postLoop)
		HandleAllGlobalEvents(EventType::Pickup, (Activator)itemNumber);
}

void LogicHandler::OnVehicleEnter(short itemNumber, bool postLoop)
{
	PerformMoveableCallbacks(LevelFuncCallbackPoint::VehicleEnter, CallbackPoint::PreVehicleEnter, CallbackPoint::PostVehicleEnter, itemNumber, postLoop);
	if (postLoop)
		HandleAllGlobalEvents(EventType::VehicleEnter, (Activator)itemNumber);
}

void LogicHandler::OnVehicleLeave(short itemNumber, bool postLoop)
{
	PerformMoveableCallbacks(LevelFuncCallbackPoint::VehicleLeave, CallbackPoint::PreVehicleLeave, CallbackPoint::PostVehicleLeave, itemNumber, postLoop);
	if (postLoop)
		HandleAllGlobalEvents(EventType::VehicleLeave, (Activator)itemNumber);
}

void LogicHandler::OnFreeze()
{
	PerformCallbacks(CallbackPoint::PreFreeze);

	PerformConsoleInput();
	PerformLevelFuncCallback(LevelFuncCallbackPoint::Freeze);
	PerformCallbacks(CallbackPoint::PostFreeze);
	HandleAllGlobalEvents(EventType::Freeze, (Activator)short(LaraItem->Index));
}

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

		if (func.valid())
		{
			auto it = std::find_if(_levelFuncs_luaFunctions.begin(), _levelFuncs_luaFunctions.end(),
						[&](const auto& pair) { return pair.second == func; });

			if (it != _levelFuncs_luaFunctions.end())
			{
				auto message = std::string("Lua callback ") + it->first + " is being redefined";
				if (it->first != fullName)
					message += " to " + fullName;
				message += ". Check your level script.";
				TENLog(message, LogLevel::Warning);
			}
			else
			{
				TENLog("Unknown redefinition of the callback " + fullName + ".", LogLevel::Warning);
			}
		}

		func = _levelFuncs_luaFunctions[fnh.m_funcName];

		if (!func.valid())
			TENLog("Level script does not define callback " + fullName + ". Defaulting to no " + fullName + " behaviour.", LogLevel::Info, LogConfig::Debug);
	};

	for (int i = 0; i < (int)_levelFuncCallbacks.size(); ++i)
		assignCB(_levelFuncCallbacks[i], LEVELFUNC_CALLBACK_POINTS[(LevelFuncCallbackPoint)i]);

	// COMPATIBILITY
	assignCB(_levelFuncCallbacks[(int)LevelFuncCallbackPoint::Loop], "OnControlPhase");
}
