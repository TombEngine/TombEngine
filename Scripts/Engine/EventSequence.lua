-----
--- Event sequence - a chain of functions to call at specified times, modeled after TRNG's organizers.
--
-- Works atop the Timer, and so is updated automatically pre-OnControlPhase, and saved automatically when the game is saved.
-- The sequence can be paused, unpaused, stopped, and started, and can be set to loop.
--
--
-- To use EventSequence inside scripts you need to call the module:
--	local EventSequence = require("Engine.EventSequence")
--
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
--				{seconds = true, deciseconds = true}, -- timer format, see Timer for details
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
local Type= require("Engine.Type")
local Utility = require("Engine.Util")

local EventSequence = {}
EventSequence.__index = EventSequence

LevelFuncs.Engine.EventSequence = {}
LevelVars.Engine.EventSequence = {sequences = {}}

LevelFuncs.Engine.EventSequence.CallNext = function(sequenceName, nextTimerName, func, ...)
	local thisES = LevelVars.Engine.EventSequence.sequences[sequenceName]
	func(...)

	thisES.currentTimer = thisES.currentTimer + 1
	if thisES.currentTimer <= #thisES.timers then
		local theTimer = Timer.Get(nextTimerName)
		theTimer:Start(true)
	elseif thisES.loop then
		local theTimer = Timer.Get(thisES.firstTimerName)
		theTimer:Start(true)
		thisES.currentTimer = 1
	else
		thisES.currentTimer = 1
	end
end

--- Create (but do not start) a new event sequence.
--
-- @tparam string name A label to give the sequence; used to retrieve the timer later as well as internally by TEN.
-- @tparam bool loop if true, the sequence will start again from its first timer once its final function has been called
-- @tparam ?table|bool timerFormat Same as in @{Timer.Create}. This is mainly for debugging. __This will not work properly if another sequence or timer is showing a countdown.__
-- @tparam ?float|LevelFuncs|table ... A variable number of pairs of arguments, each pair consisting of:
--
-- a time in seconds (positive values are accepted and with only 1 tenth of a second [__0.1__]),
--
-- followed by the function defined in the *LevelFuncs* table to call once the time has elapsed,
--
-- followed by another duration in seconds, another function name, etc.
--
-- You can specify a function either by its name, or by a *table* __{}__ with the function name as the first member, followed by its arguments (see example).
-- @treturn EventSequence The inactive sequence.
-- @usage
--	local EventSequence = require("Engine.EventSequence")
--	local TimerFormat = {seconds = true, deciseconds = true}
--
--	-- Example 1 function without arguments:
--	-- This creates a sequence that calls LevelFuncs.Func after 2 seconds
--	-- then LevelFuncs.Func after 3 seconds
--	-- and finally LevelFuncs.Func after 4 seconds
--	LevelFuncs.Func = function ()
--		local pos = TEN.Vec2(TEN.Util.PercentToScreen(50, 10))
--		local str = TEN.Strings.DisplayString("Repeated function without arguments", pos, 1)
--		TEN.Strings.ShowString(str, 1)
--	end
--	EventSequence.Create(
--	    "test1",true,TimerFormat,
--	    2.0,
--	    LevelFuncs.Func,
--	    3.0,
--	    LevelFuncs.Func,
--	    4.0,
--	    LevelFuncs.Func)
--
--	-- Example 2 function with arguments:
--	-- This creates a sequence that calls LevelFuncs.Func2("1", 5, 10) after 2.3 seconds
--	-- then LevelFuncs.Func2("2", 5, 15) after 3.1 seconds
--	-- and finally LevelFuncs.Func2("3", 5, 20) after 4.8 seconds
--	LevelFuncs.Func2 = function (text, x, y)
--		local pos = TEN.Vec2(TEN.Util.PercentToScreen(x, y))
--		local str = TEN.Strings.DisplayString("Function " .. text .. "!", pos, 1)
--		TEN.Strings.ShowString(str, 1)
--	end
--	EventSequence.Create(
--	    "test2",true,false,
--	    2.3,
--	    {LevelFuncs.Func2, "1", 5, 10},
--	    3.1,
--	    {LevelFuncs.Func2, "2", 5, 15},
--	    4.8,
--	    {LevelFuncs.Func2, "3", 5, 20})
EventSequence.Create =function (name, loop, timerFormat, ...)
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

	if not Type.IsBoolean(loop) then
		TEN.Util.PrintLog("Warning in EventSequence.Create(): wrong value for loop, loop for '".. name .."' sequence will be set to false", TEN.Util.LogLevel.WARNING)
		loop = false
	end
	thisES.loop = loop

	local errorCheckFormat = "Warning in EventSequence.Create(): wrong value for timerFormat, timerFormat for '".. name .."' sequence will be set to false"
	thisES.timerFormat = Utility.CheckTimeFormat(timerFormat, errorCheckFormat)

	thisES.timesFuncsAndArgs = {...}
	local tfa = thisES.timesFuncsAndArgs

	for i = 1, #tfa, 2 do
		local time = tfa[i]
		local funcAndArgs = tfa[i+1]
		local error = false
		if not Type.IsNumber(time) or time < 0 then
			TEN.Util.PrintLog("Error in EventSequence.Create(): wrong value for seconds, '".. name .."' sequence was not created", TEN.Util.LogLevel.ERROR)
			error = true
		end
		if not (Type.IsLevelFunc(funcAndArgs) or (Type.IsTable(funcAndArgs) and Type.IsLevelFunc(funcAndArgs[1]))) then
			TEN.Util.PrintLog("Error in EventSequence.Create(): wrong value for function, '".. name .."' sequence was not created", TEN.Util.LogLevel.ERROR)
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
		local nextTimerName = "__TEN_eventSequence_" .. name .. "_timer" .. timerIndex + 1

		local func, args
		if i == 1 then
			thisES.firstTimerName = timerName
		end
		if Type.IsTable(funcAndArgs) then
			func = table.remove(funcAndArgs, 1)
			args = funcAndArgs
		else
			func = funcAndArgs
			args = {}
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
-- @tparam string name The label that was given to the sequence when it was created
-- @treturn EventSequence The sequence
EventSequence.Get = function(name)
	local self = {}
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in EventSequence.Get(): invalid name", TEN.Util.LogLevel.ERROR)
		self = {name = "noError", errorName = name}
	elseif not LevelVars.Engine.EventSequence.sequences[name] then
		TEN.Util.PrintLog("Warning in EventSequence.Get(): sequence with name '".. name .."' sequence not found", TEN.Util.LogLevel.WARNING)
		self = {name = "noError", errorName = name}
	else
		self = {name = name}
	end
	return setmetatable(self, EventSequence)
