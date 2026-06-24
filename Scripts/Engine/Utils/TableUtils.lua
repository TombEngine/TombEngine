-----<style>table.function_list td.name {min-width: 365px;}</style>
--- Lua support functions for working with tables.
---
--- **Design Philosophy:**
--- TableUtils is designed primarily for:
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
---	local TableUtils = require("Engine.TableUtils")
-- @luautil TableUtils

local Type = require("Engine.Type")
local Util = require("Engine.Util")
local TableUtils = {}

local TableHasValueRaw = Util.TableHasValue
local GetMaxNumericIndex = Util.GetMaxNumericIndex

local IsTable = Type.IsTable

local LogMessage  = TEN.Util.PrintLog
local logLevelEnums = TEN.Util.LogLevel
local logLevelError  = logLevelEnums.ERROR
local logLevelWarning = logLevelEnums.WARNING

local pairs = pairs
local ipairs = ipairs
local next = next
local tableRemove = table.remove

local MAX_DEPTH = 10        -- Maximum recursion depth for deep operations (prevents stack overflow)
local MAX_ELEMENTS = 1000   -- Maximum elements processed in deep operations (prevents performance issues)

local _nextCompareId = 1       -- Progressive ID generator for each comparison operation
local _activeCompares = {}     -- Tracks active comparisons: { [id] = { depth, elementCount, visited } }

-- Support function for recursive comparison
local function CompareRecursive(t1, t2, compareId)
    local context = _activeCompares[compareId]

    -- Check maximum depth
    if context.depth >= MAX_DEPTH then
        LogMessage("Warning in TableUtils.CompareTablesDeep: Maximum depth (" .. MAX_DEPTH .. ") exceeded.", logLevelWarning)
        return false
    end

    -- Prevent infinite loops: check if we've already visited this pair
    local pairKey = tostring(t1) .. "-" .. tostring(t2)
    if context.visited[pairKey] then
        return true  -- Already visited, assume equal
    end
    context.visited[pairKey] = true

    -- Increment depth
    context.depth = context.depth + 1

    -- Single loop: compare all keys from both tables
    local currentKeysChecked = {}

    for key, value1 in next, t1 do
        context.elementCount = context.elementCount + 1

        -- Check maximum elements
        if context.elementCount >= MAX_ELEMENTS then
            LogMessage("Warning in TableUtils.CompareTablesDeep: Maximum elements (" ..  MAX_ELEMENTS .. ") exceeded.", logLevelWarning)
            return false
        end

        local value2 = t2[key]
        currentKeysChecked[key] = true

        -- If key doesn't exist in t2, tables are different
        if value2 == nil then
            context.depth = context.depth - 1
            return false
        end

        -- Compare values
        if IsTable(value1) and IsTable(value2) then
            if not CompareRecursive(value1, value2, compareId) then
                context.depth = context.depth - 1
                return false
            end
        elseif value1 ~= value2 then
            context.depth = context.depth - 1
            return false
        end
    end

    -- Check if t2 has keys that t1 doesn't have
    for key, _ in next, t2 do
        if not currentKeysChecked[key] then
            context.depth = context.depth - 1
            return false
        end
    end

    -- Decrement depth before returning
    context.depth = context.depth - 1
    return true
end

--- Get the number of elements in a table (works for non-sequential tables).
-- @tparam table tbl The table to count.
-- @treturn[1] int The number of elements.
-- @treturn[2] int 0 if input is invalid.
-- @usage
-- -- Example with non-sequential table:
-- local tbl = { apple = 1, banana = 2, cherry = 3 }
-- local count = TableUtils.TableSize(tbl) -- Result: 3
TableUtils.TableSize = function(tbl)
    if not Type.IsTable(tbl) then
        LogMessage("Error in TableUtils.TableSize: input must be a table.", logLevelError)
        return 0
    end
    local count = 0
    for _ in next, tbl do
        count = count + 1
    end
    return count
end

