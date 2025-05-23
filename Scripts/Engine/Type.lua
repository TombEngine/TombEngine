-----
--- Type - This molule contains functions that allow to check the data type of a variable. It also contains functions that allow to check if the variable is a TEN primitive class or a LevelFuncs.
--
--
-- To use the functions within the scripts, the module must be called:
--	local Type = require("Engine.Type")
--
--
-- Example usage: check if a variable is of type Vec3()
--	local Type= require("Engine.Type")
--
--	LevelFuncs.SetLaraPos = function (pos)
--      if Type.IsVec3(pos) then
--          Lara:SetPosition(pos)
--      end
--	end
--
--
-- You can use the `not` keyword together with the functions of the Type module.
--
-- Example: checking if variable does not have a null value
--	LevelFuncs.AddProp = function (prop)
--      if not Type.IsNull(prop) then
--          LevelVars.property = prop
--      end
--	end
-- @luautil Type

local color = TEN.Color(0,0,0)
local rotation = TEN.Rotation(0, 0, 0)
local time = TEN.Time()
local vec2 = TEN.Vec2(0,0)
local vec3 = TEN.Vec3(0,0,0)
LevelFuncs.TypeControlLevelFunc = function () end

local Type = {}

--- Check if the variable is a number.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a number, *false* otherwise
-- @usage
-- --example of use
--  local num = 255
--  if Type.IsNumber(num) then
--      num = num + 1
--  end
Type.IsNumber = function (variable)
    return type(variable) == "number"
end

--- Check if the variable is a string.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a string, *false* otherwise
-- @usage
-- --example of use
--  local str = "Hi"
--  if Type.IsString(str) then
--      TEN.Util.PrintLog(str .. "everyone!", Util.LogLevel.INFO)
--  end
Type.IsString = function (variable)
    return type(variable) == "string"
end


--- Check if the variable is a boolean.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a boolean, *false* otherwise
-- @usage
-- --example of use
--  LevelFuncs.test = function (test)
--      if Type.IsBoolean(test) then
--          LevelVars.test = test
--      else
--          TEN.Util.PrintLog("Error!", Util.LogLevel.ERROR)
--      end
--  end
Type.IsBoolean = function (variable)
    return type(variable) == "boolean"
end

--- Check if the variable is a table.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a table, *false* otherwise
-- @usage
-- --example of use
--  LevelFuncs.PairsTable = function (table)
--      if Type.IsTable(table) then
--          for k, v in pairs(table) do
--              TEN.Util.PrintLog(tostring(k) .. " - " .. tostring(v), Util.LogLevel.INFO)
--          end
--      end
--  end
Type.IsTable = function (variable)
    return type(variable) == "table"
end

--- Check if the variable has a null value.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a null, *false* otherwise
-- @usage
-- --example of use
--  LevelFuncs.AddProp = function (prop)
--      if Type.IsNull(prop) then
--          TEN.Util.PrintLog("Error!", Util.LogLevel.ERROR)
--      else
--          LevelVars.property = prop
--      end
--  end
Type.IsNull = function (variable)
    return type(variable) == "nil"
end

--- Check if the variable is a function.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a function, *false* otherwise
-- @usage
-- --example of use
--  LevelFuncs.RunFunc = function (func)
--      if Type.IsFunction(func) then
--          func()
--      end
--  end
Type.IsFunction = function (variable)
    return type(variable) == "function"
end

--- Check if the variable is a @{Color}.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a Color, *false* otherwise
-- @usage
-- --example of use
--  LevelFuncs.SetColor = function(color)
--      if Type.IsColor(color) then
--          string:SetColor(color)
--      end
--  end
Type.IsColor = function (variable)
    return getmetatable(variable) == getmetatable(color)
end

--- Check if the variable is a @{Rotation}.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a Rotation, *false* otherwise
-- @usage
-- --example of use
--  LevelFuncs.SetRotation = function (rot)
--      if Type.IsRotation(rot) then
--          Lara:SetRotation(rot)
--      end
--  end
Type.IsRotation = function (variable)
    return getmetatable(variable) == getmetatable(rotation)
end

--- Check if the variable is a @{Vec2}.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a Vec2, *false* otherwise
-- @usage
-- --example of use
--  LevelFuncs.SetSpritePos = function (pos)
--      if Type.IsVec2(pos) then
--          sprite:SetPosition(pos)
--      end
--  end
Type.IsVec2 = function (variable)
    return getmetatable(variable) == getmetatable(vec2)
end

--- Check if the variable is a @{Vec3}.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a Vec3, *false* otherwise
-- @usage
-- --example of use
--	LevelFuncs.SetLaraPos = function (pos)
--      if Type.IsVec3(pos) then
--          Lara:SetPosition(pos)
--      end
--	end
Type.IsVec3 = function (variable)
    return getmetatable(variable) == getmetatable(vec3)
end

--- Check if the variable is a @{Time} object.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a Time object, *false* otherwise
-- @usage
-- --example of use
--	LevelFuncs.IncreaseTime = function (time)
--      if Type.IsTime(time) then
--          time + 1
--      end
--	end
Type.IsTime = function (variable)
    return getmetatable(variable) == getmetatable(time)
end

--- Check if the variable is a LevelFunc.
-- @tparam variable variable to be check
-- @treturn boolean *true* if the variable is a LevelFunc, *false* otherwise
-- @usage
-- --example of use
--  LevelFuncs.SetCallback = function (func)
--      if Type.IsFunction(func) then
--          TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRELOOP, func)
--      end
--  end
Type.IsLevelFunc = function (variable)
    return getmetatable(variable) == getmetatable(LevelFuncs.TypeControlLevelFunc)
end

return Type