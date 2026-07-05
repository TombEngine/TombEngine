-- ldignore

-- Internal functions specific to modules. These are not intended for end users. These functions are not documented in the API reference.

local Type = require("Engine.Type")
local Util = {}

local LogMessage  = TEN.Util.PrintLog
local logLevelWarning = TEN.Util.LogLevel.WARNING
local Color = TEN.Color
local IsString = Type.IsString
local IsTable = Type.IsTable
local IsBoolean = Type.IsBoolean
local floor = math.floor
local max = math.max
local min = math.min
local abs = math.abs
local sin = math.sin
local cos = math.cos
local asin = math.asin
local atan = math.atan
local deg = math.deg
local sqrt = math.sqrt
local rad = math.rad
local pi = math.pi
local IsNumber = Type.IsNumber
local IsVec2 = Type.IsVec2
local IsVec3 = Type.IsVec3
local IsColor = Type.IsColor
local IsRotation = Type.IsRotation

-- Time format keys
-- For backward compatibility, deciseconds is still accepted, but centiseconds is preferred. Both keys will work, but if both are present, centiseconds will be used.
local VALID_KEYS = { hours = true, minutes = true, seconds = true, deciseconds = true, centiseconds = true }

-- Comparison operators constant table. Used for validating operators.
local COMPARISON_OPS_FUNC =
{
    function(a, b) return a == b end,   -- 0: equal
    function(a, b) return a ~= b end,   -- 1: not equal
    function(a, b) return a < b end,    -- 2: less than
    function(a, b) return a <= b end,   -- 3: less than or equal
    function(a, b) return a > b end,    -- 4: greater than
    function(a, b) return a >= b end,   -- 5: greater than or equal
}

local function pad2(n)
    return (n < 10) and ("0" .. n) or tostring(n)
end

-- HSL to RGB conversion
local function HueToRgb(p, q, t)
    if t < 0 then
        t = t + 1
    end
    if t > 1 then
        t = t - 1
    end
    if t < 1 / 6 then
        return p + (q - p) * 6 * t
    end
    if t < 1 / 2 then
        return q
    end
    if t < 2 / 3 then
        return p + (q - p) * (2 / 3 - t) * 6
    end
    return p
end

-- sRGB and Linear color space conversion
local function SrgbToLinear(c)
    if c <= 0.04045 then
        return c / 12.92
    else
        return ((c + 0.055) / 1.055) ^ 2.4
    end
end

-- Linear to sRGB color space conversion
local function LinearToSrgb(c)
    if c <= 0.0031308 then
        return c * 12.92
    else
        return 1.055 * (c ^ (1 / 2.4)) - 0.055
    end
end

Util.Constants =
{
    FPS = 30,              -- Default frames per second for time-frame conversions
    MAX_DEPTH = 10,        -- Maximum recursion depth for deep operations (prevents stack overflow)
    MAX_ELEMENTS = 1000,   -- Maximum elements processed in deep operations (prevents performance issues)
    ZERO = TEN.Time(),     -- Zero time constant for comparisons and resets
    DEFAULT_TEXT_OPTIONS = -- Default text options for display strings in Time and Stopwatch modules
    {
        TEN.Strings.DisplayStringOption.CENTER,
        TEN.Strings.DisplayStringOption.SHADOW,
        TEN.Strings.DisplayStringOption.VERTICAL_CENTER
    },
    DEFAULT_TIMER_FORMAT = -- Default timer format for Time and Stopwatch modules
    {
        minutes = true,
        seconds = true,
        centiseconds = true
    },
    Operators = -- Comparison operators for easy comparisons
    {
        EQUAL = 0,        -- Equal
        NOT_EQUAL = 1,    -- Not equal
        LESS = 2,         -- Less than
        LESS_EQUAL = 3,   -- Less than or equal
        GREATER = 4,      -- Greater than
        GREATER_EQUAL = 5 -- Greater than or equal
    }
}
Util.Constants.FRAME_TIME = 1 / Util.Constants.FPS


Util.ShortenTENCalls = function()
	print("Util.ShortenTENCalls is deprecated; its functionality is now performed automatically by TombEngine.")
end

-- Check if the operator is valid and return the corresponding comparison function.
Util.CheckOperator = function(operator)
	if not Type.IsNumber(operator) then
		return nil
	end
    local op = COMPARISON_OPS_FUNC[operator + 1]
    return Type.IsFunction(op) and op or nil
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

-- Get the maximum positive integer index in a table.
-- Used by array-like operations that must work with sparse tables.
-- Used by: StringUtils.lua,
Util.GetMaxNumericIndex = function(tbl)
    local maxIndex = 0
    for key, _ in next, tbl do
        if type(key) == "number" and key > 0 and floor(key) == key and key > maxIndex then
            maxIndex = key
        end
    end
    return maxIndex
end

-- Angle wrapping (used in interpolation)
-- Used by: MathUtils.lua
Util.WrapAngleRaw = function(angle, minVal, range)
    return angle - range * floor((angle - minVal) / range)
end

