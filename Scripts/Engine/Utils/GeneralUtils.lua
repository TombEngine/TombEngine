-----<style>table.function_list td.name {min-width: 419px;}</style>
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
---	local GeneralUtils = require("Engine.Utils.GeneralUtils")
-- @luautil GeneralUtils

local Type = require("Engine.Type")
local Utility = require("Engine.Util")
local TableUtils = require("Engine.Utils.TableUtils")

local MAX_DEPTH = Utility.Constants.MAX_DEPTH
local MAX_ELEMENTS = Utility.Constants.MAX_ELEMENTS

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

local Vec2 = TEN.Vec2
local Vec3 = TEN.Vec3
local Rotation = TEN.Rotation
local Color = TEN.Color
local Time = TEN.Time

local IsNumber = Type.IsNumber
local IsVec2 = Type.IsVec2
local IsVec3 = Type.IsVec3
local IsColor = Type.IsColor
local IsTime = Type.IsTime
local IsRotation = Type.IsRotation
local IsString = Type.IsString
local IsTable = Type.IsTable
local IsNull = Type.IsNull
local IsBoolean = Type.IsBoolean

local InfoLog = Utility.InfoLog
local ErrorLog = Utility.ErrorLog
local WarningLog = Utility.WarningLog

-- State for deep table copy (CloneValue)
local _nextCopyId = 1          -- Progressive ID generator for each copy operation
local _activeCopies = {}       -- Tracks active copy operations: { [id] = { depth, elementCount, visited } }

local compareErrorMessage = "Error in {context}: operand and reference must be equal types."

local GeneralUtils = {}

GeneralUtils.Operators = -- Comparison operators for easy comparisons
    {
        EQUAL = 0,        -- Equal
        NOT_EQUAL = 1,    -- Not equal
        LESS = 2,         -- Less than
        LESS_EQUAL = 3,   -- Less than or equal
        GREATER = 4,      -- Greater than
        GREATER_EQUAL = 5 -- Greater than or equal
    }
TableUtils.SetTableReadOnly(GeneralUtils.Operators)

local function CheckOperator(operator)
	if not Type.IsNumber(operator) then
		return nil
	end
    local op = COMPARISON_OPS_FUNC[operator + 1]
    return Type.IsFunction(op) and op or nil
end

-- Support function for deep table copy
local function DeepCopyRecursive(original, copyId)
    local context = _activeCopies[copyId]

    -- Check maximum depth
    if context.depth >= MAX_DEPTH then
        WarningLog("Warning in GeneralUtils.CloneValue: Maximum depth ({max}) exceeded.", {max = MAX_DEPTH})
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
            WarningLog("Warning in GeneralUtils.CloneValue: Maximum elements ({max}) exceeded.", {max = MAX_ELEMENTS})
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

--- Log a info message with optional variable substitution.
-- @tparam string str The message string, which can contain placeholders like `{varName}`. If the message is not a string, it will be converted to a string using `tostring()`.
-- @tparam[opt] table vars A table of variables to substitute into the message. If provided, it must be a table. If it is not a table it will be ignored.
-- @usage
-- -- Simple usage:
-- GeneralUtils.InfoLog("Simple info message.")
-- -- Result: [2026-Jul-16 16:55:01] [info] Simple info message.
--
-- -- With table of variables:
-- -- Tracking player state during development:
-- GeneralUtils.InfoLog("Player {name} entered room {room}.", {name = "Lara", room = "secret_01"})
GeneralUtils.InfoLog = function (str, vars)
    if not IsNull(vars) and not IsTable(vars) then
        vars = nil
    end
    InfoLog(tostring(str), vars)
end

--- Log a warning message with optional variable substitution.
-- @tparam string str The message string, which can contain placeholders like `{varName}`. If the message is not a string, it will be converted to a string using `tostring()`.
-- @tparam[opt] table vars A table of variables to substitute into the message. If provided, it must be a table. If it is not a table it will be ignored
-- @usage
-- -- -- Simple usage:
-- GeneralUtils.WarningLog("This is a warning message.")
-- -- Result: [2026-Jul-16 16:55:01] [warning] This is a warning message.
--
-- -- With table of variables:
-- -- Warning during module initialization:
-- local speed = 200
-- if speed > 100 then
--     GeneralUtils.WarningLog("Config speed {speed} exceeds maximum, clamping to 100.", {speed = speed})
--     speed = 100
-- end
GeneralUtils.WarningLog = function (str, vars)
    if not IsNull(vars) and not IsTable(vars) then
        vars = nil
    end
    WarningLog(tostring(str), vars)
