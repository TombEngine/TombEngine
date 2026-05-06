--- A chain of functions to call at specified times, modeled after TRNG's organizers.
--
-- Works atop the Timer, and so is updated automatically pre-OnControlPhase, and saved automatically when the game is saved.
-- The sequence can be paused, unpaused, stopped, and started, and can be set to loop.
--
-- To use EventSequence inside scripts you need to call the module:
--	local EventSequence = require("Engine.EventSequence")
--
-- Example usage:
--	local EventSequence = require("Engine.EventSequence")
--
--	-- These will be called by the sequence
--	LevelFuncs.HealLara = function()
--		Lara:SetHP(Lara:GetHP()+10)
--	end
--	
--	local nSpawned = 0
--	LevelFuncs.SpawnBaddy = function(baddy, name, pos)
--		local myBaddy = TEN.Objects.Moveable(baddy, name..nSpawned, pos, nil, 0)
--		myBaddy:Enable()
--		nSpawned = nSpawned + 1
--	end
--
--	-- This function triggers the sequence
--	LevelFuncs.TriggerSequence = function(obj) 
--		local posSteve = TEN.Objects.GetMoveableByName("stevePosNullmesh"):GetPosition()
--		local posChris = TEN.Objects.GetMoveableByName("chrisPosNullmesh"):GetPosition()
--		if not EventSequence.IfExists("my_seq") then
--			EventSequence.Create("my_seq",
--				false, -- does not loop
--				{seconds = true, centiseconds = true}, -- timer format, see Timer for details
--				6, -- seconds until call the function specified in next arg 
--				LevelFuncs.HealLara, -- first function to call. If we don't need to pass any arguments, we can just pass the function
--				2.1, -- seconds until the next function, after the previous one has been called
--				{LevelFuncs.SpawnBaddy, TEN.Objects.ObjID.BADDY1, "steve", posSteve}, -- if we DO want to pass arguments to the function to be called, we give a table with the function (LevelFuncs.SpawnBaddy in this case) followed by the args to pass to it
--				0.5,
--				{LevelFuncs.SpawnBaddy, TEN.Objects.ObjID.SAS_CAIRO, "chris", posChris},
--				1,
--				LevelFuncs.HealLara)
--		end
--
--		-- event sequences are inactive to begin with and so need to be started
--		EventSequence.Get("my_seq"):Start()
--	end
--
-- @luautil EventSequence

local Timer = require("Engine.Timer")
local Type = require("Engine.Type")
local Utility = require("Engine.Util")

local EventSequence = {}
EventSequence.__index = EventSequence

LevelFuncs.Engine.EventSequence = {}
LevelVars.Engine.EventSequence = {sequences = {}}

LevelFuncs.Engine.EventSequence.CallNext = function(sequenceName, nextTimerName, func, ...)
	local thisES = LevelVars.Engine.EventSequence.sequences[sequenceName]
	if not thisES then
		return
	end
	func(...)

	-- Callback may stop or delete the sequence; re-read state and bail out safely.
	thisES = LevelVars.Engine.EventSequence.sequences[sequenceName]
	if not thisES then
		return
	end

	if thisES.stopRequested then
		thisES.stopRequested = false
		return
	end

	thisES.currentTimer = thisES.currentTimer + 1
	if thisES.currentTimer <= #thisES.timers then
		if Timer.IfExists(nextTimerName) then
			local theTimer = Timer.Get(nextTimerName)
			theTimer:Start(true)
		end
	elseif thisES.loop then
		if Timer.IfExists(thisES.firstTimerName) then
			local theTimer = Timer.Get(thisES.firstTimerName)
			theTimer:Start(true)
		end
		thisES.currentTimer = 1
	else
		thisES.currentTimer = 1
	end
end

