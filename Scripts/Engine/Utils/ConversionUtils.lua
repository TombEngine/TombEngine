-----<style>table.function_list td.name {min-width: 291px;}</style>
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
---	local ConversionUtils = require("Engine.ConversionUtils")
-- @luautil ConversionUtils

local ConversionUtils = {}
local Type = require("Engine.Type")
local Util = require("Engine.Util")

local FPS = 30 -- Default frames per second for time-frame conversions
local logLevelEnums = TEN.Util.LogLevel
local logLevelError  = logLevelEnums.ERROR
local logLevelWarning = logLevelEnums.WARNING

local HSLtoColorRaw = Util.HSLtoColorRaw
local ColorToHSLRaw = Util.ColorToHSLRaw
local ColorToOKLchRaw = Util.ColorToOKLchRaw
local OKLchToColorRaw = Util.OKLchToColorRaw

local IsNumber = Type.IsNumber
local IsColor = Type.IsColor
local IsString = Type.IsString

local floor = math.floor
local max = math.max
local min = math.min

local LogMessage  = TEN.Util.PrintLog
local Color = TEN.Color

--- Convert seconds to frames (assuming 30 FPS).
-- @tparam float seconds Time in seconds. Seconds can be a float value with two decimal places.
-- @tparam[opt=30] int fps Frames per second.
-- @treturn[1] float Number of frames.
-- @treturn[2] int 0 If an error occurs.
-- @usage
-- local frames = ConversionUtils.SecondsToFrames(2.0) -- Result: 60
ConversionUtils.SecondsToFrames = function(seconds, fps)
    fps = fps or FPS
    if not IsNumber(seconds) or not IsNumber(fps) then
        LogMessage("Error in ConversionUtils.SecondsToFrames: seconds and fps must be numbers.", logLevelError)
        return 0
    end

    -- Check if fps is a float and warn user
    if fps ~= floor(fps) then
        LogMessage("Warning in ConversionUtils.SecondsToFrames: fps should be an integer. Rounding " .. fps .. " to " .. floor(fps + 0.5) .. ".", logLevelWarning)
        fps = floor(fps + 0.5)
    end

    return floor(seconds * fps + 0.5)
end

--- Convert frames to seconds (assuming 30 FPS).
-- @tparam int frames Number of frames.
-- @tparam[opt=30] int fps Frames per second. No negative values allowed.
-- @treturn[1] float Time in seconds.
-- @treturn[2] float 0 If an error occurs.
-- @usage
-- local seconds = ConversionUtils.FramesToSeconds(60) -- Result: 2.0
ConversionUtils.FramesToSeconds = function(frames, fps)
    fps = fps or FPS
    if not IsNumber(frames) or frames < 0 or (fps and not IsNumber(fps)) then
        LogMessage("Error in ConversionUtils.FramesToSeconds: frames and fps must be numbers.", logLevelError)
        return 0
    end

    -- Check if frames is a float and warn user
    if frames ~= floor(frames) then
        LogMessage("Warning in ConversionUtils.FramesToSeconds: frames should be an integer. Rounding " .. frames .. " to " .. floor(frames + 0.5) .. ".", logLevelWarning)
        frames = floor(frames + 0.5)
    end

    -- Check if fps is a float and warn user
    if fps ~= floor(fps) then
        LogMessage("Warning in ConversionUtils.FramesToSeconds: fps should be an integer. Rounding " .. fps .. " to " .. floor(fps + 0.5) .. ".", logLevelWarning)
        fps = floor(fps + 0.5)
    end

    if fps == 0 then
        LogMessage("Error in ConversionUtils.FramesToSeconds: fps cannot be zero.", logLevelError)
        return 0
    end

    return frames / fps
end

