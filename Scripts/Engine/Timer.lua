-----
--- Basic timer - after a specified number of seconds, the chosen event happens.
--<style>table.function_list td.name {min-width: 395px;}</style>
-- Timers are updated automatically at every frame before OnLoop event.
--
--
-- To use Timer inside scripts you need to call the module:
--	local Timer = require("Engine.Timer")
--
--
-- Example usage:
--	local Timer = require("Engine.Timer")
--
--	-- This will be called when the timer runs out
--	LevelFuncs.FinishTimer = function(victoryMessage)
--		-- Open a door, display a message, make an explosion... whatever you wish
--		local pos = TEN.Vec2(TEN.Util.PercentToScreen(50, 10))
--		local str = TEN.Strings.DisplayString(victoryMessage, pos)
--		TEN.Strings.ShowString(str, 1)
--	end
--	
--	-- This function triggers the timer
--	LevelFuncs.TriggerTimer = function(obj) 
--		Timer.Create("my_timer",
--			5.0,
--			false,
--			{minutes = false, seconds = true, deciseconds = true},
--			LevelFuncs.FinishTimer,
--			"Well done!")
--		Timer.Get("my_timer"):Start()
--	end
--
-- @luautil Timer

local Type= require("Engine.Type")
local Utility = require("Engine.Util")

local Timer = {}
local zero = TEN.Time()
Timer.__index = Timer
LevelFuncs.Engine.Timer = {}
LevelVars.Engine.Timer = {}
LevelVars.Engine.Timer = {timers = {}}

--- Create (but do not start) a new timer.
--
--<h3 id="timerFormat">Timer format:</h3>
-- <ul>You have the option of displaying the remaining time of timer.
--
-- You can display hours, minutes, seconds and deciseconds (deciseconds are 1/10th of a second).
--
--To set which unit to display, you can use a **table** or a **boolean value**.
--
-- Timer format details:
--
--@advancedDesc
--	-- hours:mins:secs.decisecs
--	local myTimeFormat = {hours = true, minutes = true, seconds = true, deciseconds = true}
--
--	-- mins:secs
--	local myTimeFormat1 = {minutes = true, seconds = true, deciseconds = false}
--
--	-- also mins:secs
--	local myTimeFormat2 = {minutes = true, seconds = true}
--	
--	-- secs.decisecs
--	local myTimeFormat3 = {seconds = true, deciseconds = true}
--
--	-- secs; to display only seconds you can pass a table or true
--	local myTimeFormat4 = {seconds = true}
--	local myTimeFormat5 = true
--
--	-- no remaining time display
--	local myTimeFormat6 = false
--
--
--Use this sparingly; in the classics, timed challenges did not have visible countdowns. For shorter timers, the gameplay benefit from showing the remaining time might not be necessary, and could interfere with the atmosphere of the level.
--
--At any given time, multiple timers can show their countdown.</ul>
--
-- @tparam string name A label to give this timer; used to retrieve the timer later.
--
-- __Do not give your timers a name beginning with __TEN, as this is reserved for timers used by other internal libaries__.
-- @tparam float totalTime Duration of the timer, in seconds.
--
-- Values with only 1 tenth of a second (0.1) are accepted, example: 1.5 - 6.0 - 9.9 - 123.6. No negative values allowed!
-- @tparam[opt] bool loop If true, the timer will start again immediately after the time has elapsed. __Default: false__
-- @tparam[opt] ?table|bool timerFormat Sets the remaining time display. See <a href="#timerFormat">timer format</a>. __Default: false__.
-- @tparam[opt] LevelFunc func The function defined in the *LevelFuncs* table to call when the time is up
-- @tparam[opt] any ... a variable number of arguments with which the above function will be called
-- @treturn Timer The timer in its paused state
--
--
-- @usage
--  -- Example 1 simple timer:
--  Timer.Create("my_timer", 6.1)
--
--  -- Example 2 Timer that executes a function when it expires:
--  local TimeFormat = {minutes = true, seconds = true, deciseconds = true}
--  LevelFuncs.FinishTimer = function()
--      TEN.Util.PrintLog("Timer expired", TEN.Util.LogLevel.INFO)
--  end
--  Timer.Create("my_timer", 6.1, false, TimerFormat, LevelFuncs.FinishTimer)
Timer.Create = function (name, totalTime, loop, timerFormat, func, ...)
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in Timer.Create(): invalid name, '" .. tostring(name) .."' timer was not created", TEN.Util.LogLevel.ERROR)
		return
	end
	if not Type.IsNumber(totalTime) or totalTime < 0 then
		TEN.Util.PrintLog("Error in Timer.Create(): wrong value for totalTime, '".. name .."' timer was not created", TEN.Util.LogLevel.ERROR)
		return
	end
	if not Type.IsNull(func) and not Type.IsLevelFunc(func) then
		TEN.Util.PrintLog("Error in Timer.Create(): wrong value for func, '".. name .."' timer was not created", TEN.Util.LogLevel.ERROR)
		return
	end

	local self = {name = name}
	if LevelVars.Engine.Timer.timers[name] then
		TEN.Util.PrintLog("Warning in Timer.Create(): a timer with name '" .. name .. "' already exists; overwriting it with a new one...", TEN.Util.LogLevel.WARNING)
	end
	LevelVars.Engine.Timer.timers[name] = {}
	local thisTimer = LevelVars.Engine.Timer.timers[name]
	thisTimer.name = name
	thisTimer.totalTime = TEN.Time((math.floor(totalTime * 10) / 10)  * 30)
	thisTimer.realRemainingTime = thisTimer.totalTime
	thisTimer.remainingTime = thisTimer.totalTime

	loop = loop or false
	if not Type.IsBoolean(loop) then
		TEN.Util.PrintLog("Warning in Timer.Create(): Wrong value for loop, loop for '".. name .."' timer will be set to false", TEN.Util.LogLevel.WARNING)
		loop = false
	end
	thisTimer.loop = loop

	timerFormat = timerFormat or false
	thisTimer.timerFormat = Utility.CheckTimeFormat(timerFormat, "Warning in Timer.Create(): wrong value for timerFormat, timerFormat for '".. name .."' timer will be set to false")

	thisTimer.func = func
	thisTimer.funcArgs = {...}
	thisTimer.active = false
	thisTimer.paused = true
	thisTimer.first = true
	thisTimer.clock = true
	thisTimer.pos = TEN.Vec2(TEN.Util.PercentToScreen(50, 90))
	thisTimer.scale = 1
	thisTimer.unpausedColor = TEN.Color(255, 255, 255)
	thisTimer.pausedColor = TEN.Color(255, 255, 0)
	thisTimer.stringOption = {TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW}
	return setmetatable(self, Timer)