-- HSL to Color conversion (no type checking, used internally)
-- Takes h, s, l, a as separate parameters to avoid table allocation
-- Returns a Color object directly
Util.HSLtoColorRaw = function(h, s, l, a)
    local r, g, b

    if s == 0 then
        r, g, b = l, l, l  -- Achromatic (gray)
    else
        local q = l < 0.5 and l * (1 + s) or l + s - l * s
        local p = 2 * l - q
        local hk = h / 360
        r = HueToRgb(p, q, hk + 1/3)
        g = HueToRgb(p, q, hk)
        b = HueToRgb(p, q, hk - 1/3)
    end

    -- Round to nearest integer and convert to 0-255 range
    return Color(
        floor(r * 255 + 0.5),
        floor(g * 255 + 0.5),
        floor(b * 255 + 0.5),
        floor(a * 255 + 0.5)
    )
end

-- Color to HSL conversion (no type checking, used internally)
-- Returns h, s, l as multiple values (no table allocation)
Util.ColorToHSLRaw = function(color)
    local r = color.r / 255
    local g = color.g / 255
    local b = color.b / 255

    local h = color:GetHue()

    local maxValue = max(r, g, b)
    local minValue = min(r, g, b)
    local l = (maxValue + minValue) / 2

    local s
    if maxValue == minValue then
        s = 0
    else
        local delta = maxValue - minValue
        s = l > 0.5 and delta / (2 - maxValue - minValue) or delta / (maxValue + minValue)
    end

    return h, s, l
end

-- Color to OKLch conversion (no type checking, used internally)
-- Returns L, C, h as multiple values (no table allocation)
Util.ColorToOKLchRaw = function(color)
    local r = SrgbToLinear(color.r / 255)
    local g = SrgbToLinear(color.g / 255)
    local b = SrgbToLinear(color.b / 255)

    local l_ = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    local m_ = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    local s_ = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b

    local l_cbrt = l_ >= 0 and l_ ^ (1/3) or -((-l_) ^ (1/3))
    local m_cbrt = m_ >= 0 and m_ ^ (1/3) or -((-m_) ^ (1/3))
    local s_cbrt = s_ >= 0 and s_ ^ (1/3) or -((-s_) ^ (1/3))

    local L = 0.2104542553 * l_cbrt + 0.7936177850 * m_cbrt - 0.0040720468 * s_cbrt
    local A = 1.9779984951 * l_cbrt - 2.4285922050 * m_cbrt + 0.4505937099 * s_cbrt
    local B = 0.0259040371 * l_cbrt + 0.7827717662 * m_cbrt - 0.8086757660 * s_cbrt

    local C = sqrt(A * A + B * B)
    local h = deg(atan(B, A))
    if h < 0 then
        h = h + 360
    end

    return L, C, h
end

-- OKLch to Color conversion (no type checking, used internally)
-- Takes L, C, h, a as separate parameters to avoid table allocation
-- Returns a Color object directly
Util.OKLchToColorRaw = function(L, C, h, a)

    -- Convert back to OKLab
    local hRad = rad(h)
    local A = C * cos(hRad)
    local B = C * sin(hRad)

    -- OKLab to LMS (inverse matrix)
    local l_cbrt = L + 0.3963377774 * A + 0.2158037573 * B
    local m_cbrt = L - 0.1055613458 * A - 0.0638541728 * B
    local s_cbrt = L - 0.0894841775 * A - 1.2914855480 * B

    -- Cube (inverse of cube root)
    local l_ = l_cbrt * l_cbrt * l_cbrt
    local m_ = m_cbrt * m_cbrt * m_cbrt
    local s_ = s_cbrt * s_cbrt * s_cbrt

    -- LMS to linear RGB (inverse matrix)
    local r_lin =  4.0767416621 * l_ - 3.3077115913 * m_ + 0.2309699292 * s_
    local g_lin = -1.2684380046 * l_ + 2.6097574011 * m_ - 0.3413193965 * s_
    local b_lin = -0.0041960863 * l_ - 0.7034186147 * m_ + 1.7076147010 * s_

    -- Clamp to valid range before gamma correction
    r_lin = max(0, min(1, r_lin))
    g_lin = max(0, min(1, g_lin))
    b_lin = max(0, min(1, b_lin))

    -- Linear RGB to sRGB
    local r = LinearToSrgb(r_lin)
    local g = LinearToSrgb(g_lin)
    local b = LinearToSrgb(b_lin)

    -- Clamp and convert to 0-255 range
    r = floor(r * 255 + 0.5)
    g = floor(g * 255 + 0.5)
    b = floor(b * 255 + 0.5)
    local alpha = floor(max(0, min(1, a)) * 255 + 0.5)

    return Color(r, g, b, alpha)
end

-- Check if a value is valid for interpolation (number, color, rotation, or vector)
Util.IsValidInterpolationValue = function(value)
    return IsNumber(value) or
           IsColor(value) or
           IsRotation(value) or
           IsVec2(value) or
           IsVec3(value)
end

-- Core interpolation that handles different types (number, color, rotation, vector) without clamping t.
-- Clamping is handled by the caller to allow for extrapolation if desired.
local InterpolateValuesRaw = function(a, b, t)
    if IsNumber(a) then
        return a + (b - a) * t
    end
    return a:Lerp(b, t)
