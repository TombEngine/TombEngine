-----<style>table.function_list td.name {min-width: 345px;}</style>
--- Lua support functions for string manipulation
---
--- **Design Philosophy:**
--- StringUtils is designed primarily for:
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
---	local StringUtils = require("Engine.StringUtils")
-- @luautil StringUtils

local StringUtils = {}

local Type= require("Engine.Type")
local Util = require("Engine.Util")

local logLevelError  = TEN.Util.LogLevel.ERROR

local floor = math.floor
local IsNumber = Type.IsNumber
local IsString = Type.IsString
local IsTable = Type.IsTable
local GetMaxNumericIndex = Util.GetMaxNumericIndex
local LogMessage  = TEN.Util.PrintLog

--- Split a string into a table using a specified delimiter.
-- @tparam string inputStr The string to split.
-- @tparam[opt=" " (space)] string delimiter The delimiter to use for splitting.
-- @treturn[1] table A table containing the split substrings.
-- @treturn[2] table {} An empty table if an error occurs.
-- @usage
-- local str = "apple,banana,cherry"
-- local result = StringUtils.SplitString(str, ",")
-- -- Result: {"apple", "banana", "cherry"}
StringUtils.SplitString = function(inputStr, delimiter)
    if not IsString(inputStr) then
        LogMessage("Error in StringUtils.SplitString: inputStr is not a string.", logLevelError)
        return {}
    end

	delimiter = delimiter or " "
    if not IsString(delimiter) then
        LogMessage("Error in StringUtils.SplitString: delimiter is not a string.", logLevelError)
        return {}
    end

    local t = {}
    local start = 1
    local delimLen = #delimiter
    while true do
        local pos = inputStr:find(delimiter, start, true)  -- plain match
        if not pos then
            local last = inputStr:sub(start)
            if #last > 0 then t[#t + 1] = last end
            break
        end
        local segment = inputStr:sub(start, pos - 1)
        if #segment > 0 then t[#t + 1] = segment end
        start = pos + delimLen
    end
    return t
end

