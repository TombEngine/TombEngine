--- Internal file used by the RingInventory module.
-- @module RingInventory.Utilities
-- @local

local Utilities = {}

Utilities.Clamp = function(value, minValue, maxValue)
    return math.max(minValue, math.min(value, maxValue))
end

Utilities.NormalizeAngle = function(angle)
    angle = angle % 360
    if angle < 0 then
        angle = angle + 360
    end

    return angle
end

Utilities.GetShortestAngleDelta = function(current, target)
    local delta = (target - current + 180) % 360 - 180
    if delta < -180 then
        delta = delta + 360
    end

    return delta
end

Utilities.GetRingSliceAngle = function(itemCount)
    if not itemCount or itemCount <= 0 then
        return 0
    end

    return 360 / itemCount
end

Utilities.GetRingItemAngle = function(itemIndex, itemCount, ringAngle)
    ringAngle = ringAngle or 0
    return Utilities.GetRingSliceAngle(itemCount) * (itemIndex - 1) + ringAngle
end

Utilities.GetSelectedRingAngle = function(itemIndex, itemCount)
    return -Utilities.GetRingItemAngle(itemIndex, itemCount)
end

Utilities.ColorCombine = function(color, transparency)
    return TEN.Color(color.r, color.g, color.b, transparency)
end

Utilities.OffsetY = function(position, offsetY)
    return TEN.Vec3(position.x, position.y + offsetY, position.z)
end

Utilities.CopyRotation = function(r)
    return TEN.Rotation(r.x, r.y, r.z)
end

Utilities.NormalizeRotation = function(rotation)
    return TEN.Rotation(
        Utilities.NormalizeAngle(rotation.x),
        Utilities.NormalizeAngle(rotation.y),
        Utilities.NormalizeAngle(rotation.z))
end

Utilities.Contains = function(tbl, value)
    for _, v in ipairs(tbl) do
        if v == value then
            return true
        end
    end

    return false
end

Utilities.GetAspectRatioMultiplier = function()
    local THRESHOLD = 16 / 10
    local BASELINE  = 16 / 9
    local current = TEN.View.GetAspectRatio()

    if current >= THRESHOLD then
        return 1.0
    else
        return current / BASELINE
    end
end

Utilities.GetAlphaLerpFactor = function(alphaSpeed, maxAlpha)
    maxAlpha = maxAlpha or 255
    return Utilities.Clamp(alphaSpeed / maxAlpha, 0, 1)
end

Utilities.CopyTable = function(original)
    local copy = {}
    for k, v in pairs(original) do
        if type(v) == "table" then
            copy[k] = Utilities.CopyTable(v)
        else
            copy[k] = v
        end
    end
    return copy
end

return Utilities