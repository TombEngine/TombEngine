-----<style>table.function_list td.name {min-width: 620px;}</style>
--- Lua support functions for 3D point and vector transformations.
---
--- **Design Philosophy:**
--- Transform3DUtils is designed primarily for:
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
---	local Transform3DUtils = require("Engine.Utils.Transform3DUtils")
-- @luautil Transform3DUtils

local Transform3DUtils = {}
local Type = require("Engine.Type")
local Utility = require("Engine.Util")

local IsNumber = Type.IsNumber
local IsVec3 = Type.IsVec3
local IsRotation = Type.IsRotation
local IsBoolean = Type.IsBoolean
local IsString = Type.IsString
local IsTable = Type.IsTable
local ErrorLog = Utility.ErrorLog
local RotationInverseRaw = Utility.RotationInverseRaw
local abs = math.abs
local sin = math.sin
local cos = math.cos
local rad = math.rad
local Vec3 = TEN.Vec3
local Rotation = TEN.Rotation

-- Support function for orbit position calculation (no type checking, used internally)
-- axis: lowercase string ("x", "y", "z") or Vec3; angle in degrees
-- Returns center + offset on the orbital plane
local function orbitPositionRaw(center, radius, angle, axis)
    local angleRad = rad(angle)
    local c = cos(angleRad)
    local s = sin(angleRad)

    if axis == "y" then
        return center + Vec3(c * radius, 0, s * radius)
    elseif axis == "x" then
        return center + Vec3(0, c * radius, s * radius)
    elseif axis == "z" then
        return center + Vec3(c * radius, s * radius, 0)
    end

    -- Vec3 custom axis
    local axisN = axis:Normalize()

    -- Pick an arbitrary vector guaranteed to be non-parallel to axisN.
    -- If axisN is close to the Y axis (|y| > 0.99), use (1, 0, 0) as the
    -- arbitrary vector; otherwise (0, 1, 0). The threshold 0.99 avoids
    -- numerical instability from cross products of near-parallel vectors.
    local arbitrary = (abs(axisN.y) > 0.99) and Vec3(1, 0, 0) or Vec3(0, 1, 0)
    local perp1 = axisN:Cross(arbitrary):Normalize()
    local perp2 = axisN:Cross(perp1):Normalize()
    return center + (perp1 * c + perp2 * s) * radius
end

-- Support function for local-to-world transform (no type checking, used internally)
-- localRotation can be nil (position-only) or a Rotation (position + rotation)
-- Returns worldPos, worldRot (worldRot is nil when localRotation is nil)
local function transformLocalToWorldRaw(parentPos, parentRot, localOffset, localRotation)
    local worldPos = parentPos + localOffset:Rotate(parentRot)
    if localRotation then
        return worldPos, Rotation(
            parentRot.x + localRotation.x,
            parentRot.y + localRotation.y,
            parentRot.z + localRotation.z
        )
    end
    return worldPos, nil
end

