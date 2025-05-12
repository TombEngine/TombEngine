file = {"../TombEngine/Scripting/Internal", "../Scripts/Engine"}
new_type("mem", "Members", false)
new_type("files", "Script Files", true)
new_type("entityclass", "Entity classes", true)
new_type("pregameclass", "Pre-game script classes", true)
new_type("miscclass", "Misc classes", true)
new_type("tentable", "1 Modules", true)
new_type("tenclass", "2 Classes", true)
new_type("tenprimitive", "3 Primitive Classes", true)
new_type("enum", "4 Enums", true)
new_type("luautil", "5 Lua utility modules", true)

not_luadoc = true

local version = "1.8.2"
project = "&nbsp;TombEngine"
title = "TombEngine " .. version .. " Lua API"
description = "TombEngine " .. version .. " scripting interface"
full_description = [[Welcome to the TombEngine scripting API.

Note that this is primarily a reference document, not a tutorial, so expect descriptions to be fairly sparse.
At the time of writing, there is a tutorial describing the basics of Lua, as well as a number of example scripts, on <a href="https://www.tombengine.com">the TombEngine website</a>.

####Module Hierarchy
Other than the "special tables" (`GameVars`, `LevelVars` and `LevelFuncs`), every module described herein is held in a master table called TEN.
For convenience, these modules and classes are automatically put in the global table. For example, you can call GetMoveableByName either of these two ways:
	local door = TEN.Objects.GetMoveableByName("door_type4_14")
	local door = GetMoveableByName("door_type4_14")

####Always check logs/TENLog.txt
If you are scripting levels, TombEngine will often kick you back to the title screen, even if `errorMode` (see @{Flow.Settings}) is set to `ErrorMode.WARN` or `ErrorMode.SILENT`.

This might get annoying, but it's on purpose. If your Lua script contains a syntax error (e.g. you're missing `end` at the end of a function), the Lua interpreter will not be able to continue running the script. If it tried to keep running, you'd probably see some pretty strange behaviour, and would possibly get a crash regardless. If this happens, check __logs/TENLog.txt__ and look for an error message with the word "unrecoverable".

Happy building!

\- _squidshire and the TombEngine development team._
]]

convert_opt=true 
style = true
wrap = true
template = true
format = 'markdown'
no_space_before_args = true
keep_menu_order = true

custom_display_name_handler = function(item, default_handler)
  if item.type == 'files' then
    return item.name .. ' scripts'
  end
  local hand = default_handler(item)
  return hand
end