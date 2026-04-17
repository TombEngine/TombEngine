#pragma once

// This file is used for documentation of GlobalVars.

/// A table with global data which persists across game sessions and is saved to an external file.
// @specialtable GlobalVars
// This is for information that should persist across all game sessions and survive engine restarts,
// such as achievements, title screen modifications based on game progress, or unlockable game modes.
//
// For example, you may wish to unlock a new game mode after the player completes the game:
// 	GlobalVars.gameCompleted = true
// And in the title screen script, you could check:
// 	if GlobalVars.gameCompleted then
// 		-- Show "New Game+" option in the menu
// 	end
//
// GlobalVars are saved to an external file when the engine exits or switches a level, and restored
// when the engine launches.
//
// In GlobalVars, you can only save certain types of variables:
// 
// - number (integers and floats)
// - boolean (true and false)
// - @{string} (text)
// - @{table} (tables containing any of these types, including nested tables)
// - @{Vec2} (2D vectors)
// - @{Vec3} (3D vectors)
// - @{Rotation} (rotation values)
// - @{Time} (time values)
// - @{Color} (color values)
//
// If you try to save a variable of an unsupported type (like a function or userdata), an error will be raised.
// 
// <h2>Note:</h2>
//
// - GlobalVars is created automatically. Never assign a value to GlobalVars.<br>
// For example, do not write:
// 	GlobalVars = {} -- This will break everything!
// or
// 	GlobalVars = GlobalVars -- not needed, GlobalVars already exists
// Instead, just assign values to members of GlobalVars, like so:
// 	GlobalVars.someValue = 42
// 	GlobalVars.anotherValue = "Hello, world!"
// 	GlobalVars.aTable = { key1 = "value1", key2 = "value2" }
// 	GlobalVars.someValue = 11 -- change value of existing variable
//
// - GlobalVars is saved to an external file when the engine exits, and loaded on the next engine launch.
//
// - Unlike @{LevelVars} and @{GameVars}, this table persists across all game sessions, including engine restarts.
//
// - __*GlobalVars.Engine*__ is a reserved table used internally by TombEngine's libs. __Do not modify, overwrite, or add to it.__
//