--- Apply the inverse of a Rotation to a Vec3.
-- Returns the Vec3 as if it had been rotated by the inverse of `rot`.
--
-- **When to use:** converting a world-space vector (position, velocity,
-- direction) into the local space of a rotated object, or "undoing" a
-- rotation. For multi-axis rotations, this is the mathematically correct
-- inverse (the transpose of the rotation matrix). Negating each component
-- of `rot` would only be correct for rotations around a single axis.
--
-- **Common use cases:** converting a world-space velocity into the local
-- frame of a moving object, finding a target's offset in a parent's
-- coordinate space, undoing a rotation that was applied to a vector.
-- @tparam Rotation rot The rotation whose inverse should be applied.
-- @tparam Vec3 vec The vector to apply the inverse rotation to.
-- @tparam[opt="Transform3DUtils.RotationInverse"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Vec3 The vector with the inverse rotation applied.
-- @treturn[2] Vec3 The original `vec` if an error occurs.
-- @usage
-- -- Convert a world-space velocity into the parent's local space:
-- local worldVelocity = TEN.Vec3(0, 0, 100)  -- forward in world
-- local parentRot = TEN.Rotation(0, 45, 0)   -- parent facing 45° yaw
-- local localVelocity = Transform3DUtils.RotationInverse(parentRot, worldVelocity)
Transform3DUtils.RotationInverse = function(rot, vec, errorContext)
    errorContext = errorContext or "Transform3DUtils.RotationInverse"
    if not IsRotation(rot) then
        ErrorLog("Error in {context}: rot must be a Rotation.", {context = errorContext})
        return vec
    end
    if not IsVec3(vec) then
        ErrorLog("Error in {context}: vec must be a Vec3.", {context = errorContext})
        return vec
    end
    return RotationInverseRaw(rot, vec)
end

--- Rotate a point around an arbitrary axis passing through a pivot point.
-- Supports rotation around standard axes (X, Y, Z) or custom axis vectors. Examples use `SecondsToFrames` for time-based animations.
-- Uses TEN's Vec3:Rotate() method for efficient calculation.
-- @tparam Vec3 point The point to rotate.
-- @tparam Vec3 pivot The pivot point (center of rotation).
-- @tparam string|Vec3 axis The rotation axis. Can be "x", "y", "z" (case-insensitive) or a custom Vec3 direction.
-- @tparam float angle The rotation angle in degrees.
-- @tparam[opt="Transform3DUtils.RotatePointAroundAxis"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Vec3 The rotated point.
-- @treturn[2] nil If an error occurs.
-- @usage
-- -- Example: Rotate point around Y axis (pivot at origin)
-- local point = TEN.Vec3(100, 0, 0)
-- local pivot = TEN.Vec3(0, 0, 0)
-- local rotated = Transform3DUtils.RotatePointAroundAxis(point, pivot, "y", 90)
-- -- Result: Vec3(0, 0, -100) (rotated 90° counterclockwise around Y)
--
-- -- Example: Rotate around pivot (not origin)
-- local point = TEN.Vec3(150, 50, 100)
-- local pivot = TEN.Vec3(100, 50, 100)  -- Pivot at x=100
-- local rotated = Transform3DUtils.RotatePointAroundAxis(point, pivot, "y", 180)
-- -- Result: Vec3(50, 50, 100) (point mirrored around pivot)
--
-- -- Example: Rotate around custom axis (diagonal) - Complete working example
-- local satellite = TEN.Objects.GetMoveableByName("Satellite1")
-- local planet = TEN.Objects.GetMoveableByName("Planet")
-- local startPos = satellite:GetPosition()  -- Store initial position (read ONCE outside loop)
-- local pivot = planet:GetPosition()
-- local customAxis = TEN.Vec3(1, 0, 1)  -- Diagonal axis XZ (automatically normalized)
-- local angle = 0
-- local rotationSpeed = 360 / ConversionUtils.SecondsToFrames(10)  -- Complete rotation in 10 seconds
-- LevelFuncs.RotateSatelliteDiagonal = function()
--     angle = (angle + rotationSpeed) % 360
--     -- IMPORTANT: Rotate the INITIAL position (startPos), not current position!
--     local newPos = Transform3DUtils.RotatePointAroundAxis(startPos, pivot, customAxis, angle)
--     satellite:SetPosition(newPos)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.RotateSatelliteDiagonal)
--
-- -- Example: Orbital animation around Y axis - Complete working example
-- local satellite = TEN.Objects.GetMoveableByName("Satellite1")
-- local planet = TEN.Objects.GetMoveableByName("Planet")
-- local startPos = satellite:GetPosition()  -- Initial position (FIXED reference)
-- local pivot = planet:GetPosition()
-- local angle = 0
-- local rotationSpeed = 360 / ConversionUtils.SecondsToFrames(8)  -- 8 seconds per orbit
-- LevelFuncs.OrbitSatelliteY = function()
--     angle = (angle + rotationSpeed) % 360
--     local newPos = Transform3DUtils.RotatePointAroundAxis(startPos, pivot, "y", angle)
--     satellite:SetPosition(newPos)
--     
--     -- Optional: make satellite face the planet
--     local lookDir = (pivot - newPos):Normalize()
--     satellite:SetRotation(TEN.Rotation(lookDir))
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.OrbitSatelliteY)
--
-- -- Example: Swing/pendulum animation - Complete working example
-- local pendulum = TEN.Objects.GetMoveableByName("Pendulum")
-- local anchor = TEN.Objects.GetMoveableByName("Anchor"):GetPosition()  -- Pivot point (fixed)
-- local restPos = pendulum:GetPosition()  -- Rest position below anchor
-- local swingAngle = 0
-- local swingSpeed = 360 / ConversionUtils.SecondsToFrames(2)  -- 2 second period
-- LevelFuncs.SwingPendulum = function()
--     swingAngle = swingAngle + swingSpeed
--     -- Sine wave creates back-and-forth motion: -45° to +45°
--     local currentAngle = 45 * math.sin(math.rad(swingAngle))
--     local swingingPos = Transform3DUtils.RotatePointAroundAxis(restPos, anchor, "z", currentAngle)
--     pendulum:SetPosition(swingingPos)
--     
--     -- Rotate the pendulum object to match swing angle (realistic pendulum motion)
--     pendulum:SetRotation(TEN.Rotation(0, 0, currentAngle))
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.SwingPendulum)
--
-- -- Example: Look at pivot while rotating
-- local rotatedPos = Transform3DUtils.RotatePointAroundAxis(pos, pivot, "y", angle)
-- obj:SetPosition(rotatedPos)
-- local lookDir = (pivot - rotatedPos):Normalize()
-- obj:SetRotation(TEN.Rotation(lookDir))
--
-- -- Error handling example:
-- local rotated = Transform3DUtils.RotatePointAroundAxis(point, pivot, axis, angle)
-- if rotated == nil then
--     TEN.Util.PrintLog("Failed to rotate point", TEN.Util.LogLevel.ERROR)
--     return
-- end
-- obj:SetPosition(rotated)
--
-- -- Safe approach with default fallback:
-- local rotated = Transform3DUtils.RotatePointAroundAxis(point, pivot, "y", angle) or point
Transform3DUtils.RotatePointAroundAxis = function(point, pivot, axis, angle, errorContext)
    errorContext = errorContext or "Transform3DUtils.RotatePointAroundAxis"
    -- Type validation
    if not IsVec3(point) then
        ErrorLog("Error in {context}: point must be a Vec3.", {context = errorContext})
        return nil
    end
    if not IsVec3(pivot) then
        ErrorLog("Error in {context}: pivot must be a Vec3.", {context = errorContext})
        return nil
    end
    if not IsNumber(angle) then
        ErrorLog("Error in {context}: angle must be a number.", {context = errorContext})
        return nil
    end

    -- Translate point to pivot's local space
    local localPoint = point - pivot

    -- Rotate based on axis type
    local rotatedLocal
    if IsString(axis) then
        local axisLower = axis:lower()
        local rotation
        if axisLower == "x" then
            rotation = Rotation(angle, 0, 0)
        elseif axisLower == "y" then
            rotation = Rotation(0, angle, 0)
        elseif axisLower == "z" then
            rotation = Rotation(0, 0, angle)
        else
            ErrorLog("Error in {context}: axis string must be 'x', 'y', or 'z'.", {context = errorContext})
            return nil
        end
        rotatedLocal = localPoint:Rotate(rotation)
    elseif IsVec3(axis) then
        if axis:Length() < 0.001 then
            ErrorLog("Error in {context}: axis Vec3 cannot be zero.", {context = errorContext})
            return nil
        end
        -- Custom axis: use Rodrigues' rotation formula
        -- v_rot = v*cos(θ) + (k × v)*sin(θ) + k*(k·v)*(1-cos(θ))
        local k = axis:Normalize()
        local angleRad = rad(angle)
        local cosTheta = cos(angleRad)
        local sinTheta = sin(angleRad)

        local kCrossV = k:Cross(localPoint)
        local kDotV = k:Dot(localPoint)

        rotatedLocal = localPoint * cosTheta + kCrossV * sinTheta + k * (kDotV * (1 - cosTheta))
    else
        ErrorLog("Error in {context}: axis must be a string ('x', 'y', 'z') or Vec3.", {context = errorContext})
        return nil
    end

    -- Translate back to world space
    local result = rotatedLocal + pivot

    return result