--- Convert a hexadecimal color string to a TEN.Color object.
--
-- Allowed formats: "#RRGGBB", "RRGGBB", "#RRGGBBAA", "RRGGBBAA" (case-insensitive).
-- @tparam string hex The hexadecimal color string
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
-- -- Error handling example:
-- local color = ConversionUtils.HexToColor("GHIJKL") -- Result: nil (invalid hex string)
-- if color == nil then
--     TEN.Util.PrintLog("Failed to convert hex to color", TEN.Util.LogLevel.ERROR)
--     return  -- or use a fallback value
-- end
-- -- Apply color to a sprite
-- sprite:SetColor(color)
--
-- -- Safe approach with default fallback:
-- local color = ConversionUtils.HexToColor(hexString) or TEN.Color(255, 255, 255, 255)
ConversionUtils.HexToColor = function(hex)
    if not IsString(hex) then
        LogMessage("Error in ConversionUtils.HexToColor: hex must be a string.", logLevelError)
        return nil
    end

    -- Remove '#' if present
    hex = hex:gsub("^#", "")

    -- Get length of hex string
    local hexLen = #hex

    -- Validate length (6 for RGB, 8 for RGBA)
    if hexLen ~= 6 and hexLen ~= 8 then
        LogMessage("Error in ConversionUtils.HexToColor: invalid hex string length. Expected 6 or 8 characters.", logLevelError)
        return nil
    end

    -- Extract color components
    local r = tonumber(hex:sub(1, 2), 16)
    local g = tonumber(hex:sub(3, 4), 16)
    local b = tonumber(hex:sub(5, 6), 16)
    local a = hexLen == 8 and tonumber(hex:sub(7, 8), 16) or 255

    -- Validate conversion
    if not (r and g and b and a) then
        LogMessage("Error in ConversionUtils.HexToColor: invalid hexadecimal values.", logLevelError)
        return nil
    end

    return Color(r, g, b, a)
end

--- Convert HSL (Hue, Saturation, Lightness) values to a TEN.Color object. All values are clamped to valid ranges.
-- @tparam float h Hue value (0.0 to 360.0 degrees).
-- @tparam float s Saturation value (0.0 to 1.0).
-- @tparam float l Lightness value (0.0 to 1.0).
-- @tparam[opt=1.0] float a Alpha value (0.0 to 1.0).
-- @treturn[1] Color The TEN.Color object.
-- @treturn[2] nil If parameters are invalid.
-- @usage
-- -- Example: Pure red
-- local color = ConversionUtils.HSLtoColor(0, 1, 0.5) -- Result: TEN.Color(255, 0, 0, 255)
--
-- -- Example: Cyan
-- local color = ConversionUtils.HSLtoColor(180, 1, 0.5) -- Result: TEN.Color(0, 255, 255, 255)
--
-- -- Example: Semi-transparent yellow
-- local color = ConversionUtils.HSLtoColor(60, 1, 0.5, 0.5) -- Result: TEN.Color(255, 255, 0, 127)
--
-- -- Example: Desaturated blue (gray-blue)
-- local color = ConversionUtils.HSLtoColor(240, 0.3, 0.5) -- Result: TEN.Color(89, 89, 165, 255)
--
-- -- Error handling example:
-- local color = ConversionUtils.HSLtoColor(400, 1, 0.5) -- Result: nil (invalid hue)
-- if color == nil then
--     TEN.Util.PrintLog("Failed to convert HSL to color", TEN.Util.LogLevel.ERROR)
--     return  -- or use a fallback value
-- end
-- -- Apply color to a sprite
-- sprite:SetColor(color)
--
-- -- Safe approach with default fallback:
-- local color = ConversionUtils.HSLtoColor(hue, saturation, lightness, alpha) or TEN.Color(255, 255, 255, 255)
ConversionUtils.HSLtoColor = function(h, s, l, a)
    -- Validate parameters
    if not (IsNumber(h) and IsNumber(s) and IsNumber(l)) then
        LogMessage("Error in ConversionUtils.HSLtoColor: h, s, and l must be numbers.", logLevelError)
        return nil
    end

    -- Default alpha to 1.0 if not provided
    a = a or 1.0

    if not IsNumber(a) then
        LogMessage("Warning in ConversionUtils.HSLtoColor: a should be a number. Defaulting to 1.0.", logLevelWarning)
        a = 1.0
    end

    -- Clamp values to valid ranges
    h = h % 360
    s = max(0, min(1, s))
    l = max(0, min(1, l))
    a = max(0, min(1, a))

    return HSLtoColorRaw(h, s, l, a)
