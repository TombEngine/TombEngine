-----
--- Basic timer - after a specified number of seconds, the specified thing happens.
--
-- Timers are updated automatically every frame before OnLoop.
--
-- Example usage:
--	local Timer = require("Engine.Timer")
--
--	-- This will be called when the timer runs out
--	LevelFuncs.FinishTimer = function(healthWhenStarted, victoryMessage)
--		-- Open a door, display a message, make an explosion... whatever you wish
--		DoSomething(healthWhenStarted, victoryMessage)
--	end
--	
--	-- This function triggers the timer
--
--	LevelFuncs.TriggerTimer = function(obj) 
--		local myTimer = Timer.Create("my_timer",
--			5.0,
--			false,
--			{minutes = false, seconds = true, deciseconds = true},
--			LevelFuncs.FinishTimer,
--			Lara:GetHP(),
--			"Well done!")
--		myTimer:Start()
--	end
--
-- @luautil Timer

LevelFuncs.Engine.Timer = {}
LevelVars.Engine.Timer = {timers = {}}

local Timer

local unpausedColor = TEN.Color(255, 255, 255)
local pausedColor = TEN.Color(255, 255, 0)
local str = TEN.Strings.DisplayString("TIMER", Vec2 (0, 0), 1, unpausedColor, false, {TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW} )