end

--- Calculate a position on a circular orbit around a center point.
-- Generates positions parametrically using radius and angle, ideal for orbital animations. Examples use `SecondsToFrames` for time-based animations.
-- @tparam Vec3 center The center of the orbit.
-- @tparam float radius The radius of the orbit.
-- @tparam float angle The parametric angle in degrees (0-360).
-- @tparam[opt="y"] string|Vec3 axis The orbital plane axis. Can be "x", "y", "z" (case-insensitive) or custom Vec3.
-- @tparam[opt="Transform3DUtils.OrbitPosition"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Vec3 The calculated orbital position.
-- @treturn[2] nil If an error occurs.
-- @usage
-- -- Example: Simple circular orbit on XZ plane - Complete working example
-- local satellite = TEN.Objects.GetMoveableByName("Satellite1")
-- local planet = TEN.Objects.GetMoveableByName("Planet")
-- local center = planet:GetPosition()
-- local radius = 2048
-- local angle = 0
-- local rotationSpeed = 360 / ConversionUtils.SecondsToFrames(10)  -- 10 seconds per orbit
-- LevelFuncs.OrbitSatelliteXZ = function()
--     angle = (angle + rotationSpeed) % 360
--     local orbitPos = Transform3DUtils.OrbitPosition(center, radius, angle, "y")
--     satellite:SetPosition(orbitPos)
--     
--     -- Optional: make satellite face the planet
--     local lookDir = (center - orbitPos):Normalize()
--     satellite:SetRotation(TEN.Rotation(lookDir))
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.OrbitSatelliteXZ)
--
-- -- Example: Orbit on XY plane (vertical orbit) - Complete working example
-- local satellite = TEN.Objects.GetMoveableByName("Satellite1")
-- local center = TEN.Objects.GetMoveableByName("Planet"):GetPosition()
-- local radius = 1536
-- local angle = 0
-- local rotationSpeed = 360 / ConversionUtils.SecondsToFrames(6)  -- 6 seconds per orbit
-- LevelFuncs.OrbitSatelliteXY = function()
--     angle = (angle + rotationSpeed) % 360
--     local orbitPos = Transform3DUtils.OrbitPosition(center, radius, angle, "z")  -- Z axis = XY plane
--     satellite:SetPosition(orbitPos)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.OrbitSatelliteXY)
--
-- -- Example: Multiple satellites with phase offset - Complete working example
-- local sat1 = TEN.Objects.GetMoveableByName("Satellite1")
-- local sat2 = TEN.Objects.GetMoveableByName("Satellite2")
-- local sat3 = TEN.Objects.GetMoveableByName("Satellite3")
-- local planet = TEN.Objects.GetMoveableByName("Planet")
-- local center = planet:GetPosition()
-- local radius = 2048
-- local satellites = { sat1, sat2, sat3 }
-- local phaseOffset = 360 / #satellites  -- 120° spacing (360/3)
-- local baseAngle = 0
-- local rotationSpeed = 360 / ConversionUtils.SecondsToFrames(12)  -- 12 seconds per orbit
-- LevelFuncs.OrbitMultipleSatellites = function()
--     baseAngle = (baseAngle + rotationSpeed) % 360
--     for i, sat in ipairs(satellites) do
--         local angle = (baseAngle + (i - 1) * phaseOffset) % 360
--         local orbitPos = Transform3DUtils.OrbitPosition(center, radius, angle, "y")
--         sat:SetPosition(orbitPos)
--     end
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.OrbitMultipleSatellites)
--
-- -- Example: Custom diagonal orbital plane - Complete working example
-- local satellite = TEN.Objects.GetMoveableByName("Satellite1")
-- local center = TEN.Objects.GetMoveableByName("Planet"):GetPosition()
-- local radius = 1024
-- local customAxis = TEN.Vec3(1, 1, 0)  -- Diagonal XY axis (automatically normalized)
-- local angle = 0
-- local rotationSpeed = 360 / ConversionUtils.SecondsToFrames(8)  -- 8 seconds per orbit
-- LevelFuncs.OrbitDiagonalPlane = function()
--     angle = (angle + rotationSpeed) % 360
--     local orbitPos = Transform3DUtils.OrbitPosition(center, radius, angle, customAxis)
--     satellite:SetPosition(orbitPos)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.OrbitDiagonalPlane)
--
-- -- Error handling example:
-- local orbitPos = Transform3DUtils.OrbitPosition(center, radius, angle, "y")
-- if orbitPos == nil then
--     TEN.Util.PrintLog("Failed to calculate orbit position", TEN.Util.LogLevel.ERROR)
--     return
-- end
-- obj:SetPosition(orbitPos)
--
-- -- Safe approach with default fallback:
-- local orbitPos = Transform3DUtils.OrbitPosition(center, radius, angle, "y") or center
Transform3DUtils.OrbitPosition = function(center, radius, angle, axis, errorContext)
    errorContext = errorContext or "Transform3DUtils.OrbitPosition"
    if not IsVec3(center) then
        ErrorLog("Error in {context}: center must be a Vec3.", {context = errorContext})
        return nil
    end
    if not IsNumber(radius) then
        ErrorLog("Error in {context}: radius must be a number.", {context = errorContext})
        return nil
    end
    if not IsNumber(angle) then
        ErrorLog("Error in {context}: angle must be a number.", {context = errorContext})
        return nil
    end

    axis = axis or "y"

    if IsString(axis) then
        axis = axis:lower()
        if axis ~= "x" and axis ~= "y" and axis ~= "z" then
            ErrorLog("Error in {context}: axis string must be 'x', 'y', or 'z'.", {context = errorContext})
            return nil
        end
    elseif not IsVec3(axis) then
        ErrorLog("Error in {context}: axis must be a string ('x', 'y', 'z') or Vec3.", {context = errorContext})
        return nil
    end

    return orbitPositionRaw(center, radius, angle, axis)