--- Create (but do not start) a new event sequence.
--
-- @tparam string name A label to give the sequence; used to retrieve the timer later as well as internally by TEN.
-- @tparam bool loop If `true`, the sequence will start again from its first timer once its final function has been called.
-- @tparam ?table|bool timerFormat Same as in <a href="Timer.html#timerFormat">Timer format</a> for Timer. This is mainly for debugging. __This will not work properly if another sequence or timer is showing a countdown.__
-- @tparam float|LevelFuncs|table ... A variable number of pairs of arguments, each pair consisting of:<br>
-- - a time in seconds that can be rounded internally to the nearest game frame (1/30 of a second),<br>
-- - followed by the function defined in the *LevelFuncs* table to call once the time has elapsed,<br>
-- - followed by another duration in seconds, another function name, etc.
--
-- You can specify a function either by its name, or by a *table* __{ }__ with the function name as the first member, followed by its arguments (see example).
-- @treturn[1] EventSequence The inactive sequence.
-- @treturn[2] nil If there was an error creating the sequence.
-- @usage
-- local EventSequence = require("Engine.EventSequence")
-- local TimerFormat = {seconds = true, centiseconds = true}
--
-- -- Example 1 function without arguments:
-- -- This creates a sequence that calls LevelFuncs.Func after 2 seconds
-- -- then LevelFuncs.Func after 3 seconds
-- -- and finally LevelFuncs.Func after 4 seconds
-- LevelFuncs.Func = function ()
--    local pos = TEN.Vec2(TEN.Util.PercentToScreen(50, 10))
--    local str = TEN.Strings.DisplayString("Repeated function without arguments", pos, 1)
--    TEN.Strings.ShowString(str, 1)
-- end
-- EventSequence.Create(
--    "test1", -- sequence's name
--    true, -- loop
--    TimerFormat, -- timer format
--    2.0,  -- seconds until call the function
--    LevelFuncs.Func, -- first function to call
--    3.0, 
--    LevelFuncs.Func,
--    4.0,
--    LevelFuncs.Func)
--
-- -- Example 2 function with arguments:
-- -- This creates a sequence that calls LevelFuncs.Func2("1", 5, 10) after 2.3 seconds
-- -- then LevelFuncs.Func2("2", 5, 15) after 3.1 seconds
-- -- and finally LevelFuncs.Func2("3", 5, 20) after 4.8 seconds
-- LevelFuncs.Func2 = function (text, x, y)
--    local pos = TEN.Vec2(TEN.Util.PercentToScreen(x, y))
--    local str = TEN.Strings.DisplayString("Function " .. text .. "!", pos, 1)
--    TEN.Strings.ShowString(str, 1)
-- end
-- EventSequence.Create(
--    "test2", -- sequence's name
--    true, -- loop
--    false, -- no countdown is displayed
--    2.3,
--    {LevelFuncs.Func2, "1", 5, 10},
--    3.1,
--    {LevelFuncs.Func2, "2", 5, 15},
--    4.8,
--    {LevelFuncs.Func2, "3", 5, 20})
EventSequence.Create = function (name, loop, timerFormat, ...)
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in EventSequence.Create(): invalid name, sequence was not created", TEN.Util.LogLevel.ERROR)
		return
	end
	local self = {name = name}
	if LevelVars.Engine.EventSequence.sequences[name] then
		TEN.Util.PrintLog("Warning in EventSequence.Create(): an EventSequence with name '" .. name .. "' already exists; overwriting it with a new one...", TEN.Util.LogLevel.WARNING)
	end
	LevelVars.Engine.EventSequence.sequences[name] = {}
	local thisES = LevelVars.Engine.EventSequence.sequences[name]
	thisES.name = name
	thisES.timers = {}
	thisES.currentTimer = 1
	thisES.stopRequested = false

	if not Type.IsBoolean(loop) then
		TEN.Util.PrintLog("Warning in EventSequence.Create(): wrong value for loop, loop for '".. name .."' sequence will be set to false", TEN.Util.LogLevel.WARNING)
		loop = false
	end
	thisES.loop = loop

	local errorCheckFormat = "Warning in EventSequence.Create(): wrong value for timerFormat, timerFormat for '".. name .."' sequence will be set to false"
	thisES.timerFormat = Utility.CheckTimeFormat(timerFormat, errorCheckFormat)

	thisES.timesFuncsAndArgs = {...}
	local tfa = thisES.timesFuncsAndArgs

	if #tfa == 0 then
		TEN.Util.PrintLog("Error in EventSequence.Create(): no timers/functions specified, '".. name .."' sequence was not created", TEN.Util.LogLevel.ERROR)
		LevelVars.Engine.EventSequence.sequences[name] = nil
		return
	end
	if #tfa % 2 ~= 0 then
		TEN.Util.PrintLog("Error in EventSequence.Create(): odd number of variable arguments, each time value must be followed by a function, '".. name .."' sequence was not created", TEN.Util.LogLevel.ERROR)
		LevelVars.Engine.EventSequence.sequences[name] = nil
		return
	end
	for i = 1, #tfa, 2 do
		local time = tfa[i]
		local funcAndArgs = tfa[i+1]
		local error = false
		if not Type.IsNumber(time) or time < 0 then
			TEN.Util.PrintLog("Error in EventSequence.Create(): wrong value for seconds, '".. name .."' sequence was not created", TEN.Util.LogLevel.ERROR)
			error = true
		end
		if not (Type.IsLevelFunc(funcAndArgs) or (Type.IsTable(funcAndArgs) and Type.IsLevelFunc(funcAndArgs[1]))) then
			TEN.Util.PrintLog("Error in EventSequence.Create(): wrong value for function arguments, used function must be inside LevelFuncs. '".. name .."' sequence was not created", TEN.Util.LogLevel.ERROR)
			error = true
		end
		if error then
			for z = 1, #LevelVars.Engine.EventSequence.sequences[name].timers do
				Timer.Delete(LevelVars.Engine.EventSequence.sequences[name].timers[z])
			end
			LevelVars.Engine.EventSequence.sequences[name] = nil
			return
		end

		local timerIndex = #thisES.timers + 1
		local timerName = "__TEN_eventSequence_" .. name .. "_timer" .. timerIndex
		local nextTimerName = "__TEN_eventSequence_" .. name .. "_timer" .. (timerIndex + 1)

		local func
		local args = {}
		if i == 1 then
			thisES.firstTimerName = timerName
		end
		if Type.IsTable(funcAndArgs) then
			local n = #funcAndArgs
			func = funcAndArgs[1]
			if n > 1 then
				table.move(funcAndArgs, 2, n, 1, args) -- Copy arguments.
			end
		else
			func = funcAndArgs
		end
		Timer.Create(timerName,
				time,
				false,
				thisES.timerFormat,
				LevelFuncs.Engine.EventSequence.CallNext,
				name,
				nextTimerName,
				func,
				table.unpack(args)
				)
		thisES.timers[timerIndex] = timerName
	end
	return setmetatable(self, EventSequence)