end

--- Delete a timer.
-- @tparam string name The label that was given to the timer when it was created
-- @usage
--	-- Example:
--  Timer.Delete("my_timer")
Timer.Delete = function (name)
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in Timer.Delete(): invalid name", TEN.Util.LogLevel.ERROR)
	elseif LevelVars.Engine.Timer.timers[name] then
		LevelVars.Engine.Timer.timers[name] = nil
	else
		TEN.Util.PrintLog("Warning in Timer.Delete(): '" .. name .. "' timer does not exist and can't be deleted.", TEN.Util.LogLevel.WARNING)
	end
end

--- Get a timer by its name.
-- @tparam string name The label that was given to the timer when it was created
-- @treturn ?Timer|nil *The timer* or *nil* if timer does not exist
-- @usage
--	-- Example:
--	Timer.Get("my_timer")
Timer.Get = function (name)
    if not Type.IsString(name) then
        return TEN.Util.PrintLog("Error in Timer.Get(): invalid name", TEN.Util.LogLevel.ERROR)
    end

    local timer = LevelVars.Engine.Timer.timers[name]
    if not timer then
        return TEN.Util.PrintLog("Warning in Timer.Get(): '" .. name .. "' timer not found", TEN.Util.LogLevel.ERROR)
    end

    return setmetatable({ name = name }, Timer)
end

--- Check if a timer exists.
-- @tparam string name The label that was given to the timer when it was created
-- @usage
--	-- Example:
--	-- This function checks if a timer named "my_timer" exists and starts it
--	LevelFuncs.CheckAndStart = function()
--		if Timer.IfExists("my_timer") then
--			Timer.Get("my_timer"):Start()
--		end
--	end
Timer.IfExists = function (name)
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in Timer.IfExists(): invalid name", TEN.Util.LogLevel.ERROR)
		return false
	end
	return LevelVars.Engine.Timer.timers[name] and true or false