end

--- Arrange multiple objects in a circular formation around a center point.
-- Places objects evenly spaced on a circle, with optional rotation control.
-- Uses OrbitPosition internally for efficient calculation.
-- @tparam Vec3|Objects.Moveable|Objects.Static center Center of the circle (Vec3, Moveable, or Static).
-- @tparam table objects Array of Moveable or Static objects to arrange.
-- @tparam float radius Distance from center to each object.
-- @tparam[opt] table options Optional configuration: {axis = "y", startAngle = 0, faceDirection = nil}
--   - axis (string|Vec3): Orbital plane axis ("x"/"y"/"z" or custom Vec3, default "y")
--   - startAngle (number): Starting angle in degrees (default 0)
--   - faceDirection (string): "center" = face inward, "outward" = face outward, nil = no rotation
-- @tparam[opt="Transform3DUtils.ArrangeInCircle"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] bool True if successful.
-- @treturn[2] bool False if an error occurs.
-- @usage
-- -- Example: Torches around altar (simple XZ circle) - Complete working example
-- local altar = TEN.Objects.GetMoveableByName("Altar")
-- local torches = {
--     TEN.Objects.GetMoveableByName("Torch1"),
--     TEN.Objects.GetMoveableByName("Torch2"),
--     TEN.Objects.GetMoveableByName("Torch3"),
--     TEN.Objects.GetMoveableByName("Torch4")
-- }
-- Transform3DUtils.ArrangeInCircle(altar, torches, 1024)
-- -- Result: 4 torches evenly spaced (90° apart) at radius 1024 on XZ plane
--
-- -- Example: Pickups around player with rotation facing center
-- local player = TEN.Objects.GetLaraObject()
-- local pickups = {
--     TEN.Objects.GetMoveableByName("Pickup1"),
--     TEN.Objects.GetMoveableByName("Pickup2"),
--     TEN.Objects.GetMoveableByName("Pickup3")
-- }
-- Transform3DUtils.ArrangeInCircle(player, pickups, 512, {faceDirection = "center"})
-- -- Result: 3 pickups at 120° spacing, all rotated to face player
--
-- -- Example: Enemies spawn formation facing outward
-- local spawnPoint = TEN.Vec3(5000, 1000, 5000)
-- local enemies = {
--     TEN.Objects.GetMoveableByName("Enemy1"),
--     TEN.Objects.GetMoveableByName("Enemy2"),
--     TEN.Objects.GetMoveableByName("Enemy3"),
--     TEN.Objects.GetMoveableByName("Enemy4"),
--     TEN.Objects.GetMoveableByName("Enemy5")
-- }
-- Transform3DUtils.ArrangeInCircle(spawnPoint, enemies, 2048, {faceDirection = "outward"})
-- -- Result: 5 enemies at 72° spacing, facing outward (defensive circle)
--
-- -- Example: Vertical circle (XY plane) with custom start angle
-- local center = TEN.Vec3(10000, 2000, 8000)
-- local platforms = {
--     TEN.Objects.GetStaticByName("Platform1"),
--     TEN.Objects.GetStaticByName("Platform2"),
--     TEN.Objects.GetStaticByName("Platform3")
-- }
-- Transform3DUtils.ArrangeInCircle(center, platforms, 1536, {axis = "z", startAngle = 90})
-- -- Result: Vertical circle starting at 90° (top position)
--
-- -- Example: Diagonal orbital plane (custom axis)
-- local center = TEN.Objects.GetMoveableByName("Hub")
-- local satellites = {}
-- for i = 1, 6 do
--     satellites[i] = TEN.Objects.GetMoveableByName("Satellite" .. i)
-- end
-- local customAxis = TEN.Vec3(1, 1, 0)  -- Diagonal XY
-- Transform3DUtils.ArrangeInCircle(center, satellites, 2048, {
--     axis = customAxis,
--     startAngle = 45,
--     faceDirection = "center"
-- })
-- -- Result: 6 objects on tilted orbital plane, facing center
--
-- -- Example: Error handling
-- local success = Transform3DUtils.ArrangeInCircle(center, objects, radius, options)
-- if not success then
--     TEN.Util.PrintLog("Failed to arrange objects in circle", TEN.Util.LogLevel.ERROR)
--     return
-- end
--
-- -- Example: Dynamic arrangement in loop (moving center)
-- local hub = TEN.Objects.GetMoveableByName("Hub")
-- local satellites = { ... }
-- LevelFuncs.ArrangeCircleDynamic = function()
--     -- Rearrange every frame as hub moves
--     Transform3DUtils.ArrangeInCircle(hub, satellites, 1024, {faceDirection = "center"})
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.ArrangeCircleDynamic)
Transform3DUtils.ArrangeInCircle = function(center, objects, radius, options, errorContext)
    errorContext = errorContext or "Transform3DUtils.ArrangeInCircle"
    -- Parse center (Vec3, Moveable, or Static)
    local centerPos
    if IsVec3(center) then
        centerPos = center
    elseif center and center.GetPosition then
        centerPos = center:GetPosition()
    else
        ErrorLog("Error in {context}: center must be a Vec3, Moveable, or Static object.", {context = errorContext})
        return false
    end

    -- Validate objects array
    if not IsTable(objects) then
        ErrorLog("Error in {context}: objects must be a table.", {context = errorContext})
        return false
    end
    if #objects == 0 then
        ErrorLog("Error in {context}: objects table is empty.", {context = errorContext})
        return false
    end

    -- Validate radius
    if not IsNumber(radius) then
        ErrorLog("Error in {context}: radius must be a number.", {context = errorContext})
        return false
    end
    if radius <= 0 then
        ErrorLog("Error in {context}: radius must be positive.", {context = errorContext})
        return false
    end

    -- Parse options with defaults
    if not IsTable(options) then
        options = {}
    end
    local axis = options.axis or "y"
    local startAngle = options.startAngle or 0
    local faceDirection = options.faceDirection

    -- Validate axis (normalize string to lowercase once, before the loop)
    if IsString(axis) then
        axis = axis:lower()
        if axis ~= "x" and axis ~= "y" and axis ~= "z" then
            ErrorLog("Error in {context}: axis string must be 'x', 'y', or 'z'.", {context = errorContext})
            return false
        end
    elseif IsVec3(axis) then
        if axis:Length() < 0.001 then
            ErrorLog("Error in {context}: axis Vec3 cannot be zero.", {context = errorContext})
            return false
        end
    else
        ErrorLog("Error in {context}: axis must be a string ('x', 'y', 'z') or Vec3.", {context = errorContext})
        return false
    end

    -- Validate startAngle
    if not IsNumber(startAngle) then
        ErrorLog("Error in {context}: startAngle must be a number.", {context = errorContext})
        return false
    end

    -- Validate faceDirection
    if faceDirection ~= nil and faceDirection ~= "center" and faceDirection ~= "outward" then
        ErrorLog("Error in {context}: faceDirection must be nil, 'center', or 'outward'.", {context = errorContext})
        return false
    end

    -- Calculate angle step for even spacing
    local angleStep = 360 / #objects

    -- Arrange each object
    for i, obj in ipairs(objects) do
        -- Calculate position angle
        local angle = startAngle + (i - 1) * angleStep

        -- Calculate position using raw function (all inputs already validated)
        local position = orbitPositionRaw(centerPos, radius, angle, axis)

        -- Set position
        if not obj or not obj.SetPosition then
            ErrorLog("Error in {context}: object {objectIndex} is invalid or missing SetPosition method.", {context = errorContext, objectIndex = i})
            return false
        end
        obj:SetPosition(position)

        -- Set rotation if requested
        if faceDirection then
            local direction
            if faceDirection == "center" then
                -- Face inward (toward center)
                direction = (centerPos - position):Normalize()
            else -- "outward"
                -- Face outward (away from center)
                direction = (position - centerPos):Normalize()
            end

            -- Convert direction to rotation
            local rotation = Rotation(direction)
            if obj.SetRotation then
                obj:SetRotation(rotation)
            end
        end
    end

    return true