end

--- Get an event sequence by its name.
-- @tparam string name The label that was given to the sequence when it was created.
-- @treturn[1] EventSequence The sequence if it exists.
-- @treturn[2] nil If the sequence does not exist.
-- @usage
-- -- Example:
-- EventSequence.Get("my_seq")
EventSequence.Get = function(name)
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in EventSequence.Get(): invalid name", TEN.Util.LogLevel.ERROR)
		return nil
	end

	local thisES = LevelVars.Engine.EventSequence.sequences[name]
	if not thisES then
		TEN.Util.PrintLog("Warning in EventSequence.Get(): sequence with name '".. name .."' not found", TEN.Util.LogLevel.WARNING)
		return nil
	end
	return setmetatable({name = name}, EventSequence)
end

--- Check if an event sequence exists.
-- @tparam string name The label that was given to the event sequence when it was created.
-- @treturn bool `true` if the event sequence exists, `false` if it does not exist.
-- @usage
-- -- Example:
-- -- This function checks if an event sequence named "my_seq" exists and starts it
-- LevelFuncs.CheckAndStart = function()
--    if EventSequence.IfExists("my_seq") then
--       EventSequence.Get("my_seq"):Start()
--    end
-- end
EventSequence.IfExists = function (name)
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in EventSequence.IfExists(): invalid name", TEN.Util.LogLevel.ERROR)
		return false
	end
	return LevelVars.Engine.EventSequence.sequences[name] and true or false
end

--- Delete an event sequence.
-- @tparam string name The label that was given to the event sequence when it was created.
-- @usage
-- -- Example:
-- EventSequence.Delete("my_seq")
EventSequence.Delete = function (name)
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in EventSequence.Delete(): invalid name", TEN.Util.LogLevel.ERROR)
		return
	elseif not LevelVars.Engine.EventSequence.sequences[name] then
		TEN.Util.PrintLog("Error in EventSequence.Delete(): sequence with name '".. name .."' sequence does not exist", TEN.Util.LogLevel.ERROR)
	else
		for i = 1, #LevelVars.Engine.EventSequence.sequences[name].timers do
			Timer.Delete(LevelVars.Engine.EventSequence.sequences[name].timers[i])
		end
		LevelVars.Engine.EventSequence.sequences[name] = nil
	end
end

