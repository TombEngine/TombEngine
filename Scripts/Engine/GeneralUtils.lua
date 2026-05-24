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

return GeneralUtils