--- Compare two tables for equality.
--- This function checks if both tables have the same keys and corresponding values. Works for shallow comparisons only.
--- @tparam table tbl1 The first table to compare.
--- @tparam table tbl2 The second table to compare.
--- @treturn[1] bool True if the tables are equal, false otherwise.
--- @treturn[2] bool false if an error occurs.
--- @usage
--- local tblA = { a = 1, b = 2 }
--- local tblB = { a = 1, b = 2 }
--- local tblC = { a = 1, b = 3 }
--- local isEqualAB = TableUtils.CompareTables(tblA, tblB) -- Result: true
--- local isEqualAC = TableUtils.CompareTables(tblA, tblC) -- Result: false
TableUtils.CompareTables = function (tbl1, tbl2)
    if not (IsTable(tbl1) and IsTable(tbl2)) then
        LogMessage("Error in TableUtils.CompareTables: both inputs must be tables.", logLevelError)
        return false
    end

    -- Track keys checked from tbl1
    local keysChecked = {}

    -- Check all keys from tbl1
    for key, value in next, tbl1 do
        if tbl2[key] ~= value then
            return false
        end
        keysChecked[key] = true
    end

    -- Check if tbl2 has any extra keys not in tbl1
    for key, _ in next, tbl2 do
        if not keysChecked[key] then
            return false  -- tbl2 has a key that tbl1 doesn't have
        end
    end

    return true
end

--- Deeply compare two tables for equality.
--- This function checks if both tables have the same keys and corresponding values, including nested tables.
--- **Limits:** Maximum depth of 10 levels and 1000 total elements processed to prevent performance issues.
--- @tparam table tbl1 The first table to compare.
--- @tparam table tbl2 The second table to compare.
--- @treturn[1] bool True if the tables are deeply equal, false otherwise.
--- @treturn[2] bool false if an error occurs.
--- @usage
--- local tblA = { a = 1, b = { c = 2, d = 3 } }
--- local tblB = { a = 1, b = { c = 2, d = 3 } }
--- local tblC = { a = 1, b = { c = 2, d = 4 } }
--- local isEqualAB = TableUtils.CompareTablesDeep(tblA, tblB) -- Result: true
--- local isEqualAC = TableUtils.CompareTablesDeep(tblA, tblC) -- Result: false
TableUtils.CompareTablesDeep = function (tbl1, tbl2)
    if not (IsTable(tbl1) and IsTable(tbl2)) then
        LogMessage("Error in TableUtils.CompareTablesDeep: both inputs must be tables.", logLevelError)
        return false
    end

    -- Generate unique ID for this comparison
    local compareId = _nextCompareId
    _nextCompareId = _nextCompareId + 1

    -- Initialize context for this comparison
    _activeCompares[compareId] = {
        depth = 0,
        elementCount = 0,
        visited = {},  -- Prevents infinite loops on circular tables
        keysChecked = {}  -- Tracks which keys we've already processed
    }

    -- Execute comparison
    local result = CompareRecursive(tbl1, tbl2, compareId)

    -- Cleanup: remove context for this comparison
    _activeCompares[compareId] = nil

    return result
end

--- Check if a table contains a specific value.
-- @tparam table tbl The table to check.
-- @tparam any val The value to search for.
-- @treturn[1] bool True if the value is found, false otherwise.
-- @treturn[2] bool false if an error occurs.
-- @usage
-- -- Example with associative table:
-- local tbl = { apple = 1, banana = 2, cherry = 3 }
-- local hasBanana = TableUtils.TableHasValue(tbl, 3) -- Result: true
-- local hasGrape = TableUtils.TableHasValue(tbl, 0) -- Result: false
--
-- -- Example with array table:
-- local tbl = { "apple", "banana", "cherry" }
-- local hasBanana = TableUtils.TableHasValue(tbl, "banana") -- Result: true
-- local hasGrape = TableUtils.TableHasValue(tbl, "grape") -- Result: false
TableUtils.TableHasValue = function (tbl, val)
    if not IsTable(tbl) then
        LogMessage("Error in TableUtils.TableHasValue: input is not a table.", logLevelError)
        return false
    end
    return TableHasValueRaw(tbl, val)