end


Timer.UpdateAll = function (dt)
	print("Timer.UpdateAll is deprecated; timers and event sequences now get updated automatically pre-control phase.")
end

----
-- The list of all methods of the Timer object. We suggest that you always use the Timer.Get() function to use the methods of the Timer object to prevent errors or unexpected behavior
-- @type Timer
-- @usage
--	-- Examples of some methods
--	Timer.Get("my_timer"):Start()
--	Timer.Get("my_timer"):Stop()
--	Timer.Get("my_timer"):SetPaused(true)


--- Begin or unpause a timer. If showing the remaining time on-screen, its default color will be set to white.
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @tparam[opt] bool reset If true, the timer will restart from the beginning (total time)
-- @usage
--  local TimeFormat = {minutes = true, seconds = true, deciseconds = true}
--  Timer.Create("my_timer", 6.1, false, TimerFormat)
--
--	-- Example 1: Start the timer
--	-- This function starts the timer named my_timer
--	LevelFuncs.StartTimer = function()
--  	if Timer.IfExists("timer1") then
--			Timer.Get("my_timer"):Start()
--  	end
--	end
--
--	-- Example 2: Start the timer and reset it
--	-- This function resets the timer named my_timer and starts it
--	LevelFuncs.Reset_StartTimer = function()
--  	if Timer.IfExists("timer1") then
--			Timer.Get("my_timer"):Start(true)
--  	end
--	end
function Timer:Start(reset)
	local thisTimer = LevelVars.Engine.Timer.timers[self.name]
	thisTimer.remainingTime = reset and thisTimer.totalTime or thisTimer.remainingTime
	thisTimer.realRemainingTime = reset and thisTimer.totalTime or thisTimer.realRemainingTime
	thisTimer.active = true
	thisTimer.paused = false
end

--- Stop the timer.
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @usage
--  -- example
--  local TimeFormat = {minutes = true, seconds = true, deciseconds = true}
--  Timer.Create("my_timer", 6.1, false, TimerFormat)
--	
--  -- This function stops the timer named my_timer
--  LevelFuncs.StopTimer = function()
--		if Timer.IfExists("timer1") then
--			Timer.Get("my_timer"):Stop()
--		end
--	end
function Timer:Stop()
	LevelVars.Engine.Timer.timers[self.name].active = false
end