end

--- Transform local coordinates to world space using parent transform.
-- Converts position and rotation from parent's local space to world space.
-- This is the low-level mathematical function used by AttachToObject.
-- @tparam Vec3 parentPos Parent's world position.
-- @tparam Rotation parentRot Parent's world rotation.
-- @tparam Vec3 localOffset Offset in parent's local space.
-- @tparam[opt] Rotation localRotation Rotation in parent's local space (optional).
-- @tparam[opt="Transform3DUtils.TransformLocalToWorld"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Vec3 World position.
-- @treturn[1] Rotation World rotation (or nil if localRotation not provided).
-- @treturn[2] nil If an error occurs.
-- @usage
-- -- Example: Calculate world position of satellite relative to ship
-- local shipPos = TEN.Vec3(5000, 1000, 5000)
-- local shipRot = TEN.Rotation(0, 45, 0)
-- local localOffset = TEN.Vec3(500, 200, 0)  -- 500 units right, 200 units up in ship's space
-- local worldPos, worldRot = Transform3DUtils.TransformLocalToWorld(shipPos, shipRot, localOffset)
-- -- Result: worldPos accounts for ship's rotation
--
-- -- Example: Transform with local rotation
-- local shipPos = TEN.Vec3(5000, 1000, 5000)
-- local shipRot = TEN.Rotation(0, 90, 0)
-- local localOffset = TEN.Vec3(300, 0, 0)
-- local localRot = TEN.Rotation(0, 45, 0)  -- Additional 45° yaw
-- local worldPos, worldRot = Transform3DUtils.TransformLocalToWorld(shipPos, shipRot, localOffset, localRot)
-- turret:SetPosition(worldPos)
-- turret:SetRotation(worldRot)
--
-- -- Example: Manual attachment in loop
-- local parent = TEN.Objects.GetMoveableByName("Ship")
-- local child = TEN.Objects.GetMoveableByName("Turret")
-- local localOffset = TEN.Vec3(0, 300, -500)  -- Behind and above
-- LevelFuncs.AttachTurret = function()
--     local parentPos = parent:GetPosition()
--     local parentRot = parent:GetRotation()
--     local worldPos, worldRot = Transform3DUtils.TransformLocalToWorld(parentPos, parentRot, localOffset, TEN.Rotation(0, 0, 0))
--     child:SetPosition(worldPos)
--     child:SetRotation(worldRot)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.AttachTurret)
--
-- -- Error handling example:
-- local worldPos, worldRot = Transform3DUtils.TransformLocalToWorld(parentPos, parentRot, localOffset)
-- if worldPos == nil then
--     TEN.Util.PrintLog("Failed to transform local to world", TEN.Util.LogLevel.ERROR)
--     return
-- end
-- obj:SetPosition(worldPos)
--
-- -- Safe approach with fallback:
-- local worldPos = Transform3DUtils.TransformLocalToWorld(parentPos, parentRot, localOffset) or parentPos
Transform3DUtils.TransformLocalToWorld = function(parentPos, parentRot, localOffset, localRotation, errorContext)
    errorContext = errorContext or "Transform3DUtils.TransformLocalToWorld"
    if not IsVec3(parentPos) then
        ErrorLog("Error in {context}: parentPos must be a Vec3.", {context = errorContext})
        return nil
    end
    if not IsRotation(parentRot) then
        ErrorLog("Error in {context}: parentRot must be a Rotation.", {context = errorContext})
        return nil
    end
    if not IsVec3(localOffset) then
        ErrorLog("Error in {context}: localOffset must be a Vec3.", {context = errorContext})
        return nil
    end
    if localRotation and not IsRotation(localRotation) then
        ErrorLog("Error in {context}: localRotation must be a Rotation or nil.", {context = errorContext})
        return nil
    end

    return transformLocalToWorldRaw(parentPos, parentRot, localOffset, localRotation)