end

--- Log an error message with optional variable substitution.
-- @tparam string str The message string, which can contain placeholders like `{varName}`. If the message is not a string, it will be converted to a string using `tostring()`.
-- @tparam[opt] table vars A table of variables to substitute into the message. If provided, it must be a table. If it is not a table it will be ignored.
-- @usage
-- -- Simple usage:
-- GeneralUtils.ErrorLog("This is an error message.")
-- -- Result: [2026-Jul-16 16:55:01] [error] This is an error message.
--
-- -- With table of variables:
-- GeneralUtils.ErrorLog("Failed to load resource {resource}.lua .", {resource = "MyFunc"})
GeneralUtils.ErrorLog = function (str, vars)
    if not IsNull(vars) and not IsTable(vars) then
        vars = nil
    end
    ErrorLog(tostring(str), vars)
end

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
    WarningLog("Warning in GeneralUtils.CloneValue: unsupported type '{valueType}'. Returning nil.", {valueType = valueType})
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
    if IsNull(value) then
        return defaultValue
    end
    return value
end

--- Validate a value and return a default if it fails validation or is nil.
-- Unlike GetOrDefault, this also checks a condition and logs a warning on failure.
-- @tparam any value The value to validate.
-- @tparam bool isValid The result of your validation check (evaluated before calling).
-- @tparam any defaultValue The default value to return if validation fails.
-- @tparam[opt=""] string warningMsg A warning message to log on validation failure. If not provided or if warningMsg is not a string, a generic warning is logged.
-- @treturn any The value if valid, otherwise defaultValue.
-- @usage
-- -- Instead of:
-- if a == nil then a = 1.0
-- elseif not IsNumber(a) or a < 0 or a > 1 then
--     TEN.Util.PrintLog("Warning: ...", TEN.Util.LogLevel.WARNING)
--     a = 1.0
-- end
--
-- -- Write:
-- local a = GeneralUtils.ValidateOrDefault(hsl.a,
--     IsNumber(hsl.a) and hsl.a >= 0 and hsl.a <= 1,
--     1.0,
--     "Warning: a should be a number in [0, 1].")
--
-- -- Silently assign a default value
-- local a = GeneralUtils.ValidateOrDefault(hsl.a,
--     IsNumber(hsl.a) and hsl.a >= 0 and hsl.a <= 1,
--     1.0)
--
-- -- Practical use: validate user input
-- function PlaySound(soundID)
--     soundID = GeneralUtils.ValidateOrDefault(soundID,
--         IsNumber(soundID) and soundID > 0 and soundID <= 1000,  -- Example validation: soundID must be in [1, 1000]
--         1,
--         "Warning: soundID should be a positive number and less than or equal to 1000.")
--     -- Play the sound with the validated soundID
--     TEN.Sound.PlaySound(soundID)
-- end
GeneralUtils.ValidateOrDefault = function(value, isValid, defaultValue, warningMsg)
    if not IsBoolean(isValid) then
        ErrorLog("Error in GeneralUtils.ValidateOrDefault: isValid must be a boolean.")
        return defaultValue
    end
    if IsNull(value) then
        return defaultValue
    end
    if isValid then
        return value
    end
    if warningMsg and IsString(warningMsg) then
        ErrorLog(warningMsg)
    else
        ErrorLog("Error in GeneralUtils.ValidateOrDefault: value failed validation, using default value : {default}", {default = defaultValue})
    end
    return defaultValue
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
    if IsNull(value) then
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

