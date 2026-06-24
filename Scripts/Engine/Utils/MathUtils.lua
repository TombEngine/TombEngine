-----<style>table.function_list td.name {min-width: 291px;}</style>
--- Lua support functions for mathematical operations and rounding.
---
--- **Design Philosophy:**
--- MathUtils is designed primarily for:
--- 
--- - Writing Lua modules and scripts
--- - Simplifying Node creation in TombEditor's Node Editor
--- - Providing safe, predictable helper functions
---
--- **Type Checking:**
--- All functions perform runtime type validation.
--- This ensures:
--- 
--- - Early error detection during development
--- - Predictable results when users make mistakes
---
--- To use, include the module with:
---
---	local MathUtils = require("Engine.MathUtils")
-- @luautil MathUtils

local MathUtils = {}
local Type = require("Engine.Type")
local Util = require("Engine.Util")

local logLevelError  = TEN.Util.LogLevel.ERROR
local logLevelWarning  = TEN.Util.LogLevel.WARNING
local Round = Util.Round
local WrapAngleRaw = Util.WrapAngleRaw
local IsNumber = Type.IsNumber
local IsVec2 = Type.IsVec2
local IsVec3 = Type.IsVec3
local IsColor = Type.IsColor
local IsTime = Type.IsTime
local IsRotation = Type.IsRotation
local floor = math.floor
local max = math.max
local min = math.min
local random = math.random
local randomseed = math.randomseed
local Vec2 = TEN.Vec2
local Vec3 = TEN.Vec3
local Rotation = TEN.Rotation
local Color = TEN.Color
local Time = TEN.Time
local LogMessage  = TEN.Util.PrintLog
local errorMessageMax = "Error in MathUtils.Max: all arguments must be the same type."
local errorMessageMin = "Error in MathUtils.Min: all arguments must be the same type."
local errorMessageRandom = "Error in MathUtils.Random: minValue and maxValue must be the same type."
local errorMessageBase = "Error in MathUtils.Clamp: value, minValue, and maxValue must be the same type."

--- Checks if a value is an integer (a number without fractional part).
-- @tparam number n The value to check
-- @treturn[1] boolean: true if the value is an integer, false otherwise
-- @treturn[2] boolean: false if the input is not a number
-- @usage
-- MathUtils.IsInteger(10)      -- true
-- MathUtils.IsInteger(10.0)    -- true
-- MathUtils.IsInteger(10.5)    -- false
-- MathUtils.IsInteger(-5)      -- true
-- MathUtils.IsInteger("10")    -- false
-- MathUtils.IsInteger(nil)     -- false
MathUtils.IsInteger = function(n)
    if not IsNumber(n) then
        LogMessage("Error in MathUtils.IsInteger: parameter must be a number.", logLevelError)
        return false
    end
    return (n % 1) == 0
end

