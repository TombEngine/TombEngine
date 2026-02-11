#pragma once

// This file is used for documentation of GameVars.

/// A table with game data which will be saved and loaded.
// @specialtable GameVars
// This is for information not specific to any level, but which concerns your whole
// levelset or game, that you want to store in saved games.
//
// For example, you may wish to have a final boss say a specific voice line based on
// a choice the player made in a previous level. In the level with the choice, you could
// write:
// 	GameVars.playerSnoopedInDrawers = true
// And in the script file for the level with the boss, you could write:
// 	if GameVars.playerSnoopedInDrawers then
// 		TEN.Sound.PlayAudioTrack("how_dare_you.wav", TEN.Sound.SoundTrackType.LOOPED)
// 	end
// 
// In GameVars, you can only save certain types of variables:
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
// If you try to save a variable of an unsupported type (like a function or userdata), an error will be raised when you attempt to save the game.
// 
// <h2>Note:</h2>
//
// - GameVars is created automatically. Never assign a value to GameVars.<br>
// For example, do not write:
// 	GameVars = {} -- This will break everything!
// or
// 	GameVars = GameVars -- not needed, GameVars already exists
// Instead, just assign values to members of GameVars, like so:
// 	GameVars.someValue = 42
// 	GameVars.anotherValue = "Hello, world!"
// 	GameVars.aTable = { key1 = "value1", key2 = "value2" }
// 	GameVars.someValue = 11 -- chainge value of existing variable
//
// - GameVars is saved and loaded automatically when the game is saved and loaded. There is no need to write custom save/load code for it.
//
// - Unlike @{LevelVars}, this table will remain intact for the entirety of the game.
// 
// - __*GameVars.Engine*__ is a reserved table used internally by TombEngine's libs. __Do not modify, overwrite, or add to it.__
//