----
-- List of all methods of the EventSequence object. It is always recommended to check the existence of a sequence with the *EventSequence.IfExists()* function before using methods, to avoid errors or unexpected behavior. When calling a method, it is recommended to use the *EventSequence.Get()* function, to avoid errors after loading a save game.
-- @type EventSequence
-- @usage
-- -- Examples of some methods
-- EventSequence.Get("my_seq"):Start()
-- EventSequence.Get("my_seq"):Stop()
-- EventSequence.Get("my_seq"):SetPaused(true)
--
-- -- check if sequence exists before using methods
-- if EventSequence.IfExists("my_seq") then
--    EventSequence.Get("my_seq"):Start()
-- end

--- Begin or unpause a sequence. If showing the remaining time on-screen, its color will be set to white.
-- @usage
-- -- Example:
-- if EventSequence.IfExists("my_seq") then
--    EventSequence.Get("my_seq"):Start()
-- end
function EventSequence:Start()
	local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
	thisES.stopRequested = false -- Clear stale stop requests before a new run.
	Timer.Get(thisES.timers[thisES.currentTimer]):Start()
end

--- Restart the sequence from its first timer. If showing the remaining time on-screen, its color will be set to white.
-- @usage
-- -- Example:
-- if EventSequence.IfExists("my_seq") then
--    EventSequence.Get("my_seq"):Restart()
-- end
function EventSequence:Restart()
	local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
	-- Ensure a clean restart by stopping every timer in this sequence.
	for i = 1, #thisES.timers do
		local timerName = thisES.timers[i]
		if Timer.IfExists(timerName) then
			Timer.Get(timerName):Stop()
		end
	end
	thisES.currentTimer = 1
	thisES.stopRequested = false -- clear stale stop requests before a new run
	Timer.Get(thisES.timers[thisES.currentTimer]):Start(true)
end

--- Pause or unpause the sequence. If showing the remaining time on-screen, its color will be set to yellow (paused) or white (unpaused).
-- @tparam bool p If `true`, the sequence will be paused; if `false`, it will be unpaused.
-- @usage
-- -- Example 1: Pause the sequence
-- if EventSequence.IfExists("my_seq") then
--    EventSequence.Get("my_seq"):SetPaused(true)
-- end
--
-- -- Example 2: Unpause the sequence
-- if EventSequence.IfExists("my_seq") then
--    EventSequence.Get("my_seq"):SetPaused(false)
-- end
function EventSequence:SetPaused(p)
	if not Type.IsBoolean(p) then
		TEN.Util.PrintLog("Error in EventSequence:SetPaused(): invalid value for p", TEN.Util.LogLevel.ERROR)
	else
		local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
		Timer.Get(thisES.timers[thisES.currentTimer]):SetPaused(p)
	end
end

--- Stop and reset the sequence to the first element.
-- @usage
-- -- Example:
-- EventSequence.Get("my_seq"):Stop()
function EventSequence:Stop()
	local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
	thisES.stopRequested = true
	for i = 1, #thisES.timers do
		local timerName = thisES.timers[i]
		if Timer.IfExists(timerName) then
			Timer.Get(timerName):Stop()
		end
	end
	thisES.currentTimer = 1
	local timer = Timer.Get(thisES.timers[thisES.currentTimer])
	timer:SetRemainingTime(timer:GetTotalTimeInSeconds())
end

--- Returns whether the sequence is paused.
-- @treturn bool `true` If the timer is paused, `false` if otherwise.
-- @usage
-- -- Example 1: paused sequence
-- if not EventSequence.Get("my_seq"):IsPaused() then
--    EventSequence.Get("my_seq"):SetPaused(true)
-- end
--
-- -- Example 2: unpause the sequence
-- if EventSequence.Get("my_seq"):IsPaused() then
--    EventSequence.Get("my_seq"):SetPaused(false)
-- end
function EventSequence:IsPaused()
	local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
	return Timer.Get(thisES.timers[thisES.currentTimer]):IsPaused()
end

--- Returns whether the sequence is active.
-- @treturn bool `true` If the sequence is active, `false` if otherwise.
-- @usage
-- -- Example:
-- if not EventSequence.Get("my_seq"):IsActive() then
--    EventSequence.Get("my_seq"):Start()
-- end
function EventSequence:IsActive()
	local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
	return Timer.Get(thisES.timers[thisES.currentTimer]):IsActive()
end

return EventSequence