--- Get the minimum value between multiple arguments (supports numbers, Vec2, Vec3, Time).
-- For Vec2/Vec3, returns component-wise minimum. Values to compare (at least 2)
-- @tparam number|Vec2|Vec3|Time a
-- @tparam number|Vec2|Vec3|Time b (same type as a)
-- @tparam number|Vec2|Vec3|Time ... (same type as a and b)
-- @treturn[1] number|Vec2|Vec3|Time The minimum value.
-- @treturn[2] number|Vec2|Vec3|Time the value of `*a*` if an error occurs (type mismatch or unsupported type), with an error message.
-- @usage
-- local min = MathUtils.Min(5, 10, 3) -- Result: 3
-- local minVec = MathUtils.Min(TEN.Vec3(1, 5, 3), TEN.Vec3(2, 4, 6)) -- Result: Vec3(1, 4, 3)
-- local minTime = MathUtils.Min(TEN.Time(30), TEN.Time(60)) -- Result: Time(30)
--
-- -- Error handling example:
-- local min = MathUtils.Min(5, "10", 3) -- Result: 5 (type mismatch, error logged)
-- if min == nil then
--     TEN.Util.PrintLog("Failed to compute minimum value", TEN.Util.LogLevel.ERROR)
--     return  -- or use a fallback value
-- end
--
-- -- Safe approach with default fallback:
-- -- with numbers
-- local min = MathUtils.Min(value1, value2, value3) or 0
-- -- with Vec2
-- local minVec2 = MathUtils.Min(vec2_1, vec2_2) or TEN.Vec2(0, 0)
-- -- with Vec3
-- local minVec = MathUtils.Min(vec1, vec2, vec3) or TEN.Vec3(0, 0, 0)
-- -- with Time
-- local minTime = MathUtils.Min(time1, time2) or TEN.Time(0)
MathUtils.Min = function(a, b, ...)
    -- Fast path: exactly 2 arguments (most common case)
    local extraCount = select("#", ...)

    if IsNumber(a) then
        if not IsNumber(b) then
            LogMessage(errorMessageMin, logLevelError)
            return a
        end
        local minVal = a < b and a or b
        for i = 1, extraCount do
            local v = select(i, ...)
            if not IsNumber(v) then
                LogMessage(errorMessageMin, logLevelError)
                return a
            end
            if v < minVal then minVal = v end
        end
        return minVal

    elseif IsVec2(a) then
        if not IsVec2(b) then
            LogMessage(errorMessageMin, logLevelError)
            return a
        end
        local rx, ry = min(a.x, b.x), min(a.y, b.y)
        for i = 1, extraCount do
            local v = select(i, ...)
            if not IsVec2(v) then
                LogMessage(errorMessageMin, logLevelError)
                return a
            end
            rx = min(rx, v.x)
            ry = min(ry, v.y)
        end
        return Vec2(rx, ry)

    elseif IsVec3(a) then
        if not IsVec3(b) then
            LogMessage(errorMessageMin, logLevelError)
            return a
        end
        local rx, ry, rz = min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)
        for i = 1, extraCount do
            local v = select(i, ...)
            if not IsVec3(v) then
                LogMessage(errorMessageMin, logLevelError)
                return a
            end
            rx = min(rx, v.x)
            ry = min(ry, v.y)
            rz = min(rz, v.z)
        end
        return Vec3(rx, ry, rz)

    elseif IsTime(a) then
        if not IsTime(b) then
            LogMessage(errorMessageMin, logLevelError)
            return a
        end
        local minTime = a:GetFrameCount() < b:GetFrameCount() and a or b
        for i = 1, extraCount do
            local v = select(i, ...)
            if not IsTime(v) then
                LogMessage(errorMessageMin, logLevelError)
                return a
            end
            if v:GetFrameCount() < minTime:GetFrameCount() then minTime = v end
        end
        return minTime
    end

    LogMessage("Error in MathUtils.Min: unsupported type.", logLevelError)
    return a
end