end

--- Check if an event sequence exists.
-- @tparam string name The label that was given to the event sequence when it was created
-- @usage
--	-- Example:
--	-- This function checks if an event sequence named "my_seq" exists and starts it
--	LevelFuncs.CheckAndStart = function()
--      if EventSequence.IfExists("my_seq") then
--          EventSequence.Get("my_seq"):Start()
--      end
--	end
EventSequence.IfExists = function (name)
	if not Type.IsString(name) then
		TEN.Util.PrintLog("Error in EventSequence.IfExists(): invalid name", TEN.Util.LogLevel.ERROR)
		return false
	end
	return LevelVars.Engine.EventSequence.sequences[name] and true or false
end

--- Delete an event sequence.
-- @tparam string name The label that was given to the event sequence when it was created
-- @usage
--	-- Example:
--  EventSequence.Delete("my_seq")
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
-- The list of all methods of the EventSequence object. We suggest that you always use the EventSequence.Get() function to use the methods of the EventSequence object to prevent errors or unexpected behavior
-- @type EventSequence
-- @usage
--	-- Examples of some methods
--	EventSequence.Get("my_seq"):Start()
--	EventSequence.Get("my_seq"):Stop()
--	EventSequence.Get("my_seq"):SetPaused(true)

--- Begin or unpause a sequence. If showing the remaining time on-screen, its color will be set to white.
-- @usage
--	-- Example:
--	EventSequence.Get("my_seq"):Start()
function EventSequence:Start()
	if self.errorName then
		TEN.Util.PrintLog("Error in EventSequence:Start(): '" .. self.errorName .. "' sequence does not exist", TEN.Util.LogLevel.ERROR)
	else
		local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
		Timer.Get(thisES.timers[thisES.currentTimer]):Start(true)
	end
end

--- Pause or unpause the sequence. If showing the remaining time on-screen, its color will be set to yellow (paused) or white (unpaused).
-- @tparam bool p If true, the sequence will be paused; if false, it will be unpaused
-- @usage
--	-- Example 1: Pause the sequence
--	EventSequence.Get("my_seq"):SetPaused(true)
--
--	-- Example 2: Unpause the sequence
--	EventSequence.Get("my_seq"):SetPaused(false)
function EventSequence:SetPaused(p)
	if self.errorName then
		TEN.Util.PrintLog("Error in EventSequence:SetPaused(): '" .. self.errorName .. "' sequence does not exist", TEN.Util.LogLevel.ERROR)
		return
	end
	if not Type.IsBoolean(p) then
		TEN.Util.PrintLog("Error in EventSequence:SetPaused(): invalid value for p", TEN.Util.LogLevel.ERROR)
	else
		local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
		Timer.Get(thisES.timers[thisES.currentTimer]):SetPaused(p)
	end
end

--- Stop the sequence.
-- @usage
--	-- Example:
--	EventSequence.Get("my_seq"):Stop()
function EventSequence:Stop()
	if self.errorName then
		TEN.Util.PrintLog("Error in EventSequence:Stop(): '" .. self.errorName .. "' sequence does not exist", TEN.Util.LogLevel.ERROR)
	else
		local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
		Timer.Get(thisES.timers[thisES.currentTimer]):Stop()
	end
end

--- Get whether or not the sequence is paused
-- @treturn bool true If the timer is paused, false if otherwise
-- @usage
--	-- Example 1: paused sequence
--	if not EventSequence.Get("my_seq"):IsPaused() then
--		EventSequence.Get("my_seq"):SetPaused(true)
--	end
--
--	-- Example 2: unpause the sequence
--	if EventSequence.Get("my_seq"):IsPaused() then
--		EventSequence.Get("my_seq"):SetPaused(false)
--	end
function EventSequence:IsPaused()
	if self.errorName then
		TEN.Util.PrintLog("Error in EventSequence:IsPaused(): '" .. self.errorName .. "' sequence does not exist", TEN.Util.LogLevel.ERROR)
		return false
	end
	local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
	return Timer.Get(thisES.timers[thisES.currentTimer]):IsPaused()
end

--- Get whether or not the sequence is active
-- @treturn bool true If the sequence is active, false if otherwise
-- @usage
--	-- Example:
--	if not EventSequence.Get("my_seq"):IsActive() then
--		EventSequence.Get("my_seq"):Start()
--	end
function EventSequence:IsActive()
	if self.errorName then
		TEN.Util.PrintLog("Error in EventSequence:IsActive(): '" .. self.errorName .. "' sequence does not exist", TEN.Util.LogLevel.ERROR)
		return false
	end
	local thisES = LevelVars.Engine.EventSequence.sequences[self.name]
	return Timer.Get(thisES.timers[thisES.currentTimer]):IsActive()
end

return EventSequence