end

--- Convert a TEN.Color object to HSL (Hue, Saturation, Lightness) values.
-- Uses the Color:GetHue() method for accurate hue extraction.
-- @tparam Color color The TEN.Color object to convert.
-- @treturn[1] table A table with h, s, l, a values { h = float, s = float, l = float, a = float }.
-- @treturn[2] nil If the parameter is not a valid TEN Color.
-- @usage
-- -- Example: Get HSL values from a color
-- local color = TEN.Color(255, 87, 51, 255)
-- local hsl = ConversionUtils.ColorToHSL(color)
-- -- Result: { h = 14.0, s = 1.0, l = 0.6, a = 1.0 }
--
-- -- Practical example: Adjust saturation
-- local originalColor = TEN.Color(255, 100, 50, 255)
-- local hsl = ConversionUtils.ColorToHSL(originalColor)
-- if hsl then
--     hsl.s = hsl.s * 0.5  -- Reduce saturation by 50%
--     local desaturatedColor = ConversionUtils.HSLtoColor(hsl.h, hsl.s, hsl.l, hsl.a)
--     -- Apply color to sprite
--     sprite:SetColor(desaturatedColor)
-- end
--
-- -- Example: Brighten a color
-- local darkColor = TEN.Color(50, 50, 150, 255)
-- local hsl = ConversionUtils.ColorToHSL(darkColor)
-- if hsl then
--     hsl.l = math.min(1.0, hsl.l + 0.2)  -- Increase lightness by 20%
--     local brighterColor = ConversionUtils.HSLtoColor(hsl.h, hsl.s, hsl.l, hsl.a)
--     sprite:SetColor(brighterColor)
-- end
--
-- -- Error handling example:
-- local hsl = ConversionUtils.ColorToHSL(invalidColor)
-- if hsl == nil then
--     TEN.Util.PrintLog("Failed to convert color to HSL", TEN.Util.LogLevel.ERROR)
--     return
-- end
--
-- -- Safe approach with default fallback:
-- local hsl = ConversionUtils.ColorToHSL(color) or { h = 0, s = 0, l = 0, a = 1.0 }
ConversionUtils.ColorToHSL = function(color)
    if not IsColor(color) then
        LogMessage("Error in ConversionUtils.ColorToHSL: color must be a Color object.", logLevelError)
        return nil
    end

    local h, s, l = ColorToHSLRaw(color)
    return { h = h, s = s, l = l, a = color.a / 255 }
end