--- Get the maximum value between multiple arguments (supports numbers, Vec2, Vec3, Time).
-- For Vec2/Vec3, returns component-wise maximum. Values to compare (at least 2)
-- @tparam number|Vec2|Vec3|Time a
-- @tparam number|Vec2|Vec3|Time b (same type as a)
-- @tparam number|Vec2|Vec3|Time ... (same type as a and b)
-- @treturn[1] number|Vec2|Vec3|Time The maximum value.
-- @treturn[2] number|Vec2|Vec3|Time the value of `*a*` if an error occurs (type mismatch or unsupported type), with an error message
-- @usage
-- local max = MathUtils.Max(5, 10, 3) -- Result: 10
-- local maxVec = MathUtils.Max(TEN.Vec3(1, 5, 3), TEN.Vec3(2, 4, 6)) -- Result: Vec3(2, 5, 6)
-- local maxTime = MathUtils.Max(TEN.Time(30), TEN.Time(60)) -- Result: Time(60)
--
-- -- Error handling example:
-- local max = MathUtils.Max(5, "10", 3) -- Result: 5 (type mismatch, error logged)
-- if max == nil then
--     -- Handle error
-- end
--
-- -- Safe approach with default fallback:
-- -- with numbers
-- local max = MathUtils.Max(value1, value2, value3) or 0
-- -- with Vec2
-- local maxVec2 = MathUtils.Max(vec2_1, vec2_2) or TEN.Vec2(0, 0)
-- -- with Vec3
-- local maxVec3 = MathUtils.Max(vec3_1, vec3_2) or TEN.Vec3(0, 0, 0)
-- -- with Time
-- local maxTime = MathUtils.Max(time1, time2) or TEN.Time(0)
MathUtils.Max = function(a, b, ...)
    -- Fast path: exactly 2 arguments (most common case)
    local extraCount = select("#", ...)

    if IsNumber(a) then
        if not IsNumber(b) then
            LogMessage(errorMessageMax, logLevelError)
            return a
        end
        local maxVal = a > b and a or b
        for i = 1, extraCount do
            local v = select(i, ...)
            if not IsNumber(v) then
                LogMessage(errorMessageMax, logLevelError)
                return a
            end
            if v > maxVal then maxVal = v end
        end
        return maxVal

    elseif IsVec2(a) then
        if not IsVec2(b) then
            LogMessage(errorMessageMax, logLevelError)
            return a
        end
        local rx, ry = max(a.x, b.x), max(a.y, b.y)
        for i = 1, extraCount do
            local v = select(i, ...)
            if not IsVec2(v) then
                LogMessage(errorMessageMax, logLevelError)
                return a
            end
            rx = max(rx, v.x)
            ry = max(ry, v.y)
        end
        return Vec2(rx, ry)

    elseif IsVec3(a) then
        if not IsVec3(b) then
            LogMessage(errorMessageMax, logLevelError)
            return a
        end
        local rx, ry, rz = max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)
        for i = 1, extraCount do
            local v = select(i, ...)
            if not IsVec3(v) then
                LogMessage(errorMessageMax, logLevelError)
                return a
            end
            rx = max(rx, v.x)
            ry = max(ry, v.y)
            rz = max(rz, v.z)
        end
        return Vec3(rx, ry, rz)

    elseif IsTime(a) then
        if not IsTime(b) then
            LogMessage(errorMessageMax, logLevelError)
            return a
        end
        local maxTime = a:GetFrameCount() > b:GetFrameCount() and a or b
        for i = 1, extraCount do
            local v = select(i, ...)
            if not IsTime(v) then
                LogMessage(errorMessageMax, logLevelError)
                return a
            end
            if v:GetFrameCount() > maxTime:GetFrameCount() then maxTime = v end
        end
        return maxTime
    end

    LogMessage("Error in MathUtils.Max: unsupported type.", logLevelError)
    return a
end

--- Round a number to a specified number of decimal places.
-- @tparam float num The number to round.
-- @tparam[opt=0] float decimals Number of decimal places.
-- @treturn[1] float The rounded number.
-- @treturn[2] float 0 If an error occurs.
-- @usage
-- local rounded1 = MathUtils.Round(3.14159)       -- Result: 3
-- local rounded2 = MathUtils.Round(3.14159, 2)    -- Result: 3.14
-- local rounded3 = MathUtils.Round(2.675, 2)      -- Result: 2.68
-- local rounded4 = MathUtils.Round(-1.2345, 1)    -- Result: -1.2
MathUtils.Round = function(num, decimals)
    decimals = decimals or 0
    if not IsNumber(num) or not IsNumber(decimals) then
        LogMessage("Error in MathUtils.Round: num and decimals must be numbers.", logLevelError)
        return 0
    end
    local mult = 10 ^ decimals
    return Round(num, mult)
end

--- Truncate a number to a specified number of decimal places (without rounding).
-- @tparam float num The number to truncate.
-- @tparam[opt=0] float decimals Number of decimal places.
-- @treturn[1] float The truncated number.
-- @treturn[2] float 0 If an error occurs.
-- @usage
-- local truncated1 = MathUtils.Truncate(3.14159)       -- Result: 3
-- local truncated2 = MathUtils.Truncate(3.14159, 2)    -- Result: 3.14
-- local truncated3 = MathUtils.Truncate(2.999, 2)      -- Result: 2.99 (not rounded!)
-- local truncated4 = MathUtils.Truncate(2.99, 0)       -- Result: 2     
-- local truncated4 = MathUtils.Truncate(-1.2345, 1)    -- Result: -1.2
MathUtils.Truncate = function(num, decimals)
    decimals = decimals or 0
    if not IsNumber(num) or not IsNumber(decimals) then
        LogMessage("Error in MathUtils.Truncate: num and decimals must be numbers.", logLevelError)
        return 0
    end
    local mult = 10 ^ decimals
        local result
    if num >= 0 then
        result = floor(num * mult) / mult
    else
        result = -floor(-num * mult) / mult
    end
    if decimals == 0 then
        result = floor(result)
    end
    return result