--- Pause or unpause the timer. If showing the remaining time on-screen, its default color will be set to yellow (paused) or white (unpaused).
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @tparam bool p If true, the timer will be paused; if false, it would be unpaused
-- @usage
--  local TimeFormat = {minutes = true, seconds = true, deciseconds = true}
--  Timer.Create("my_timer", 6.1, false, TimerFormat)
--
--  -- Example 1 paused timer:
--  if Timer.IfExists("timer1") then
--  	Timer.Get("my_timer"):SetPaused(true)
--  end
--
--  -- Example 2 unpaused timer:
--  if Timer.IfExists("timer1") then
--  	Timer.Get("my_timer"):SetPaused(false)
--  end
function Timer:SetPaused(p)
	if not Type.IsBoolean(p) then
		TEN.Util.PrintLog("Error in Timer:SetPaused(): wrong value for pause in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
		LevelVars.Engine.Timer.timers[self.name].paused = p
	end
end

--- Get the remaining time of a timer in game frames.
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @treturn Time The remaining time in *game frames* of timer.
-- @usage
--  -- Example:
--	local TimeFormat = {minutes = true, seconds = true, deciseconds = true}
--	Timer.Create("my_timer", 6.1, false, TimerFormat)
--
--	local timer = TEN.Time()
--	if Timer.IfExists("timer1") then
--      time = Timer.Get("my_timer"):GetRemainingTime()
--	end
function Timer:GetRemainingTime()
	local thisTimer = LevelVars.Engine.Timer.timers[self.name]
	return thisTimer.remainingTime
end

--- Get the remaining time of a timer in seconds.
-- @treturn float The remaining time in *seconds* of timer.
--
-- Seconds have an accuracy of 0.1 tenths. Example: 1.5 - 6.0 - 9.9 - 123.6
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @usage
--  -- Example:
--	local timer = 0
--	if Timer.IfExists("timer1") then
--      time = Timer.Get("my_timer"):GetRemainingTimeInSeconds()
--	end
function Timer:GetRemainingTimeInSeconds()
	local thisTimer = LevelVars.Engine.Timer.timers[self.name]
	local remainingTime = thisTimer.remainingTime
	return remainingTime.s + 60 * remainingTime.m + math.floor(remainingTime.c / 10) * 0.1
end

--- Get the formatted remaining time of a timer.
-- @tparam ?table|bool timerFormat Sets the remaining time display. See <a href="#timerFormat">timer format</a>.
-- @treturn string The formatted remaining time.
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @usage
--  -- Example:
--	local TimerFormat = {seconds = true, deciseconds = true}
--	if Timer.IfExists("timer1") then
--      local pos = TEN.Vec2(TEN.Util.PercentToScreen(50, 10))
--      local timer = Timer.Get("my_timer"):GetRemainingTimeFormatted(TimerFormat)
--      local str = TEN.Strings.DisplayString("Timer: " .. timer, pos)
--      TEN.Strings.ShowString(str, 1)
--	end
function Timer:GetRemainingTimeFormatted(timerFormat)
	local thisTimer = LevelVars.Engine.Timer.timers[self.name]
	local errorFormat = "Error in Timer:GetRemainingTimeFormatted(): wrong value for timerFormat in '" .. self.name .. "' timer"
	return Utility.GenerateTimeFormattedString(thisTimer.remainingTime, timerFormat, errorFormat)
end

--- Set the remaining time of a timer.
-- @tparam float remainingTime The new time remaining for the timer
--
-- Values with only 1 tenth of a second (0.1) are accepted, example: 1.5 - 6.0 - 9.9 - 123.6. No negative values allowed!
-- @usage
--  -- Example:
--  Timer.Get("my_timer"):SetRemainingTime(3.5)
function Timer:SetRemainingTime(remainingTime)
	if not Type.IsNumber(remainingTime) or remainingTime < 0 then
		TEN.Util.PrintLog("Error in Timer:SetRemainingTime(): wrong value  (" .. tostring(remainingTime) .. ")  for remainingTime in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
        local thisTimer = LevelVars.Engine.Timer.timers[self.name]
    	thisTimer.remainingTime = TEN.Time((math.floor(remainingTime * 10) / 10) * 30)
		thisTimer.realRemainingTime = thisTimer.remainingTime
		thisTimer.first = true
    end
end

--- Compares the remaining time with a value (in seconds). 
--
-- It's recommended to use the *IfRemainingTimeIs()* method to have error-free comparisons
-- @tparam int operator The type of comparison
--
-- 0 : If the remaining time is equal to the value
--
-- 1 : If the remaining time is different from the value
--
-- 2 : If the remaining time is less the value
--
-- 3 : If the remaining time is less or equal to the value
--
-- 4 : If the remaining time is greater the value
--
-- 5 : If the remaining time is greater or equal to the value
-- @tparam float seconds The value in seconds to compare.
--
-- Values with only 1 tenth of a second (0.1) are accepted, example: 1.5 - 6.0 - 9.9 - 123.6. No negative values allowed!
-- @treturn bool *true* if comparison is true, *false* if comparison is false or timer does not exist
-- @usage
--  -- Example:
--  if Timer.IfExists("timer1") and Timer.Get("timer1"):IfRemainingTimeIs(0, 1.5) then
--      local pos = TEN.Vec2(TEN.Util.PercentToScreen(50, 10))
--      local str = TEN.Strings.DisplayString("QUICK!", pos, 1)
--      TEN.Strings.ShowString(str, 1)
--  end
function Timer:IfRemainingTimeIs(operator, seconds)
	if not Type.IsNumber(operator) or operator < 0 or operator > 5 then
		TEN.Util.PrintLog("Error in Timer:IfRemainingTimeIs(): invalid operator for '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
		return false
	elseif not Type.IsNumber(seconds) or seconds < 0 then
		TEN.Util.PrintLog("Error in Timer:IfRemainingTimeIs(): wrong value (" .. tostring(seconds) .. ") for seconds in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
		return false
	end
	local remainingTime = LevelVars.Engine.Timer.timers[self.name].remainingTime
	local seconds_ = math.floor(seconds * 10) / 10
	local time = TEN.Time(seconds_ * 30)
	if LevelVars.Engine.Timer.timers[self.name].clock then
		return Utility.CompareValue(remainingTime, time, operator)
	else
		return false
	end
end

--- Get the total time of a timer in game frames. This is the amount of time the timer will start with, as well as when starting a new loop.
-- @treturn Time The timer's total time in *game frames*.
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @usage
--  -- Example:
--  local total = TEN.Time()
--	if Timer.IfExists("timer1") then
--      total = Timer.Get("my_timer"):GetTotalTime()
--	end
function Timer:GetTotalTime()
	local thisTimer = LevelVars.Engine.Timer.timers[self.name]
	return thisTimer.totalTime
end

--- Get the total time of a timer in seconds. This is the amount of time the timer will start with, as well as when starting a new loop
-- @treturn float The timer's total time in *seconds*.
--
-- Seconds have an accuracy of 0.1 tenths. Example: 1.5 - 6.0 - 9.9 - 123.6
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @usage
--  -- Example:
--  local total = 0
--	if Timer.IfExists("timer1") then
--      total = Timer.Get("my_timer"):GetTotalTimeInSeconds()
--	end
function Timer:GetTotalTimeInSeconds()
	local thisTimer = LevelVars.Engine.Timer.timers[self.name]
	local totalTime = thisTimer.totalTime
	return totalTime.s + 60 * totalTime.m + math.floor(totalTime.c / 10) * 0.1
end

--- Get the formatted total time of a timer. This is the amount of time the timer will start with, as well as when starting a new loop
-- @tparam ?table|bool timerFormat Sets the remaining time display. See <a href="#timerFormat">timer format</a>.
-- @treturn string The *formatted total time*.
--
-- __Please note:__ It's recommended to check that the timer exists.
-- @usage
--  -- Example:
--	local TimerFormat = {minutes = false, seconds = true, deciseconds = true}
--	if Timer.IfExists("timer1") then
--      local pos = TEN.Vec2(TEN.Util.PercentToScreen(50, 10))
--      local totalTime = Timer.Get("my_timer"):GetTotalTimeFormatted(TimerFormat)
--      local str = TEN.Strings.DisplayString("Total time is: " .. totalTime, pos)
--	end
function Timer:GetTotalTimeFormatted(timerFormat)
	local thisTimer = LevelVars.Engine.Timer.timers[self.name]
	local errorFormat = "Error in Timer:GetTotalTimeFormatted(): wrong value for timerFormat in '" .. self.name .. "' timer"
	return Utility.GenerateTimeFormattedString(thisTimer.totalTime, timerFormat, errorFormat)
end

--- Set the total time for a timer.
--
-- __Please note:__ The total time is changed when the timer is next started
-- @tparam float totalTime Timer's new total time
--
-- Values with only 1 tenth of a second (0.1) are accepted, example: 1.5 - 6.0 - 9.9 - 123.6. No negative values allowed!
-- @usage
--  -- Example:
--  Timer.Get("my_timer"):SetTotalTime(3.5)
function Timer:SetTotalTime(totalTime)
	if not Type.IsNumber(totalTime) or totalTime < 0 then
		TEN.Util.PrintLog("Error in Timer:SetTotalTime(): wrong value (" .. tostring(totalTime) .. ") for totalTime in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
		local seconds = math.floor(totalTime * 10) / 10
		LevelVars.Engine.Timer.timers[self.name].totalTime = TEN.Time(seconds * 30)
	end
end

--- Compares the total time with a value (in seconds).
--
-- It's recommended to use the *IfTotalTimeIs()* method to have error-free comparisons
-- @tparam int operator The type of comparison
--
-- 0 : If the total time is equal to the value
--
-- 1 : If the total time is different from the value
--
-- 2 : If the total time is less the value
--
-- 3 : If the total time is less or equal to the value
--
-- 4 : If the total time is greater the value
--
-- 5 : If the total time is greater or equal to the value
-- @tparam float seconds the value in seconds to compare
--
-- Values with only 1 tenth of a second (0.1) are accepted, example: 1.5 - 6.0 - 9.9 - 123.6. No negative values allowed!
-- @treturn bool *true* if comparison is true, *false* if comparison is false or timer does not exist
-- @usage
--  -- Example:
--  local test = Timer.Get("timer1"):IfTotalTimeIs(0, 5.1)
function Timer:IfTotalTimeIs(operator, seconds)
	if not Type.IsNumber(operator) or operator < 0 or operator > 5 then
		TEN.Util.PrintLog("Error in Timer:IfTotalTimeIs(): invalid operator for '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
		return false
	elseif not Type.IsNumber(seconds) or seconds < 0 then
		TEN.Util.PrintLog("Error in Timer:IfTotalTimeIs(): wrong value (" .. tostring(seconds) .. ") for seconds in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
		return false
	end
	local totalTime = LevelVars.Engine.Timer.timers[self.name].totalTime
	local seconds_ = math.floor(seconds * 10) / 10
	local time = TEN.Time(seconds_*30)
	return Utility.CompareValue(totalTime, time, operator)
end

--- Set whether or not the timer loops.
--
-- __Default value__: *false*
-- @tparam bool looping Whether or not the timer loops
-- @usage
--  -- Example:
--  Timer.Get("my_timer"):SetLooping(true)
function Timer:SetLooping(looping)
	if not Type.IsBoolean(looping) then
		TEN.Util.PrintLog("Error in Timer:SetLooping(): wrong value for looping in '" .. self.name .. "' timer", Util.LogLevel.ERROR)
	else
		LevelVars.Engine.Timer.timers[self.name].loop = looping
	end
end

--- Give the timer a new function and args
-- @tparam LevelFunc func The function defined in the *LevelFuncs* table to call when the time is up
-- @tparam[opt] any ... A variable number of arguments with which the above function will be called
-- @usage
--  -- Example:
--  -- This function kills Lara when the timer runs out
--  LevelFuncs.KillLara = function()
--      TEN.Util.PrintLog("Kill Lara", TEN.Util.LogLevel.INFO)
--      Lara:SetHP(0)
--  end
--  Timer.Get("my_timer"):SetFunction(LevelFuncs.KillLara)
function Timer:SetFunction(func, ...)
	if not Type.IsLevelFunc(func) then
		TEN.Util.PrintLog("Error in Timer:SetFunction(): invalid function for '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
		local thisTimer = LevelVars.Engine.Timer.timers[self.name]
		thisTimer.func = func
		thisTimer.funcArgs = {...}
	end
end

--- Set the on-screen position in percent of the displayed timer when active.
--
-- The coordinate (0,0) is in the upper left-hand corner.
--
-- __Default position__: (50,90)
-- @tparam float x The x-coordinate in percent
-- @tparam float y The y-coordinate in percent
-- @usage
--  -- Example:
--  Timer.Get("my_timer"):SetPosition(10.0,10.0)
function Timer:SetPosition(x,y)
	if not Type.IsNumber(x) then
		TEN.Util.PrintLog("Error in Timer:SetPosition(): wrong value for X in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	elseif not Type.IsNumber(y) then
		TEN.Util.PrintLog("Error in Timer:SetPosition(): wrong value for Y in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
		LevelVars.Engine.Timer.timers[self.name].pos = TEN.Vec2(TEN.Util.PercentToScreen(x, y))
	end
end

--- Set the scale of the displayed timer when it is active.
--
-- __Default value__: 1.0
-- @tparam float scale The new scale value
-- @usage
--  -- Example:
--  Timer.Get("my_timer"):SetScale(1.5)
function Timer:SetScale(scale)
	if not Type.IsNumber(scale) then
		TEN.Util.PrintLog("Error in Timer:SetScale(): wrong value for scale in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
		LevelVars.Engine.Timer.timers[self.name].scale = scale
	end
end

--- Set the paused color of the displayed timer when it is active.
--
-- __Default paused color__: TEN.Color(255, 255, 0, 255) [yellow]
-- @tparam Color color Timer's new paused color
-- @usage
--  -- Example:
--  Timer.Get("my_timer"):SetPausedColor(TEN.Color(0, 255, 0, 255))
function Timer:SetPausedColor(color)
	if not Type.IsColor(color) then
		TEN.Util.PrintLog("Error in Timer:SetPausedColor(): wrong value for color in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
		LevelVars.Engine.Timer.timers[self.name].pausedColor = color
	end
end

--- Set the color of the displayed timer when it is active.
--
-- __Default color__: TEN.Color(255, 255, 255, 255) [white]
-- @tparam Color color Timer's new color
-- @usage
--  -- Example:
--  Timer.Get("my_timer"):SetUnpausedColor(TEN.Color(0, 255, 255, 255))
function Timer:SetUnpausedColor(color)
	if not Type.IsColor(color) then
		TEN.Util.PrintLog("Error in Timer:SetUnpausedColor(): wrong value for color in '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
		LevelVars.Engine.Timer.timers[self.name].unpausedColor = color
	end
end

--- Set text options for a timer.
--
-- __Default options__: {TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW}
-- @tparam table optionsTable Table containing timer's new text options. See @{Strings.DisplayStringOption}
--
-- __Please note:__ the new table overwrites the existing table options
-- @usage
--  -- Example 1
--  -- right alignment
--  Timer.Get("my_timer"):SetTextOption({TEN.Strings.DisplayStringOption.RIGHT})
--
--  -- Example 2
--  -- defaul arguments + BLINK effect
--	newOptins = {TEN.Strings.DisplayStringOption.CENTER,
--				TEN.Strings.DisplayStringOption.SHADOW,
--				TEN.Strings.DisplayStringOption.BLINK}
--  Timer.Get("my_timer"):SetTextOption(newOptins)
--
--  -- Example 3
--  -- left alignment
--  Timer.Get("my_timer"):SetTextOption()
function Timer:SetTextOption(optionsTable)
	optionsTable = optionsTable or {}
	if type(optionsTable) ~= "table" then
		TEN.Util.PrintLog("Error in Timer:SetTextOption(): options is not a table for '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
	else
		for _, v in pairs(optionsTable) do
			if not Type.IsNumber(v) or v < 0 or v > 3 then
				TEN.Util.PrintLog("Error in Timer:SetTextOption(): invalid value in options for '" .. self.name .. "' timer", TEN.Util.LogLevel.ERROR)
				return
			end
		end
		LevelVars.Engine.Timer.timers[self.name].stringOption = optionsTable
	end
end

--- Get whether or not the timer is paused
-- @treturn bool *true* if the timer is paused, *false* if it is not paused or timer does not exist
-- @usage
--  -- Example:
--  local pauseState = Timer.Get("my_timer"):IsPaused()
function Timer:IsPaused()
	return LevelVars.Engine.Timer.timers[self.name].paused
end

--- Get whether or not the timer is active
-- @treturn bool *true* if the timer is active, *false* if it is not active or timer does not exist
-- @usage
--  -- Example:
--  local state = Timer.Get("my_timer"):IsActive()
function Timer:IsActive()
	return LevelVars.Engine.Timer.timers[self.name].active
end

--- Ticks the time of the timer 
-- @treturn bool Tick of timer. Returns *true* every 0.1 seconds with respect to the timer
function Timer:TimerCheck()
	return LevelVars.Engine.Timer.timers[self.name].clock
end

LevelFuncs.Engine.Timer.Decrease = function ()
	for _, t in pairs(LevelVars.Engine.Timer.timers) do
		if t.active and not t.paused then
			if t.first then
				t.first = false
			else
				t.realRemainingTime = t.realRemainingTime - 1
				t.clock = (t.realRemainingTime.c % 10 == 0)
				if t.clock then
					t.remainingTime = t.remainingTime - 3
				end
			end
		end
	end
end

LevelFuncs.Engine.Timer.UpdateAll = function()
	for _, t in pairs(LevelVars.Engine.Timer.timers) do
		if t.active then
			if t.timerFormat then
				local str = TEN.Strings.DisplayString("TIMER", t.pos, t.scale, t.unpausedColor, false, t.stringOption)
				str:SetKey(Utility.GenerateTimeFormattedString(t.remainingTime, t.timerFormat))
				str:SetColor(t.paused and t.pausedColor or t.unpausedColor)
				TEN.Strings.ShowString(str, (t.remainingTime == zero and not t.loop and not string.match(t.name, "__TEN")) and 1 or 1/30)
			end
			if t.remainingTime == zero then
				if t.loop then
					t.realRemainingTime = t.totalTime
					t.remainingTime = t.totalTime
				else
					t.active = false
				end
				t.first = true
				if t.func then
					t.func(table.unpack(t.funcArgs))
				end
			end
		end
	end
end

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRELOOP, LevelFuncs.Engine.Timer.Decrease)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.POSTLOOP, LevelFuncs.Engine.Timer.UpdateAll)

return Timer