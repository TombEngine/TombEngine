-----<style>table.function_list td.name {min-width: 320px;}</style>
--- Lua support functions to simplify operations in scripts.
---
--- **Design Philosophy:**
--- GeneralUtils is designed primarily for:
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
---	local GeneralUtils = require("Engine.GeneralUtils")
-- @luautil GeneralUtils

local Type = require("Engine.Type")
local MAX_DEPTH = 10        -- Maximum recursion depth for deep operations (prevents stack overflow)
local MAX_ELEMENTS = 1000   -- Maximum elements processed in deep operations (prevents performance issues)
local COMPARISON_OPS =
{
    function(a, b) return a == b end,   -- 0: equal
    function(a, b) return a ~= b end,   -- 1: not equal
    function(a, b) return a < b end,    -- 2: less than
    function(a, b) return a <= b end,   -- 3: less than or equal
    function(a, b) return a > b end,    -- 4: greater than
    function(a, b) return a >= b end,   -- 5: greater than or equal
}
-- ----------------------------------------------------------------------------
-- ANSI ESCAPE CODES
-- Lookup table for console styling (Styled and ColorLog functions).
-- Maps style names to ANSI SGR parameter numbers.
-- ----------------------------------------------------------------------------
local ESC = "\27["
local ANSI_RESET = "\27[0m"
local ANSI_CODES =
{
    -- Text colors (foreground)
    black   = "30", red     = "31", green   = "32", yellow  = "33",
    blue    = "34", magenta = "35", cyan    = "36", white   = "37",
    -- Background colors
    bg_black   = "40", bg_red     = "41", bg_green   = "42", bg_yellow  = "43",
    bg_blue    = "44", bg_magenta = "45", bg_cyan    = "46", bg_white   = "47",
    -- Styles
    bold      = "1",
    underline = "4",
    dim       = "2",
    blink     = "5",
    -- Selective resets
    ["/bold"]      = "22",
    ["/underline"] = "24",
    ["/dim"]       = "22",
    ["/blink"]     = "25",
    ["/color"]     = "39",
    ["/bg"]        = "49",
}
local Vec2 = TEN.Vec2
local Vec3 = TEN.Vec3
local Rotation = TEN.Rotation
local Color = TEN.Color
local Time = TEN.Time
local logLevelEnums = TEN.Util.LogLevel
local logLevelError  = logLevelEnums.ERROR
local logLevelWarning = logLevelEnums.WARNING

local IsNumber = Type.IsNumber
local IsVec2 = Type.IsVec2
local IsVec3 = Type.IsVec3
local IsColor = Type.IsColor
local IsTime = Type.IsTime
local IsRotation = Type.IsRotation
local IsString = Type.IsString
local IsTable = Type.IsTable
local IsEnumValue = Type.IsEnumValue
local LogMessage  = TEN.Util.PrintLog

-- State for deep table copy (CloneValue)
local _nextCopyId = 1          -- Progressive ID generator for each copy operation
local _activeCopies = {}       -- Tracks active copy operations: { [id] = { depth, elementCount, visited } }

local GeneralUtils = {}

-- Support function for deep table copy
local function DeepCopyRecursive(original, copyId)
    local context = _activeCopies[copyId]

    -- Check maximum depth
    if context.depth >= MAX_DEPTH then
        LogMessage("Warning in GeneralUtils.CloneValue: Maximum depth (" ..  MAX_DEPTH .. ") exceeded.", logLevelWarning)
        return {}
    end

    -- Check if we've already copied this table (prevents infinite loops)
    if context.visited[original] then
        return context.visited[original]
    end

    -- Create new table and register it immediately
    local copy = {}
    context.visited[original] = copy
    context.depth = context.depth + 1

    for key, value in next, original do
        context.elementCount = context.elementCount + 1

        -- Check maximum elements
        if context.elementCount >= MAX_ELEMENTS then
            LogMessage("Warning in GeneralUtils.CloneValue: Maximum elements (" .. MAX_ELEMENTS .. ") exceeded.", logLevelWarning)
            return copy
        end

        -- Deep copy nested tables
        if IsTable(value) then
            copy[key] = DeepCopyRecursive(value, copyId)
        else
            copy[key] = value
        end
    end

    context.depth = context.depth - 1
    return copy