end

--- Generate a random number or vector/color/time with optional seed.
-- The function uses the current game time in frames as seed for randomness if the seed parameter is not provided, ensuring different random values each time the game is played.
-- For reproducible randomness, a specific seed can be provided.
-- @tparam float|Vec2|Vec3|Rotation|Color|Time minValue Minimum value.
-- @tparam float|Vec2|Vec3|Rotation|Color|Time maxValue Maximum value (same type as minValue).
-- @tparam[opt] float seed Seed for reproducible randomness.
-- @treturn[1] float|Vec2|Vec3|Rotation|Color|Time Random value between minValue and maxValue.
-- @treturn[2] nil If an error occurs.
-- @usage
-- -- Random number between 1 and 10
-- local rand1 = MathUtils.Random(1, 10)
--
-- -- Random float between 0.0 and 1.0
-- local rand2 = MathUtils.Random(0.0, 1.0)
--
-- -- Random number with seed 42
-- local rand3 = MathUtils.Random(1, 100, 42)
--
-- -- Random position in a rectangle:
-- local randomPos = MathUtils.Random(
--     TEN.Vec2(0, 0), 
--     TEN.Vec2(1024, 768)
-- )
-- 
-- -- Random 3D position in a box:
-- local randomPos3D = MathUtils.Random(
--     TEN.Vec3(-100, 0, -100), 
--     TEN.Vec3(100, 200, 100)
-- )
-- 
-- -- Random color between red and yellow:
-- local randomColor = MathUtils.Random(
--     TEN.Color(255, 0, 0, 255),
--     TEN.Color(255, 255, 0, 255)
-- )
--
-- -- Random time in frames (30-90 frames = 1-3 seconds @ 30fps):
-- local randomFrames = MathUtils.Random(
--     TEN.Time(ConversionUtils.SecondsToFrames(1)),
--     TEN.Time(ConversionUtils.SecondsToFrames(3))
-- )
--
-- -- Random rotation between two angles:
-- local randomRotation = MathUtils.Random(
--     TEN.Rotation(0, 0, 0),
--     TEN.Rotation(0, 180, 0)
-- )
--
-- -- Error handling example:
-- local randomPos = MathUtils.Random(minVec, maxVec)
-- if randomPos == nil then
--     TEN.Util.PrintLog("Failed to generate random position", TEN.Util.LogLevel.ERROR)
--     return  -- or use a fallback value
-- end
-- entity:SetPosition(randomPos)
--
-- -- Safe approach with default fallback:
-- local randomColor = MathUtils.Random(color1, color2) or TEN.Color(255, 255, 255, 255)
-- sprite:SetColor(randomColor)
MathUtils.Random = function(minValue, maxValue, seed)
    if seed then
        if not IsNumber(seed) then
            LogMessage("Warning: seed must be a number. Will be used current game time in frames", logLevelWarning)
        else
            randomseed(seed)
            -- Discard first few values to improve randomness of initial seed (common practice with some RNG implementations)
            for i = 1, 3 do random() end
        end
    end

    if IsNumber(minValue) then
        if not IsNumber(maxValue) then
            LogMessage(errorMessageRandom, logLevelError)
            return nil
        end
        return minValue + random() * (maxValue - minValue)
    elseif IsVec2(minValue) then
        if not IsVec2(maxValue) then
            LogMessage(errorMessageRandom, logLevelError)
            return nil
        end
        return Vec2(
            minValue.x + random() * (maxValue.x - minValue.x),
            minValue.y + random() * (maxValue.y - minValue.y)
        )
    elseif IsVec3(minValue) then
        if not IsVec3(maxValue) then
            LogMessage(errorMessageRandom, logLevelError)
            return nil
        end
        return Vec3(
            minValue.x + random() * (maxValue.x - minValue.x),
            minValue.y + random() * (maxValue.y - minValue.y),
            minValue.z + random() * (maxValue.z - minValue.z)
        )
    elseif IsColor(minValue) then
        if not IsColor(maxValue) then
            LogMessage(errorMessageRandom, logLevelError)
            return nil
        end
        return Color(
            floor(minValue.r + random() * (maxValue.r - minValue.r)),
            floor(minValue.g + random() * (maxValue.g - minValue.g)),
            floor(minValue.b + random() * (maxValue.b - minValue.b)),
            floor(minValue.a + random() * (maxValue.a - minValue.a))
        )
    elseif IsTime(minValue) then
        if not IsTime(maxValue) then
            LogMessage(errorMessageRandom, logLevelError)
            return nil
        end
        -- Generate random frames between minValue and maxValue (Time objects work with gameFrames)
        local minFrames = minValue:GetFrameCount()
        local maxFrames = maxValue:GetFrameCount()
        local randomFrames = floor(minFrames + random() * (maxFrames - minFrames))
        return Time(randomFrames)
    elseif IsRotation(minValue) then
        if not IsRotation(maxValue) then
            LogMessage(errorMessageRandom, logLevelError)
            return nil
        end
        return Rotation(
            minValue.x + random() * (maxValue.x - minValue.x),
            minValue.y + random() * (maxValue.y - minValue.y),
            minValue.z + random() * (maxValue.z - minValue.z)
        )
    end
    LogMessage("Error in MathUtils.Random: unsupported type", logLevelError)
    return nil