--- Format a string by replacing `{key}` placeholders with values from a table.
-- Each `{key}` in the template is replaced with `tostring(vars[key])`.
-- If a key is not found in the table, the placeholder is left unchanged (e.g. `"{key}"`).
-- Variables explicitly set to `nil` in Lua are not stored in tables, so they also remain as `"{key}"`.
-- @tparam string str The template string containing `{key}` placeholders.
-- @tparam[opt={} (empty table)] table vars A table mapping keys to values.
-- @treturn[1] string The formatted string.
-- @treturn[2] string The original string unchanged if an error occurs.
-- @usage
-- -- Basic usage:
-- local msg = StringUtils.Format("Hello {name}!", { name = "Lara" })
-- -- Result: "Hello Lara!"
--
-- -- Multiple variables:
-- local msg = StringUtils.Format("{enemy} HP: {hp}/{maxHp}", { enemy = "Skeleton", hp = 45, maxHp = 100 })
-- -- Result: "Skeleton HP: 45/100"
--
-- -- With TEN types (uses tostring):
-- local pos = TEN.Vec3(100, 200, 300)
-- local msg = StringUtils.Format("Player at {pos}", { pos = pos })
-- -- Result: "Player at {100, 200, 300}" (depends on Vec3's tostring)
--
-- -- Missing variable (placeholder preserved):
-- local msg = StringUtils.Format("Value: {x}", {})
-- -- Result: "Value: {x}"
--
-- -- Booleans and numbers:
-- local msg = StringUtils.Format("Enabled: {flag}, Count: {n}", { flag = true, n = 42 })
-- -- Result: "Enabled: true, Count: 42"
--
-- -- No variables table (all placeholders preserved):
-- local msg = StringUtils.Format("Test {a} and {b}")
-- -- Result: "Test {a} and {b}"
--
-- -- False and 0 are valid values (not stripped):
-- local msg = StringUtils.Format("Active: {flag}, HP: {hp}", { flag = false, hp = 0 })
-- -- Result: "Active: false, HP: 0"
--
-- -- Nil values (placeholders preserved):
-- local msg = StringUtils.Format("Value: {x}", { x = nil })
-- -- Result: "Value: {x}" (because nil values are not stored in tables)
--
-- -- Monitoring nil variables:
-- -- When working with external variables (e.g. LevelVars) that may become nil,
-- -- wrap them with tostring() to display "nil" instead of preserving the placeholder.
-- -- Without tostring: { target = LevelVars.target } → "Target: {target}" (if target is nil)
-- -- With tostring:    { target = tostring(LevelVars.target) } → "Target: nil"
-- LevelVars.target = nil
-- local msg = StringUtils.Format("Target: {target}", { target = tostring(LevelVars.target) })
-- -- Result: "Target: nil"
--
-- LevelVars.target = 5
-- local msg = StringUtils.Format("Target: {target}", { target = tostring(LevelVars.target) })
-- -- Result: "Target: 5"
--
-- -- Practical: debug log
-- local msg = StringUtils.Format("Frame {frame}: {obj} moved to {pos}", {
--     frame = frameCount,
--     obj = "door_01",
--     pos = entity:GetPosition()
-- })
-- TEN.Util.PrintLog(msg, TEN.Util.LogLevel.INFO) -- Result: "Frame 120: door_01 moved to {100, 200, 300}"
--
-- -- Practical: UI display
-- local msg = StringUtils.Format("Timer: {seconds}s | Score: {score}", {
--     seconds = MathUtils.Round(timer, 1), -- round to 1 decimal place for display with MathUtils module
--     score = playerScore
-- })
-- -- Result: "Timer: 12.3s | Score: 1500"
--
-- -- Practical: error messages
-- local msg = StringUtils.Format("Error: {errorMsg}", { errorMsg = "Failed to load resource" })
-- -- Result: "Error: Failed to load resource"
StringUtils.Format = function(str, vars)
    if not IsString(str) then
        LogMessage("Error in StringUtils.Format: str is not a string.", logLevelError)
        return str
    end

    vars = vars or {}
    if not IsTable(vars) then
        LogMessage("Error in StringUtils.Format: vars is not a table.", logLevelError)
        return str
    end

    return (str:gsub("{(.-)}", function(key)
        local val = vars[key]
        if val == nil then return "{" .. key .. "}" end
        return tostring(val)
    end))
end

--- Join array elements into a single string with a separator.
-- Iterates over sequential integer keys (1 to the highest numeric index).
-- Each element is converted to a string via `tostring()`.   Elements with a `nil` value are skipped (not included in the result).
-- Non-array keys (string keys) are ignored — only integer indices are processed.
-- The separator is treated as literal text (special characters like `.`, `%`, `{` are safe).
-- @tparam table tbl The array-like table to join.
-- @tparam[opt=", " (comma space)] string separator The separator inserted between elements.
-- @treturn[1] string The joined string.
-- @treturn[2] string "" An empty string if the table is empty or an error occurs.
-- @usage
-- -- Basic usage:
-- local result = StringUtils.Join({"apple", "banana", "cherry"})
-- -- Result: "apple, banana, cherry"
--
-- -- Custom separator:
-- local result = StringUtils.Join({"apple", "banana", "cherry"}, " | ")
-- -- Result: "apple | banana | cherry"
--
-- -- Empty separator (concatenation):
-- local result = StringUtils.Join({"a", "b", "c"}, "")
-- -- Result: "abc"
--
-- -- Mixed types (auto tostring):
-- local result = StringUtils.Join({42, true, "hello", TEN.Vec3(1, 2, 3)})
-- -- Result: "42, true, hello, {1, 2, 3}" (Vec3 depends on its tostring)
--
-- -- Nil values are skipped:
-- local result = StringUtils.Join({"a", nil, "c"})
-- -- Result: "a, c"
--
-- -- Sparse array:
-- local t = {}
-- t[1] = "first"
-- t[5] = "fifth"
-- local result = StringUtils.Join(t)
-- -- Result: "first, fifth" (indices 2-4 are nil, skipped)
--
-- -- Empty table:
-- local result = StringUtils.Join({})
-- -- Result: ""
--
-- -- Single element (no separator):
-- local result = StringUtils.Join({"only"})
-- -- Result: "only"
--
-- -- Non-array keys are ignored:
-- local result = StringUtils.Join({[1] = "a", [2] = "b", name = "Lara"})
-- -- Result: "a, b" (string key "name" is ignored)
--
-- -- Special characters in separator (safe, no escaping needed):
-- local result = StringUtils.Join({"a", "b", "c"}, "%%")
-- -- Result: "a%%b%%c"
--
-- -- Inverse of SplitString:
-- local parts = StringUtils.SplitString("apple,banana,cherry", ",")
-- local rejoined = StringUtils.Join(parts, ",")
-- -- Result: "apple,banana,cherry"
--
-- -- Practical: build a path
-- local path = StringUtils.Join({"rooms", "level1", "secret_area"}, "/")
-- -- Result: "rooms/level1/secret_area"
--
-- -- Practical: debug log of collected items
-- local items = {"Shotgun", "Medipack", "Key of Anubis"}
-- local msg = StringUtils.Format("Inventory: {items}", { items = StringUtils.Join(items) })
-- -- Result: "Inventory: Shotgun, Medipack, Key of Anubis"
StringUtils.Join = function(tbl, separator)
    if not IsTable(tbl) then
        LogMessage("Error in StringUtils.Join: tbl is not a table.", logLevelError)
        return ""
    end

    separator = separator or ", "
    if not IsString(separator) then
        LogMessage("Error in StringUtils.Join: separator is not a string.", logLevelError)
        return ""
    end

    local maxIndex = GetMaxNumericIndex(tbl)
    if maxIndex == 0 then
        return ""
    end

    local parts = {}
    local count = 0
    for i = 1, maxIndex do
        local val = tbl[i]
        if val ~= nil then
            count = count + 1
            parts[count] = tostring(val)
        end
    end

    return table.concat(parts, separator)
end

--- Remove leading and trailing whitespace (or custom characters) from a string.
-- Uses an efficient `find` + `sub` approach: locates the first and last non-stripped
-- characters by index, then extracts the substring in a single allocation.
-- When custom characters are provided, they are plain characters — no Lua pattern knowledge is needed.
-- Special characters like `.`, `-`, `%`, `^`, `]` are handled automatically.
-- Multiple characters can be combined: `"._-"` removes dots, underscores and dashes.
-- @tparam string str The string to trim.
-- @tparam[opt] string chars One or more literal characters to strip instead of whitespace.
-- If omitted, strips whitespace (spaces, tabs, newlines, carriage returns, form feeds, vertical tabs).
-- @treturn[1] string The trimmed string.
-- @treturn[2] string "" An empty string if the input is all stripped characters or an error occurs.
-- @usage
-- -- Basic whitespace trim:
-- local result = StringUtils.Trim("  hello  ")
-- -- Result: "hello"
--
-- -- Tabs and newlines:
-- local result = StringUtils.Trim("\t  hello world \n")
-- -- Result: "hello world"
--
-- -- Internal spaces preserved:
-- local result = StringUtils.Trim("  hello   world  ")
-- -- Result: "hello   world"
--
-- -- Only whitespace (returns empty string):
-- local result = StringUtils.Trim("   ")
-- -- Result: ""
--
-- -- Empty string:
-- local result = StringUtils.Trim("")
-- -- Result: ""
--
-- -- No whitespace (unchanged):
-- local result = StringUtils.Trim("hello")
-- -- Result: "hello"
--
-- -- Custom characters (remove dots):
-- local result = StringUtils.Trim("...path.to.file...", ".")
-- -- Result: "path.to.file"
--
-- -- Custom characters (remove dashes):
-- local result = StringUtils.Trim("---hello---", "-")
-- -- Result: "hello"
--
-- -- Multiple custom characters (remove underscores and dots):
-- local result = StringUtils.Trim("__.hello.__", "_.")
-- -- Result: "hello"
--
-- -- Custom characters (remove hash marks):
-- local result = StringUtils.Trim("###title###", "#")
-- -- Result: "title"
--
-- -- Empty chars string (returns str unchanged):
-- local result = StringUtils.Trim("  hello  ", "")
-- -- Result: "  hello  "
--
-- -- Practical: clean user input
-- local input = "  Lara Croft  "
-- local name = StringUtils.Trim(input)
-- -- Result: "Lara Croft"
--
-- -- Practical: clean parsed data
-- local parts = StringUtils.SplitString("apple , banana , cherry", ",")
-- for i, v in ipairs(parts) do
--     parts[i] = StringUtils.Trim(v)
-- end
-- -- Result: {"apple", "banana", "cherry"}
StringUtils.Trim = function(str, chars)
    if not IsString(str) then
        LogMessage("Error in StringUtils.Trim: str is not a string.", logLevelError)
        return ""
    end

    if #str == 0 then
        return ""
    end

    -- Default: whitespace trimming (optimized path, no escaping needed)
    if chars == nil then
        local i = str:find("%S")
        if not i then return "" end
        local j = str:find("%S%s*$", i)
        return str:sub(i, j)
    end

    if not IsString(chars) then
        LogMessage("Error in StringUtils.Trim: chars is not a string.", logLevelError)
        return ""
    end

    if #chars == 0 then
        return str  -- Nothing to trim
    end

    -- Escape special Lua pattern characters for safe use inside []
    -- Only these are special inside []: ] ^ - %
    local escaped = chars:gsub("([%]%^%-%%])", "%%%1")

    -- Build patterns using [] for both positions (safe for any character)
    local frontPattern = "[^" .. escaped .. "]"
    local backPattern  = "[^" .. escaped .. "][" .. escaped .. "]*$"

    local i = str:find(frontPattern)
    if not i then
        return ""  -- Entire string is stripped characters
    end

    local j = str:find(backPattern, i)
    return str:sub(i, j)
