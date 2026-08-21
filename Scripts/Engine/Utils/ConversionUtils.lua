-----<style>table.function_list td.name {min-width: 367px;}</style>
--- Lua support functions for converting between different units and formats.
---
--- **Design Philosophy:**
--- ConversionUtils is designed primarily for:
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
---	local ConversionUtils = require("Engine.Utils.ConversionUtils")
-- @luautil ConversionUtils

local ConversionUtils = {}
local Type = require("Engine.Type")
local Utility = require("Engine.Util")

local FPS = Utility.Constants.FPS

local Color = TEN.Color
local Time = TEN.Time
local HSLtoColorRaw = Utility.HSLtoColorRaw
local ColorToHSLRaw = Utility.ColorToHSLRaw
local ColorToOKLchRaw = Utility.ColorToOKLchRaw
local OKLchToColorRaw = Utility.OKLchToColorRaw
local ErrorLog = Utility.ErrorLog
local WarningLog = Utility.WarningLog
local IsNumber = Type.IsNumber
local IsColor = Type.IsColor
local IsString = Type.IsString
local IsTable = Type.IsTable
local floor = math.floor
local max = math.max
local min = math.min

--- Convert seconds to frames (assuming 30 FPS).
-- @tparam float seconds Time in seconds. Seconds can be a positive float value with two decimal places. No negative values allowed.
-- @tparam[opt=30] int fps Frames per second.
-- @tparam[opt="ConversionUtils.SecondsToFrames"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float `result`: Number of frames, or 0 if an error occurs.
-- @treturn[1] bool `ok`: true if conversion is successful, false if an error occurs.
-- @usage
-- -- Basic usage:
-- local frames = ConversionUtils.SecondsToFrames(2.0) -- Result: 60 for 30 FPS
--
-- -- Advanced: error handling in a module function
-- local function GetFrameCount(seconds, fps)
--     local frames, ok = ConversionUtils.SecondsToFrames(seconds, fps, "GetFrameCount")
--     if not ok then
--         return 0  -- or fallback value
--     end
--     return frames
-- end
ConversionUtils.SecondsToFrames = function(seconds, fps, errorContext)
    fps = fps or FPS
    errorContext = errorContext or "ConversionUtils.SecondsToFrames"
    if not IsNumber(seconds) or not IsNumber(fps) or seconds < 0 or fps < 0 then
        ErrorLog("Error in {context}: seconds and fps must be positive numbers.", {context = errorContext })
        return 0, false
    end

    -- Check if fps is a float and warn user
    if fps ~= floor(fps) then
        local roundFps = floor(fps + 0.5)
        WarningLog("Warning in {context}: fps should be an integer. Rounding {fps} to {round}", {context = errorContext, fps = fps, round = roundFps})
        fps = roundFps
    end

    return floor(seconds * fps), true
end

--- Convert frames to seconds (assuming 30 FPS).
-- @tparam int frames Number of frames. Frames can be a positive integer. No negative values allowed.
-- @tparam[opt=30] int fps Frames per second. No negative values or zero allowed.
-- @tparam[opt="ConversionUtils.FramesToSeconds"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float `result`: Time in seconds, or 0 if an error occurs.
-- @treturn[1] bool ok true if conversion is successful, false if an error occurs.
-- @usage
-- -- Basic usage:
-- local seconds = ConversionUtils.FramesToSeconds(60) -- Result: 2.0
--
-- -- Advanced: error handling in a module function
-- local function GetDurationInSeconds(frames, fps)
--     local seconds, ok = ConversionUtils.FramesToSeconds(frames, fps, "GetDurationInSeconds")
--     if not ok then
--         return 0  -- or fallback value
--     end
--     return seconds
-- end
ConversionUtils.FramesToSeconds = function(frames, fps, errorContext)
    fps = fps or FPS
    errorContext = errorContext or "ConversionUtils.FramesToSeconds"
    if not IsNumber(frames) or frames < 0 then
        ErrorLog("Error in {context}: frames must be positive numbers", {context = errorContext})
        return 0, false
    end
    if not IsNumber(fps) or fps <= 0 then
        ErrorLog("Error in {context}: fps must be a positive number and greater than zero.", {context = errorContext})
        return 0, false
    end

    -- Check if frames is a float and warn user
    if frames ~= floor(frames) then
        local roundFrames = floor(frames + 0.5)
        WarningLog("Warning in {context}: frames should be an integer. Rounding {frames} to {round}", {context = errorContext, frames = frames, round = roundFrames})
        frames = roundFrames
    end

    -- Check if fps is a float and warn user
    if fps ~= floor(fps) then
        local roundFps = floor(fps + 0.5)
        WarningLog("Warning in {context}: fps should be an integer. Rounding {frames} to {round}", { context = errorContext, fps = fps, round = roundFps})
        fps = roundFps
    end

    return frames / fps , true
end

--- Convert seconds to a @{Time} object. TEN.Time internally uses game frames at 30 FPS; seconds are converted to frames and rounded to the nearest frame.
-- @tparam float seconds Time in seconds. Seconds can be a positive float value with two decimal places. No negative values allowed.
-- @tparam[opt=0] float minimum Minimum allowed value for seconds.
-- @tparam[opt="ConversionUtils.SecondsToTime"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Time `result`: Time Seconds converted to game frames, or 0 game frames if an error occurs (with a log message)
-- @treturn[1] bool `ok` true if conversion is successful, false if an error occurs.
-- @usage
-- -- Example: Convert 2 seconds to a Time object
-- local time, ok = ConversionUtils.SecondsToTime(2.0) -- Result: Time object representing 60 frames (2 seconds at 30 FPS)
--
-- -- Error handling example in a custom module function:
-- local function SetAnimationDuration(seconds)
--     local time, ok = ConversionUtils.SecondsToTime(seconds, 0, "SetAnimationDuration")
--     if not ok then
--         return  -- or fallback value
--     end
--     -- time is guaranteed valid here
--     animation:SetDuration(time)
-- end
--
-- -- Result of calling with invalid input:
-- SetAnimationDuration(-1) -- Logs: "Error in SetAnimationDuration: seconds must be greater than or equal to 0."
ConversionUtils.SecondsToTime = function (seconds, minimum, errorContext)
    errorContext = errorContext or "ConversionUtils.SecondsToTime"
    if not IsNumber(seconds) or seconds < 0 then
        ErrorLog("Error in {context}: seconds must be a positive number.", {context = errorContext })
        return Time(), false
    end
    if minimum and (not IsNumber(minimum) or minimum < 0) then
        WarningLog("Warning in {context}: minimum should be a number, using default 0.", {context = errorContext})
        minimum = 0
    end
    minimum = minimum or 0
    if seconds < minimum then
        ErrorLog("Error in {context}: seconds must be greater than or equal to {minimum}.", {context = errorContext, minimum = minimum})
        return Time(), false
    end
    return Time(floor(seconds * FPS)), true
end

--- Convert a hexadecimal color string to a TEN.Color object.
--
-- Allowed formats: "#RRGGBB", "RRGGBB", "#RRGGBBAA", "RRGGBBAA" (case-insensitive).
-- @tparam string hex The hexadecimal color string
-- @tparam[opt="ConversionUtils.HexToColor"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Color The TEN.Color object.
-- @treturn[2] nil If the input string is invalid.
-- @usage
-- -- Example with 6-digit hex (RGB):
-- local color = ConversionUtils.HexToColor("#FF5733") -- Result: TEN.Color(255, 87, 51, 255)
--
-- -- Example without hash:
-- local color = ConversionUtils.HexToColor("00FF00") -- Result: TEN.Color(0, 255, 0, 255)
--
-- -- Example with lowercase hex:
-- local color = ConversionUtils.HexToColor("#0000ff") -- Result: TEN.Color(0, 0, 255, 255)
--
-- -- Example with 8-digit hex (RGBA):
-- local color = ConversionUtils.HexToColor("#FF573380") -- Result: TEN.Color(255, 87, 51, 128)
--
-- -- Advanced: load color from config with error handling
-- local function ApplyThemeColor(sprite, hexColor)
--     local color = ConversionUtils.HexToColor(hexColor, "ApplyThemeColor")
--     if not color then
--         color = TEN.Color(255, 255, 255)  -- fallback: white
--         -- the error will be generated by HexToColor, no need to log again
--     end
--     sprite:SetColor(color)
-- end
--
-- -- Safe approach with default fallback:
-- local color = ConversionUtils.HexToColor(hexString) or TEN.Color(255, 255, 255, 255)
ConversionUtils.HexToColor = function(hex, errorContext)
    errorContext = errorContext or "ConversionUtils.HexToColor"
    if not IsString(hex) then
        ErrorLog("Error in {context}: hex must be a string.", {context = errorContext})
        return nil
    end

    -- Remove '#' if present
    hex = hex:gsub("^#", "")

    -- Get length of hex string
    local hexLen = #hex

    -- Validate length (6 for RGB, 8 for RGBA)
    if hexLen ~= 6 and hexLen ~= 8 then
        ErrorLog("Error in {context}: invalid hex string length. Expected 6 or 8 characters.", {context = errorContext})
        return nil
    end

    -- Extract color components
    local r = tonumber(hex:sub(1, 2), 16)
    local g = tonumber(hex:sub(3, 4), 16)
    local b = tonumber(hex:sub(5, 6), 16)
    local a = hexLen == 8 and tonumber(hex:sub(7, 8), 16) or 255

    -- Validate conversion
    if not (r and g and b and a) then
        ErrorLog("Error in {context}: invalid hexadecimal values.", {context = errorContext})
        return nil
    end

    return Color(r, g, b, a)
end

--- Convert a TEN.Color object to HSL (Hue, Saturation, Lightness) values.
-- Uses the Color:GetHue() method for accurate hue extraction.
-- @tparam Color color The TEN.Color object to convert.
-- @tparam[opt="ConversionUtils.ColorToHSL"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] HSLData A table with h, s, l, a values { h = float, s = float, l = float, a = float }.
-- @treturn[2] nil If the parameter is not a valid TEN Color.
-- @usage
-- -- Example: Get HSL values from a color
-- local color = TEN.Color(255, 87, 51, 255)
-- local hsl = ConversionUtils.ColorToHSL(color)
-- -- Result: { h = 14.0, s = 1.0, l = 0.6, a = 1.0 }
--
-- -- Invalid color example:
-- local invalidColor = "not_a_color"
-- local hsl = ConversionUtils.ColorToHSL(invalidColor)
-- -- Result: nil (invalid color)
--
-- -- Advanced: desaturate an enemy on defeat
-- local function DesaturateEnemy(moveable)
--     local original = moveable:GetColor()
--     local hsl = ConversionUtils.ColorToHSL(original, "DesaturateEnemy")
--     if not hsl then 
--         return
--         -- the error will be generated by ColorToHSL, no need to log again
--     end
--     hsl.s = hsl.s * 0.2  -- Reduce saturation to 20%
--     moveable:SetColor(ConversionUtils.HSLtoColor(hsl))
-- end
--
-- -- Safe approach with default fallback:
-- local hsl = ConversionUtils.ColorToHSL(color) or { h = 0, s = 0, l = 0, a = 1.0 }
ConversionUtils.ColorToHSL = function(color, errorContext)
    errorContext = errorContext or "ConversionUtils.ColorToHSL"
    if not IsColor(color) then
        ErrorLog("Error in {context}: color must be a Color object.", {context = errorContext})
        return nil
    end

    local h, s, l = ColorToHSLRaw(color)
    return { h = h, s = s, l = l, a = color.a / 255 }
end

--- Convert an HSL color table to a TEN.Color object. Typically used with the table returned by @{ColorToHSL}.
-- Out-of-range values are clamped to valid ranges with a warning; only invalid types cause an error.
-- @tparam HSLData hsl An `HSLData` table with h, s, l, a fields.
-- @tparam[opt="ConversionUtils.HSLtoColor"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Color The TEN.Color object (always valid — out-of-range values are clamped with a warning).
-- @treturn[2] nil If the input is not a table, or if h, s, l are not numbers.
-- @usage
-- -- From a ColorToHSL round-trip:
-- local hsl = ConversionUtils.ColorToHSL(TEN.Color(255, 100, 50))
-- local color = ConversionUtils.HSLtoColor(hsl)
--
-- -- From a literal table:
-- local red = ConversionUtils.HSLtoColor({h = 0, s = 1, l = 0.5}) -- Result: TEN.Color(255, 0, 0)
-- local cyan = ConversionUtils.HSLtoColor({h = 180, s = 1, l = 0.5}) -- Result: TEN.Color(0, 255, 255)
--
-- -- Practical example: Adjust saturation
-- local originalColor = TEN.Color(255, 100, 50, 255)
-- local hsl = ConversionUtils.ColorToHSL(originalColor) -- convert to HSL color table
-- if hsl then
--     hsl.s = hsl.s * 0.5  -- edit color: Reduce saturation by 50%
--     local desaturatedColor = ConversionUtils.HSLtoColor(hsl) -- recorverted in TEN.Color
--     sprite:SetColor(desaturatedColor)
-- end
--
-- -- Practical example 2: Brighten a color
-- local darkColor = TEN.Color(50, 50, 150, 255)
-- local hsl = ConversionUtils.ColorToHSL(darkColor)
-- if hsl then
--     hsl.l = math.min(1.0, hsl.l + 0.2)  -- edit color: Increase lightness by 20%
--     local brighterColor = ConversionUtils.HSLtoColor(hsl) -- recorverted in TEN.Color
--     sprite:SetColor(brighterColor)
-- end
--
-- -- Advanced example: Rainbow function using callbacks
-- local obj = TEN.Objects.GetMoveableByName("ColorWheel")
-- local hsl = ConversionUtils.ColorToHSL(TEN.Color(255, 0, 0))  -- Start with red
-- local hueAngle = 0
-- local hueSpeed = 360 / ConversionUtils.SecondsToFrames(8)
-- LevelFuncs.Rainbow = function()
--     hueAngle = (hueAngle + hueSpeed) % 360
--     hsl.h = hueAngle -- edit color: Update hue for rainbow effect
--     local color = ConversionUtils.HSLtoColor(hsl) -- recorverted in TEN.Color
--     obj:SetColor(color)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.Rainbow)
--
-- -- Out-of-range handling: values are clamped with a warning.
-- local orange = ConversionUtils.HSLtoColor({h = 400, s = 1, l = 0.5}) -- h=400 wrapped to 40°
-- sprite:SetColor(orange)  -- Always safe with a warning message, no nil check needed
ConversionUtils.HSLtoColor = function(hsl, errorContext)
    errorContext = errorContext or "ConversionUtils.HSLtoColor"
    if not IsTable(hsl) then
        ErrorLog("Error in {context}: expected an HSLData table.", {context = errorContext})
        return nil
    end

    local h = hsl.h
    local s = hsl.s
    local l = hsl.l
    local a = hsl.a

    -- Validate parameters
    if not (IsNumber(h) and IsNumber(s) and IsNumber(l)) then
        ErrorLog("Error in {context}: expected an HSLData table.", {context = errorContext})
        return nil
    end
    if h < 0 or h > 360 then
        WarningLog("Warning in {context}: h = {h} is outside range [0, 360]. Wrapping to [0, 360).", {context = errorContext, h = h})
        h = h % 360
    end
    if s < 0 or s > 1 then
        WarningLog("Warning in {context}: s = {s} is outside range [0, 1]. Clamping.", {context = errorContext, s = s})
        s = max(0, min(1, s))
    end
    if l < 0 or l > 1 then
        WarningLog("Warning in {context}: l = {l} is outside range [0, 1]. Clamping.", {context = errorContext, l = l})
        l = max(0, min(1, l))
    end

    -- Default alpha to 1.0 if not provided or not a number
    if a == nil then
        a = 1.0
    elseif not IsNumber(a) then
        WarningLog("Warning in {context}: a should be a number. Defaulting to 1.0.", {context = errorContext})
        a = 1.0
    elseif a < 0 or a > 1 then
        WarningLog("Warning in {context}: a = {a} is outside range [0, 1]. Clamping.", {context = errorContext, a = a})
        a = max(0, min(1, a))
    end

    return HSLtoColorRaw(h, s, l, a)
end

--- Convert a TEN.Color object to OKLch (Lightness, Chroma, Hue) values.
--
-- OKLch is a perceptually uniform color space, ideal for:
--
-- - Color interpolations that look smooth to human eyes
--
-- - Adjusting saturation (chroma) without affecting perceived brightness
--
-- - Rainbow gradients with consistent perceived brightness
-- @tparam Color color The TEN.Color object to convert.
-- @tparam[opt="ConversionUtils.ColorToOKLch"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] OKLchData A table with l, c, h, a values { l = float (0-1), c = float (0-0.4), h = float (0-360), a = float (0-1) }.
-- @treturn[2] nil If the parameter is not a valid TEN Color.
-- @usage
-- -- Example: Get OKLch values from a color
-- local color = TEN.Color(255, 87, 51, 255)
-- local oklch = ConversionUtils.ColorToOKLch(color)
-- -- Result: { l = 0.68, c = 0.18, h = 29.2, a = 1.0 }
--
-- -- Invalid color example:
-- local invalidColor = "not_a_color"
-- local oklch = ConversionUtils.ColorToOKLch(invalidColor)
-- -- Result: nil (invalid color)
--
-- -- Advanced: adjust chroma in a module function
-- local function ReduceVividness(moveable)
--     local original = moveable:GetColor()
--     local oklch = ConversionUtils.ColorToOKLch(original, "ReduceVividness")
--     if not oklch then return end
--     -- the error will be generated by ColorToOKLch, no need to log again
--     oklch.c = oklch.c * 0.3
--     moveable:SetColor(ConversionUtils.OKLchToColor(oklch))
-- end
--
-- -- Safe approach with default fallback:
-- local oklch = ConversionUtils.ColorToOKLch(color) or { l = 0.5, c = 0, h = 0, a = 1.0 }
ConversionUtils.ColorToOKLch = function(color, errorContext)
    errorContext = errorContext or "ConversionUtils.ColorToOKLch"
    if not IsColor(color) then
        ErrorLog("Error in {context}: color must be a Color object.", {context = errorContext})
        return nil
    end

    local L, C, h = ColorToOKLchRaw(color)
    return { l = L, c = C, h = h, a = color.a / 255 }
end

--- Convert an OKLch color table to a TEN.Color object. Typically used with the table returned by @{ColorToOKLch}.
-- OKLch is a perceptually uniform color space, ideal for smooth color transitions. Out-of-range values are clamped to valid ranges with a warning; only invalid types cause an error.
--
-- Important: **Not all combinations of l, c, h can be displayed on a standard monitor (sRGB)**.
-- High chroma values may be silently adjusted (colors become less vivid or shift slightly).
-- This is most noticeable with blues at high brightness. To avoid this:
--
-- - Use lower chroma (c ≤ 0.15) for full hue cycles — all colors will display correctly
--
-- - Use higher chroma (up to 0.4) only for specific hues like red or orange
--
-- - When in doubt, start with c = 0.15 and increase until the result looks right
--
-- For full-spectrum color cycling (e.g. rainbow effects), prefer `HSLtoColor` which always produces displayable colors, at the cost of non-uniform perceived brightness.
-- @tparam OKLchData oklch An `OKLchData` table with l, c, h, a fields.
-- @tparam[opt="ConversionUtils.OKLchToColor"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Color The TEN.Color object (always valid — out-of-range values are clamped with a warning).
-- @treturn[2] nil If the input is not a table, or if l, c, h are not numbers.
-- @usage
-- -- From a ColorToOKLch round-trip:
-- local oklch = ConversionUtils.ColorToOKLch(TEN.Color(255, 0, 128))
-- local color = ConversionUtils.OKLchToColor(oklch)  -- Pass the table directly
--
-- -- From a literal table:
-- local red = ConversionUtils.OKLchToColor({l = 0.63, c = 0.26, h = 29})
-- local gray = ConversionUtils.OKLchToColor({l = 0.5, c = 0, h = 0})  -- Hue irrelevant when c=0
--
-- -- Practical example: Desaturate while preserving perceived brightness
-- local vividColor = TEN.Color(255, 0, 128)
-- local oklch = ConversionUtils.ColorToOKLch(vividColor) -- convert to OKLch color table
-- if oklch then
--     oklch.c = oklch.c * 0.5  -- edit color: Reduce chroma by 50%
--     local desaturatedColor = ConversionUtils.OKLchToColor(oklch)  -- reconverted in TEN.Color
--     sprite:SetColor(desaturatedColor)
-- end
--
-- -- Example: Brighten color perceptually uniformly
-- local obj = TEN.Objects.GetMoveableByName("Object")
-- local darkColor = TEN.Color(50, 50, 150, 255)
-- local oklch = ConversionUtils.ColorToOKLch(darkColor) -- convert to OKLch color table
-- if oklch then
--     oklch.l = math.min(1.0, oklch.l + 0.2)  -- edit color: Increase lightness
--     local brighterColor = ConversionUtils.OKLchToColor(oklch)  -- reconverted in TEN.Color to apply color to object
--     obj:SetColor(brighterColor)
-- end
--
-- -- Advanced example: Uniform brightness rainbow function using callbacks
-- local obj = TEN.Objects.GetMoveableByName("ColorWheel")
-- local oklch = ConversionUtils.ColorToOKLch(TEN.Color(255, 0, 0))  -- Start with red
-- local hueAngle = 0
-- local hueSpeed = 360 / ConversionUtils.SecondsToFrames(8)
-- LevelFuncs.RainbowOKLch = function()
--     hueAngle = (hueAngle + hueSpeed) % 360
--     oklch.h = hueAngle -- edit color: Update hue for rainbow effect
--     local color = ConversionUtils.OKLchToColor(oklch)  -- reconverted in TEN.Color to apply color to object
--     obj:SetColor(color)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.RainbowOKLch)
--
-- -- Example: Torch flicker simulation (warm color oscillation) using callbacks
-- local torchLight = TEN.Objects.GetMoveableByName("TorchFlame")
-- local warmOrange = ConversionUtils.ColorToOKLch(TEN.Color(255, 120, 40, 255))
-- local brightYellow = ConversionUtils.ColorToOKLch(TEN.Color(255, 200, 80, 255))
-- local flickerTime = 0
-- local flickerSpeed = 1 / ConversionUtils.SecondsToFrames(0.5)
-- LevelFuncs.TorchFlicker = function()
--     flickerTime = (flickerTime + flickerSpeed) % 1
--     local t = (math.sin(flickerTime * math.pi * 2) + 1) / 2
--     local color = ConversionUtils.OKLchToColor({
--         l = warmOrange.l + (brightYellow.l - warmOrange.l) * t,
--         c = warmOrange.c + (brightYellow.c - warmOrange.c) * t,
--         h = warmOrange.h + (brightYellow.h - warmOrange.h) * t
--     })
--     torchLight:SetColor(color)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.TorchFlicker)
--
-- -- Example: Lava pulse simulation (brightness variation) using callbacks
-- local lavaObj = TEN.Objects.GetMoveableByName("LavaGlow")
-- local baseLava = ConversionUtils.ColorToOKLch(TEN.Color(200, 60, 20))
-- local baseLavaL = baseLava.l  -- Store the original lightness value
-- local pulseTime = 0
-- local pulseSpeed = 1 / ConversionUtils.SecondsToFrames(2)
-- LevelFuncs.LavaPulse = function()
--     pulseTime = (pulseTime + pulseSpeed) % 1
--     local pulse = (math.sin(pulseTime * math.pi * 2) + 1) / 2
--     baseLava.l = baseLavaL + pulse * 0.15  -- edit color: Modifying lightness for a pulsing effect
--     local color = ConversionUtils.OKLchToColor(baseLava)  -- reconverted in TEN.Color to apply color to object
--     lavaObj:SetColor(color)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.LavaPulse)
--
-- -- Advanced: apply a themed color with error context
-- local function ApplyAccentColor(sprite, oklchColor)
--     local color = ConversionUtils.OKLchToColor(oklchColor, "ApplyAccentColor")
--     if not color then
--         color = TEN.Color(255, 255, 255)  -- fallback: white
--         -- the error will be generated by OKLchToColor, no need to log again
--     end
--     sprite:SetColor(color)
-- end
--
-- -- Out-of-range handling: values are clamped with a warning.
-- local color = ConversionUtils.OKLchToColor({l = -0.2, c = 0.9, h = 400})
-- -- l=-0.2 clamped to 0, c=0.9 clamped to 0.4, h=400 wrapped to 40°
-- sprite:SetColor(color)  -- Always safe with a warning message, no nil check needed
ConversionUtils.OKLchToColor = function(oklch, errorContext)
    errorContext = errorContext or "ConversionUtils.OKLchToColor"
    if not IsTable(oklch) then
        ErrorLog("Error in {context}: expected an OKLchData table.", {context = errorContext})
        return nil
    end

    local l = oklch.l
    local c = oklch.c
    local h = oklch.h
    local a = oklch.a

    -- Validate parameters
    if not (IsNumber(l) and IsNumber(c) and IsNumber(h)) then
        ErrorLog("Error in {context}: l, c, h must be numbers.", {context = errorContext})
        return nil
    end
    if l < 0 or l > 1 then
        WarningLog("Warning in {context}: l = {l} is outside range [0, 1]. Clamping.", {context = errorContext, l = l})
        l = max(0, min(1, l))
    end
    if c < 0 or c > 0.4 then
        WarningLog("Warning in {context}: c = {c} is outside sRGB safe range [0, 0.4]. Clamping. Colors may be less vivid or shift slightly.", {context = errorContext, c = c})
        c = max(0, min(0.4, c))
    end
    if h < 0 or h > 360 then
        WarningLog("Warning in {context}: h = {h} is outside range [0, 360]. Wrapping to [0, 360).", {context = errorContext, c = c})
        h = h % 360
    end

    -- Default alpha to 1.0 if not provided or not a number
    if a == nil then
        a = 1.0
    elseif not IsNumber(a) then
        WarningLog("Warning in {context}: a should be a number. Defaulting to 1.0.", {context = errorContext})
        a = 1.0
    elseif a < 0 or a > 1 then
        WarningLog("Warning in {context}: a = {a} is outside range [0, 1]. Clamping.", {context = errorContext, a = a})
        a = max(0, min(1, a))
    end

    return OKLchToColorRaw(l, c, h, a)
end

----
-- Color Tables
--
-- Tables used in HSL and OKLch conversion methods.
-- @section tables_color_conversion

---
-- Table setup for creating HSL colors.
-- @table HSLData
-- @tfield float h Hue value (0.0 to 360.0 degrees)
-- @tfield float s Saturation value (0.0 to 1.0)
-- @tfield float l Lightness value (0.0 to 1.0)
-- @tfield[opt=1.0] float a Alpha value (0.0 to 1.0)

---
-- Table setup for creating OKLch colors.
-- @table OKLchData
-- @tfield float l Lightness value (0.0 to 1.0)
-- @tfield float c Chroma value (0.0 to ~0.4)
-- @tfield float h Hue angle in degrees (0 to 360)
-- @tfield[opt=1.0] float a Alpha value (0.0 to 1.0)

return ConversionUtils