end

--- Clamp a value between a minimum and maximum for numbers, Vec2, Vec3, Color, Rotation, and Time.
-- Supports numbers and TEN primitives (Color, Rotation, Time, Vec2, Vec3).
-- For primitives, each component is clamped individually between corresponding minValue/maxValue components.
-- @tparam number|Color|Rotation|Time|Vec2|Vec3 value The value to clamp.
-- @tparam number|Color|Rotation|Time|Vec2|Vec3 minValue The minimum value (same type as value).
-- @tparam number|Color|Rotation|Time|Vec2|Vec3 maxValue The maximum value (same type as value).
-- @treturn[1] number|Color|Rotation|Time|Vec2|Vec3 The clamped value.
-- @treturn[2] number|Color|Rotation|Time|Vec2|Vec3 The original value if an error occurs.
-- @usage
-- -- Example: Clamp to 0-1 range for normalized values
-- local clampedValue = MathUtils.Clamp(value, 0, 1)
--
-- -- Practical use: Clamp value between 0 and 1
-- local inputValue = 1.5
-- local clampedValue = MathUtils.Clamp(inputValue, 0, 1) -- Result: 1
--
-- -- Use clampedValue for further calculations
-- local scaledValue = clampedValue * 100  -- Scale to 0-100 range
--
-- -- Example: Clamp a Vec2 position within screen bounds
-- local position = TEN.Vec2(1200, -50)
-- local minBounds = TEN.Vec2(0, 0)
-- local maxBounds = TEN.Vec2(1920, 1080)
-- local clampedPosition = MathUtils.Clamp(position, minBounds, maxBounds)
-- -- Result: Vec2(1200, 0)
--
-- -- Example: Clamp a Vec3 position within a bounding box
-- local position = TEN.Vec3(150, -20, 300)
-- local minBounds = TEN.Vec3(0, 0, 0)
-- local maxBounds = TEN.Vec3(100, 100, 100)
-- local clampedPosition = MathUtils.Clamp(position, minBounds, maxBounds)
-- -- Result: Vec3(100, 0, 100)
--
-- -- Example: Clamp a Color's RGB components
-- local color = TEN.Color(251, 120, 20, 255)
-- local minColor = TEN.Color(0, 0, 0, 255)
-- local maxColor = TEN.Color(255, 100, 10, 255)
-- local clampedColor = MathUtils.Clamp(color, minColor, maxColor)
-- -- Result: TEN.Color(251, 100, 10, 255)
--
-- -- Example: Clamp a Time duration between 1 and 5 seconds
-- local duration = TEN.Time(ConversionUtils.SecondsToFrames(7))
-- local minDuration = TEN.Time(ConversionUtils.SecondsToFrames(1))
-- local maxDuration = TEN.Time(ConversionUtils.SecondsToFrames(5))
-- local clampedDuration = MathUtils.Clamp(duration, minDuration, maxDuration)
-- -- Result: TEN.Time.FromSeconds(5)
--
-- -- Example: Clamp a Rotation's pitch between -45 and 45 degrees
-- local rotation = TEN.Rotation(45, 90, 0)
-- local minRotation = TEN.Rotation(15, 0, 0)
-- local maxRotation = TEN.Rotation(0, 90, 0)
-- local clampedRotation = MathUtils.Clamp(rotation, minRotation, maxRotation)
-- -- Result: TEN.Rotation(15, 90, 0)
--
-- -- Error handling example:
-- local clampedValue = MathUtils.Clamp(value, min, max)
-- if clampedValue == value then
--     TEN.Util.PrintLog("Failed to clamp value", TEN.Util.LogLevel.ERROR)
--     return  -- or use a fallback value
-- end
-- -- Safe approach with default fallback:
-- local clampedValue = MathUtils.Clamp(value, minValue, maxValue) or defaultValue
MathUtils.Clamp = function(value, minValue, maxValue)
    -- Lazy type checking: check only what's needed
    if IsNumber(value) then
        if not (IsNumber(minValue) and IsNumber(maxValue)) then
            LogMessage(errorMessageBase, logLevelError)
            return value
        end
        if minValue > maxValue then
            LogMessage("Error in MathUtils.Clamp: minValue cannot be greater than maxValue.", logLevelError)
            return value
        end
        return max(minValue, min(maxValue, value))
    end

    if IsVec2(value) then
        if not (IsVec2(minValue) and IsVec2(maxValue)) then
            LogMessage(errorMessageBase, logLevelError)
            return value
        end
        return Vec2(
            max(minValue.x, min(maxValue.x, value.x)),
            max(minValue.y, min(maxValue.y, value.y))
        )
    end

    if IsVec3(value) then
        if not (IsVec3(minValue) and IsVec3(maxValue)) then
            LogMessage(errorMessageBase, logLevelError)
            return value
        end
        return Vec3(
            max(minValue.x, min(maxValue.x, value.x)),
            max(minValue.y, min(maxValue.y, value.y)),
            max(minValue.z, min(maxValue.z, value.z))
        )
    end

    if IsRotation(value) then
        if not (IsRotation(minValue) and IsRotation(maxValue)) then
            LogMessage(errorMessageBase, logLevelError)
            return value
        end
        return Rotation(
            max(minValue.x, min(maxValue.x, value.x)),
            max(minValue.y, min(maxValue.y, value.y)),
            max(minValue.z, min(maxValue.z, value.z))
        )
    end

    if IsColor(value) then
        if not (IsColor(minValue) and IsColor(maxValue)) then
            LogMessage(errorMessageBase, logLevelError)
            return value
        end
        return Color(
            max(minValue.r, min(maxValue.r, value.r)),
            max(minValue.g, min(maxValue.g, value.g)),
            max(minValue.b, min(maxValue.b, value.b)),
            max(minValue.a, min(maxValue.a, value.a))
        )
    end

    if IsTime(value) then
        if not (IsTime(minValue) and IsTime(maxValue)) then
            LogMessage(errorMessageBase, logLevelError)
            return value
        end
        return Time(max(minValue:GetFrameCount(), min(maxValue:GetFrameCount(), value:GetFrameCount())))
    end

    LogMessage("Error in MathUtils.Clamp: unsupported type.", logLevelError)
    return value