Timer = {
	--- Create (but do not start) a new timer.
	--
	-- You have the option of displaying the remaining time on the clock. Timer format details:
	--
	--@advancedDesc
	--	-- deciseconds are 1/10th of a second
	--	
	--	-- mins:secs
	--	local myTimeFormat1 = {minutes = true, seconds = true, deciseconds = false}
	--
	--	-- also mins:secs
	--	local myTimeFormat2 = {minutes = true, seconds = true}
	--	
	--	-- secs:decisecs
	--	local myTimeFormat3 = {seconds = true, deciseconds = true}
	--
	--	-- secs; also what is printed if you pass true instead of a table
	--	local myTimeFormat4 = {seconds = true}
	--
	--Use this sparingly; in the classics, timed challenges did not have visible countdowns. For shorter timers, the gameplay benefit from showing the remaining time might not be necessary, and could interfere with the atmosphere of the level.
	--
	--At any given time, only one timer can show its countdown.
	--
	-- @string name A label to give this timer; used to retrieve the timer later. __Do not give your timers a name beginning with __TEN, as this is reserved for timers used by other internal libaries__.
	-- @number totalTime The duration of the timer, in seconds
	-- @bool loop if true, the timer will start again immediately after the time has elapsed
	-- @tparam ?table|bool timerFormat If a table is given, the remaining time will be shown as a string, formatted according to the values in the table. If true, the remaining seconds, rounded up, will show at the bottom of the screen. If false, the remaining time will not be shown on screen. 
	-- @func func The LevelFunc function to call when the time is up
	-- @param[opt] ... a variable number of arguments with which the above function will be called
	-- @treturn Timer The timer in its paused state
	--
	Create = function(name, totalTime, loop, timerFormat, func, ...)
		local obj = {}
		local mt = {}
		mt.__index = Timer
		setmetatable(obj, mt)

		obj.name = name

		if LevelVars.Engine.Timer.timers[name] then
			print("Warning: a timer with name " .. name .. " already exists; overwriting it with a new one...")
		end

		LevelVars.Engine.Timer.timers[name] = {} 
		local thisTimer = LevelVars.Engine.Timer.timers[name]
		thisTimer.name = name
		thisTimer.totalTime = totalTime
		thisTimer.remainingTime = totalTime
		thisTimer.func = func
		thisTimer.funcArgs = {...}
		thisTimer.loop = loop
		thisTimer.active = false
		thisTimer.paused = true
		if type(timerFormat) == "table" then
			thisTimer.timerFormat = timerFormat
		elseif timerFormat then
			thisTimer.timerFormat = {seconds = true}
		end
		return obj
	end;
	
	Delete = function(name)
		if LevelVars.Engine.Timer.timers[name] then
			LevelVars.Engine.Timer.timers[name] = nil
		else
			print("Warning: a timer with name " .. name .. " does not exist and can't be deleted.")
		end
	end;

	--- Get a timer by its name.
	-- @string name The label that was given to the timer when it was created
	-- @treturn Timer The timer
	Get = function(name)
		if LevelVars.Engine.Timer.timers[name] then
			local obj = {}
			local mt = {}
			mt.__index = Timer
			setmetatable(obj, mt)
			obj.name = name
			return obj
		end
		return nil
	end;

	Update = function(t, dt)
		if t.active then
			if not t.paused then
				t.remainingTime = t.remainingTime - dt

				if t.remainingTime <= 0 then
					if not t.loop then
						t.active = false
					else
						t.remainingTime = t.remainingTime + t.totalTime
					end
					
					if (t.func ~= nil) then
						t.func(table.unpack(t.funcArgs))
					end
				end
			end

			if t.timerFormat then
				TEN.Strings.HideString(str)

				local fmt = ""
				local remaining = math.max(t.remainingTime, 0)

				local round = math.floor
				local subSecond = remaining - math.floor(remaining)

				local fmtBefore = false

				-- deciseconds
				if t.timerFormat.deciseconds then
					fmt = math.floor(10*subSecond)
					fmtBefore = true
				end

				-- seconds
				if t.timerFormat.seconds then
					if not fmtBefore then
						round = math.ceil
					else
						round = math.floor
						fmt = ":" .. fmt
					end
					local roundedSeconds = round(remaining)
					local toBeDisplayed = roundedSeconds
					if t.timerFormat.minutes then
						toBeDisplayed = roundedSeconds % 60
					end
					fmt = string.format("%02d", toBeDisplayed) .. fmt

					remaining = roundedSeconds 
					fmtBefore = true
				end

				-- minutes
				if t.timerFormat.minutes then
					if not fmtBefore then
						round = math.ceil
					else
						round = math.floor
						fmt = ":" .. fmt
					end

					local roundedMinutes = round(remaining/60)
					local toBeDisplayed = roundedMinutes

					fmt = string.format("%02d", toBeDisplayed) .. fmt
					fmtBefore = true
				end

				str:SetKey(fmt)
				local myX, myY = PercentToScreen(50, 90)
				str:SetPosition(myX, myY)

				-- Do this again in case the player has loaded while the timer was paused already
				-- Need a better solution for this
				if t.paused then
					str:SetColor(pausedColor)
				end

				TEN.Strings.ShowString(str, 1, false)
			end

		end
	end;

	UpdateAll = function(dt)
		print("Timer.UpdateAll is deprecated; timers and event sequences now get updated automatically pre-control phase.")
	end;

	--- Give the timer a new function and args
	-- @function myTimer:SetFunction
	-- @tparam function func The LevelFunc member to call when the time is up
	-- @param[opt] ... a variable number of arguments with which the above function will be called
	SetFunction = function(t, func, ...)
		local thisTimer = LevelVars.Engine.Timer.timers[t.name]
		thisTimer.func = func
		thisTimer.funcArgs = {...}
	end;

	--- Begin or unpause a timer. If showing the remaining time on-screen, its color will be set to white.
	-- @function myTimer:Start
	Start = function(t)
		local thisTimer = LevelVars.Engine.Timer.timers[t.name]
		if not thisTimer.active then
			thisTimer.active = true
		end

		thisTimer.paused = false

		if thisTimer.timerFormat then
			str:SetColor(unpausedColor)
		end
	end;

	--- Stop the timer.
	-- @function myTimer:Stop
	Stop = function(t)
		LevelVars.Engine.Timer.timers[t.name].active = false
	end;

	--- Get whether or not the timer is active
	-- @function myTimer:IsActive
	-- @treturn bool true if the timer is active, false if otherwise
	IsActive = function(t)
		return LevelVars.Engine.Timer.timers[t.name].active
	end;

	--- Pause or unpause the timer. If showing the remaining time on-screen, its color will be set to yellow (paused) or white (unpaused).
	-- @function myTimer:SetPaused
	-- @bool p if true, the timer will be paused; if false, it would be unpaused 
	SetPaused = function(t, p)
		local thisTimer = LevelVars.Engine.Timer.timers[t.name]
		thisTimer.paused = p
		if thisTimer.timerFormat then
			if p then
				str:SetColor(pausedColor)
			else
				str:SetColor(unpausedColor)
			end
		end
	end;

	--- Get whether or not the timer is paused
	-- @function myTimer:IsPaused
	-- @treturn bool true if the timer is paused, false if otherwise
	IsPaused = function(t)
		return LevelVars.Engine.Timer.timers[t.name].paused
	end;

	--- Get the remaining time for a timer.
	-- @function myTimer:GetRemainingTime
	-- @treturn float the time in seconds remaining on the clock
	GetRemainingTime = function(t)
		return LevelVars.Engine.Timer.timers[t.name].remainingTime
	end;

	--- Set the remaining time for a timer
	-- @function myTimer:SetRemainingTime
	-- @number remainingTime the new time remaining for the timer
	SetRemainingTime = function(t, remainingTime)
		LevelVars.Engine.Timer.timers[t.name].remainingTime = remainingTime
	end;

	--- Get the total time for a timer.
	-- This is the amount of time the timer will start with, as well as when starting a new loop
	-- @function myTimer:GetTotalTime
	-- @treturn float the timer's total time
	GetTotalTime = function(t)
		return LevelVars.Engine.Timer.timers[t.name].totalTime
	end;

	--- Set the total time for a timer
	-- @function myTimer:SetTotalTime
	-- @number totalTime timer's new total time
	SetTotalTime = function(t, totalTime)
		LevelVars.Engine.Timer.timers[t.name].totalTime = totalTime
	end;

	--- Set whether or not the timer loops 
	-- @function myTimer:SetLooping
	-- @bool looping whether or not the timer loops
	SetLooping = function(t, looping)
		LevelVars.Engine.Timer.timers[t.name].loop = looping
	end;
}

LevelFuncs.Engine.Timer.UpdateAll = function(dt) 
	for _, t in pairs(LevelVars.Engine.Timer.timers) do
		Timer.Update(t, dt)
	end
end

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRELOOP, LevelFuncs.Engine.Timer.UpdateAll)

return Timer