--- Convert a TEN.Color object to OKLch (Lightness, Chroma, Hue) values.
-- OKLch is a perceptually uniform color space, ideal for:
-- - Color interpolations that look smooth to human eyes
-- - Adjusting saturation (chroma) without affecting perceived brightness
-- - Rainbow gradients with consistent perceived brightness
-- @tparam Color color The TEN.Color object to convert.
-- @treturn[1] table A table with l, c, h, a values { l = float (0-1), c = float (0-0.4), h = float (0-360), a = float (0-1) }.
-- @treturn[2] nil If the parameter is not a valid TEN Color.
-- @usage
-- -- Example: Get OKLch values from a color
-- local color = TEN.Color(255, 87, 51, 255)
-- local oklch = ConversionUtils.ColorToOKLch(color)
-- -- Result: { l = 0.68, c = 0.18, h = 29.2, a = 1.0 }
--
-- -- Practical example: Desaturate while preserving perceived brightness
-- local vividColor = TEN.Color(255, 0, 128, 255)
-- local oklch = ConversionUtils.ColorToOKLch(vividColor)
-- if oklch then
--     oklch.c = oklch.c * 0.5  -- Reduce chroma by 50%
--     local desaturatedColor = ConversionUtils.OKLchToColor(oklch.l, oklch.c, oklch.h, oklch.a)
--     sprite:SetColor(desaturatedColor)
-- end
--
-- -- Example: Rainbow gradient with uniform brightness - Complete working example
-- local rainbowObj = TEN.Objects.GetMoveableByName("rainbowObject")
-- local baseColor = TEN.Color(255, 0, 0, 255)  -- Start with red
-- local oklch = ConversionUtils.ColorToOKLch(baseColor)
-- local hueAngle = 0
-- local hueSpeed = 360 / ConversionUtils.SecondsToFrames(5)  -- Full rainbow cycle in 5 seconds
-- LevelFuncs.OnLoop = function()
--     hueAngle = (hueAngle + hueSpeed) % 360
--     local rainbowColor = ConversionUtils.OKLchToColor(oklch.l, oklch.c, hueAngle, oklch.a)
--     rainbowObj:SetColor(rainbowColor)
--     -- All colors have same perceived brightness throughout the cycle!
-- end
--
-- -- Example: Brighten color perceptually uniformly
-- local obj = TEN.Objects.GetMoveableByName("Object")
-- local darkColor = TEN.Color(50, 50, 150, 255)
-- local oklch = ConversionUtils.ColorToOKLch(darkColor)
-- if oklch then
--     oklch.l = math.min(1.0, oklch.l + 0.2)  -- Increase lightness
--     local brighterColor = ConversionUtils.OKLchToColor(oklch.l, oklch.c, oklch.h, oklch.a)
--     obj:SetColor(brighterColor)
-- end
--
-- -- Error handling example:
-- local oklch = ConversionUtils.ColorToOKLch(invalidColor)
-- if oklch == nil then
--     TEN.Util.PrintLog("Failed to convert color to OKLch", TEN.Util.LogLevel.ERROR)
--     return
-- end
--
-- -- Safe approach with default fallback:
-- local oklch = ConversionUtils.ColorToOKLch(color) or { l = 0.5, c = 0, h = 0, a = 1.0 }
ConversionUtils.ColorToOKLch = function(color)
    if not IsColor(color) then
        LogMessage("Error in ConversionUtils.ColorToOKLch: color must be a Color object.", logLevelError)
        return nil
    end

    local L, C, h = ColorToOKLchRaw(color)
    return { l = L, c = C, h = h, a = color.a / 255 }
end