end

--- Check if a value is within a range (inclusive).
-- @tparam float value The value to check.
-- @tparam float minValue Minimum value.
-- @tparam float maxValue Maximum value.
-- @treturn[1] bool True if value is within range.
-- @treturn[2] bool false If an error occurs.
-- @usage
-- local inRange = MathUtils.IsInRange(5, 1, 10) -- true
-- local outOfRange = MathUtils.IsInRange(15, 1, 10) -- false
--
-- -- Edge cases:
--
-- -- This will log an error and return false because minValue is greater than maxValue
-- local errorCase = MathUtils.IsInRange(5, 10, 1)
--
-- -- This will log an error and return false because value is not a number
-- local errorCase2 = MathUtils.IsInRange("5", 1, 10)
MathUtils.IsInRange = function(value, minValue, maxValue)
    if not (IsNumber(value) and IsNumber(minValue) and IsNumber(maxValue)) then
        LogMessage("Error in MathUtils.IsInRange: all parameters must be numbers.", logLevelError)   
        return false
    end

    if minValue > maxValue then
        LogMessage("Error in MathUtils.IsInRange: minValue cannot be greater than maxValue.", logLevelError)
        return false
    end

    return value >= minValue and value <= maxValue
end

--- Wrap an angle to a specific range (e.g., 0-360 or -180 to 180).
-- Useful for normalizing rotation angles and preventing overflow.
-- Unlike Clamp, this function wraps values cyclically (e.g., 450° → 90°).
-- @tparam float angle The angle to wrap.
-- @tparam[opt=0] float minValue Minimum value of the range.
-- @tparam[opt=360] float maxValue Maximum value of the range.
-- @treturn[1] float The wrapped angle.
-- @treturn[2] float The original angle if an error occurs.
-- @usage
-- -- Normalize angles to 0-360 range:
-- local wrapped1 = MathUtils.WrapAngle(450, 0, 360)  -- Result: 90
-- local wrapped2 = MathUtils.WrapAngle(-30, 0, 360)  -- Result: 330
-- local wrapped3 = MathUtils.WrapAngle(720, 0, 360)  -- Result: 0
--
-- -- Normalize to -180 to 180 range (common for game rotations):
-- local wrapped4 = MathUtils.WrapAngle(200, -180, 180)  -- Result: -160
-- local wrapped5 = MathUtils.WrapAngle(-200, -180, 180) -- Result: 160
--
-- -- Difference from Clamp (important!):
-- local clamped = MathUtils.Clamp(450, 0, 360)    -- Result: 360 (cuts at max)
-- local wrapped = MathUtils.WrapAngle(450, 0, 360) -- Result: 90 (wraps around)
--
-- -- Practical example: Continuous rotation without overflow
-- local currentAngle = 0
-- LevelFuncs.OnLoop = function()
--     currentAngle = currentAngle + 5  -- Rotate 5° per frame
--     currentAngle = MathUtils.WrapAngle(currentAngle, 0, 360)  -- Keep in 0-360
--     entity:SetRotation(TEN.Rotation(0, currentAngle, 0))
-- end
--
-- -- Example: Calculate shortest rotation path
-- local currentYaw = 350  -- Facing almost north
-- local targetYaw = 10    -- Target is just past north
-- -- Direct difference would be: 10 - 350 = -340° (wrong direction!)
-- -- Wrapped difference:
-- local delta = MathUtils.WrapAngle(targetYaw - currentYaw, -180, 180)
-- -- Result: 20° (correct shortest path: turn right 20°)
MathUtils.WrapAngle = function(angle, minValue, maxValue)
    minValue = minValue or 0
    maxValue = maxValue or 360

    if not (IsNumber(angle) and IsNumber(minValue) and IsNumber(maxValue)) then
        LogMessage("Error in MathUtils.WrapAngle: all parameters must be numbers.", logLevelError)
        return angle
    end

    local range = maxValue - minValue
    if range == 0 then
        LogMessage("Error in MathUtils.WrapAngle: minValue cannot equal maxValue.", logLevelError)
        return angle
    end

    -- return angle - range * floor((angle - minValue) / range)
    return WrapAngleRaw(angle, minValue, range)
end

LevelFuncs.StartRandomSeed = function()
    -- Use current game time in frames as seed for randomness
    local seed = TEN.Flow.GetGlobalGameTime():GetFrameCount()
    randomseed(seed)
    -- Discard first few values to improve randomness of initial seed (common practice with some RNG implementations)
    for i = 1, 3 do random() end
end

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_START, LevelFuncs.StartRandomSeed)

return MathUtils