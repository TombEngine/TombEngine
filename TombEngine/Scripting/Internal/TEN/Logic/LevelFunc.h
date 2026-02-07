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

/// A hierarchical table system for organizing level-specific functions.
// @specialtable LevelFuncs
// This serves a few purposes: it holds the level callbacks (listed below) as well as
// any trigger functions you might have specified.
// 
// For example, if you give a trigger a Lua name of "my_trigger" in Tomb Editor, you will have to implement it as a member
// of this table:
//	LevelFuncs.my_trigger = function()
//		--implementation goes here
//	end
// You can organise functions into tables within the hierarchy:
//	LevelFuncs.enemyFuncs = {}
//
//	LevelFuncs.enemyFuncs.makeBaddyRunAway = function()
//		--implementation goes here
//	end
//
//	LevelFuncs.enemyFuncs.makeBaddyUseMedkit = function()
//		--implementation goes here
//	end
// 
// <h2>Level Callbacks</h2>
// The level's .lua file contains predefined functions, called callbacks, that will be called on specific events in the level. If you want to execute code or call other functions in these events, you'll need to place them inside these functions.
// 
// The functions are:
// 
// * __OnStart__ Will be called when a level is entered by completing a previous level or by selecting it in the menu. Will not be called when loaded from a saved game.
// * __OnLoad__ Will be called when a level is loaded from a saved game.
// * __OnLoop__ Will be called once per frame while the level is active.
// * __OnSave__ Will be called when the game is saved while in the level.
// * __OnEnd__ Will be called when leaving a level. This includes finishing it, exiting to the menu, or loading a save in a different level. It can take an @{Logic.EndReason}.
// <br>For example:
//	LevelFuncs.OnEnd = function(reason)
//		if reason == TEN.Logic.EndReason.LEVEL_COMPLETE then
//			--implementation goes here
//		end
//		if reason == TEN.Logic.EndReason.DEATH then
//			print("death")
//		end
//	end
// * __OnUseItem__ Will be called when the player uses an item from their inventory. It can take a @{Objects.ObjID}.
// <br>For example:
//	LevelFuncs.OnUseItem = function(itemID)
//		if itemID == TEN.Objects.ObjID.SMALLMEDI_ITEM then
//			--implementation goes here
//		end
//	end
// * __OnFreeze__ Will be called when any of the Freeze modes are activated.
//
// <br>__The order of loading is as follows:__
// 
// 1. The level data itself is loaded.
// 
// 2. The level script itself is run (i.e. any code you put outside the `LevelFuncs` callbacks is executed).
// 
// 3. Save data is loaded, if saving from a saved game (will empty `LevelVars` and `GameVars` and repopulate them with what they contained when the game was saved).
// 
// 4. If loading from a save, OnLoad will be called. Otherwise, OnStart will be called.
// 
// 5. The control loop, in which OnLoop will be called once per frame, begins.
//
// <h2>Note:</h2>
//
// - LevelFuncs is created automatically. Never assign a value to LevelFuncs itself, as that will overwrite the entire table.<br>
// For example, do NOT do this:
// 	LevelFuncs = {} -- This will break everything!
// or
// 	LevelFuncs = LevelFuncs -- not needed, LevelFuncs already exists.
//
// - __*LevelFuncs.External*__ is for 'third-party' functions.<br>
// For example, if you write a library providing LevelFuncs functions for other builders to use in their levels, put those functions in:
//	LevelFuncs.External.YourLibraryNameHere = {}
//	LevelFuncs.External.YourLibraryNameHere.yourFunction = function()
//		--implementation goes here
//	end
//	LevelFuncs.External.YourLibraryNameHere.yourFunction2 = function()
//		--implementation goes here
//	end
//
// - __*LevelFuncs.Engine*__ is a reserved table used internally by TombEngine's libs. __Do not modify, overwrite, or add to it.__
//