end

--- Check if a table contains a specific key.
-- @tparam table tbl The table to check.
-- @tparam any key The key to search for.
-- @treturn[1] bool True if the key is found, false otherwise.
-- @treturn[2] bool false if an error occurs.
-- @usage
-- -- Example with associative table:
-- local tbl = { apple = 1, banana = 2, cherry = 3 }
-- local hasBananaKey = TableUtils.TableHasKey(tbl, "banana") -- Result: true
-- local hasGrapeKey = TableUtils.TableHasKey(tbl, "grape") -- Result: false
--
-- -- Example with array table:
-- local tbl = { "apple", "banana", "cherry" }
-- local hasBananaKey = TableUtils.TableHasKey(tbl, 2) -- Result: true
-- local hasGrapeKey = TableUtils.TableHasKey(tbl, 4) -- Result: false
TableUtils.TableHasKey = function (tbl, key)
    if not IsTable(tbl) then
        LogMessage("Error in TableUtils.TableHasKey: input is not a table.", logLevelError)
        return false
    end
    return tbl[key] ~= nil
end

--- Create a shallow copy of a table.
-- Creates a new table with the same key-value pairs. Nested tables are NOT copied (they remain references).
--
-- **Important:** This is a shallow copy. If the original table contains nested tables, both the original and the copy will reference the same nested tables. Modifying a nested table in the copy will affect the original.
--
-- For a deep copy that also clones nested tables, use `LuaUtils.CloneValue` instead.
-- @tparam table tbl The table to copy.
-- @treturn[1] table A shallow copy of the input table.
-- @treturn[2] table An empty table if input is not a table.
-- @usage
-- -- Example with simple table:
-- local original = { a = 1, b = 2, c = 3 }
-- local copy = TableUtils.CopyTable(original)
-- copy.a = 10
-- -- original.a is still 1, copy.a is 10
--
-- -- Example with nested tables (shallow copy limitation):
-- local original = { name = "Lara", inventory = { sword = 1, shield = 2 } }
-- local copy = TableUtils.CopyTable(original)
-- copy.inventory.sword = 5
-- -- WARNING: original.inventory.sword is now also 5! (nested table is shared)
-- -- For nested tables, use TableUtils.CloneValue instead
--
-- -- Example with array:
-- local original = { "red", "green", "blue" }
-- local copy = TableUtils.CopyTable(original)
-- copy[2] = "yellow"
-- -- original: { "red", "green", "blue" }
-- -- copy: { "red", "yellow", "blue" }
--
-- -- Practical use: backup a table before modification
-- local backup = TableUtils.CopyTable(playerStats)
-- playerStats.health = 0
-- if needRestore then
--     playerStats = backup
-- end
TableUtils.CopyTable = function(tbl)
    if not IsTable(tbl) then
        LogMessage("Error in TableUtils.CopyTable: input is not a table.", logLevelError)
        return {}
    end

    local copy = {}
    for key, value in next, tbl do
        copy[key] = value
    end
    return copy
end