end

local CheckOperator = function(operator)
	if not Type.IsNumber(operator) then
		return nil
	end
    local op = COMPARISON_OPS[operator + 1]
    return Type.IsFunction(op) and op or nil
end

-- neutralizes ANSI escape codes in a string to prevent corrupted sequences
local function NeutralizeANSI(s)
    return s:gsub("\27%[([0-9;]+)m", function(seq)
        return "\27[\226\128\139" .. seq .. "m"
    end)
end


--- General utilities.
-- Generic helper functions that work with multiple data types.
-- @section general

--- Clone a value, creating an independent copy.
-- Works with lua primitives and TEN primitives (`Vec2`, `Vec3`, `Rotation`, `Color`, `Time`).
-- For primitive types (number, string, bool, nil), returns the value itself.
-- This solves the reference assignment problem where modifying a copy affects the original.
-- @tparam nil|number|string|boolean|table|Vec2|Vec3|Rotation|Color|Time value The value to clone (can be any type).
-- @treturn[1] nil|number|string|boolean|table|Vec2|Vec3|Rotation|Color|Time An independent copy of the value.
-- @treturn[2] nil If the type is unsupported.
-- @usage
-- -- Problem: reference assignment
-- local row = { TEN.Rotation(244, 90, 276) }
-- local t1 = { rotation = row[1] }
-- local t2 = t1.rotation  -- This is a REFERENCE, not a copy!
-- t2.x = 500
-- print(row[1].x)  -- Prints 500! Original was modified
--
-- -- Solution: use CloneValue
-- local t2 = GeneralUtils.CloneValue(t1.rotation)  -- Independent copy
-- t2.x = 500
-- print(row[1].x)  -- Prints 244 (original unchanged)
--
-- -- Example with Vec3:
-- local pos1 = TEN.Vec3(100, 200, 300)
-- local pos2 = GeneralUtils.CloneValue(pos1)
-- pos2.x = 999
-- -- pos1.x is still 100
--
-- -- Example with Color:
-- local color1 = TEN.Color(255, 0, 0, 255)
-- local color2 = GeneralUtils.CloneValue(color1)
-- color2.r = 0
-- -- color1.r is still 255
--
-- -- Example with table:
-- local config = { speed = 10, enabled = true }
-- local configCopy = GeneralUtils.CloneValue(config)
-- configCopy.speed = 20
-- -- config.speed is still 10
--
-- -- Example with primitives (returned as-is):
-- local num = GeneralUtils.CloneValue(42)        -- Returns 42
-- local str = GeneralUtils.CloneValue("hello")   -- Returns "hello"
-- local bool = GeneralUtils.CloneValue(true)     -- Returns true
--
-- -- Error handling example:
-- local pos1 = TEN.Vec3(100, 200, 300)
-- local posCopy = GeneralUtils.CloneValue(pos1)
-- if posCopy then
--     posCopy.x = 999
--     -- pos1.x is still 100
-- else
--     TEN.Util.PrintLog("Failed to clone value", TEN.Util.LogLevel.ERROR)
-- end
--
-- -- Practical use: safe parameter passing
-- function ModifyPosition(pos)
--     if not Type.IsVec3(pos) then -- Validate input type with Type module
--         TEN.Util.PrintLog("Error: expected Vec3", TEN.Util.LogLevel.ERROR)
--         return
--     end
--     local safePos = GeneralUtils.CloneValue(pos)
--     safePos.x = safePos.x + 100
--     return safePos  -- Original pos is unchanged
-- end
--
-- -- Unsupported types (e.g., functions) are returned nil
-- local funcCopy = GeneralUtils.CloneValue(function() end)  -- Logs warning, returns nil
GeneralUtils.CloneValue = function(value)
    -- Handle primitive types (these are copied by value in Lua)
    local valueType = type(value)
    if valueType == "nil" or valueType == "boolean" or valueType == "number" or valueType == "string" then
        return value
    end

    -- Handle TEN engine types (userdata)
    if IsVec2(value) then
        return Vec2(value.x, value.y)
    end

    if IsVec3(value) then
        return Vec3(value.x, value.y, value.z)
    end

    if IsRotation(value) then
        return Rotation(value.x, value.y, value.z)
    end

    if IsColor(value) then
        return Color(value.r, value.g, value.b, value.a)
    end

    if IsTime(value) then
        return Time(value:GetFrameCount())
    end

    -- Handle Lua tables (deep copy)
    if IsTable(value) then
        -- Generate unique ID for this copy operation
        local copyId = _nextCopyId
        _nextCopyId = _nextCopyId + 1

        -- Initialize context for this copy
        _activeCopies[copyId] = {
            depth = 0,
            elementCount = 0,
            visited = {}  -- Prevents infinite loops on circular references
        }

        -- Execute deep copy
        local result = DeepCopyRecursive(value, copyId)

        -- Cleanup: remove context for this copy
        _activeCopies[copyId] = nil

        return result
    end

    -- Unsupported type
    LogMessage("Warning in GeneralUtils.CloneValue: unsupported type '" .. valueType .. "'. Returning nil.", logLevelWarning)
    return nil