end

--- Calculate local offset from child to parent in parent's local space.
-- This helper function computes the offset needed for `AttachToObject`.
-- Call this ONCE during setup, then use the returned offset in your loop.
-- Works with both Moveable and Static objects.
-- @tparam Objects.Moveable|Objects.Static parent Parent object.
-- @tparam Objects.Moveable|Objects.Static child Child object to calculate offset for.
-- @tparam[opt="Transform3DUtils.CalculateLocalOffset"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] Vec3 Local offset in parent's space.
-- @treturn[2] nil If an error occurs.
-- @usage
-- -- Example: Calculate offset for turret on ship (setup phase)
-- local ship = TEN.Objects.GetMoveableByName("Ship")
-- local turret = TEN.Objects.GetMoveableByName("Turret")
-- local offset = Transform3DUtils.CalculateLocalOffset(ship, turret)
-- -- Now use 'offset' in your loop with AttachToObject
--
-- -- Example: Complete attachment workflow
-- local parent = TEN.Objects.GetMoveableByName("Vehicle")
-- local child = TEN.Objects.GetMoveableByName("Wheel")
-- 
-- -- STEP 1: Calculate offset ONCE (outside loop)
-- local localOffset = Transform3DUtils.CalculateLocalOffset(parent, child)
-- 
-- -- STEP 2: Use offset every frame
-- LevelFuncs.AttachWheel = function()
--     Transform3DUtils.AttachToObject(parent, child, localOffset, true)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.AttachWheel)
--
-- -- Example: Multiple children with different offsets
-- local ship = TEN.Objects.GetMoveableByName("Ship")
-- local turret1 = TEN.Objects.GetMoveableByName("Turret1")
-- local turret2 = TEN.Objects.GetMoveableByName("Turret2")
-- local offset1 = Transform3DUtils.CalculateLocalOffset(ship, turret1)
-- local offset2 = Transform3DUtils.CalculateLocalOffset(ship, turret2)
-- LevelFuncs.AttachTurrets = function()
--     Transform3DUtils.AttachToObject(ship, turret1, offset1, true)
--     Transform3DUtils.AttachToObject(ship, turret2, offset2, true)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.AttachTurrets)
--
-- -- Example: Static object attachment
-- local platform = TEN.Objects.GetStaticByName("Platform")
-- local crate = TEN.Objects.GetMoveableByName("Crate")
-- local offset = Transform3DUtils.CalculateLocalOffset(platform, crate)
-- LevelFuncs.AttachStaticCrate = function()
--     Transform3DUtils.AttachToObject(platform, crate, offset, false)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.AttachStaticCrate)
--
-- -- Error handling example:
-- local offset = Transform3DUtils.CalculateLocalOffset(parent, child)
-- if offset == nil then
--     TEN.Util.PrintLog("Failed to calculate local offset", TEN.Util.LogLevel.ERROR)
--     return
-- end
-- -- Use offset...
--
-- -- Safe approach with fallback:
-- local offset = Transform3DUtils.CalculateLocalOffset(parent, child) or TEN.Vec3(0, 0, 0)
Transform3DUtils.CalculateLocalOffset = function(parent, child, errorContext)
    errorContext = errorContext or "Transform3DUtils.CalculateLocalOffset"
    -- Type validation (check for GetPosition and GetRotation methods)
    if not parent or not child then
        ErrorLog("Error in {context}: parent and child cannot be nil.", {context = errorContext})
        return nil
    end

    local parentPos = parent.GetPosition and parent:GetPosition()
    local parentRot = parent.GetRotation and parent:GetRotation()
    local childPos = child.GetPosition and child:GetPosition()

    if not parentPos or not parentRot or not childPos then
        ErrorLog("Error in {context}: parent and child must have GetPosition() and GetRotation() methods.", {context = errorContext})
        return nil
    end

    -- Calculate world offset
    local worldOffset = childPos - parentPos

    -- Convert world offset to parent's local space.
    -- Use RotationInverseRaw (mathematically correct inverse, see Util.lua for
    -- details). Negating each Rotation component is NOT correct for multi-axis
    -- rotations because the inverse depends on the rotation order.
    local localOffset = RotationInverseRaw(parentRot, worldOffset)

    return localOffset
