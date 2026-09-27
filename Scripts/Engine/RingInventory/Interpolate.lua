--- Internal file used by the RingInventory module.
-- @module RingInventory.Interpolate
-- @local

local Interpolate = {}

-- Internal motion progress tracker
LevelVars.Engine.InterpolateProgress = LevelVars.Engine.InterpolateProgress or {}

-- Easing functions
Interpolate.Easing = 
{
    Linear = function(t) return t end,
    
    EaseInQuad = function(t) return t * t end,
    EaseOutQuad = function(t) return t * (2 - t) end,
    EaseInOutQuad = function(t)
        return t < 0.5 and 2 * t * t or -1 + (4 - 2 * t) * t
    end,
    
    EaseInCubic = function(t) return t * t * t end,
    EaseOutCubic = function(t)
        local t1 = t - 1
        return t1 * t1 * t1 + 1
    end,
    EaseInOutCubic = function(t)
        return t < 0.5 and 4 * t * t * t or (t - 1) * (2 * t - 2) * (2 * t - 2) + 1
    end,
    
    EaseInElastic = function(t)
        if t == 0 or t == 1 then return t end
        local p = 0.3
        local s = p / 4
        local t1 = t - 1
        return -(2 ^ (10  * t1) * math.sin((t1 - s) * (2 * math.pi) / p))
    end,
    
    EaseOutElastic = function(t)
        if t == 0 or t == 1 then return t end
        local p = 0.3
        local s = p / 4
        return  2 ^ (-10 * t) * math.sin((t - s) * (2 * math.pi) / p) + 1
    end,
    
    Softstep = function(t)
        return t * t * (3 - 2 * t)
    end,

    Smoothstep = function(t)
        return t * t * t * (t * (t * 6 - 15) + 10)
    end
}

-- Pure interpolation function (no state)
function Interpolate.Lerp(start, finish, t, easing)
    easing = easing or Interpolate.Easing.Smoothstep
    local factor = easing(t)
    
    -- Number
    if type(start) == "number" then
        return start + (finish - start) * factor
    end
    
    -- Vec2
    if start.x and start.y and not start.z then
        return Vec2(
            start.x + (finish.x - start.x) * factor,
            start.y + (finish.y - start.y) * factor)
    end
    
    -- Vec3 or Rotation
    if start.x and start.y and start.z then
        
        local ROTATION = TEN.Rotation(0, 0, 0)
        local IsRotation = getmetatable(start) == getmetatable(ROTATION)
        
        if IsRotation then
            return Rotation(
                start.x + (finish.x - start.x) * factor,
                start.y + (finish.y - start.y) * factor,
                start.z + (finish.z - start.z) * factor)
        else
            -- It's a Vec3
            return Vec3(
                start.x + (finish.x - start.x) * factor,
                start.y + (finish.y - start.y) * factor,
                start.z + (finish.z - start.z) * factor)
        end
    end
    
    -- Color
    if start.r and start.g and start.b then
        return Color(
            math.floor(start.r + (finish.r - start.r) * factor),
            math.floor(start.g + (finish.g - start.g) * factor),
            math.floor(start.b + (finish.b - start.b) * factor),
            math.floor(start.a + (finish.a - start.a) * factor))
    end
    
    return finish  -- Fallback
end

-- Perform single interpolation
function Interpolate.Calculate(name, oldValue, newValue, time, easing)
    if LevelVars.Engine.InterpolateProgress[name] == nil then
        LevelVars.Engine.InterpolateProgress[name] = 0
    end

    local interval = 1 / (time * 30)

    LevelVars.Engine.InterpolateProgress[name] = math.min(LevelVars.Engine.InterpolateProgress[name] + interval, 1)

    local factor = LevelVars.Engine.InterpolateProgress[name]

    local outputValue = Interpolate.Lerp(oldValue, newValue, factor, easing)

    return
    {
        output = outputValue,
        progress = LevelVars.Engine.InterpolateProgress[name]
    }
end

-- Clear motion progress by name
function Interpolate.Clear(name)
    if LevelVars.Engine.InterpolateProgress[name] then
        LevelVars.Engine.InterpolateProgress[name] = nil
    end
end

-- Clear progress for all
function Interpolate.ClearAll()
    LevelVars.Engine.InterpolateProgress = {}
end

function Interpolate.GetProgress(motionName)
    return LevelVars.Engine.InterpolateProgress[motionName] or 0
end

function Interpolate.StepAlpha(current, target, alphaSpeed)
    if current < target then
        return math.min(current + alphaSpeed, target)
    elseif current > target then
        return math.max(current - alphaSpeed, target)
    end
    return current
end

return Interpolate