end

-- Linear interpolation (no type checking, used internally)
Util.LerpRaw = function (a, b, t)
    return InterpolateValuesRaw(a, b, t)
end

-- Smoothstep interpolation (no type checking, used internally)
-- edge0 and edgeDelta are used to scale and bias t before applying the smoothstep formula
-- edgeDelta is the difference between edge1 and edge0, which defines the range over which the smoothstep transition occurs
Util.SmoothstepRaw = function(a, b, t, edge0, edgeDelta)
    -- Scale, bias and saturate t to 0..1 range
    t = max(0, min(1, (t - edge0) / edgeDelta))

    -- Evaluate polynomial
    -- Smoothstep formula: t²(3 - 2t) = 3t² - 2t³
    t = t * t * (3 - 2 * t)
    return InterpolateValuesRaw(a, b, t)
end

-- Smootherstep interpolation (no type checking, used internally)
-- edge0 and edgeDelta are used to scale and bias t before applying the smootherstep formula
-- edgeDelta is the difference between edge1 and edge0, which defines the range over which the smootherstep transition occurs
Util.SmootherstepRaw  = function(a, b, t, edge0, edgeDelta)
    -- Scale, bias and saturate t to 0..1 range
    t = max(0, min(1, (t - edge0) / edgeDelta))

    -- Ken Perlin's smootherstep polynomial: 6t⁵ - 15t⁴ + 10t³
    -- This is identical to LevelFuncs.Engine.Node.Smoothstep
    t = t * t * t * (t * (t * 6 - 15) + 10)
    return InterpolateValuesRaw(a, b, t)
end

-- Ease-in-out interpolation (no type checking, used internally)
-- Uses the easeInOutQuad formula, which provides a smooth acceleration and deceleration.
Util.EaseInOutRaw  = function(a, b, t)
    -- EaseInOutQuad formula
    local easedT
    if t < 0.5 then
        easedT = 2 * t * t  -- Ease in: acceleration
    else
        easedT = 1 - 2 * (1 - t) * (1 - t)  -- Ease out: deceleration
    end
    return InterpolateValuesRaw(a, b, easedT)
end

-- Ease-in-out elastic interpolation (no type checking, used internally)
-- This function creates an elastic easing effect, where the value overshoots and oscillates before settling at the target.
-- The amplitude controls the height of the overshoot, while the period controls the frequency of the oscillations.
Util.ElasticRaw  = function(a, b, t, amplitude, period)
    -- Handle edge cases (no oscillation at start/end)
    if t == 0 then
        return a
    elseif t == 1 then
        return b
    end

    -- EaseInOutElastic formula
    local twoPi = 2 * pi

    -- Calculate phase shift 's' to adjust the sine wave's starting point
    -- The phase shift ensures the elastic curve starts at 0 and ends at 1
    -- Formula: s = period / (2π) * arcsin(1 / amplitude)
    local s = period / (2 * pi) * asin(1 / amplitude)
    local periodOverTwoPi = twoPi / period
    local easedT

    if t < 0.5 then
        -- Ease In (first half) - undershoot at start
        t = t * 2
        easedT = -(amplitude * (2 ^ (10 * (t - 1))) * sin((t - 1 - s) * periodOverTwoPi)) / 2
    else
        -- Ease Out (second half) - overshoot at end
        t = t * 2 - 1
        easedT = (amplitude * (2 ^ (-10 * t)) * sin((t - s) * periodOverTwoPi)) / 2 + 1
    end
    return InterpolateValuesRaw(a, b, easedT)
end

-- Bounce interpolation (no type checking, used internally)
-- This function creates a bouncing effect, where the value bounces towards the target before settling.
-- The bounces parameter controls how many times it bounces, while the damping parameter controls how quickly the bounces decrease in amplitude.
Util.BounceRaw = function(a, b, t, bounces, damping)
    -- Handle edge cases
    if t == 0 then
        return a
    elseif t == 1 then
        return b
    end

    -- Bounce formula:
    -- Uses exponential decay combined with sine wave for bounce oscillations
    -- Formula: easedT = 1 - (cos(t * π * bounces) * (1 - t)^(1/damping))
    -- 
    -- The formula works as follows:
    -- 1. cos(t * π * bounces): Creates oscillations (bounces)
    -- 2. (1 - t)^(1/damping): Exponential decay envelope
    --    - damping controls decay speed
    --    - Lower damping = faster energy loss
    --    - Higher damping = longer bounces
    -- 3. Multiply them: Bounces that decrease in amplitude
    -- 4. (1 - result): Invert so we approach target value instead of 0

    local decay = (1 - t) ^ (1 / (damping + 0.1))  -- Add 0.1 to prevent division issues
    local oscillation = abs(cos(t * pi * bounces))
    local easedT = 1 - (oscillation * decay)
    return InterpolateValuesRaw(a, b, easedT)
end

-- Support function for rounding numbers to a specified number of decimal places
Util.Round = function(num, mult)
    return floor(num * mult + 0.5) / mult
end

return Util