--- Merge two tables into a new table.
-- Creates a new table combining keys from both tables. If a key exists in both, the value from tbl2 takes precedence.
-- This is a shallow merge (nested tables are not merged recursively).
-- @tparam table tbl1 The first table (base table).
-- @tparam table tbl2 The second table (override table).
-- @treturn[1] table A new table with merged contents.
-- @treturn[2] table An empty table if either input is not a table.
-- @usage
-- -- Example with configuration merge:
-- local defaults = { volume = 100, fullscreen = false, difficulty = "normal" }
-- local userSettings = { volume = 80, fullscreen = true }
-- local finalSettings = TableUtils.MergeTables(defaults, userSettings)
-- -- finalSettings: { volume = 80, fullscreen = true, difficulty = "normal" }
--
-- -- Example with player stats:
-- local baseStats = { health = 100, stamina = 100, damage = 10 }
-- local bonuses = { health = 20, damage = 5 }
-- local totalStats = TableUtils.MergeTables(baseStats, bonuses)
-- -- totalStats: { health = 20, stamina = 100, damage = 5 }
-- -- Note: values are replaced, not added! Use custom logic for addition.
--
-- -- Example with arrays (numeric keys):
-- local array1 = { "a", "b", "c" }
-- local array2 = { "x", "y" }
-- local merged = TableUtils.MergeTables(array1, array2)
-- -- merged: { "x", "y", "c" } (indices 1 and 2 are overridden)
--
-- -- Practical use: apply temporary modifications
-- local defaultConfig = { speed = 10, color = "blue" }
-- local nightMode = { color = "black", brightness = 50 }
-- local activeConfig = TableUtils.MergeTables(defaultConfig, nightMode)
-- -- activeConfig: { speed = 10, color = "black", brightness = 50 }
TableUtils.MergeTables = function(tbl1, tbl2)
    if not IsTable(tbl1) then
        LogMessage("Error in TableUtils.MergeTables: tbl1 is not a table.", logLevelError)
        return {}
    end
    if not IsTable(tbl2) then
        LogMessage("Error in TableUtils.MergeTables: tbl2 is not a table.", logLevelError)
        return {}
    end

    local merged = {}

    -- Copy all keys from tbl1
    for key, value in next, tbl1 do
        merged[key] = value
    end

    -- Copy all keys from tbl2 (overriding tbl1 if keys match)
    for key, value in next, tbl2 do
        merged[key] = value
    end

    return merged
end

