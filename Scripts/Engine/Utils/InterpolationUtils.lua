-----<style>table.function_list td.name {min-width: 365px;}</style>
--- Lua support functions for interpolating between values.
-- Different interpolation methods provide various speed curves and behaviors.
--
-- <h3>Interpolation methods comparison:</h3>
-- <style> table, th, td {border: 1px solid black;} .tableSP {border-collapse: collapse; width: 100%; text-align: center; } .tableSP th {background-color: #525252; color: white; padding: 6px;}</style>
-- <style> .tableSP td {padding: 4px;} .tableSP tr:nth-child(even) {background-color: #f2f2f2;} .tableSP tr:hover {background-color: #ddd;}</style>
-- <table class="tableSP">
-- <tr><th>Method</th><th>Speed curve</th><th>Behavior</th><th>Use case</th></tr>
-- <tr><td>`Lerp`</td><td>Linear</td><td>Constant speed throughout</td><td>Simple animations, mechanical movements</td></tr>
-- <tr><td>`Smoothstep`</td><td>Smooth S-curve</td><td>Gentle ease-in and ease-out</td><td>UI transitions, standard animations</td></tr>
-- <tr><td>`Smootherstep`</td><td>Ultra-smooth S-curve</td><td>Very gentle ease-in/out (C² continuity)</td><td>Cinematic effects, premium visuals</td></tr>
-- <tr><td>`EaseInOut`</td><td>Quadratic curve</td><td>Pronounced acceleration/deceleration</td><td>Dramatic movements, elevators</td></tr>
-- <tr><td>`Elastic`</td><td>Spring oscillation</td><td>Overshoot with smooth bounce back</td><td>Playful UI, cartoon effects</td></tr>
-- <tr><td>`Bounce`</td><td>Damped oscillation</td><td>Smooth bounces with energy decay</td><td>Falling objects, ball physics, collision effects</td></tr>
-- </table>
--
-- <br>Comparison of interpolation methods (0 to 10):
-- <table class="tableSP">
-- <tr><th>t</th><th>Lerp</th><th>Smoothstep</th><th>Smootherstep</th><th>EaseInOut</th><th>Elastic</th><th>Bounce</th></tr>
-- <tr><td>0.00</td><td>0.00</td><td>0.00</td><td>0.00</td><td>0.00</td><td>0.00</td><td>0.00</td></tr>
-- <tr><td>0.10</td><td>1.00</td><td>0.28</td><td>0.16</td><td>0.20</td><td>-0.04</td><td>0.95</td></tr>
-- <tr><td>0.25</td><td>2.50</td><td>1.56</td><td>1.04</td><td>1.25</td><td>0.44</td><td>3.75</td></tr>
-- <tr><td>0.50</td><td>5.00</td><td>5.00</td><td>5.00</td><td>5.00</td><td>5.00</td><td>7.50</td></tr>
-- <tr><td>0.75</td><td>7.50</td><td>8.44</td><td>8.96</td><td>8.75</td><td>9.56</td><td>9.82</td></tr>
-- <tr><td>0.90</td><td>9.00</td><td>9.72</td><td>9.84</td><td>9.80</td><td>10.04</td><td>9.98</td></tr>
-- <tr><td>1.00</td><td>10.00</td><td>10.00</td><td>10.00</td><td>10.00</td><td>10.00</td><td>10.00</td></tr>
-- </table>
--
-- <br>IMPORTANT: Using interpolation functions with TEN primitives
--
-- When you use `Lerp`, `Smoothstep`, `Smootherstep`, `EaseInOut`, and `Elastic`
-- with TEN primitives (`Rotation`, `Vec2`, `Vec3`, `Color`), these functions automatically call the native methods
-- of those primitives (e.g., `Rotation:Lerp()`, `Vec3:Lerp()`, `Color:Lerp()`).
--
-- For Rotation primitives specifically:
--
-- `Rotation:Lerp()` always calculates the **shortest angular distance** for each component (x, y, z)
--
-- Example
-- <pre class="example">
--<span class="keyword">local</span> currentRot = obj:GetRotation()
--<span class="keyword">local</span> targetRot = TEN.Rotation(<span class="number">0</span>, <span class="number">350</span>, <span class="number">0</span>)
--<span class="keyword">local</span> newRot = InterpolationUtils.Lerp(currentRot, targetRot, <span class="number">0.5</span>)  <span class="comment">-- Automatically takes shortest path
--</span>obj:SetRotation(newRot)</pre>
--
-- <h3>Special interpolations:</h3>
-- There are two special interpolation functions for specific use cases:
-- <table class="tableSP">
-- <tr><th>Method</th><th>Speed curve</th><th>Behavior</th><th>Use case</th></tr>
-- <tr><td>`LerpAngle`</td><td>Linear (shortest)</td><td>Constant speed, wraps around 0°/360°</td><td>2D UI sprites (compass, indicators)</td></tr>
-- <tr><td>`InterpolateColor`</td><td>Configurable (Linear/HSL/OKLch)</td><td>Component-wise color interpolation</td><td>Color transitions, fades</td></tr>
-- </table>
--
-- <br>**LerpAngle** behaves like Lerp when not crossing 0°/360° boundary.
--
-- When to use LerpAngle:
--
-- - Only when interpolating _single float values_ that represent angles (not `Rotation` primitives)
-- - When you need custom angle ranges (e.g., -180 to 180 instead of 0-360)
-- - External data where you only have float values
--
-- <br>When interpolating angles (rotations, compass, turrets):
-- <table class="tableSP">
-- <tr><th>Start</th><th>End</th><th>t</th><th>Lerp result</th><th>LerpAngle result</th><th>Which is correct?</th></tr>
-- <tr><td>350°</td><td>10°</td><td>0.5</td><td>180°</td><td>0° (crosses 0°)</td><td>LerpAngle</td></tr>
-- <tr><td>10°</td><td>350°</td><td>0.5</td><td>180°</td><td>0° (crosses 0°)</td><td>LerpAngle</td></tr>
-- <tr><td>90°</td><td>270°</td><td>0.5</td><td>180°</td><td>180°</td><td>Both same</td></tr>
-- <tr><td>5°</td><td>15°</td><td>0.5</td><td>10°</td><td>10°</td><td>Both same</td></tr>
-- </table>
--
-- <br>LerpAngle is NOT needed when working with `Rotation` primitives
--
-- Example - INCORRECT (redundant) approach:
--
-- <pre class="example">
--<span class="keyword">local</span> currentRot = obj:GetRotation()
--<span class="keyword">local</span> targetRot = TEN.Rotation(<span class="number">0</span>, <span class="number">350</span>, <span class="number">0</span>)
--currentRot.y = InterpolationUtils.LerpAngle(currentRot.y, targetRot.y, <span class="number">0.5</span>)  <span class="comment">-- Redundant!
--</span>obj:SetRotation(currentRot)</pre>
--
-- <br>**InterpolateColor** supports multiple color spaces for different use cases:
-- 
-- When to use each color space:
--
-- - `RGB (0)`: Simple fades, alpha transitions, color to gray/black/white
-- - `HSL (1)`: Rainbow effects, hue rotation, color wheel animations  
-- - `OKLch (2)`: Perceptually uniform transitions, professional color grading
--
-- <br>huePath options (HSL/OKLch only):
-- <table class="tableSP">
-- <tr><th>huePath</th><th>Red → Cyan</th><th>Use case</th></tr>
-- <tr><td>"shortest"</td><td>Red → Yellow → Green → Cyan</td><td>Natural transitions (default)</td></tr>
-- <tr><td>"longest"</td><td>Red → Magenta → Blue → Cyan</td><td>Full spectrum effects</td></tr>
-- <tr><td>"increasing"</td><td>Red → Yellow → Green → Cyan</td><td>Always clockwise (0° → 360°)</td></tr>
-- <tr><td>"decreasing"</td><td>Red → Magenta → Blue → Cyan</td><td>Always counter-clockwise (360° → 0°)</td></tr>
-- </table>
--
-- <br>Additional options: `preserveSaturation` and `preserveLightness` keep the starting color's saturation/lightness throughout the transition (useful for rainbow effects with consistent intensity).
--
-- <h3>Choosing the right interpolation method:</h3>
--
-- - Use `Lerp` for: numbers, positions (Vec2/Vec3), colors, sizes, mechanical movements
-- - Use `Smoothstep` for: UI fades, smooth transitions, general animations
-- - Use `Smootherstep` for: cinematic camera movements, premium effects, AAA-quality visuals
-- - Use `EaseInOut` for: dramatic movements, pronounced acceleration/deceleration
-- - Use `Elastic` for: bouncy UI, cartoon effects, playful feedback, spring animations
-- - Use `Bounce` for: falling objects, ball physics, collision effects (with aggressive parameters)
-- - Use `LerpAngle` for: 2D UI sprite rotations (DisplaySprite with single float angles)
-- - Use `InterpolateColor` for: color transitions, fades between colors
--
-- <h3>Note about practical examples:</h3>
-- All examples below use `LevelFuncs.OnLoop` to demonstrate the interpolation logic.
-- 
--
-- **Important:**
-- - `LevelFuncs.OnLoop` is a **single function** in your level's Lua file (e.g., `level1.lua`)
-- - You **cannot** have multiple `OnLoop` functions
-- - These examples show different use cases **separately** for clarity
-- 
-- **In your actual level script, you would combine logic like this:**
-- 
-- <pre class="example">
--LevelFuncs.OnLoop = <span class="keyword">function</span>()
--<span class="comment">   -- Update fog (from Lerp example)</span>
--<span class="comment">   -- Copy the fog interpolation code from the Lerp example</span><br>
--<span class="comment">   -- Update elevator (from EaseInOut example)</span>
--<span class="comment">   -- Copy the elevator interpolation code from the EaseInOut example</span><br>
--<span class="comment">   -- Update door (from LerpAngle example)</span>
--<span class="comment">   -- Copy the door interpolation code from the LerpAngle example</span>
--<span class="keyword">end</span></pre>
--
--- To use, include the module with:
---
---	local InterpolationUtils = require("Engine.Utils.InterpolationUtils")
-- @luautil InterpolationUtils

local InterpolationUtils = {}
local Utility = require("Engine.Util")
local Type = require("Engine.Type")
local TableUtils = require("Engine.Utils.TableUtils")

local Round = Utility.Round
local WrapAngleRaw = Utility.WrapAngleRaw
local HSLtoColorRaw = Utility.HSLtoColorRaw
local ColorToHSLRaw = Utility.ColorToHSLRaw
local ColorToOKLchRaw = Utility.ColorToOKLchRaw
local OKLchToColorRaw = Utility.OKLchToColorRaw

local IsValidInterpolationValue = Utility.IsValidInterpolationValue
local LerpRaw = Utility.LerpRaw
local SmoothstepRaw = Utility.SmoothstepRaw
local SmootherstepRaw = Utility.SmootherstepRaw
local EaseInOutRaw = Utility.EaseInOutRaw
local ElasticRaw = Utility.ElasticRaw
local BounceRaw = Utility.BounceRaw

local ErrorLog = Utility.ErrorLog
local WarningLog = Utility.WarningLog

local IsNumber = Type.IsNumber
local IsColor = Type.IsColor
local IsBoolean = Type.IsBoolean
local IsTable = Type.IsTable

local max = math.max
local min = math.min

local LogMessage  = TEN.Util.PrintLog
local logLevelEnums = TEN.Util.LogLevel
local logLevelError  = logLevelEnums.ERROR
local logLevelWarning = logLevelEnums.WARNING

local Color = TEN.Color

InterpolationUtils.Spaces = {
    RGB = 0,
    HSL = 1,
    OKLch = 2
}
TableUtils.SetTableReadOnly(InterpolationUtils.Spaces)

-- Helper function for hue interpolation with different modes
local function InterpolateHue(h1, h2, t, mode)
    local delta = h2 - h1

    if mode == "shortest" then
        delta = ((delta + 180) % 360) - 180

    elseif mode == "longest" then
        delta = ((delta + 180) % 360) - 180
        if delta > 0 then delta = delta - 360 else delta = delta + 360 end

    elseif mode == "increasing" then
        if delta < 0 then delta = delta + 360 end

    elseif mode == "decreasing" then
        if delta > 0 then delta = delta - 360 end
    end

    return (h1 + delta * t) % 360
end

local function ValidateAB (a, b, errorContext)
    if not IsValidInterpolationValue(a) then
        ErrorLog("Error in {context}: value a is not a valid interpolation type.", {context = errorContext})
        return false
    end
    if not IsValidInterpolationValue(b) then
        ErrorLog("Error in {context}: value b is not a valid interpolation type.", {context = errorContext})
        return false
    end
    if getmetatable(a) ~= getmetatable(b) then
        ErrorLog("Error in {context}: value a and b are of different types.", {context = errorContext})
        return false
    end
    return true
end

--- Interpolation functions
-- @section interpolations

--- Linearly interpolate between two values. Formula: result = a + (b - a) * t.
-- Provides constant-speed interpolation between start and end values.
-- Unlike Smoothstep and EaseInOut, Lerp maintains uniform velocity throughout the entire transition.
-- @tparam float|Color|Rotation|Vec2|Vec3 a The start value (number, Color, Rotation, Vec2, or Vec3).
-- @tparam float|Color|Rotation|Vec2|Vec3 b The end value (number, Color, Rotation, Vec2, or Vec3).
-- @tparam float t The interpolation factor (0.0 to 1.0). It will be clamped to this range if out of bounds.
-- @tparam[opt="InterpolationUtils.Lerp"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float|Color|Rotation|Vec2|Vec3 The interpolated value.
-- @treturn[2] float|Color|Rotation|Vec2|Vec3 Value `a` if an error occurs.
-- @usage
-- -- Most common usage (numbers):
-- local interpolated = InterpolationUtils.Lerp(0, 10, 0.5) -- Result: 5
--
-- -- Demonstration of linear progression (0 to 10):
-- --   t    | result
-- --  ------|--------
-- --  0.00  | 0.00
-- --  0.10  | 1.00   (constant speed)
-- --  0.25  | 2.50
-- --  0.50  | 5.00   (midpoint)
-- --  0.75  | 7.50
-- --  0.90  | 9.00   (constant speed)
-- --  1.00  | 10.00
--
-- -- Example with Colors (linear fade red → blue):
-- local color1 = TEN.Color(255, 0, 0, 255)    -- Red
-- local color2 = TEN.Color(0, 0, 255, 255)    -- Blue
--
-- --   t    | R   | G | B   (linear color transition)
-- --  ------|-----|---|-----
-- --  0.00  | 255 | 0 | 0
-- --  0.25  | 191 | 0 | 64
-- --  0.50  | 127 | 0 | 127
-- --  0.75  | 64  | 0 | 191
-- --  1.00  | 0   | 0 | 255
-- local interpolatedColor = InterpolationUtils.Lerp(color1, color2, 0.5) -- Result: Purple (127, 0, 127, 255)
--
-- -- Example with Rotations:
-- local rot1 = TEN.Rotation(0, 0, 0)
-- local rot2 = TEN.Rotation(90, 180, 45)
--
-- --   t    | X    | Y   | Z
-- --  ------|------|-----|-----
-- --  0.00  | 0    | 0   | 0
-- --  0.25  | 22.5 | 45  | 11.25
-- --  0.50  | 45   | 90  | 22.5
-- --  0.75  | 67.5 | 135 | 33.75
-- --  1.00  | 90   | 180 | 45
-- local interpolatedRot = InterpolationUtils.Lerp(rot1, rot2, 0.5)
--
-- -- Example with Vec2:
-- local vec1 = TEN.Vec2(100, 200)
-- local vec2 = TEN.Vec2(300, 400)
--
-- --   t    | X   | Y
-- --  ------|-----|-----
-- --  0.00  | 100 | 200
-- --  0.25  | 150 | 250
-- --  0.50  | 200 | 300
-- --  0.75  | 250 | 350
-- --  1.00  | 300 | 400
-- local interpolatedVec2 = InterpolationUtils.Lerp(vec1, vec2, 0.5) -- Result: Vec2(200, 300)
--
-- -- Example with Vec3 (linear camera movement):
-- local vec3_1 = TEN.Vec3(100, 200, 300)
-- local vec3_2 = TEN.Vec3(200, 400, 600)
--
-- --   t    | X   | Y   | Z
-- --  ------|-----|-----|-----
-- --  0.00  | 100 | 200 | 300
-- --  0.25  | 125 | 250 | 375
-- --  0.50  | 150 | 300 | 450
-- --  0.75  | 175 | 350 | 525
-- --  1.00  | 200 | 400 | 600
-- local interpolatedVec3 = InterpolationUtils.Lerp(vec3_1, vec3_2, 0.5) -- Result: Vec3(150, 300, 450)
--
-- -- Practical animation example (moving platform with constant speed over 4 seconds):
-- -- Lerp is perfect for mechanical movements that should be predictable and constant
-- local platform = TEN.Objects.GetMoveableByName("moving_platform_1")
-- local startPos = TEN.Vec3(8704, -384, 15872)     -- Starting position
-- local endPos = TEN.Vec3(8704, -384, 13824)      -- Ending position
--                                                  -- Total distance: 2048 units (2 sectors horizontal)
--
-- local animationDuration = ConversionUtils.SecondsToFrames(4)  -- 4 seconds = 120 frames @ 30fps
-- local currentFrame = 0
-- local animationComplete = false
--
-- LevelFuncs.OnLoop = function()
--     if not animationComplete then
--         if currentFrame <= animationDuration then
--             local t = currentFrame / animationDuration  -- 0.0 to 1.0
--
--             -- Linear interpolation creates constant speed movement (perfect for platforms!)
--             local currentPos = InterpolationUtils.Lerp(startPos, endPos, t)
--             platform:SetPosition(currentPos)
--
--             -- Visual progression (linear, constant speed):
--             --   t=0.0   → Z=15872  (start position)
--             --   t=0.25  → Z=15360  (moved 512 units)
--             --   t=0.50  → Z=14848  (moved 1024 units = 1 sector) - halfway, perfect timing
--             --   t=0.75  → Z=14336  (moved 1536 units)
--             --   t=1.0   → Z=13824  (moved 2048 units = 2 sectors) - arrived
--             --
--             -- Notice: each 25% of time = same distance traveled (512 units)
--             -- This is CONSTANT SPEED, perfect for platforms and mechanical objects
--
--             currentFrame = currentFrame + 1
--         else
--             -- Animation complete, set final position
--             platform:SetPosition(endPos)
--             animationComplete = true
--         end
--     end
--     -- After animation completes, platform remains at final position
-- end
--
-- -- Why Lerp for platforms?
-- -- ✓ Constant speed is predictable for players (they can time jumps)
-- -- ✓ Mechanical feel matches physical platforms/trains/elevators
-- -- ✓ No acceleration/deceleration = industrial, mechanical movement
-- --
-- -- When NOT to use Lerp:
-- -- ✗ Organic movements (use Smoothstep)
-- -- ✗ Cinematic camera (use Smootherstep)
-- -- ✗ Natural phenomena like fog, wind (use Smoothstep/Smootherstep)
InterpolationUtils.Lerp = function(a, b, t, errorContext)
    errorContext = errorContext or "InterpolationUtils.Lerp"
    if not ValidateAB(a, b, errorContext) then
        return a
    end
    if not IsNumber(t) then
        ErrorLog("Error in {context}: interpolation factor t is not a number.", {context = errorContext})
        return a
    end
    local clampedT = max(0, min(1, t))

    return LerpRaw(a, b, clampedT)
end

--- Smoothly interpolate between two values using Hermite interpolation.
-- The function first normalizes the input value t to a 0-1 range using edge0 and edge1,
-- then applies a smooth S-curve (Hermite polynomial: 3t² - 2t³ or t²(3 - 2t)) for smoother transitions.
-- @tparam float|Color|Rotation|Vec2|Vec3 a Start value (returned when t <= edge0).
-- @tparam float|Color|Rotation|Vec2|Vec3 b End value (returned when t >= edge1).
-- @tparam float t The interpolation factor (0.0 to 1.0). It will be clamped to this range if out of bounds or if edge0/edge1 are invalid.
-- @tparam[opt=0] float edge0 Lower edge: the value of t that maps to 0 (start of interpolation range).
-- @tparam[opt=1] float edge1 Upper edge: the value of t that maps to 1 (end of interpolation range).
-- @tparam[opt="InterpolationUtils.Smoothstep"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float|Color|Rotation|Vec2|Vec3 Smoothly interpolated result.
-- @treturn[2] float|Color|Rotation|Vec2|Vec3 Value `a` if an error occurs.
-- @usage
-- -- Most common usage (edge0=0, edge1=1 implicit):
-- local smoothValue = InterpolationUtils.Smoothstep(0, 10, 0.5) -- Result: 5.0
-- 
-- -- Demonstration of smooth progression (0 to 10):
-- --   t    | result
-- --  ------|--------
-- --  0.00  | 0.00
-- --  0.10  | 0.28   (slow start)
-- --  0.25  | 1.56
-- --  0.50  | 5.00   (midpoint)
-- --  0.75  | 8.44
-- --  0.90  | 9.72   (slow end)
-- --  1.00  | 10.00
-- 
-- -- Example with Colors (smooth fade red → blue):
-- local color1 = TEN.Color(255, 0, 0, 255)  -- Red
-- local color2 = TEN.Color(0, 0, 255, 255)  -- Blue
-- 
-- --   t    | R   | G | B   (smooth color transition)
-- --  ------|-----|---|-----
-- --  0.00  | 255 | 0 | 0
-- --  0.25  | 199 | 0 | 56
-- --  0.50  | 127 | 0 | 127
-- --  0.75  | 56  | 0 | 199
-- --  1.00  | 0   | 0 | 255
-- local smoothColor = InterpolationUtils.Smoothstep(color1, color2, 0.5)
-- 
-- -- Example with Vec3 (smooth camera movement):
-- local startPos = TEN.Vec3(0, 0, 0)
-- local endPos = TEN.Vec3(1000, 500, 2000)
-- 
-- --   t    | X    | Y   | Z
-- --  ------|------|-----|------
-- --  0.00  | 0    | 0   | 0
-- --  0.25  | 156  | 78  | 312
-- --  0.50  | 500  | 250 | 1000
-- --  0.75  | 844  | 422 | 1688
-- --  1.00  | 1000 | 500 | 2000
-- local smoothPos = InterpolationUtils.Smoothstep(startPos, endPos, 0.75)
--
-- -- Example with custom range (health bar that depletes from 100 to 0 over time):
-- local currentHealth = 75  -- Current health value
-- local fadedHealth = InterpolationUtils.Smoothstep(100, 0, currentHealth, 0, 100)
-- -- Maps health 0-100 to smooth 100-0 transition:
-- --   Health | Result
-- --   -------|-------
-- --   0      | 100
-- --   25     | 84.4
-- --   50     | 50
-- --   75     | 15.6
-- --   100    | 0
--
-- -- Example with temperature sensor (map sensor reading 20-30°C to 0-1 range):
-- local temperature = 25  -- Current temperature in Celsius
-- local normalizedTemp = InterpolationUtils.Smoothstep(0, 1, temperature, 20, 30)
-- -- Result: 0.5 (smooth transition between 20°C and 30°C)
--
-- -- Practical animation example (platform moving smoothly over 3 seconds):
-- local bridge = TEN.Objects.GetMoveableByName("bridge_flat_1")
-- local startPos = TEN.Vec3(0, 0, 0)
-- local endPos = TEN.Vec3(-512, 0, 0)  -- Move left 512 units
-- local bridgeInitialPos = bridge:GetPosition()
-- local animationDuration = Conversion Utils.SecondsToFrames(3)  -- 3 seconds = 90 frames @ 30fps
-- local currentFrame = 0
-- local animationComplete = false
-- LevelFuncs.OnLoop = function()
--     if not animationComplete and currentFrame <= animationDuration then
--         local t = currentFrame / animationDuration
--         local offset = InterpolationUtils.Smoothstep(startPos, endPos, t)
--         bridge:SetPosition(bridgeInitialPos + offset)
--         currentFrame = currentFrame + 1
--     else
--         bridge:SetPosition(bridgeInitialPos + endPos)
--         currentFrame = 0
--         animationComplete = true
--     end
-- end
InterpolationUtils.Smoothstep = function (a, b, t, edge0, edge1, errorContext)
    errorContext = errorContext or "InterpolationUtils.Smoothstep"
    if not ValidateAB(a, b, errorContext) then
        return a
    end
    -- Default edge0 and edge1 if not provided
    edge0 = edge0 or 0
    edge1 = edge1 or 1

    if not IsNumber(t) then
        ErrorLog("Error in {context}: t must be a number.", {context = errorContext})
        return a
    end

    if not (IsNumber(edge0) and IsNumber(edge1)) then
        ErrorLog("Error in {context}: edge0 and edge1 must be numbers.", {context = errorContext})
        return a
    end

    local edgeDelta = edge1 - edge0

    -- Check if edge0 and edge1 are equal (division by zero)
    if edgeDelta == 0 then
        ErrorLog("Error in {context}: edge0 and edge1 cannot be equal.", {context = errorContext})
        return a
    end

    return SmoothstepRaw(a, b, t, edge0, edgeDelta)
end

--- Smoothly interpolate with smootherstep curve (Ken Perlin's improved version).
-- Provides an even smoother transition than Smoothstep with zero first and second derivatives at edges.
-- This creates ultra-smooth animations ideal for high-quality cinematics and professional visual effects.
-- Uses the polynomial: 6t⁵ - 15t⁴ + 10t³ (Ken Perlin's improved smoothstep formula).
-- Identical to **LevelFuncs.Engine.Node.Smoothstep** used in the Node Editor.
-- @tparam float|Color|Rotation|Vec2|Vec3 a Start value.
-- @tparam float|Color|Rotation|Vec2|Vec3 b End value.
-- @tparam float t Interpolation factor (0.0 to 1.0). It will be clamped to this range if out of bounds or if edge0/edge1 are invalid.
-- @tparam[opt=0] float edge0 Left edge for custom input range (optional, defaults to 0).
-- @tparam[opt=1] float edge1 Right edge for custom input range (optional, defaults to 1).
-- @tparam[opt="InterpolationUtils.Smootherstep"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float|Color|Rotation|Vec2|Vec3 The interpolated value.
-- @treturn[2] float|Color|Rotation|Vec2|Vec3 Value `a` if an error occurs.
-- @usage
-- -- Most common usage (numbers):
-- local smootherValue = InterpolationUtils.Smootherstep(0, 10, 0.5) -- Result: 5
--
-- -- Comparison: Smoothstep vs Smootherstep (0 to 10):
-- --   t    | Smoothstep | Smootherstep | Difference
-- --  ------|------------|--------------|------------
-- --  0.00  | 0.00       | 0.00         | 0.00
-- --  0.10  | 0.28       | 0.16         | -0.12 (even slower start)
-- --  0.25  | 1.56       | 1.04         | -0.52 (more gradual)
-- --  0.50  | 5.00       | 5.00         | 0.00 (same midpoint)
-- --  0.75  | 8.44       | 8.96         | +0.52 (more gradual)
-- --  0.90  | 9.72       | 9.84         | +0.12 (even slower end)
-- --  1.00  | 10.00      | 10.00        | 0.00
--
-- -- Visual comparison of curves:
-- -- Smoothstep:    starts/ends moderately smooth ━━╱⎺⎺╲━━
-- -- Smootherstep:  starts/ends VERY smooth       ━━━⎺╲━━━  (flatter at edges)
--
-- -- Example with Colors (ultra-smooth fade red → blue):
-- local color1 = TEN.Color(255, 0, 0, 255)  -- Red
-- local color2 = TEN.Color(0, 0, 255, 255)  -- Blue
-- 
-- --   t    | R   | G | B   (smootherstep color transition)
-- --  ------|-----|---|-----
-- --  0.00  | 255 | 0 | 0
-- --  0.25  | 228 | 0 | 27   (very gradual at start)
-- --  0.50  | 127 | 0 | 127
-- --  0.75  | 27  | 0 | 228  (very gradual at end)
-- --  1.00  | 0   | 0 | 255
-- local smootherColor = InterpolationUtils.Smootherstep(color1, color2, 0.5)
--
-- -- Example with Vec3 (ultra-smooth camera position movement):
-- local startPos = TEN.Vec3(0, 1000, 0)
-- local endPos = TEN.Vec3(2000, 1000, 3000)
-- 
-- --   t    | X    | Y    | Z     (smootherstep movement)
-- --  ------|------|------|-------
-- --  0.00  | 0    | 1000 | 0
-- --  0.25  | 209  | 1000 | 313    (very gentle start)
-- --  0.50  | 1000 | 1000 | 1500
-- --  0.75  | 1791 | 1000 | 2687   (very gentle end)
-- --  1.00  | 2000 | 1000 | 3000
-- local smootherPos = InterpolationUtils.Smootherstep(startPos, endPos, 0.75)
--
-- -- Example with custom range (smooth light intensity fade based on distance):
-- local distance = 1500  -- Distance from light source
-- local intensity = InterpolationUtils.Smootherstep(1.0, 0.0, distance, 1000, 2000)
-- -- Maps distance 1000-2000 to intensity 1.0-0.0:
-- --   Distance | Intensity (smootherstep)
-- --   ---------|-------------------------
-- --   1000     | 1.00 (full brightness)
-- --   1250     | 0.90 (very gradual fade)
-- --   1500     | 0.50 (half)
-- --   1750     | 0.10 (very gradual fade)
-- --   2000     | 0.00 (dark)
--
-- -- Example with Rotation (ultra-smooth door closing):
-- local openRot = TEN.Rotation(0, 0, 0)
-- local closedRot = TEN.Rotation(0, 90, 0)
-- 
-- --   t    | X | Y      | Z  (smootherstep rotation)
-- --  ------|---|--------|----
-- --  0.00  | 0 | 0      | 0
-- --  0.25  | 0 | 9.4    | 0   (very slow start)
-- --  0.50  | 0 | 45     | 0
-- --  0.75  | 0 | 80.6   | 0   (very slow end)
-- --  1.00  | 0 | 90     | 0
-- local smootherRot = InterpolationUtils.Smootherstep(openRot, closedRot, 0.5)
--
-- -- Practical example 1: Cinematic camera fly-through (ultra-smooth position change over 6 seconds)
-- -- This creates a professional, broadcast-quality camera movement
-- local camera = TEN.Objects.GetCameraByName("camera_1")
-- local waypoint1 = TEN.Vec3(5632, 0, 11776)  -- Starting position
-- local waypoint2 = TEN.Vec3(5632, 0, 13824) -- Ending position
-- local animationDuration = ConversionUtils.SecondsToFrames(6)  -- 6 seconds = 180 frames
-- local currentFrame = 0
-- local animationActive = true
-- 
-- LevelFuncs.OnLoop = function()
--     if animationActive and currentFrame <= animationDuration then
--         local t = currentFrame / animationDuration
--         local cameraPos = InterpolationUtils.Smootherstep(waypoint1, waypoint2, t)
--         camera:SetPosition(cameraPos)
--         camera:Play()  -- Activate camera this frame
--         currentFrame = currentFrame + 1
--     else
--         animationActive = false
--     end
-- end
--
-- -- Practical example 2: Dynamic fog transition (ultra-smooth fog density change over 8 seconds)
-- -- Creates a smooth, professional environmental effect
-- local level = TEN.Flow.GetCurrentLevel()
-- local clearFog = TEN.Flow.Fog(TEN.Color(220, 230, 255), 15, 25)  -- Light blue, far fog
-- local denseFog = TEN.Flow.Fog(TEN.Color(60, 70, 90), 2, 8)       -- Dark blue, near fog
-- local fogDuration = ConversionUtils.SecondsToFrames(8)  -- 8 seconds
-- local fogFrame = 0
-- local fogActive = true
-- 
-- LevelFuncs.OnLoop = function()
--     if fogActive and fogFrame <= fogDuration then
--         local t = fogFrame / fogDuration
--         
--         -- Interpolate fog color (light blue → dark blue)
--         local fogColor = InterpolationUtils.Smootherstep(clearFog.color, denseFog.color, t)
--         
--         -- Interpolate fog min distance (15 → 2 sectors, ultra-smooth)
--         local fogMin = InterpolationUtils.Smootherstep(clearFog.minDistance, denseFog.minDistance, t)
--         
--         -- Interpolate fog max distance (25 → 8 sectors, ultra-smooth)
--         local fogMax = InterpolationUtils.Smootherstep(clearFog.maxDistance, denseFog.maxDistance, t)
--         
--         level.fog = TEN.Flow.Fog(fogColor, fogMin, fogMax)
--         
--         -- Progression visualization:
--         --   t=0.0  → Light blue (220,230,255), min=15, max=25 (clear)
--         --   t=0.25 → Bluish (184,204,228), min=11.7, max=20.8 (very gradual)
--         --   t=0.5  → Medium blue (140,150,172), min=8.5, max=16.5 (smooth)
--         --   t=0.75 → Dark blue (96,116,145), min=5.3, max=12.2 (very gradual)
--         --   t=1.0  → Very dark blue (60,70,90), min=2, max=8 (dense)
--         
--         fogFrame = fogFrame + 1
--     else
--         -- Set final fog state
--         level.fog = denseFog
--         fogActive = false
--     end
-- end
--
-- -- Practical example 3: Ultra-smooth particle color fade (for premium visual effects)
-- -- Creates particles that smoothly fade from bright yellow to dark red
-- local particleLifetime = 5.0  -- 5 seconds
-- local particleAge = 0.0
-- local startColor = TEN.Color(255, 255, 100, 255)  -- Bright yellow
-- local endColor = TEN.Color(180, 20, 0, 0)         -- Dark red, transparent
-- 
-- LevelFuncs.OnLoop = function()
--     particleAge = particleAge + (1.0 / 30.0)  -- Assuming 30 FPS
--     
--     if particleAge <= particleLifetime then
--         local t = particleAge / particleLifetime
--         local currentColor = InterpolationUtils.Smootherstep(startColor, endColor, t)
--         
--         -- Emit particle with smooth color transition
--         TEN.Effects.EmitParticle(
--             TEN.Vec3(5632, -768, 11776),  -- Position
--             TEN.Vec3(0, 50, 0),          -- Velocity (upward)
--             14,                          -- Sprite ID
--             -10,                         -- Gravity (descend)
--             5,                           -- Rotation velocity
--             currentColor,                -- Start color (interpolated)
--             currentColor,                -- End color (same for consistency)
--             TEN.Effects.BlendID.ADDITIVE,
--             30,                          -- Start size
--             10,                          -- End size
--             1.0                          -- Life (1 second per particle)
--         )
--         
--         -- Color progression (ultra-smooth):
--         --   t=0.0  → Bright yellow (255,255,100,255)
--         --   t=0.25 → Yellow-orange (238,201,62,199) - very gradual
--         --   t=0.5  → Orange (217,137,50,127) - smooth midpoint
--         --   t=0.75 → Red-orange (197,58,12,56) - very gradual
--         --   t=1.0  → Dark red (180,20,0,0) - invisible
--     end
-- end
--
-- -- Practical example 4: Smooth dynamic light intensity (pulsing light effect)
-- -- Creates a smooth, professional breathing light effect
-- local lightPos = TEN.Vec3(5632, -768, 11776)
-- local lightColor = TEN.Color(255, 180, 100)  -- Warm orange
-- local minRadius = 5   -- Minimum light radius (in sectors)
-- local maxRadius = 12  -- Maximum light radius (in sectors)
-- local pulseDuration = ConversionUtils.SecondsToFrames(3)  -- 3 seconds per pulse
-- local pulseFrame = 0
-- local pulseDirection = 1  -- 1 = expanding, -1 = contracting
-- 
-- LevelFuncs.OnLoop = function()
--     local t = pulseFrame / pulseDuration
--     
--     -- Use smootherstep for ultra-smooth radius transition
--     local currentRadius = InterpolationUtils.Smootherstep(minRadius, maxRadius, t)
--     
--     -- Emit light this frame with smooth radius
--     TEN.Effects.EmitLight(
--         lightPos,
--         lightColor,
--         currentRadius,
--         false,           -- No shadows for performance
--         "pulsing_light"  -- Name for interpolation
--     )
--     
--     -- Update pulse
--     pulseFrame = pulseFrame + pulseDirection
--     
--     -- Reverse direction at edges (create continuous pulse)
--     if pulseFrame >= pulseDuration then
--         pulseDirection = -1
--     elseif pulseFrame <= 0 then
--         pulseDirection = 1
--     end
--     
--     -- Radius progression (ultra-smooth breathing):
--     --   t=0.0  → radius=5 (dim)
--     --   t=0.25 → radius=6.3 (very gradual expansion)
--     --   t=0.5  → radius=8.5 (half brightness)
--     --   t=0.75 → radius=10.7 (very gradual expansion)
--     --   t=1.0  → radius=12 (full brightness)
--     -- Then reverses smoothly back to radius=5
-- end
--
-- -- When to use Smootherstep vs Smoothstep:
-- -- 
-- -- Use Smootherstep for:
-- -- ✓ Cinematic camera movements (broadcast quality)
-- -- ✓ Premium particle effects (AAA game quality)
-- -- ✓ Professional environmental transitions (fog, lighting)
-- -- ✓ High-end UI animations (luxury feel)
-- -- ✓ Any effect where you need the SMOOTHEST possible transition
-- -- 
-- -- Use Smoothstep for:
-- -- ✓ Standard game animations (good quality)
-- -- ✓ UI transitions (normal smoothness)
-- -- ✓ General-purpose smooth interpolation
-- -- ✓ When performance matters more than ultra-smoothness
-- --
-- -- Smootherstep is ~15% more expensive computationally than Smoothstep
-- -- (requires evaluating a degree-5 polynomial vs degree-3)
InterpolationUtils.Smootherstep = function (a, b, t, edge0, edge1, errorContext)
    errorContext = errorContext or "InterpolationUtils.Smootherstep"
    if not ValidateAB(a, b, errorContext) then
        return a
    end
    -- Default edge0 and edge1 if not provided
    edge0 = edge0 or 0
    edge1 = edge1 or 1

    if not IsNumber(t) then
        ErrorLog("Error in {context}: t must be a number.", {context = errorContext})
        return a
    end

    if not (IsNumber(edge0) and IsNumber(edge1)) then
        ErrorLog("Error in {context}: edge0 and edge1 must be numbers.", {context = errorContext})
        return a
    end

    local edgeDelta = edge1 - edge0

    -- Check if edge0 and edge1 are equal (division by zero)
    if edgeDelta == 0 then
        ErrorLog("Error in {context}: edge0 and edge1 cannot be equal.", {context = errorContext})
        return a
    end

    return SmootherstepRaw(a, b, t, edge0, edgeDelta)
end

--- Smoothly interpolate with ease-in-out quadratic curve.
-- Provides gentle acceleration at the start and deceleration at the end.
-- Quadratic curve with more pronounced acceleration/deceleration than Smoothstep but smoother than linear interpolation.
-- Uses quadratic formula: t < 0.5 → 2t², otherwise → 1 - 2(1-t)²
-- @tparam float|Color|Rotation|Vec2|Vec3 a Start value.
-- @tparam float|Color|Rotation|Vec2|Vec3 b End value.
-- @tparam float t Interpolation factor (0.0 to 1.0). It will be clamped to this range if out of bounds.
-- @tparam[opt="InterpolationUtils.EaseInOut"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float|Color|Rotation|Vec2|Vec3 The interpolated value.
-- @treturn[2] float|Color|Rotation|Vec2|Vec3 Value `a` if an error occurs.
-- @usage
-- -- Most common usage (numbers):
-- local easeValue = InterpolationUtils.EaseInOut(0, 10, 0.5) -- Result: 5
--
-- -- Demonstration of ease-in-out progression (0 to 10):
-- --   t    | result
-- --  ------|--------
-- --  0.00  | 0.00
-- --  0.10  | 0.20   (gentle acceleration)
-- --  0.25  | 1.25
-- --  0.50  | 5.00   (midpoint)
-- --  0.75  | 8.75
-- --  0.90  | 9.80   (gentle deceleration)
-- --  1.00  | 10.00
--
-- -- Example with Colors (ease-in-out fade red → blue):
-- local color1 = TEN.Color(255, 0, 0, 255)  -- Red
-- local color2 = TEN.Color(0, 0, 255, 255)  -- Blue
-- 
-- --   t    | R   | G | B   (ease-in-out color transition)
-- --  ------|-----|---|-----
-- --  0.00  | 255 | 0 | 0
-- --  0.25  | 191 | 0 | 64
-- --  0.50  | 127 | 0 | 127
-- --  0.75  | 64  | 0 | 191
-- --  1.00  | 0   | 0 | 255
-- local easeColor = InterpolationUtils.EaseInOut(color1, color2, 0.5)
--
-- -- Example with Vec3 (camera movement with acceleration/deceleration):
-- local startPos = TEN.Vec3(0, 0, 0)
-- local endPos = TEN.Vec3(1000, 500, 2000)
-- 
-- --   t    | X    | Y   | Z
-- --  ------|------|-----|------
-- --  0.00  | 0    | 0   | 0
-- --  0.25  | 125  | 62  | 250
-- --  0.50  | 500  | 250 | 1000
-- --  0.75  | 875  | 437 | 1750
-- --  1.00  | 1000 | 500 | 2000
-- local easePos = InterpolationUtils.EaseInOut(startPos, endPos, 0.75)
--
-- -- Example with Rotation (smooth door swing):
-- local rot1 = TEN.Rotation(0, 0, 0)
-- local rot2 = TEN.Rotation(0, 90, 0)
-- 
-- --   t    | X | Y    | Z
-- --  ------|---|------|---
-- --  0.00  | 0 | 0    | 0
-- --  0.25  | 0 | 11.25| 0
-- --  0.50  | 0 | 45   | 0
-- --  0.75  | 0 | 78.75| 0
-- --  1.00  | 0 | 90   | 0
-- local easeRot = InterpolationUtils.EaseInOut(rot1, rot2, 0.5)
--
-- -- Practical animation example (elevator moving with acceleration/deceleration over 4 seconds):
-- local elevator = TEN.Objects.GetMoveableByName("elevator_1")
-- local startPos = elevator:GetPosition()
-- local endPos = startPos + TEN.Vec3(0, 1024, 0)  -- Move up 1024 units
-- local animationDuration = ConversionUtils.SecondsToFrames(4)  -- 4 seconds = 120 frames @ 30fps
-- local currentFrame = 0
-- local animationComplete = false
-- LevelFuncs.OnLoop = function()
--     if currentFrame <= animationDuration then
--         local t = currentFrame / animationDuration
--         local newPos = InterpolationUtils.EaseInOut(startPos, endPos, t)
--         elevator:SetPosition(newPos)
--         currentFrame = currentFrame + 1
--     else
--         animationComplete = true
--         -- Optionally reset animation
--         currentFrame = 0
--     end
-- end
InterpolationUtils.EaseInOut = function(a, b, t, errorContext)
    errorContext = errorContext or "InterpolationUtils.EaseInOut"
    if not ValidateAB(a, b, errorContext) then
        return a
    end
    if not IsNumber(t) then
        ErrorLog("Error in {context}: interpolation factor t is not a number.", {context = errorContext})
        return a
    end

    -- Clamp t to [0, 1]
    t = max(0, min(1, t))

    return EaseInOutRaw(a, b, t)
end

--- Elastic interpolation with overshoot and bounce effect.
-- Creates a spring-like animation that overshoots the target and bounces back before settling.
-- Perfect for cartoonish UI animations, button presses, and playful feedback effects.
-- Uses EaseInOutElastic curve with configurable amplitude (overshoot amount) and period (oscillation frequency).
-- @tparam float|Color|Rotation|Vec2|Vec3 a Start value.
-- @tparam float|Color|Rotation|Vec2|Vec3 b End value.
-- @tparam float t Interpolation factor (0.0 to 1.0). It will be clamped to this range if out of bounds.
-- @tparam[opt=1.0] float amplitude Controls the overshoot amount (default: 1.0). Higher values = more pronounced bounce.
-- @tparam[opt=0.3] float period Controls oscillation frequency (default: 0.3). Lower values = faster oscillations.
-- @tparam[opt="InterpolationUtils.Elastic"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float|Color|Rotation|Vec2|Vec3 The interpolated value with elastic effect.
-- @treturn[2] float|Color|Rotation|Vec2|Vec3 Value `a` if an error occurs.
-- @usage
-- -- Most common usage (numbers with default parameters):
-- local elasticValue = InterpolationUtils.Elastic(0, 100, 0.5) -- Result: ~50 (with slight oscillation)
--
-- -- Demonstration of elastic progression (0 to 10, amplitude=1.0, period=0.3):
-- --   t    | result
-- --  ------|--------
-- --  0.00  | 0.00
-- --  0.10  | -0.04  (undershoots start!)
-- --  0.25  | 0.44   (still below expected)
-- --  0.50  | 5.00   (midpoint)
-- --  0.75  | 9.56   (above expected)
-- --  0.90  | 10.04  (overshoots target!)
-- --  1.00  | 10.00
--
-- -- Example with different amplitude (more pronounced bounce):
-- local strongBounce = InterpolationUtils.Elastic(0, 100, 0.8, 1.5, 0.3)
-- -- Higher amplitude = more overshoot:
-- --   t    | result (amp=1.5)
-- --  ------|------------------
-- --  0.75  | 96.2   (more overshoot)
-- --  0.90  | 100.6  (stronger overshoot)
--
-- -- Example with different period (faster oscillations):
-- local fastOscillation = InterpolationUtils.Elastic(0, 100, 0.8, 1.0, 0.15)
-- -- Lower period = faster bounces
--
-- -- Example with Colors (elastic color transition red → green):
-- local color1 = TEN.Color(255, 0, 0, 255)  -- Red
-- local color2 = TEN.Color(0, 255, 0, 255)  -- Green
-- 
-- --   t    | R   | G   | B (elastic color with bounce)
-- --  ------|-----|-----|---
-- --  0.00  | 255 | 0   | 0
-- --  0.25  | 216 | 39  | 0   (slight overshoot in R)
-- --  0.50  | 127 | 127 | 0
-- --  0.75  | 39  | 216 | 0   (overshoots in G)
-- --  1.00  | 0   | 255 | 0
-- local elasticColor = InterpolationUtils.Elastic(color1, color2, 0.75)
--
-- -- Example with Vec3 (UI element sliding with bounce):
-- local startPos = TEN.Vec3(-500, 100, 0)  -- Off-screen left
-- local endPos = TEN.Vec3(0, 100, 0)       -- Final position
-- 
-- --   t    | X      | Y   | Z (elastic movement)
-- --  ------|--------|-----|---
-- --  0.00  | -500   | 100 | 0
-- --  0.25  | -478   | 100 | 0   (still off-screen, bouncing)
-- --  0.50  | -250   | 100 | 0
-- --  0.75  | -22    | 100 | 0   (almost there, slight overshoot)
-- --  0.90  | 2      | 100 | 0   (overshoots right!)
-- --  1.00  | 0      | 100 | 0   (settles)
-- local elasticPos = InterpolationUtils.Elastic(startPos, endPos, 0.75, 1.2, 0.3)
--
-- -- Example with Rotation (door with elastic swing):
-- local rot1 = TEN.Rotation(0, 0, 0)
-- local rot2 = TEN.Rotation(0, 90, 0)
-- 
-- --   t    | X | Y      | Z (elastic rotation)
-- --  ------|---|--------|---
-- --  0.00  | 0 | 0      | 0
-- --  0.25  | 0 | 3.9    | 0   (slow start with bounce)
-- --  0.50  | 0 | 45     | 0
-- --  0.75  | 0 | 86     | 0   (approaching with overshoot)
-- --  0.90  | 0 | 90.4   | 0   (overshoots!)
-- --  1.00  | 0 | 90     | 0   (settles)
-- local elasticRot = InterpolationUtils.Elastic(rot1, rot2, 0.75, 1.0, 0.3)
--
-- -- Practical example: Pickup item animation (item bounces toward player):
-- local pickup = TEN.Objects.GetMoveableByName("pickup_item")
-- local startPos = pickup:GetPosition()
-- local playerPos = Lara:GetPosition()
-- local animationDuration = ConversionUtils.SecondsToFrames(1.0)  -- 1 second
-- local currentFrame = 0
-- local giveItem = false
-- LevelFuncs.OnLoop = function()
--     if currentFrame <= animationDuration then
--         local t = currentFrame / animationDuration
--         local pos = InterpolationUtils.Elastic(startPos, playerPos, t, 1.3, 0.25)
--         pickup:SetPosition(pos)
--         -- The item will "bounce" as it moves toward the player:
--         -- - First undershoots (moves back slightly)
--         -- - Then accelerates forward
--         -- - Overshoots the player position
--         -- - Wobbles and settles at player position
--         currentFrame = currentFrame + 1
--     else
--         if not giveItem then
--             TEN.Inventory.GiveItem(pickup:GetObjectID(), 1, true) -- Collect the item
--             pickup:Destroy()
--             giveItem = true
--         end
--     end
-- end
InterpolationUtils.Elastic = function(a, b, t, amplitude, period, errorContext)
    errorContext = errorContext or "InterpolationUtils.Elastic"
    if not ValidateAB(a, b, errorContext) then
        return a
    end
    if not IsNumber(t) then
        ErrorLog("Error in {context}: interpolation factor t is not a number.", {context = errorContext})
        return a
    end

    -- Set default values and validate optional parameters
    amplitude = amplitude or 1.0
    period = period or 0.3

    if not IsNumber(amplitude) or not IsNumber(period) then
        ErrorLog("Error in {context}: amplitude and period must be numbers.", {context = errorContext})
        return a
    end

    -- Validate amplitude (must be >= 1.0 for proper elastic effect)
    if amplitude < 1.0 then
        WarningLog("Warning in {context}: amplitude should be >= 1.0 for proper elastic effect. Using 1.0.", {context = errorContext})
        amplitude = 1.0
    end

    -- Clamp t to [0, 1]
    t = max(0, min(1, t))

    return ElasticRaw(a, b, t, amplitude, period)
end

--- Bounce interpolation with damped oscillation physics.
-- Creates a bouncing animation that simulates objects hitting surfaces with decreasing intensity.
-- Perfect for falling objects, ball physics, and collision effects with proper parameter tuning.
-- Uses an exponential decay curve with cosine waves to approximate bounce physics.
--
-- **Note on physics simulation:**
-- This is an easing function (mathematical curve), not a physics engine.
-- It approximates bouncing behavior for visual effects. For realistic physics:
--
-- - Use low `bounces` (2-3) and low `damping` (0.3-0.4) for hard collisions
--
-- - Use high `bounces` (5-7) and high `damping` (0.6-0.8) for elastic bouncing
-- @tparam float|Color|Rotation|Vec2|Vec3 a Start value.
-- @tparam float|Color|Rotation|Vec2|Vec3 b End value.
-- @tparam float t Interpolation factor (0.0 to 1.0). It will be clamped to this range if out of bounds.
-- @tparam[opt=4] float bounces Number of bounces (default: 4). Higher values = more bounces before settling.
-- @tparam[opt=0.5] float damping Bounce intensity/energy loss (default: 0.5, range: 0.0-1.0). Lower values = faster decay, higher values = longer bounces.
-- @tparam[opt="InterpolationUtils.Bounce"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float|Color|Rotation|Vec2|Vec3 The interpolated value with bounce effect.
-- @treturn[2] float|Color|Rotation|Vec2|Vec3 Value `a` if an error occurs.
-- @usage
-- -- Most common usage (numbers with default parameters):
-- local bounceValue = InterpolationUtils.Bounce(0, 100, 0.75) -- Result: ~100 with bounce oscillations
--
-- -- Demonstration of bounce progression (0 to 10, bounces=4, damping=0.5):
-- --   t    | result
-- --  ------|--------
-- --  0.00  | 0.00
-- --  0.10  | 0.95   (accelerating toward target)
-- --  0.25  | 3.75   (moving toward target)
-- --  0.50  | 7.50   (approaching target)
-- --  0.70  | 9.70   (first impact, slight overshoot)
-- --  0.75  | 9.82   (small bounce back)
-- --  0.85  | 9.95   (second smaller bounce)
-- --  0.95  | 9.99   (tiny final bounce)
-- --  1.00  | 10.00  (settled at target)
--
-- -- Example with more bounces (energetic ball):
-- local energeticBounce = InterpolationUtils.Bounce(0, 100, 0.8, 6, 0.6)
-- -- More bounces with slower decay:
-- --   t    | result (bounces=6, damping=0.6)
-- --  ------|--------------------------------
-- --  0.70  | 97.2   (first bounce)
-- --  0.80  | 99.5   (second bounce)
-- --  0.90  | 99.9   (third bounce)
-- --  0.95  | 100.0  (still bouncing slightly)
--
-- -- Example with fewer bounces (heavy object):
-- local heavyBounce = InterpolationUtils.Bounce(0, 100, 0.8, 2, 0.3)
-- -- Fewer, sharper bounces with fast decay:
-- --   t    | result (bounces=2, damping=0.3)
-- --  ------|--------------------------------
-- --  0.75  | 98.5   (quick first bounce)
-- --  0.90  | 99.8   (small second bounce)
-- --  1.00  | 100.0  (settled quickly)
--
-- -- Example with Colors (bouncing color transition red → blue):
-- local color1 = TEN.Color(255, 0, 0, 255)  -- Red
-- local color2 = TEN.Color(0, 0, 255, 255)  -- Blue
-- 
-- --   t    | R   | G | B   (bouncing color)
-- --  ------|-----|---|-----
-- --  0.00  | 255 | 0 | 0
-- --  0.50  | 127 | 0 | 127
-- --  0.75  | 5   | 0 | 250  (approaching blue with bounce)
-- --  0.85  | 10  | 0 | 245  (bounce back slightly)
-- --  1.00  | 0   | 0 | 255  (settled at blue)
-- local bounceColor = InterpolationUtils.Bounce(color1, color2, 0.75, 4, 0.5)
--
-- -- Example with Vec3 (ball bouncing toward ground):
-- local startPos = TEN.Vec3(0, 1000, 0)  -- High in air
-- local endPos = TEN.Vec3(0, 0, 0)       -- Ground level
-- 
-- --   t    | X | Y     | Z (bouncing ball)
-- --  ------|---|-------|---
-- --  0.00  | 0 | 1000  | 0
-- --  0.50  | 0 | 500   | 0   (falling)
-- --  0.75  | 0 | 20    | 0   (first bounce up)
-- --  0.85  | 0 | 5     | 0   (second smaller bounce)
-- --  0.95  | 0 | 1     | 0   (tiny final bounce)
-- --  1.00  | 0 | 0     | 0   (settled on ground)
-- local bouncePos = InterpolationUtils.Bounce(startPos, endPos, 0.75, 4, 0.5)
--
-- -- Example with Rotation (door slamming with bounces):
-- local rot1 = TEN.Rotation(0, 90, 0)   -- Door open
-- local rot2 = TEN.Rotation(0, 0, 0)    -- Door closed
-- 
-- --   t    | X | Y    | Z (door slamming)
-- --  ------|---|------|---
-- --  0.00  | 0 | 90   | 0
-- --  0.50  | 0 | 45   | 0   (closing)
-- --  0.75  | 0 | 2    | 0   (first impact, bounces open slightly)
-- --  0.85  | 0 | 0.5  | 0   (second bounce)
-- --  0.95  | 0 | 0.1  | 0   (final tiny bounce)
-- --  1.00  | 0 | 0    | 0   (fully closed)
-- local bounceRot = InterpolationUtils.Bounce(rot1, rot2, 0.75, 3, 0.4)
--
-- -- Practical example 1: Dropping item with realistic bounce physics
-- local item = TEN.Objects.GetMoveableByName("dropped_item")
-- local startPos = item:GetPosition()
-- local groundY = 0  -- Ground level Y coordinate
-- local endPos = TEN.Vec3(startPos.x, groundY, startPos.z)
-- local animationDuration = ConversionUtils.SecondsToFrames(2.0)  -- 2 second drop
-- local currentFrame = 0
-- LevelFuncs.OnLoop = function()
--     if currentFrame <= animationDuration then
--         local t = currentFrame / animationDuration
--         -- Item falls and bounces realistically on impact:
--         -- - Falls smoothly toward ground
--         -- - First impact creates largest bounce
--         -- - Each subsequent bounce is smaller
--         -- - Eventually settles on ground
--         local pos = InterpolationUtils.Bounce(startPos, endPos, t, 5, 0.6)
--         item:SetPosition(pos)
--         
--         -- Optional: Play impact sound on each bounce peak
--         -- (detect when position Y changes direction)
--         
--         currentFrame = currentFrame + 1
--     end
-- end
--
-- -- Practical example 2: An object simulates a slamming door with collision effects
-- -- Using aggressive parameters (low bounces, low damping) to simulate hard impacts
-- local stoneDoor = TEN.Objects.GetStaticByName("static_mesh_18")
-- local startPos = stoneDoor:GetPosition()
-- local endPos = startPos + TEN.Vec3(-1024, 0, 0)  -- Door drops 1024 units
-- local animationDuration = ConversionUtils.SecondsToFrames(1.5)  -- 1.5 second slam
-- local currentFrame = 0
-- LevelFuncs.OnLoop = function()
--     if currentFrame <= animationDuration then
--         local t = currentFrame / animationDuration
--         
--         -- Door slams down with quick, hard bounces:
--         -- - Use bounces=2 for just a couple of impacts
--         -- - Use damping=0.3 for fast energy loss (hard surface)
--         -- This creates a "slamming" effect rather than elastic bouncing
--         local pos = InterpolationUtils.Bounce(startPos, endPos, t, 2, 0.3)
--         stoneDoor:SetPosition(pos)
--         
--         -- Optional: Play slam sound when door reaches/bounces off endPos
--         
--         currentFrame = currentFrame + 1
--     end
-- end
--
-- -- Practical example 3: 2 objects simulate double door lock with collision effect
-- -- Both objects move toward each other and "bounce" on collision
-- local leftObject = TEN.Objects.GetStaticByName("static_mesh_19")
-- local rightObject = TEN.Objects.GetStaticByName("static_mesh_20")
-- local leftStart = leftObject:GetPosition()
-- local rightStart = rightObject:GetPosition()
-- 
-- -- Each object moves 1024 units (1 sector) toward the other
-- -- Objects are 1024 wide and initially 2048 apart (2 sectors gap)
-- -- After moving 1024 each, they meet in the middle without overlapping
-- local leftEnd = TEN.Vec3(leftStart.x, leftStart.y, leftStart.z + 1024)
-- local rightEnd = TEN.Vec3(rightStart.x, rightStart.y, rightStart.z - 1024)
-- 
-- local animationDuration = ConversionUtils.SecondsToFrames(1.5)
-- local currentFrame = 0
-- 
-- LevelFuncs.OnLoop = function()
--     if currentFrame <= animationDuration then
--         local t = currentFrame / animationDuration
--         
--         -- Aggressive parameters for collision effect:
--         -- bounces=3: A few quick impacts
--         -- damping=0.4: Quick energy loss for "slamming" feel
--         -- Both objects follow same curve toward their endpoints
--         
--         local leftPos = InterpolationUtils.Bounce(leftStart, leftEnd, t, 3, 0.4)
--         local rightPos = InterpolationUtils.Bounce(rightStart, rightEnd, t, 3, 0.4)
--         
--         leftObject:SetPosition(leftPos)
--         rightObject:SetPosition(rightPos)
--         
--         -- Optional: Calculate when objects are closest to detect "collision"
--         -- local distance = math.abs(leftPos.z - rightPos.z)
--         -- if distance < 10 then -- Play slam sound end
--         
--         currentFrame = currentFrame + 1
--     end
-- end
InterpolationUtils.Bounce = function(a, b, t, bounces, damping, errorContext)
    errorContext = errorContext or "InterpolationUtils.Bounce"
    if not ValidateAB(a, b, errorContext) then
        return a
    end
    if not IsNumber(t) then
        ErrorLog("Error in {context}: interpolation factor t is not a number.", {context = errorContext})
        return a
    end

    -- Set default values and validate optional parameters
    bounces = bounces or 4
    damping = damping or 0.5

    if not IsNumber(bounces) or not IsNumber(damping) then
        ErrorLog("Error in {context}: bounces and damping must be numbers.", {context = errorContext})
        return a
    end

    -- Validate bounces (must be positive integer)
    if bounces < 1 or bounces % 1 ~= 0 then
        WarningLog("Warning in {context}: bounces should be an integer >= 1. Using 1.", {context = errorContext})
        bounces = 1
    end

    -- Validate damping (0.0 to 1.0 range)
    if damping < 0.0 or damping > 1.0 then
        WarningLog("Warning in {context}: damping should be between 0.0 and 1.0. Clamping.", {context = errorContext})
        damping = max(0.0, min(1.0, damping))
    end
    -- Clamp t to [0, 1]
    t = max(0, min(1, t))

    return BounceRaw(a, b, t, bounces, damping)
end

--- Special Interpolation functions
-- @section Sinterpolations

--- Linearly interpolate between two angles, taking the shortest path.
-- **Why use LerpAngle instead of Lerp for angles?**
-- 
-- Lerp treats angles as linear numbers:
--
--      InterpolationUtils.Lerp(350°, 10°, 0.5) = 180° (rotates 170° the long way!)
-- 
-- LerpAngle calculates shortest rotation path:
--
--      InterpolationUtils.LerpAngle(350°, 10°, 0.5) = 0° ✅ (rotates 10° through 0°/360° boundary)
--
-- This prevents objects from "spinning wildly" when rotating across 0°/360°.
--
--- **IMPORTANT: When NOT to use LerpAngle**
-- 
-- LerpAngle calculates the SHORTEST path between angles.
--
-- This means:
--
-- - 0° → 360° = 0° rotation (they're the same angle!)
-- - For FULL 360° rotation, use Lerp instead
--
-- **Use LerpAngle for:**
--
-- - Shortest rotation to target (enemy, turret, compass)
-- - Door/lid opening (0° → 90° type rotations)
-- - Any rotation where direction doesn't matter
--
-- **Use Lerp for:**
--
-- - Full 360° camera orbit (specific direction)
-- - Multiple rotations (0° → 720° = 2 full circles)
-- - Continuous rotation (0° → 1000°)
--
-- **Problem with regular Lerp:**
-- <table class="tableSP">
-- <tr><th>Start</th><th>End</th><th>Lerp result</th><th>Problem</th></tr>
-- <tr><td>350°</td><td>10°</td><td>180°</td><td>Goes the LONG way (340° turn!)</td></tr>
-- <tr><td>10°</td><td>350°</td><td>180°</td><td>Goes the LONG way (340° turn!)</td></tr>
-- <tr><td>270°</td><td>90°</td><td>180°</td><td>Correct by chance</td></tr>
-- </table>
--
-- <br>**Solution with LerpAngle:**
--
-- <table class="tableSP">
-- <tr><th>Start</th><th>End</th><th>LerpAngle result</th><th>Result</th></tr>
-- <tr><td>350°</td><td>10°</td><td>0° (360°)</td><td>SHORT way (20° turn through 0°)</td></tr>
-- <tr><td>10°</td><td>350°</td><td>0° (360°)</td><td>SHORT way (20° turn through 0°)</td></tr>
-- <tr><td>270°</td><td>90°</td><td>180°</td><td>SHORT way (180° turn)</td></tr>
-- </table>
--
-- @tparam float a Start angle (in degrees).
-- @tparam float b End angle (in degrees).
-- @tparam float t Interpolation factor (0.0 to 1.0).
-- @tparam[opt=0] float minValue Minimum angle of range (default: 0 for 0-360°).
-- @tparam[opt=360] float maxValue Maximum angle of range (default: 360 for 0-360°).
-- @tparam[opt="InterpolationUtils.LerpAngle"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] float The interpolated angle, taking the shortest path.
-- @treturn[2] float Value `a` if an error occurs.
-- @usage
-- -- Basic example: Why LerpAngle is needed
-- local angle1 = InterpolationUtils.Lerp(350, 10, 0.5)        -- Result: 180° (WRONG! Long way)
-- local angle2 = InterpolationUtils.LerpAngle(350, 10, 0.5)   -- Result: 0° (CORRECT! Short way)
--
-- -- Demonstration: Rotating from 350° to 10° (should cross 0°/360°)
-- --   t    | Lerp  | LerpAngle | Correct?
-- --  ------|-------|-----------|----------
-- --  0.00  | 350   | 350       | ✓
-- --  0.25  | 265   | 355       | ✓ (short path)
-- --  0.50  | 180   | 0         | ✓ (crosses boundary)
-- --  0.75  | 95    | 5         | ✓ (short path)
-- --  1.00  | 10    | 10        | ✓
--
-- -- Real-world example 1: 2D sprite pointing towards mouse cursor
-- -- DisplaySprite uses single float for rotation, perfect use case for LerpAngle!
-- local arrowSprite = TEN.View.DisplaySprite(1354, 16, TEN.Vec2(400, 300), 0, TEN.Vec2(3, 3))
-- local rotationSpeed = 0.1  -- How fast the arrow rotates (0.0-1.0)
--
-- LevelFuncs.OnLoop = function()
--     local mousePos = TEN.Input.GetMouseDisplayPosition()
--     local arrowPos = arrowSprite:GetPosition()
--     
--     -- Calculate angle from arrow to mouse
--     local dx = mousePos.x - arrowPos.x
--     local dy = mousePos.y - arrowPos.y
--     local targetAngle = math.atan(dy, dx) * (180 / math.pi)  -- Convert radians to degrees
--     
--     -- Smoothly rotate arrow towards mouse using shortest path
--     local currentAngle = arrowSprite:GetRotation()
--     local newAngle = InterpolationUtils.LerpAngle(currentAngle, targetAngle, rotationSpeed)
--     arrowSprite:SetRotation(newAngle)
--     
--     arrowSprite:Draw()
-- end
--
-- -- Real-world example 2: HUD compass needle rotating to point north
-- -- Perfect for 2D UI elements that need smooth rotation
-- -- Uses speedometer needle sprite from base WAD (points down by default, so we add 180° offset)
-- local objID = TEN.Objects.ObjID.SPEEDOMETER_GRAPHICS
-- local compassNeedle = TEN.View.DisplaySprite(objID, 1, TEN.Vec2(80, 80), 0, TEN.Vec2(20, 20))
--
-- LevelFuncs.OnLoop = function()
--     -- Get player's current yaw (facing direction)
--     local playerYaw = Lara:GetRotation().y
--     
--     -- Calculate angle to north
--     -- Add 180° offset because the sprite points down by default
--     local needleTargetAngle = -playerYaw + 180
--     
--     -- Smoothly rotate needle towards north using shortest path
--     local currentNeedleAngle = compassNeedle:GetRotation()
--     local newNeedleAngle = InterpolationUtils.LerpAngle(currentNeedleAngle, needleTargetAngle, 0.15)
--     compassNeedle:SetRotation(newNeedleAngle)
--     
--     compassNeedle:Draw()
-- end
InterpolationUtils.LerpAngle = function(a, b, t, minValue, maxValue, errorContext)
    errorContext = errorContext or "InterpolationUtils.LerpAngle"
    minValue = minValue or 0
    maxValue = maxValue or 360

    if not (IsNumber(a) and IsNumber(b) and IsNumber(t)) then
        ErrorLog("Error in {context}: a, b, and t must be numbers.", {context = errorContext})
        return a
    end

    if not (IsNumber(minValue) and IsNumber(maxValue)) then
        ErrorLog("Error in {context}: minValue and maxValue must be numbers.", {context = errorContext})
        return a
    end

    if minValue >= maxValue then
        ErrorLog("Error in {context}: minValue must be less than maxValue.", {context = errorContext})
        return a
    end

    -- Clamp t to [0, 1]
    t = max(0, min(1, t))

    -- Precalculate range
    local range = maxValue - minValue

    -- Normalize angles to range
    a = WrapAngleRaw(a, minValue, range)
    b = WrapAngleRaw(b, minValue, range)

    -- Calculate shortest delta
    local delta = b - a
    local halfRange = range * 0.5

    -- Wrap delta to [-range/2, range/2] for shortest path
    if delta > halfRange then
        delta = delta - range
    elseif delta < -halfRange then
        delta = delta + range
    end

    -- Interpolate and wrap result
    local result = a + delta * t
    return WrapAngleRaw(result, minValue, range)
end

--- Interpolates between two colors in specified color space with options.
-- Supports RGB, HSL, and OKLch color spaces with customizable hue interpolation paths and saturation/lightness preservation.
-- @tparam Color colorA Starting color.
-- @tparam Color colorB Ending color.
-- @tparam float t Interpolation factor (0.0 to 1.0).
-- @tparam[opt=InterpolationUtils.Spaces.RGB] Spaces space Color space to use for interpolation.
-- @tparam[opt={}] table options Additional options.
--
-- - `huePath` (string): Path for hue interpolation in HSL/OKLch<br>`("shortest", "longest", "increasing", "decreasing")`<br>*Default: "shortest"*.
-- - `preserveSaturation` (boolean): If true, preserves starting saturation in HSL/OKLch.<br>*Default: false*.
--
-- - `preserveLightness` (boolean): If true, preserves starting lightness in HSL/OKLch.<br>*Default: false*.
--
-- @treturn[1] Color The interpolated color.
-- @treturn[2] Color `colorA` if an error occurs.
-- @usage
-- -- Examples of interpolation from red to blue in different color spaces and with different options
-- local color1 = TEN.Color(255, 0, 0)  -- Red
-- local color2 = TEN.Color(0, 0, 255)  -- Blue
--
-- -- Example with RGB interpolation:
-- local rgbColor = InterpolationUtils.InterpolateColor(color1, color2, 0.5) -- t = 0.5, RGB space
-- --   t    | R   | G | B
-- --  ------|-----|---|-----
-- --  0.00  | 255 | 0 | 0
-- --  0.25  | 191 | 0 | 64
-- --  0.50  | 128 | 0 | 128
-- --  0.75  | 64  | 0 | 191
-- --  1.00  | 0   | 0 | 255
--
-- -- Example with HSL interpolation (shortest hue path):
-- local hslColor = InterpolationUtils.InterpolateColor(color1, color2, 0.5, 1)
-- --   t    | R   | G   | B
-- --  ------|-----|-----|-----
-- --  0.00  | 255 | 0   | 0
-- --  0.25  | 255 | 0   | 128
-- --  0.50  | 255 | 0   | 255
-- --  0.75  | 127 | 0   | 255
-- --  1.00  | 0   | 0   | 255
--
-- -- Example with HSL interpolation (longest hue path):
-- local hslLongColor = InterpolationUtils.InterpolateColor(color1, color2, 0.5, 1, { huePath = "longest" })
-- --   t    | R   | G   | B
-- --  ------|-----|-----|-----
-- --  0.00  | 255 | 0   | 0
-- --  0.25  | 255 | 255 | 0
-- --  0.50  | 0   | 255 | 0
-- --  0.75  | 0   | 255 | 255
-- --  1.00  | 0   | 0   | 255
--
-- -- Example with OKLch interpolation (shortest hue path):
-- local oklchShortColor = InterpolationUtils.InterpolateColor(color1, color2, 0.5, 2)
-- --   t    | R   | G   | B
-- --  ------|-----|-----|-----
-- --  0.00  | 255 | 0   | 0
-- --  0.25  | 232 | 0   | 123
-- --  0.50  | 186 | 0   | 194
-- --  0.75  | 122 | 25  | 244
-- --  1.00  | 0   | 0   | 255
--
-- -- Example with OKLch interpolation (preserving saturation):
-- local oklchColor = InterpolationUtils.InterpolateColor(color1, color2, 0.5, 2, { preserveSaturation = true })
-- --   t    | R   | G   | B
-- --  ------|-----|-----|-----
-- --  0.00  | 255 | 0   | 0
-- --  0.25  | 228 | 0   | 123
-- --  0.50  | 180 | 0   | 186
-- --  0.75  | 117 | 25  | 224
-- --  1.00  | 2   | 52  | 225
-- -- Note: Enabling preserveSaturation, with t = 1 does not yield pure blue due to saturation preservation.
--
-- -- Example with OKLch interpolation (preserving lightness):
-- local oklchLightColor = InterpolationUtils.InterpolateColor(color1, color2, 0.5, 2, { preserveLightness = true })
-- --   t    | R   | G   | B
-- --  ------|-----|-----|-----
-- --  0.00  | 255 | 0   | 0
-- --  0.25  | 249 | 0   | 136
-- --  0.50  | 217 | 17  | 224
-- --  0.75  | 159 | 62  | 255
-- --  1.00  | 28  | 103 | 255
-- -- Note: Enabling preserveLightness, with t = 1 does not yield pure blue due to lightness preservation.
InterpolationUtils.InterpolateColor = function(colorA, colorB, t, space, options)

    -- Validate input parameters
    if not IsColor(colorA) or not IsColor(colorB) then
        LogMessage("Error in InterpolationUtils.InterpolateColor: colorA and colorB must be TEN.Color.", logLevelError)
        return colorA
    end

    if not IsNumber(t) then
        LogMessage("Error in InterpolationUtils.InterpolateColor: t must be a number.", logLevelError)
        return colorA
    end

    t = max(0, min(1, t))  -- Clamp t to [0, 1]

    space = space or 0

    if not IsNumber(space) or (space ~= 0  and space ~= 1 and space ~= 2) then
        LogMessage("Warning in InterpolationUtils.InterpolateColor: invalid colorSpace, using RGB.", logLevelWarning)
        space = 0
    end

    -- Validate options (optional parameter)
    if not IsTable(options) then
        options = {}
    end

    local huePath = options.huePath
    if huePath ~= "shortest" and huePath ~= "longest" and huePath ~= "increasing" and huePath ~= "decreasing" then
        if huePath ~= nil then
            LogMessage("Warning in InterpolationUtils.InterpolateColor: invalid huePath, using 'shortest'.", logLevelWarning)
        end
        huePath = "shortest"
    end

    local preserveS = options.preserveSaturation
    if preserveS ~= nil and not IsBoolean(preserveS) then
        LogMessage("Warning in InterpolationUtils.InterpolateColor: preserveSaturation must be boolean. Using false.", logLevelWarning)
        preserveS = false
    end
    preserveS = preserveS or false

    local preserveL = options.preserveLightness
    if preserveL ~= nil and not IsBoolean(preserveL) then
        LogMessage("Warning in InterpolationUtils.InterpolateColor: preserveLightness must be boolean. Using false.", logLevelWarning)
        preserveL = false
    end
    preserveL = preserveL or false

    -- RGB
    if space == 0 then
        local inv = 1 - t
        return Color(
            Round(colorA.r * inv + colorB.r * t, 1),
            Round(colorA.g * inv + colorB.g * t, 1),
            Round(colorA.b * inv + colorB.b * t, 1),
            Round(colorA.a * inv + colorB.a * t, 1)
        )
    end

    -- HSL
    if space == 1 then
        local hA, sA, lA = ColorToHSLRaw(colorA)
        local hB, sB, lB = ColorToHSLRaw(colorB)

        local h = InterpolateHue(hA, hB, t, huePath)
        local s = preserveS and sA or (sA + (sB - sA) * t)
        local l = preserveL and lA or (lA + (lB - lA) * t)

        local finalColor = HSLtoColorRaw(h, s, l, 1)
        finalColor.a = colorA.a + (colorB.a - colorA.a) * t
        return finalColor
    end

    -- OKLch
    if space == 2 then
        local lA, cA, hA = ColorToOKLchRaw(colorA)
        local lB, cB, hB = ColorToOKLchRaw(colorB)

        local l = preserveL and lA or (lA + (lB - lA) * t)
        local c = preserveS and cA or (cA + (cB - cA) * t)
        local h = InterpolateHue(hA, hB, t, huePath)

        local finalColor = OKLchToColorRaw(l, c, h, 1)
        finalColor.a = colorA.a + (colorB.a - colorA.a) * t
        return finalColor
    end
end

----
-- Tables
-- @section tables

---
-- Constants for color spaces in @{InterpolationUtils.InterpolateColor}.
-- @table Spaces
-- @tfield 0 RGB RGB color space.
-- @tfield 1 HSL HSL color space.
-- @tfield 2 OKLch OKLch color space.

return InterpolationUtils