--- Compare two values based on the specified operator.
-- @tparam number|string|Time operand The first value to compare.
-- @tparam number|string|Time reference The second value to compare against.
-- @tparam Operators operator The comparison operator to use.
-- @tparam[opt="GeneralUtils.CompareValues"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] bool `result`: The result of the comparison or false if an error occurs (invalid operator or type mismatch), with an error message
-- @usage
-- -- Examples with numbers:
-- local isEqual = GeneralUtils.CompareValues(5, 5, GeneralUtils.Operators.EQUAL) -- true (equal)
-- local isLessThan = GeneralUtils.CompareValues(3.5, 4.0, GeneralUtils.Operators.LESS) -- true (3.5 < 4.0)
-- local isGreaterThan = GeneralUtils.CompareValues(10, 2, GeneralUtils.Operators.GREATER) -- true (10 > 2)
--
-- -- Examples with strings:
-- local isEqual = GeneralUtils.CompareValues("test", "test", GeneralUtils.Operators.EQUAL) -- true (equal)
--
-- local isLessThan = GeneralUtils.CompareValues("apple", "banana", GeneralUtils.Operators.LESS) -- true 
-- -- ("apple" < "banana" in lexicographical order)
--
-- local isGreaterThan = GeneralUtils.CompareValues("zebra", "ant", GeneralUtils.Operators.GREATER) -- true
-- -- ("zebra" > "ant" in lexicographical order)
--
-- local sLessThan = GeneralUtils.CompareValues("Z", "a", GeneralUtils.Operators.LESS) -- true ("Z" < "a" in ASCII)
--
-- local isLessThan = GeneralUtils.CompareValues("2", "15", GeneralUtils.Operators.LESS) -- false 
-- -- ("2" > "15" in lexicographical order, because '2' > '1')
--
-- -- Examples with Time:
-- local time1 = TEN.Time(120)  -- 120 frames
-- local time2 = TEN.Time(150)  -- 150 frames
-- local isLessThan = GeneralUtils.CompareValues(time1, time2, GeneralUtils.Operators.LESS) -- true (120 < 150)
-- local isGreaterThanOrEqual = GeneralUtils.CompareValues(time1, time2, GeneralUtils.Operators.GREATER_EQUAL) -- false (120 >= 150 is false)
--
-- -- Advanced use
-- -- Cooldown check in an OnLoop callback
-- local lastAttack = TEN.Time(0)
-- local cooldown = TEN.Time(90)  -- 3 seconds at 30 FPS
-- LevelFuncs.OnLoop = function()
--     local now = TEN.Flow.GetStatistics().timeTaken
--     local elapsed = now - lastAttack  -- TEN.Time subtraction
--     if GeneralUtils.CompareValues(elapsed, cooldown, GeneralUtils.Operators.GREATER_EQUAL, "OnLoop") then
--         -- Cooldown expired, allow next action
--         lastAttack = now
--     end
-- end
--
-- -- Difficulty-based damage multiplier
-- local difficulty = "hard"
-- local multiplier = 1.0
-- if GeneralUtils.CompareValues(difficulty, "hard", GeneralUtils.Operators.EQUAL, "SetDifficulty") then
--     multiplier = 2.0
-- elseif GeneralUtils.CompareValues(difficulty, "easy", GeneralUtils.Operators.EQUAL, "SetDifficulty") then
--     multiplier = 0.5
-- end
--
-- -- Threshold check for collectibles counter
-- local collected = 7
-- local required = 5
-- if GeneralUtils.CompareValues(collected, required, GeneralUtils.Operators.GREATER_EQUAL, "CheckProgress") then
--     GeneralUtils.InfoLog("All {required} items collected, opening door.", {required = required})
--     -- Trigger door open sequence
-- end
GeneralUtils.CompareValues = function(operand, reference, operator, errorContext)
    errorContext = errorContext or "GeneralUtils.CompareValues"
    -- Validate operator
    local op = CheckOperator(operator)
    if not op then
        ErrorLog("Error in {context}: invalid operator.", { context = errorContext })
        return false
    end

    -- Lazy type checking
    if IsNumber(operand) then
        if not IsNumber(reference) then
            ErrorLog(compareErrorMessage, { context = errorContext })
            return false
        end
        return op(operand, reference)
    end

    if IsTime(operand) then
        if not IsTime(reference) then
            ErrorLog(compareErrorMessage, { context = errorContext })
            return false
        end
        return op(operand, reference)
    end

    if IsString(operand) then
        if not IsString(reference) then
            ErrorLog(compareErrorMessage, { context = errorContext })
            return false
        end
        return op(operand, reference)
    end

    ErrorLog("Error in {context}: unsupported type.", { context = errorContext })
    return false
end

----
-- Tables
-- @section tables

---
-- Constants for operators in @{GeneralUtils.CompareValues}.
--
-- Use them as `GeneralUtils.Operators.EQUAL`, `GeneralUtils.Operators.LESS`, etc.
-- @table Operators
-- @tfield 0 EQUAL Equal operator.
-- @tfield 1 NOT_EQUAL Not equal operator.
-- @tfield 2 LESS Less than operator.
-- @tfield 3 LESS_EQUAL Less than or equal operator.
-- @tfield 4 GREATER Greater than operator.
-- @tfield 5 GREATER_EQUAL Greater than or equal operator.

return GeneralUtils