--- Remove the first occurrence of a value from an array table.
-- This function searches for the first matching value and removes it.
-- The array is compacted after removal (subsequent elements are shifted down).
-- If `value` is `nil`, the function removes the first missing numeric slot (hole)
-- in the range `1..maxNumericIndex` and compacts the array.
--
-- Note: This function is designed **for array tables (numeric indices)**.
-- @tparam table tbl The array table from which to remove the value.
-- @tparam any value The value to search for and remove.
-- @treturn[1] bool True if the value was found and removed, false otherwise.
-- @treturn[2] bool False if the input is not a table.
-- @usage
-- -- Example with array of strings:
-- local fruits = { "apple", "banana", "cherry", "banana" }
-- local removed = TableUtils.RemoveValue(fruits, "banana") -- Result: true
-- -- fruits is now: { "apple", "cherry", "banana" }
--
-- -- Example with array of numbers:
-- local numbers = { 10, 20, 30, 40 }
-- local removed = TableUtils.RemoveValue(numbers, 30) -- Result: true
-- -- numbers is now: { 10, 20, 40 }
--
-- -- Example when value is not found:
-- local colors = { "red", "green", "blue" }
-- local removed = TableUtils.RemoveValue(colors, "yellow") -- Result: false
-- -- colors remains unchanged: { "red", "green", "blue" }
--
-- -- Example with mixed types:
-- local mixed = { 1, "two", 3, "four" }
-- local removed = TableUtils.RemoveValue(mixed, "two") -- Result: true
-- -- mixed is now: { 1, 3, "four" }
--
-- -- Example with sparse array and nil removal:
-- local tableA = { "b", false, nil, 45 }
-- local removed = TableUtils.RemoveValue(tableA, nil) -- Result: true
-- -- tableA is now: { "b", false, 45 }
TableUtils.RemoveValue = function(tbl, value)
    if not IsTable(tbl) then
        LogMessage("Error in TableUtils.RemoveValue: input is not a table.", logLevelError)
        return false
    end

    if value ~= nil then
        local n = #tbl

        -- Fast path: search in 1..#tbl (covers dense arrays entirely)
        for i = 1, n do
            if tbl[i] == value then
                -- Use C-level table.remove when safe (maxIndex == #tbl)
                local maxIndex = GetMaxNumericIndex(tbl)
                if maxIndex == n then
                    tableRemove(tbl, i)
                else
                    for j = i, maxIndex - 1 do
                        tbl[j] = tbl[j + 1]
                    end
                    tbl[maxIndex] = nil
                end
                return true
            end
        end

        -- Sparse fallback: search elements beyond #tbl
        local maxIndex = GetMaxNumericIndex(tbl)
        for i = n + 1, maxIndex do
            if tbl[i] == value then
                for j = i, maxIndex - 1 do
                    tbl[j] = tbl[j + 1]
                end
                tbl[maxIndex] = nil
                return true
            end
        end

        return false
    end

    -- value == nil: remove first nil hole and shift
    local maxIndex = GetMaxNumericIndex(tbl)
    if maxIndex == 0 then
        return false
    end

    for i = 1, maxIndex do
        if tbl[i] == nil then
            for j = i, maxIndex - 1 do
                tbl[j] = tbl[j + 1]
            end
            tbl[maxIndex] = nil
            return true
        end
    end

    return false
end

--- Remove all occurrences of a value from an array table.
-- This function searches for all instances of the value and removes them.
-- The array is compacted after removal.
-- If `value` is `nil`, all missing numeric slots (holes) in the range
-- `1..maxNumericIndex` are removed by compaction.
--
-- Note: This function is designed **for array tables (numeric indices)**.
-- @tparam table tbl The array table from which to remove all occurrences of the value.
-- @tparam any value The value to search for and remove.
-- @treturn[1] int The number of occurrences removed.
-- @treturn[2] int 0 if the input is not a table.
-- @usage
-- -- Example with multiple occurrences:
-- local fruits = { "apple", "banana", "apple", "cherry", "apple" }
-- local count = TableUtils.RemoveAllValues(fruits, "apple") -- Result: 3
-- -- fruits is now: { "banana", "cherry" }
--
-- -- Example with single occurrence:
-- local numbers = { 10, 20, 30, 40 }
-- local count = TableUtils.RemoveAllValues(numbers, 30) -- Result: 1
-- -- numbers is now: { 10, 20, 40 }
--
-- -- Example when value is not found:
-- local colors = { "red", "green", "blue" }
-- local count = TableUtils.RemoveAllValues(colors, "yellow") -- Result: 0
-- -- colors remains unchanged: { "red", "green", "blue" }
--
-- -- Example with all elements being removed:
-- local data = { 5, 5, 5, 5 }
-- local count = TableUtils.RemoveAllValues(data, 5) -- Result: 4
-- -- data is now: {} (empty table)
--
-- -- Practical use: clean up inventory
-- local inventory = { "potion", "sword", "potion", "shield", "potion" }
-- local removed = TableUtils.RemoveAllValues(inventory, "potion")
-- -- Removed 3 potions, inventory: { "sword", "shield" }
--
-- -- Example with sparse array and nil removal:
-- local tableA = { "b", false, nil, 45 }
-- local count = TableUtils.RemoveAllValues(tableA, nil) -- Result: 1
-- -- tableA is now: { "b", false, 45 }
TableUtils.RemoveAllValues = function(tbl, value)
    if not IsTable(tbl) then
        LogMessage("Error in TableUtils.RemoveAllValues: input is not a table.", logLevelError)
        return 0
    end

    local maxIndex = GetMaxNumericIndex(tbl)
    if maxIndex == 0 then
        return 0
    end

    local writeIdx = 1
    local removedCount = 0

    if value ~= nil then
        -- Single-pass write-pointer compaction
        -- Also compacts nil holes as side effect
        for i = 1, maxIndex do
            local v = tbl[i]
            if v == value then
                removedCount = removedCount + 1
            elseif v ~= nil then
                if writeIdx ~= i then
                    tbl[writeIdx] = v
                end
                writeIdx = writeIdx + 1
            end
        end
    else
        -- value == nil: compact all nil holes
        for i = 1, maxIndex do
            if tbl[i] ~= nil then
                if writeIdx ~= i then
                    tbl[writeIdx] = tbl[i]
                end
                writeIdx = writeIdx + 1
            else
                removedCount = removedCount + 1
            end
        end
    end

    for i = writeIdx, maxIndex do
        tbl[i] = nil
    end

    return removedCount
end

--- Remove a key-value pair from an associative table.
-- This function removes the specified key and its associated value from the table.
-- Works with both associative tables and array tables (using numeric indices).
-- @tparam table tbl The table from which to remove the key.
-- @tparam any key The key to remove.
-- @treturn[1] bool True if the key existed and was removed, false if the key was not found.
-- @treturn[2] bool False if the input is not a table.
-- @usage
-- -- Example with associative table:
-- local player = { name = "Lara", health = 100, ammo = 50 }
-- local removed = TableUtils.RemoveKey(player, "ammo") -- Result: true
-- -- player is now: { name = "Lara", health = 100 }
--
-- -- Example when key doesn't exist:
-- local items = { sword = 1, shield = 2 }
-- local removed = TableUtils.RemoveKey(items, "helmet") -- Result: false
-- -- items remains unchanged: { sword = 1, shield = 2 }
--
-- -- Example with array table (using numeric index):
-- local colors = { "red", "green", "blue" }
-- local removed = TableUtils.RemoveKey(colors, 2) -- Result: true
-- -- colors is now: { "red", "blue" } (but indices are: [1]="red", [3]="blue")
-- -- Note: For arrays, prefer using RemoveValue or table.remove for proper compacting
--
-- -- Example with nested tables:
-- local config = { display = { width = 1920, height = 1080 }, sound = { volume = 80 } }
-- local removed = TableUtils.RemoveKey(config, "sound") -- Result: true
-- -- config is now: { display = { width = 1920, height = 1080 } }
TableUtils.RemoveKey = function(tbl, key)
    if not IsTable(tbl) then
        LogMessage("Error in TableUtils.RemoveKey: input is not a table.", logLevelError)
        return false
    end

    if tbl[key] ~= nil then
        tbl[key] = nil
        return true
    end

    return false
end

--- Clear all key-value pairs from a table.
-- This function removes all entries from the table, leaving it empty.
-- Works with both associative tables and array tables.
-- Note: __This function does not create a new table__; it modifies the existing table in place.
-- @tparam table tbl The table to clear.
-- @treturn[1] bool True if the table was successfully cleared.
-- @treturn[2] bool False if the input is not a table.
-- @usage
-- -- Example with associative table:
-- local player = { name = "Lara", health = 100, ammo = 50 }
-- local cleared = TableUtils.ClearTable(player) -- Result: true
-- -- player is now: {}
--
-- -- Example with array table:
-- local colors = { "red", "green", "blue" }
-- local cleared = TableUtils.ClearTable(colors) -- Result: true
-- -- colors is now: {}
--
-- -- Example with nested tables:
-- local config = { display = { width = 1920, height = 1080 }, sound = { volume = 80 } }
-- local cleared = TableUtils.ClearTable(config) -- Result: true
-- -- config is now: {}
--
-- -- Example with non-table input:
-- local notATable = "I am a string"
-- local cleared = TableUtils.ClearTable(notATable) -- Result: false (error logged)
-- -- notATable remains unchanged: "I am a string"
TableUtils.ClearTable = function(tbl)
    if not IsTable(tbl) then
        LogMessage("Error in TableUtils.ClearTable: input is not a table.", logLevelError)
        return false
    end

    for k in next, tbl do
        tbl[k] = nil
    end

    return true
end

--- Create a read-only version of a table.
-- @tparam table tbl The table to make read-only.
-- @treturn[1] table A read-only version of the input table.
-- @treturn[2] table An empty table if the input is not a table.
-- @usage
-- local readOnlyTable = TableUtils.SetTableReadOnly(originalTable)
TableUtils.SetTableReadOnly = function(tbl)
    if not IsTable(tbl) then
        LogMessage("Error in TableUtils.SetTableReadOnly: input is not a table.", logLevelError)
        return {}
    end

    return setmetatable({}, {
        __index = tbl,
        __newindex = function(_, key, _)
            LogMessage("Error, cannot modify '" .. tostring(key) .. "': table is read-only", logLevelError)
        end,
        __pairs = function() return pairs(tbl) end,
        __ipairs = function() return ipairs(tbl) end,
        __len = function() return #tbl end,
    })
end

return TableUtils