end

--- Get a value or return a default if the value is nil.
-- Unlike the Lua `or` operator, this function correctly handles `false` and `0` as valid values.
-- Only returns defaultValue when value is exactly `nil`.
-- @tparam any value The value to check.
-- @tparam any defaultValue The default value to return if value is nil.
-- @treturn any The value if not nil, otherwise defaultValue.
-- @usage
-- -- Problem with Lua's 'or' operator:
-- In Lua, 'false' is treated as falsy. Using 'or' will accidentally
-- overwrite an explicit 'false' value set by the user with the default value.
-- local enabled = false
-- local result = enabled or true  -- Result: true (Flawed! Overwrites the user's choice)
--
-- -- Solution with GetOrDefault:
-- local enabled = false
-- local result = GeneralUtils.GetOrDefault(enabled, true)  -- Result: false (correct!)
--
-- -- Example with 0 (another falsy value in 'or'):
-- local damage = 0
-- local finalDamage = damage or 10  -- Result: 10 (wrong! 0 is valid)
-- local finalDamage = GeneralUtils.GetOrDefault(damage, 10)  -- Result: 0 (correct!)
--
-- -- Example with nil (works like 'or'):
-- local speed = nil
-- local finalSpeed = GeneralUtils.GetOrDefault(speed, 100)  -- Result: 100 (correct!)
--
-- -- Example with configuration:
-- local config = { volume = 0, mute = false }
-- local volume = GeneralUtils.GetOrDefault(config.volume, 100)  -- Result: 0 (not 100!)
-- local mute = GeneralUtils.GetOrDefault(config.mute, true)     -- Result: false (not true!)
--
-- -- Practical use: optional function parameters
-- function SetPlayerSpeed(speed)
--     speed = GeneralUtils.GetOrDefault(speed, 10)  -- Default to 10 if not provided
--     player.speed = speed
-- end
-- SetPlayerSpeed(0)      -- Sets speed to 0 (not 10!)
-- SetPlayerSpeed(false)  -- Sets speed to false (valid in some contexts)
-- SetPlayerSpeed(nil)    -- Sets speed to 10 (default)
--
-- -- Example with table field:
-- local settings = { showHUD = false }  -- User explicitly disabled HUD
-- local showHUD = GeneralUtils.GetOrDefault(settings.showHUD, true)  -- Result: false (respects user choice)
GeneralUtils.GetOrDefault = function(value, defaultValue)
    if value == nil then
        return defaultValue
    end
    return value
end

--- Check if a value is empty.
-- Returns true for nil, empty strings, and empty tables. All other values return false.
-- Numbers (including 0), booleans (including false), and TEN types are never considered empty.
-- @tparam any value The value to check.
-- @treturn bool True if the value is nil, empty string, or empty table. False otherwise.
-- @usage
-- -- Nil values:
-- local isEmpty = GeneralUtils.IsEmpty(nil)  -- Result: true
--
-- -- Empty strings:
-- local isEmpty = GeneralUtils.IsEmpty("")  -- Result: true
-- local isEmpty = GeneralUtils.IsEmpty("   ")  -- Result: false (not empty, contains spaces)
--
-- -- Empty tables:
-- local isEmpty = GeneralUtils.IsEmpty({})  -- Result: true
-- local isEmpty = GeneralUtils.IsEmpty({ a = 1 })  -- Result: false
--
-- -- Important: Lua doesn't store nil values in tables!
-- local isEmpty = GeneralUtils.IsEmpty({nil, nil, nil})  -- Result: true (table is actually empty!)
-- local isEmpty = GeneralUtils.IsEmpty({1, nil, 3})      -- Result: false (has elements at index 1 and 3)
-- -- Explanation: In Lua, {nil, nil, nil} creates an empty table because nil values are not stored.
--
-- -- Numbers (never empty, even 0):
-- local isEmpty = GeneralUtils.IsEmpty(0)  -- Result: false
-- local isEmpty = GeneralUtils.IsEmpty(-5)  -- Result: false
--
-- -- Booleans (never empty, even false):
-- local isEmpty = GeneralUtils.IsEmpty(false)  -- Result: false
-- local isEmpty = GeneralUtils.IsEmpty(true)  -- Result: false
--
-- -- TEN types (never empty):
-- local isEmpty = GeneralUtils.IsEmpty(TEN.Vec3(0, 0, 0))  -- Result: false
-- local isEmpty = GeneralUtils.IsEmpty(TEN.Color(0, 0, 0, 0))  -- Result: false
--
-- -- Practical use: validate user input
-- function ProcessName(name)
--     if GeneralUtils.IsEmpty(name) then
--         TEN.Util.PrintLog("Error: Name cannot be empty!", TEN.Util.LogLevel.ERROR)
--         return false
--     end
--     -- Process name...
--     return true
-- end
--
-- -- Practical use: check if table has data
-- local inventory = {}
-- if GeneralUtils.IsEmpty(inventory) then
--     TEN.Util.PrintLog("Inventory is empty", TEN.Util.LogLevel.INFO)
-- else
--     -- Show inventory...
-- end
--
-- -- Practical use: validate configuration
-- local config = LoadConfig()
-- if GeneralUtils.IsEmpty(config) then
--     config = GetDefaultConfig()  -- Use defaults if config is empty
-- end
GeneralUtils.IsEmpty = function(value)
    -- Check for nil
    if value == nil then
        return true
    end

    -- Check for empty string
    if IsString(value) and value == "" then
        return true
    end

    -- Check for empty table
    if IsTable(value) then
        for _ in next, value do
            return false  -- Has at least one element
        end
        return true  -- No elements
    end

    -- All other values (numbers, booleans, TEN types, etc.) are not empty
    return false
end

--- Comparison and validation functions.
-- Utilities for comparing values and checking ranges.
-- @section comparison

--- Compare two values based on the specified operator.
-- @tparam number|string|Time operand The first value to compare.
-- @tparam number|string|Time reference The second value to compare against.
-- @tparam number operator The comparison operator<br>0: equal<br> 1: not equal<br> 2: less than<br> 3: less than or equal<br> 4: greater than<br> 5: greater than or equal
-- @treturn[1] bool The result of the comparison.
-- @treturn[2] bool false If an error occurs (invalid operator or type mismatch), with an error message.
-- @usage
-- -- Examples with numbers:
-- local isEqual = GeneralUtils.CompareValues(5, 5, 0) -- true (equal)
-- local isLessThan = GeneralUtils.CompareValues(3.5, 4.0, 2) -- true (3.5 < 4.0)
-- local isGreaterThan = GeneralUtils.CompareValues(10, 2, 4) -- true (10 > 2)
--
-- -- Examples with strings:
-- local isEqual = GeneralUtils.CompareValues("test", "test", 0) -- true (equal)
--
-- local isLessThan = GeneralUtils.CompareValues("apple", "banana", 2) -- true 
-- -- ("apple" < "banana" in lexicographical order)
--
-- local isGreaterThan = GeneralUtils.CompareValues("zebra", "ant", 4) -- true
-- -- ("zebra" > "ant" in lexicographical order)
--
-- local sLessThan = GeneralUtils.CompareValues("Z", "a", 2) -- true ("Z" < "a" in ASCII)
--
-- local isLessThan = GeneralUtils.CompareValues("2", "15", 2) -- false 
-- -- ("2" > "15" in lexicographical order, because '2' > '1')
--
-- -- Examples with Time:
-- local time1 = TEN.Time(120)  -- 120 frames
-- local time2 = TEN.Time(150)  -- 150 frames
-- local isLessThan = GeneralUtils.CompareValues(time1, time2, 2) -- true (120 < 150)
-- local isGreaterThanOrEqual = GeneralUtils.CompareValues(time1, time2, 5) -- false (120 >= 150 is false)
GeneralUtils.CompareValues = function(operand, reference, operator)
    -- Validate operator
    local op = CheckOperator(operator)
    if not op then
        LogMessage("Error in GeneralUtils.CompareValues: invalid operator for comparison", logLevelError)
        return false
    end
    local errorMessage = "Error in GeneralUtils.CompareValues: operand and reference must be equal types."

    -- Lazy type checking
    if IsNumber(operand) then
        if not IsNumber(reference) then
            LogMessage(errorMessage, logLevelError)
            return false
        end
        return op(operand, reference)
    end

    if IsString(operand) then
        if not IsString(reference) then
            LogMessage(errorMessage, logLevelError)
            return false
        end
        return op(operand, reference)
    end

    if IsTime(operand) then
        if not IsTime(reference) then
            LogMessage(errorMessage, logLevelError)
            return false
        end
        return op(operand, reference)
    end

    LogMessage("Error in GeneralUtils.CompareValues: unsupported type.", logLevelError)
    return false
end

--- Console utilities.
-- Functions for formatted console output with colors and styles.
-- Two functions are provided:
--
-- - `Styled`: returns a styled string you can store or concatenate.
-- - `ColorLog`: parses inline `{{tag}}` markup and prints directly to the console.
--
-- <h3>Available styles:</h3>
-- <table class="tableSP">
-- <tr><td>Text colors</td><td>`red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, `black`</td></tr>
-- <tr><td>Background colors</td><td>`bg_red`, `bg_green`, `bg_yellow`, `bg_blue`, `bg_magenta`, `bg_cyan`, `bg_white`, `bg_black`</td></tr>
-- <tr><td>Text styles</td><td>`bold`, `underline`, `dim`, `blink`</td></tr>
-- </table>
--
-- <h3>Reset tags (ColorLog only):</h3>
-- <table class="tableSP">
-- <tr><td>Reset all styles</td><td>`{{/}}`</td></tr>
-- <tr><td>Reset specific style</td><td>`{{/bold}}`, `{{/underline}}`, `{{/color}}`, `{{/bg}}`</td></tr>
-- </table>
--
-- <h3>Examples usage:</h3>
-- `local msg = GeneralUtils.Styled(" ALERT ", "black", "bg_red")`
--
-- `TEN.Util.PrintLog(msg, TEN.Util.LogLevel.WARNING)`
--
-- Console output: <code style="background: #000; color: #fff; padding: 4px; font-family: monospace;">
-- [2026-Feb-26 15:01:55]
-- <span style="color: yellow;">[warning]</span>
-- <span style="background-color: red; color: black; padding: 0 4px; font-weight: bold;">ALERT</span>
-- </code>
--
-- <br>`GeneralUtils.ColorLog("{{red}}Error:{{/}} file not found")`
--
-- Console output: <code style="background: #000; color: #fff; padding: 4px; font-family: monospace;">
-- [2026-Feb-26 15:01:55]
-- <span style="color: lime;">[info]</span>
-- <span style="color: red;">Error:</span> file not found
-- </code>
-- <h3>Note:</h3> GeneralUtils automatically protects your text from special sequences that could confuse the console.
-- If your message contains strange characters like "[31m" or "[0m", you will not break the display:
-- the system neutralizes them invisibly. You can write any text without worrying about errors.
-- You do not need to know ANSI codes: just use tags and style names.
-- @section console

--- Apply ANSI styling to a text string and return it.
-- Wraps the text with ANSI escape codes and appends an automatic reset at the end.
-- The returned string can be used anywhere: `PrintLog`, concatenation, tables, etc.
-- Multiple styles can be combined by passing them as separate arguments.
-- Invalid style names are silently ignored (no crash, no error) — the text is still returned.
-- @tparam string text The text to style.
-- @tparam string ... One or more style names to apply.
-- @treturn[1] string The text wrapped in ANSI escape codes.
-- @treturn[2] string The original text unchanged if no valid styles are provided or an error occurs.
-- @usage
-- -- Single color:
-- local msg = GeneralUtils.Styled("Hello", "red")
-- TEN.Util.PrintLog(msg, TEN.Util.LogLevel.INFO)
-- -- Console: "Hello" in red
--
-- -- Multiple styles combined:
-- local msg = GeneralUtils.Styled("CRITICAL", "red", "bold", "underline")
-- TEN.Util.PrintLog(msg, TEN.Util.LogLevel.ERROR)
-- -- Console: "CRITICAL" in red, bold and underlined
--
-- -- Background color:
-- local msg = GeneralUtils.Styled(" ALERT ", "white", "bg_red")
-- TEN.Util.PrintLog(msg, TEN.Util.LogLevel.WARNING)
-- -- Console: " ALERT " with white text on red background
--
-- -- Concatenate multiple styled segments:
-- local line = GeneralUtils.Styled("HP:", "cyan") .. " " .. GeneralUtils.Styled("0", "red", "bold") .. "/" .. GeneralUtils.Styled("100", "green")
-- TEN.Util.PrintLog(line, TEN.Util.LogLevel.INFO)
-- -- Console: "HP:" in cyan, "0" in red bold, "/" default, "100" in green
--
-- -- Bold header with normal body:
-- local header = GeneralUtils.Styled("[TRAP]", "yellow", "bold")
-- local body = " spike_01 activated at frame 120"
-- TEN.Util.PrintLog(header .. body, TEN.Util.LogLevel.INFO)
-- -- Console: "[TRAP]" bold yellow, rest in default color
--
-- -- Build reusable labels:
-- local OK   = GeneralUtils.Styled("[OK]", "green", "bold")
-- local FAIL = GeneralUtils.Styled("[FAIL]", "red", "bold")
-- local WARN = GeneralUtils.Styled("[WARN]", "yellow", "bold")
-- TEN.Util.PrintLog(OK .. " Door opened", TEN.Util.LogLevel.INFO)
-- TEN.Util.PrintLog(FAIL .. " Texture not found", TEN.Util.LogLevel.ERROR)
-- TEN.Util.PrintLog(WARN .. " Low health", TEN.Util.LogLevel.WARNING)
--
-- -- Highlighted value in a debug line:
-- local pos = entity:GetPosition()
-- local msg = "Position: " .. GeneralUtils.Styled(tostring(pos), "cyan", "bold")
-- TEN.Util.PrintLog(msg, TEN.Util.LogLevel.INFO)
--
-- -- Blinking warning:
-- local msg = GeneralUtils.Styled("WARNING: Lava rising!", "red", "blink")
-- TEN.Util.PrintLog(msg, TEN.Util.LogLevel.WARNING)
-- Console: "WARNING: Lava rising!" in blinking red
--
-- -- Invalid style names are ignored (no crash):
-- local msg = GeneralUtils.Styled("Hello", "pink")  -- "pink" is not valid
-- TEN.Util.PrintLog(msg, TEN.Util.LogLevel.INFO) -- Console: "Hello" in default color (invalid style ignored)
-- -- Result: "Hello" without any styling
--
-- -- No styles provided (text returned as-is):
-- local msg = GeneralUtils.Styled("Hello")
-- -- Result: "Hello"
GeneralUtils.Styled = function(text, ...)
    if not IsString(text) then
        LogMessage("Error in GeneralUtils.Styled: text is not a string.", logLevelError)
        return ""
    end

    local args = {...}
    local argCount = #args
    if argCount == 0 then
        return text
    end

    local codes = {}
    local codesCount = 0
    for i = 1, argCount do
        local styleName = args[i]
        if IsString(styleName) then
            local code = ANSI_CODES[styleName]
            if code then
                codesCount = codesCount + 1
                codes[codesCount] = code
            end
        end
    end

    if codesCount == 0 then
        return text
    end
    -- Filter: Neutralizes suspicious ANSI sequences in user text
    local safeText = NeutralizeANSI(text)
    return ESC .. table.concat(codes, ";") .. "m" .. safeText .. ANSI_RESET
end

--- Print a styled message to the console using inline `{{tag}}` markup.
-- Parses `{{styleName}}` tags in the string and converts them to ANSI escape codes.
-- Multiple styles can be stacked: each `{{tag}}` adds to the current style.
-- Use `{{/}}` to reset all styles, or selective resets like `{{/bold}}` to remove one style.
-- The function always appends a final reset to prevent style leaking into subsequent log lines.
-- Invalid tag names are silently removed (no crash, no error).
-- @tparam string str The string with `{{tag}}` markup.
-- @tparam[opt=2] number logLevel The Log level or `Util.LogLevel` value:
--
-- 0 = ERROR
--
-- 1 = WARNING
--
-- 2 = INFO<br>
-- @usage
-- -- Simple colored message:
-- GeneralUtils.ColorLog("{{red}}Error: texture not found{{/}}")
-- -- Console: "Error: texture not found" in red
--
-- -- Multiple colors in one line:
-- GeneralUtils.ColorLog("{{green}}Loaded:{{/}} room_01 — {{yellow}}Skipped:{{/}} room_02")
-- -- Console: "Loaded:" green, " room_01 — " default, "Skipped:" yellow, " room_02" default
--
-- -- Combining color and style:
-- GeneralUtils.ColorLog("{{red}}{{bold}}DANGER:{{/}} Lava is rising!")
-- -- Console: "DANGER:" in red bold, " Lava is rising!" in default
--
-- -- Selective reset (remove bold, keep color):
-- GeneralUtils.ColorLog("{{red}}{{bold}}Title{{/bold}} still red{{/}} normal text")
-- -- Console: "Title" red bold, " still red" red, " normal text" default
--
-- -- Selective reset (change color, keep bold):
-- GeneralUtils.ColorLog("{{bold}}{{red}}Red bold {{/color}}{{blue}}Blue bold{{/}}")
-- -- Console: "Red bold " red bold, "Blue bold" blue bold
--
-- -- Background color:
-- GeneralUtils.ColorLog("{{bg_red}}{{white}} CRITICAL {{/}} System failure")
-- -- Console: " CRITICAL " white on red, " System failure" default
--
-- -- Underlined text:
-- GeneralUtils.ColorLog("Press {{underline}}{{bold}}E{{/}} to interact")
-- -- Console: "Press " default, "E" bold underlined, " to interact" default
--
-- -- Blinking text:
-- GeneralUtils.ColorLog("{{red}}{{blink}}WARNING: Lava rising!{{/}}")
-- -- Console: "WARNING: Lava rising!" in blinking red
--
-- -- Dim text:
-- GeneralUtils.ColorLog("{{dim}}Debug: value = 42{{/}}")
-- -- Console: "Debug: value = 42" in dimmed color
--
-- -- With Log levels:
-- GeneralUtils.ColorLog("{{red}}{{bold}}FATAL: out of memory{{/}}", 0)                       -- ERROR
--
-- GeneralUtils.ColorLog("{{red}}{{bold}}FATAL: out of memory{{/}}", TEN.Util.LogLevel.ERROR) -- equivalent
--
-- GeneralUtils.ColorLog("{{yellow}}Warning: low ammo{{/}}", 1)                               -- WARNING
--
-- GeneralUtils.ColorLog("{{yellow}}Warning: low ammo{{/}}", TEN.Util.LogLevel.WARNING)       -- equivalent
--
-- GeneralUtils.ColorLog("{{green}}All systems operational{{/}}")                             -- INFO (default)
--
-- GeneralUtils.ColorLog("{{green}}All systems operational{{/}}", TEN.Util.LogLevel.INFO)     -- equivalent
--
-- -- Text without tags (printed as-is):
-- GeneralUtils.ColorLog("Plain text, no styling")
-- -- Console: "Plain text, no styling" in default color
--
-- -- Invalid tags are silently removed:
-- GeneralUtils.ColorLog("{{pink}}Hello{{/}}")
-- -- Console: "Hello" in default color ("pink" is not a valid style)
--
-- -- Practical: debug header with separator
-- GeneralUtils.ColorLog("{{cyan}}{{bold}}========== LEVEL START =========={{/}}")
--
-- -- Mixed with StringUtils.Format function:
-- local enemy = "Skeleton"
-- local dmg = 50
-- GeneralUtils.ColorLog(
--     StringUtils.Format(
--         "{{cyan}}{enemy}{{/}} dealt {{red}}{{bold}}{dmg}{{/}} damage", 
--         { enemy = enemy, dmg = dmg }
--     )
-- )
-- -- Console: "Skeleton" cyan, " dealt " default, "50" red bold, " damage" default
--
-- -- Practical: entity spawn log
-- GeneralUtils.ColorLog(
--     StringUtils.Format(
--         "{{green}}+{{/}} Spawned {{bold}}{name}{{/}} at {pos}", 
--         { name = "enemy_mummy_01", pos = TEN.Vec3(1024, -512, 3072) }
--     )
-- )
-- -- Console: "+" green, " Spawned " default, "enemy_mummy_01" bold, " at {1024, -512, 3072}" default
--
-- -- Styled status line:
-- local hp = 15
-- local maxHp = 100
-- GeneralUtils.ColorLog(
--     StringUtils.Format(
--         "{{red}}HP: {{bold}}{hp}{{/bold}}/{maxHp}{{/}}", 
--         { hp = hp, maxHp = maxHp }
--     )
-- )
-- -- Console: "HP: " red, "15" red bold, "/100" red
GeneralUtils.ColorLog = function(str, logLevel)
    if not IsString(str) then
        LogMessage("Error in GeneralUtils.ColorLog: str is not a string.", logLevelError)
        return
    end

    logLevel = logLevel or 2
    if not IsEnumValue(logLevel, logLevelEnums, false) then
        LogMessage("Error in GeneralUtils.ColorLog: logLevel is not a valid value. It must be 0, 1, or 2.", logLevelError)
        return
    end
    local safeText = NeutralizeANSI(str)

    local result = safeText:gsub("{{(.-)}}", function(tag)
        if tag == "/" then
            return ANSI_RESET
        end
        local code = ANSI_CODES[tag]
        if code then
            return ESC .. code .. "m"
        end
        return ""
    end)
    -- Filter: Neutralizes suspicious ANSI sequences in user text
    LogMessage(result .. ANSI_RESET, logLevel)
end

return GeneralUtils