end

--- Attach child object to parent object with automatic transform updates.
-- High-level convenience function that applies position and optionally rotation.
-- Call this EVERY FRAME in your loop. The localOffset should be calculated ONCE
-- using `CalculateLocalOffset` before the loop.
-- Works with both Moveable and Static objects.
-- @tparam Objects.Moveable|Objects.Static parent Parent object.
-- @tparam Objects.Moveable|Objects.Static child Child object to attach.
-- @tparam Vec3 localOffset Offset in parent's local space.
-- @tparam[opt=false] bool inheritRotation If true, child inherits parent's rotation.
-- @tparam[opt="Transform3DUtils.AttachToObject"] string errorContext Context string for error messages (e.g., function name).
-- @treturn[1] bool True if successful.
-- @treturn[2] bool False if an error occurs.
-- @usage
-- -- Example: Simple attachment (position only) - Complete working example
-- local vehicle = TEN.Objects.GetMoveableByName("Vehicle")
-- local crate = TEN.Objects.GetMoveableByName("Crate")
-- local offset = Transform3DUtils.CalculateLocalOffset(vehicle, crate)  -- Setup ONCE
-- LevelFuncs.AttachCrateToVehicle = function()
--     Transform3DUtils.AttachToObject(vehicle, crate, offset, false)  -- Every frame
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.AttachCrateToVehicle)
--
-- -- Example: Attachment with rotation inheritance - Complete working example
-- local ship = TEN.Objects.GetMoveableByName("Ship")
-- local turret = TEN.Objects.GetMoveableByName("Turret")
-- local offset = TEN.Vec3(0, 300, 0)  -- 300 units above ship
-- LevelFuncs.AttachTurretToShip = function()
--     -- Turret follows ship position AND rotation
--     Transform3DUtils.AttachToObject(ship, turret, offset, true)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.AttachTurretToShip)
--
-- -- Example: Multiple satellites orbiting with attachment - Complete working example
-- local planet = TEN.Objects.GetMoveableByName("Planet")
-- local sat1 = TEN.Objects.GetMoveableByName("Satellite1")
-- local sat2 = TEN.Objects.GetMoveableByName("Satellite2")
-- local offset1 = TEN.Vec3(2048, 0, 0)  -- Right side
-- local offset2 = TEN.Vec3(-2048, 0, 0)  -- Left side
-- local angle = 0
-- local rotationSpeed = 360 / ConversionUtils.SecondsToFrames(20)  -- 20 seconds per rotation
-- LevelFuncs.AttachSatellites = function()
--     -- Rotate planet
--     angle = (angle + rotationSpeed) % 360
--     planet:SetRotation(TEN.Rotation(0, angle, 0))
--     
--     -- Satellites follow planet rotation automatically
--     Transform3DUtils.AttachToObject(planet, sat1, offset1, false)
--     Transform3DUtils.AttachToObject(planet, sat2, offset2, false)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.AttachSatellites)
--
-- -- Example: Weapon held by character - Complete working example
-- local lara = Lara
-- local torch = TEN.Objects.GetMoveableByName("Torch")
-- local weaponOffset = TEN.Vec3(150, 300, 50)  -- Right hand position
-- LevelFuncs.HoldTorch = function()
--     -- Torch follows Lara's position and rotation
--     Transform3DUtils.AttachToObject(lara, torch, weaponOffset, true)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.HoldTorch)
--
-- -- Example: Cart pulled by horse - Complete working example
-- local horse = TEN.Objects.GetMoveableByName("Horse")
-- local cart = TEN.Objects.GetMoveableByName("Cart")
-- local offset = Transform3DUtils.CalculateLocalOffset(horse, cart)  -- Setup ONCE
-- LevelFuncs.PullCart = function()
--     -- Cart follows horse with original offset, inherits rotation
--     Transform3DUtils.AttachToObject(horse, cart, offset, true)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.PullCart)
--
-- -- Example: Platform with crate (Static parent) - Complete working example
-- local platform = TEN.Objects.GetStaticByName("MovingPlatform")
-- local crate = TEN.Objects.GetMoveableByName("Crate")
-- local offset = TEN.Vec3(0, 256, 0)  -- On top of platform
-- LevelFuncs.CrateOnPlatform = function()
--     -- Crate stays on platform as it moves
--     Transform3DUtils.AttachToObject(platform, crate, offset, false)
-- end
-- TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.CrateOnPlatform)
--
-- -- Error handling example:
-- local success = Transform3DUtils.AttachToObject(parent, child, offset, true)
-- if not success then
--     TEN.Util.PrintLog("Failed to attach object", TEN.Util.LogLevel.ERROR)
-- end
Transform3DUtils.AttachToObject = function(parent, child, localOffset, inheritRotation, errorContext)
    errorContext = errorContext or "Transform3DUtils.AttachToObject"
    -- Type validation
    if not parent or not child then
        ErrorLog("Error in {context}: parent and child cannot be nil.", {context = errorContext})
        return false
    end
    if not IsVec3(localOffset) then
        ErrorLog("Error in {context}: localOffset must be a Vec3.", {context = errorContext})
        return false
    end
    if inheritRotation ~= nil and not IsBoolean(inheritRotation) then
        ErrorLog("Error in {context}: inheritRotation must be a boolean or nil.", {context = errorContext})
        return false
    end

    -- Get parent transform
    local parentPos = parent.GetPosition and parent:GetPosition()
    local parentRot = parent.GetRotation and parent:GetRotation()

    if not parentPos or not parentRot then
        ErrorLog("Error in {context}: parent must have GetPosition() and GetRotation() methods.", {context = errorContext})
        return false
    end

    -- Check child has SetPosition
    if not child.SetPosition then
        ErrorLog("Error in {context}: child must have SetPosition() method.", {context = errorContext})
        return false
    end

    -- Transform local offset to world space (all inputs already validated)
    local localRot = inheritRotation and Rotation(0, 0, 0) or nil
    local worldPos, worldRot = transformLocalToWorldRaw(parentPos, parentRot, localOffset, localRot)

    -- Apply transforms
    child:SetPosition(worldPos)

    if worldRot and child.SetRotation then
        child:SetRotation(worldRot)
    end

    return true
end

return Transform3DUtils