end

--- Check if a string starts with a given prefix.
-- Uses direct byte comparison via `string.sub` — no pattern engine is involved,
-- so special characters like `.`, `%`, `(`, `[`, `{` are completely safe.
-- The comparison is case-sensitive: `"Hello"` does NOT start with `"hello"`.
-- An empty prefix `""` always returns `true` (every string starts with the empty string).
-- @tparam string str The string to check.
-- @tparam string prefix The prefix to look for.
-- @treturn[1] bool True if str starts with prefix, false otherwise.
-- @treturn[2] bool false If an error occurs (invalid argument types).
-- @usage
-- -- Basic usage:
-- local result = StringUtils.StartsWith("hello world", "hello")
-- -- Result: true
--
-- -- No match:
-- local result = StringUtils.StartsWith("hello world", "world")
-- -- Result: false
--
-- -- Case-sensitive:
-- local result = StringUtils.StartsWith("Hello", "hello")
-- -- Result: false
--
-- -- Case-insensitive workaround:
-- local str = "Hello World"
-- local result = StringUtils.StartsWith(str:lower(), "hello")
-- -- Result: true
--
-- -- Empty prefix (always true):
-- local result = StringUtils.StartsWith("hello", "")
-- -- Result: true
--
-- -- Prefix longer than string:
-- local result = StringUtils.StartsWith("hi", "hello")
-- -- Result: false
--
-- -- Special characters (safe, no escaping needed):
-- local result = StringUtils.StartsWith("[test].lua", "[test]")
-- -- Result: true
--
-- local result = StringUtils.StartsWith("100% done", "100%")
-- -- Result: true
--
-- local result = StringUtils.StartsWith("(value)", "(val")
-- -- Result: true
--
-- -- Single character:
-- local result = StringUtils.StartsWith("/path/to/file", "/")
-- -- Result: true
--
-- -- Practical: check file extension prefix
-- local filename = "level_01.trc"
-- if StringUtils.StartsWith(filename, "level_") then
--     -- Process level file...
-- end
--
-- -- Practical: check command prefix
-- local input = "/teleport 100 200 300"
-- if StringUtils.StartsWith(input, "/") then
--     -- Parse command...
-- end
--
-- -- Practical: filter items by prefix
-- local items = {"key_gold", "key_silver", "medipack", "key_bronze"}
-- local keys = {}
-- for _, item in ipairs(items) do
--     if StringUtils.StartsWith(item, "key_") then
--         keys[#keys + 1] = item
--     end
-- end
-- -- Result: {"key_gold", "key_silver", "key_bronze"}
StringUtils.StartsWith = function(str, prefix)
    if not IsString(str) then
        LogMessage("Error in StringUtils.StartsWith: str is not a string.", logLevelError)
        return false
    end

    if not IsString(prefix) then
        LogMessage("Error in StringUtils.StartsWith: prefix is not a string.", logLevelError)
        return false
    end

    return str:sub(1, #prefix) == prefix
end

--- Check if a string ends with a given suffix.
-- Uses direct byte comparison via `string.sub` with a negative index — no pattern engine
-- is involved, so special characters like `.`, `%`, `(`, `[`, `{` are completely safe.
-- The comparison is case-sensitive: `"Hello"` does NOT end with `"ELLO"`.
-- An empty suffix `""` always returns `true` (every string ends with the empty string).
-- @tparam string str The string to check.
-- @tparam string suffix The suffix to look for.
-- @treturn[1] bool True if str ends with suffix, false otherwise.
-- @treturn[2] bool false If an error occurs (invalid argument types).
-- @usage
-- -- Basic usage:
-- local result = StringUtils.EndsWith("hello world", "world")
-- -- Result: true
--
-- -- No match:
-- local result = StringUtils.EndsWith("hello world", "hello")
-- -- Result: false
--
-- -- Case-sensitive:
-- local result = StringUtils.EndsWith("Hello", "ELLO")
-- -- Result: false
--
-- -- Case-insensitive workaround:
-- local str = "Level.LUA"
-- local result = StringUtils.EndsWith(str:lower(), ".lua")
-- -- Result: true
--
-- -- Empty suffix (always true):
-- local result = StringUtils.EndsWith("hello", "")
-- -- Result: true
--
-- -- Suffix longer than string:
-- local result = StringUtils.EndsWith("hi", "hello")
-- -- Result: false
--
-- -- Special characters (safe, no escaping needed):
-- local result = StringUtils.EndsWith("file[1].lua", ".lua")
-- -- Result: true
--
-- local result = StringUtils.EndsWith("100% done", "% done")
-- -- Result: true
--
-- local result = StringUtils.EndsWith("test(value)", "(value)")
-- -- Result: true
--
-- -- Single character:
-- local result = StringUtils.EndsWith("path/to/file/", "/")
-- -- Result: true
--
-- -- Practical: check file extension
-- local filename = "level_01.lua"
-- if StringUtils.EndsWith(filename, ".lua") then
--     -- Process Lua file...
-- end
--
-- -- Practical: check string terminator
-- local line = "This is a sentence."
-- if StringUtils.EndsWith(line, ".") then
--     -- Sentence ends with a period
-- end
--
-- -- Practical: filter files by extension
-- local files = {"main.lua", "config.txt", "utils.lua", "readme.md"}
-- local luaFiles = {}
-- for _, file in ipairs(files) do
--     if StringUtils.EndsWith(file, ".lua") then
--         luaFiles[#luaFiles + 1] = file
--     end
-- end
-- -- Result: {"main.lua", "utils.lua"}
StringUtils.EndsWith = function(str, suffix)
    if not IsString(str) then
        LogMessage("Error in StringUtils.EndsWith: str is not a string.", logLevelError)
        return false
    end

    if not IsString(suffix) then
        LogMessage("Error in StringUtils.EndsWith: suffix is not a string.", logLevelError)
        return false
    end

    local suffixLen = #suffix
    if suffixLen == 0 then
        return true
    end

    return str:sub(-suffixLen) == suffix
end

--- Check if a string contains a given substring.
-- Uses `string.find` with plain mode enabled — no pattern engine is involved,
-- so special characters like `.`, `%`, `(`, `[`, `{` are completely safe.
-- The comparison is case-sensitive: `"Hello"` does NOT contain `"hello"`.
-- An empty substring `""` always returns `true` (every string contains the empty string).
-- Unlike `StartsWith` and `EndsWith`, this searches **anywhere** in the string —
-- use it when you don't know (or don't care) where the substring appears.
-- @tparam string str The string to search in.
-- @tparam string substring The substring to look for.
-- @treturn[1] bool True if str contains substring, false otherwise.
-- @treturn[2] bool false If an error occurs (invalid argument types).
-- @usage
-- -- Keyword in the middle of a name:
-- local result = StringUtils.Contains("room_secret_01", "secret")
-- -- Result: true
--
-- -- No match:
-- local result = StringUtils.Contains("enemy_fire_boss", "water")
-- -- Result: false
--
-- -- Case-sensitive:
-- local result = StringUtils.Contains("Lara Croft", "lara")
-- -- Result: false
--
-- -- Case-insensitive workaround:
-- local result = StringUtils.Contains(("Lara Croft"):lower(), "lara")
-- -- Result: true
--
-- -- Empty substring (always true):
-- local result = StringUtils.Contains("hello", "")
-- -- Result: true
--
-- -- Substring longer than string:
-- local result = StringUtils.Contains("hi", "hello world")
-- -- Result: false
--
-- -- Special characters (safe, no escaping needed):
-- local result = StringUtils.Contains("damage = hp * (1 + bonus%)", "(1 + bonus%)")
-- -- Result: true
--
-- -- Check if a separator exists to decide how to parse:
-- local entry = "health=100"
-- if StringUtils.Contains(entry, "=") then
--     local parts = StringUtils.SplitString(entry, "=")
--     -- parts: {"health", "100"}
-- end
--
-- -- Practical: detect element type in object names
-- local objName = "enemy_fire_boss"
-- if StringUtils.Contains(objName, "fire") then
--     -- Apply fire resistance logic...
-- elseif StringUtils.Contains(objName, "water") then
--     -- Apply water resistance logic...
-- end
--
-- -- Practical: check if a path goes through a specific folder
-- local path = "levels/egypt/secret/room_01"
-- if StringUtils.Contains(path, "/secret/") then
--     -- Secret area detected
-- end
--
-- -- Practical: filter objects by attribute in their name
-- local objects = {"trap_spike_01", "door_main", "trap_fire_02", "pickup_key", "trap_spike_02"}
-- local traps = {}
-- for _, obj in ipairs(objects) do
--     if StringUtils.Contains(obj, "spike") then
--         traps[#traps + 1] = obj
--     end
-- end
-- -- Result: {"trap_spike_01", "trap_spike_02"}
--
-- -- Practical: search within a multi-word description
-- local description = "An ancient golden key found in the Temple of Horus"
-- if StringUtils.Contains(description, "golden key") then
--     -- Highlight item as important
-- end
StringUtils.Contains = function(str, substring)
    if not IsString(str) then
        LogMessage("Error in StringUtils.Contains: str is not a string.", logLevelError)
        return false
    end

    if not IsString(substring) then
        LogMessage("Error in StringUtils.Contains: substring is not a string.", logLevelError)
        return false
    end

    return str:find(substring, 1, true) ~= nil
end

--- Replace occurrences of a substring with a replacement string.
-- Uses `string.find` with plain mode — no pattern engine is involved,
-- so special characters like `.`, `%`, `(`, `[`, `{` are completely safe
-- in both the search and replacement strings.
-- The search is case-sensitive: `"Hello"` will NOT match `"hello"`.
-- By default, all occurrences are replaced. Use the optional count parameter
-- to limit the number of replacements (e.g. `1` to replace only the first match).
-- @tparam string str The original string.
-- @tparam string search The substring to find. Must not be empty.
-- @tparam string replacement The string to substitute in place of each match.
-- @tparam[opt] number count Maximum number of replacements. If omitted, replaces all occurrences.
-- Must be a positive integer (>= 1).
-- @treturn[1] string The string with replacements applied.
-- @treturn[2] string The original string unchanged if search is not found or an error occurs.
-- @usage
-- -- Replace all occurrences (default):
-- local result = StringUtils.Replace("room_old_old_01", "old", "new")
-- -- Result: "room_new_new_01"
--
-- -- Replace only the first occurrence:
-- local result = StringUtils.Replace("room_old_old_01", "old", "new", 1)
-- -- Result: "room_new_old_01"
--
-- -- Replace first 2 occurrences:
-- local result = StringUtils.Replace("a-b-c-d", "-", "/", 2)
-- -- Result: "a/b/c-d"
--
-- -- No match (string unchanged):
-- local result = StringUtils.Replace("hello world", "planet", "moon")
-- -- Result: "hello world"
--
-- -- Remove a substring (empty replacement):
-- local result = StringUtils.Replace("door_01_locked", "_locked", "")
-- -- Result: "door_01"
--
-- -- Special characters in search (safe, no escaping needed):
-- local result = StringUtils.Replace("HP: 100% (full)", "100%", "50%")
-- -- Result: "HP: 50% (full)"
--
-- local result = StringUtils.Replace("items[0] = sword", "[0]", "[1]")
-- -- Result: "items[1] = sword"
--
-- -- Special characters in replacement (safe):
-- local result = StringUtils.Replace("value = x", "x", "(a + b)")
-- -- Result: "value = (a + b)"
--
-- -- Practical: rename an object ID
-- local objectId = "trap_spike_room03"
-- local newId = StringUtils.Replace(objectId, "room03", "room07")
-- -- Result: "trap_spike_room07"
--
-- -- Practical: sanitize display text (remove unwanted characters)
-- local rawText = "***DANGER***"
-- local clean = StringUtils.Replace(rawText, "*", "")
-- -- Result: "DANGER"
--
-- -- Practical: convert separator format
-- local csv = "apple;banana;cherry"
-- local tsv = StringUtils.Replace(csv, ";", "\t")
-- -- Result: "apple\tbanana\tcherry"
--
-- -- Practical: fix path separators
-- local winPath = "scripts\\levels\\egypt\\main.lua"
-- local unixPath = StringUtils.Replace(winPath, "\\", "/")
-- -- Result: "scripts/levels/egypt/main.lua"
--
-- -- Practical: build display text from template stored in a variable
-- local template = "## HP ##/## MAX_HP ##"
-- local step1 = StringUtils.Replace(template, "## HP ##", tostring(75))
-- local step2 = StringUtils.Replace(step1, "## MAX_HP ##", tostring(100))
-- -- Result: "75/100"
--
-- -- Practical: censor a word (replace first only)
-- local msg = "The secret door hides a secret passage"
-- local censored = StringUtils.Replace(msg, "secret", "hidden", 1)
-- -- Result: "The hidden door hides a secret passage"
StringUtils.Replace = function(str, search, replacement, count)
    if not IsString(str) then
        LogMessage("Error in StringUtils.Replace: str is not a string.", logLevelError)
        return str
    end

    if not IsString(search) then
        LogMessage("Error in StringUtils.Replace: search is not a string.", logLevelError)
        return str
    end

    if not IsString(replacement) then
        LogMessage("Error in StringUtils.Replace: replacement is not a string.", logLevelError)
        return str
    end

    local searchLen = #search
    if searchLen == 0 then
        LogMessage("Error in StringUtils.Replace: search string is empty.", logLevelError)
        return str
    end

    if count ~= nil then
        if not IsNumber(count) or count < 1 or floor(count) ~= count then
            LogMessage("Error in StringUtils.Replace: count must be a positive integer.", logLevelError)
            return str
        end
    end

    local parts = {}
    local partsCount = 0
    local start = 1
    local replaced = 0

    while true do
        local i, j = str:find(search, start, true)
        if not i then
            partsCount = partsCount + 1
            parts[partsCount] = str:sub(start)
            break
        end

        partsCount = partsCount + 1
        parts[partsCount] = str:sub(start, i - 1)
        partsCount = partsCount + 1
        parts[partsCount] = replacement
        start = j + 1
        replaced = replaced + 1

        if count and replaced >= count then
            partsCount = partsCount + 1
            parts[partsCount] = str:sub(start)
            break
        end
    end

    return table.concat(parts)
end

return StringUtils