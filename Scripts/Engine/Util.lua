-- ldignore

-- Internal functions specific to modules. These are not intended for end users. These functions are not documented in the API reference.

local Util = {}
local Type = require("Engine.Type")
-- For backward compatibility, deciseconds is still accepted, but centiseconds is preferred. Both keys will work, but if both are present, centiseconds will be used.
local VALID_KEYS = { hours = true, minutes = true, seconds = true, deciseconds = true, centiseconds = true }
local LogMessage  = TEN.Util.PrintLog
local logLevelWarning = TEN.Util.LogLevel.WARNING
local IsString = Type.IsString
local IsTable = Type.IsTable
local IsBoolean = Type.IsBoolean

Util.ShortenTENCalls = function()
	print("Util.ShortenTENCalls is deprecated; its functionality is now performed automatically by TombEngine.")
end

local function pad2(n)
    return (n < 10) and ("0" .. n) or tostring(n)
end

-- Check if the time format is correct.
-- Used by: Timer.lua
Util.CheckTimeFormat = function(timerFormat, errorText)
	errorText = errorText and IsString(errorText) and errorText or false
	if IsTable(timerFormat) then
		for k, v in pairs(timerFormat) do
			if not VALID_KEYS[k] or not IsBoolean(v) then
				if errorText then
					LogMessage(errorText, logLevelWarning)
				end
				return false
			end
		end
		return timerFormat
	elseif IsBoolean(timerFormat) then
		return timerFormat and { seconds = true } or timerFormat
	end
	if errorText then
		LogMessage(errorText, logLevelWarning)
	end
	return false
end

-- Generate a formatted string from a time.
-- Used by: Timer.lua
Util.GenerateTimeFormattedString = function(time, timerFormat)
    if not timerFormat then
        return ""
    end

    local h = time.h
	local m = time.m
    local out = ""

    if timerFormat.hours then
        out = pad2(h)
    end

    if timerFormat.minutes then
		local agretedMinutes = timerFormat.hours and m or (m + (60 * h))
		out = (out == "" and pad2(agretedMinutes)) or (out .. ":" .. pad2(agretedMinutes))
	end

    if timerFormat.seconds then
		local aggregatedSeconds = time.s
		if not timerFormat.minutes then
			aggregatedSeconds = aggregatedSeconds + (60 * m)
			if not timerFormat.hours then
				aggregatedSeconds = aggregatedSeconds + (3600 * h)
			end
		end
        out = (out == "" and pad2(aggregatedSeconds)) or (out .. ":" .. pad2(aggregatedSeconds))
    end

	-- Check if timerFormat.deciseconds exists for backward compatibility, but prefer timerFormat.centiseconds if it exists
	-- The visual difference with the previous version is 2 decimal places instead of 1.
	-- Before: 5.0
	-- After: 5.00
	if timerFormat.centiseconds or timerFormat.deciseconds then
        out = (out == "" and pad2(time.c)) or (out .. "." .. pad2(time.c))
    end

    return out
end

-- Check if table has particular value.
-- Used by: Timer.lua
Util.TableHasValue = function(tbl, val)
	for _, value in pairs(tbl) do
		if value == val then
			return true
		end
	end
	return false
end

return Util