--- Convert OKLch (Lightness, Chroma, Hue) values to a TEN.Color object.
-- OKLch is a perceptually uniform color space, ideal for smooth color transitions. All values are clamped to valid ranges.
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
-- <p style = "margin: 10px 0 0 0px;">For full-spectrum color cycling (e.g. rainbow effects), prefer `HSLtoColor` which always produces displayable colors, at the cost of non-uniform perceived brightness.</p>
-- @tparam float l Lightness value (0.0 to 1.0, where 0 = black, 1 = white).
-- @tparam float c Chroma value (0.0 to ~0.4, where 0 = gray, higher = more saturated).
-- @tparam float h Hue angle in degrees (0 to 360).
-- @tparam[opt=1.0] float a Alpha value (0.0 to 1.0).
-- @treturn[1] Color The TEN.Color object.
-- @treturn[2] nil If parameters are invalid.
-- @usage
-- -- Example: Create pure red in OKLch
-- local red = ConversionUtils.OKLchToColor(0.63, 0.26, 29, 1.0)
--
-- -- Example: Create gray (zero chroma)
-- local gray = ConversionUtils.OKLchToColor(0.5, 0, 0, 1.0)  -- Hue irrelevant when c=0
--
-- -- Example: Rainbow with uniform brightness - Complete working example
-- local obj = TEN.Objects.GetMoveableByName("ColorWheel")
-- local lightness = 0.7   -- Fixed lightness for uniform brightness
-- local chroma = 0.15     -- Fixed saturation
-- local hueAngle = 0
-- local hueSpeed = 360 / ConversionUtils.SecondsToFrames(8)  -- 8 seconds per full cycle
-- LevelFuncs.OnLoop = function()
--     hueAngle = (hueAngle + hueSpeed) % 360
--     local color = ConversionUtils.OKLchToColor(lightness, chroma, hueAngle, 1.0)
--     obj:SetColor(color)
--     -- All hues appear equally bright!
-- end
--
-- -- Example: Torch flicker (warm color oscillation) - Complete working example
-- -- OKLch is ideal for this: smooth transitions with perceptually uniform brightness
-- local torchLight = TEN.Objects.GetMoveableByName("TorchFlame")
-- local warmOrange = ConversionUtils.ColorToOKLch(TEN.Color(255, 120, 40, 255))  -- h ≈ 45°
-- local brightYellow = ConversionUtils.ColorToOKLch(TEN.Color(255, 200, 80, 255))  -- h ≈ 55°
-- local flickerTime = 0
-- local flickerSpeed = 1 / ConversionUtils.SecondsToFrames(0.5)  -- 0.5 second cycle
-- LevelFuncs.OnLoop = function()
--     flickerTime = (flickerTime + flickerSpeed) % 1
--     -- Sine wave for smooth back-and-forth oscillation
--     local t = (math.sin(flickerTime * math.pi * 2) + 1) / 2
--     local l = warmOrange.l + (brightYellow.l - warmOrange.l) * t
--     local c = warmOrange.c + (brightYellow.c - warmOrange.c) * t
--     local h = warmOrange.h + (brightYellow.h - warmOrange.h) * t
--     local color = ConversionUtils.OKLchToColor(l, c, h, 1.0)
--     torchLight:SetColor(color)
--     -- Smooth, natural-looking flame flicker!
-- end
--
-- -- Example: Lava pulse (brightness variation) - Complete working example
-- -- Demonstrates OKLch advantage: changing lightness without color shift
-- local lavaObj = TEN.Objects.GetMoveableByName("LavaGlow")
-- local baseLava = ConversionUtils.ColorToOKLch(TEN.Color(200, 60, 20))  -- Dark red-orange
-- local pulseTime = 0
-- local pulseSpeed = 1 / ConversionUtils.SecondsToFrames(2)  -- 2 second pulse cycle
-- LevelFuncs.OnLoop = function()
--     pulseTime = (pulseTime + pulseSpeed) % 1
--     -- Pulse lightness between base and +0.15 brighter
--     local pulse = (math.sin(pulseTime * math.pi * 2) + 1) / 2
--     local l = baseLava.l + pulse * 0.15
--     local color = ConversionUtils.OKLchToColor(l, baseLava.c, baseLava.h, 1.0)
--     lavaObj:SetColor(color)
--     -- Brightness pulses without hue shift (unlike HSL which would shift toward white)
-- end
--
-- -- Error handling example:
-- local color = ConversionUtils.OKLchToColor(0.5, 0.2, 180)
-- if color == nil then
--     TEN.Util.PrintLog("Failed to convert OKLch to color", TEN.Util.LogLevel.ERROR)
--     return
-- end
--
-- -- Safe approach with default fallback:
-- local color = ConversionUtils.OKLchToColor(l, c, h, a) or TEN.Color(128, 128, 128, 255)
ConversionUtils.OKLchToColor = function(l, c, h, a)
    -- Validate parameters
    if not (IsNumber(l) and IsNumber(c) and IsNumber(h)) then
        LogMessage("Error in ConversionUtils.OKLchToColor: l, c, h must be numbers.", logLevelError)
        return nil
    end

    -- Default alpha to 1.0 if not provided
    a = a or 1.0

    if not IsNumber(a) then
        LogMessage("Warning in ConversionUtils.OKLchToColor: a should be a number. Defaulting to 1.0.", logLevelWarning)
        a = 1.0
    end

    -- Clamp values to valid ranges
    l = max(0, min(1, l))
    c = max(0, min(0.4, c))
    h = h % 360
    a = max(0, min(1, a))

    return OKLchToColorRaw(l, c, h, a)
end

return ConversionUtils