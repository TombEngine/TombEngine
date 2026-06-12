-- ldignore
--- Generic input handling for the PhotoMode module.
-- Supports simultaneous UI + movement: controls are always active regardless
-- of whether the UI is visible.
--
-- Mapping summary:
--   WASD / Left analogue          -> move forward/back/strafe in all modes
--   Mouse / Right analogue        -> camera: rotate view
--                                    player: X=rotate Y, Y=move up/down
--                                    light:  Y=move up/down
--   R2 held + right analogue Y    -> Camera.AdjustTargetVertical (all modes)
--   Right-click held + mouse Y    -> Camera.AdjustTargetVertical (all modes)
--   Mouse scroll                  -> camera dolly (all modes)
--
-- @module Engine.PhotoMode.Input
-- @local

local Camera   = require("Engine.PhotoMode.Camera")
local Configuration = require("Engine.PhotoMode.Configuration")
local States   = require("Engine.PhotoMode.States")

local ActionID = TEN.Input.ActionID
local AxisID   = TEN.Input.AxisID
local Input    = {}

-- ============================================================================
-- Helpers
-- ============================================================================

local function ForwardFromYaw(yawDeg)
    local rad = math.rad(yawDeg)
    return TEN.Vec3(math.sin(rad), 0, math.cos(rad))
end

local function RightFromYaw(yawDeg)
    local rad = math.rad(yawDeg)
    return TEN.Vec3(math.cos(rad), 0, -math.sin(rad))
end

local function Vec3Add(a, b)
    return TEN.Vec3(a.x + b.x, a.y + b.y, a.z + b.z)
end

local function Vec3Scale(v, s)
    return TEN.Vec3(v.x * s, v.y * s, v.z * s)
end

-- Dead-zone threshold for analogue axes.
local AXIS_DEAD_ZONE = 0.15

local function ApplyDeadZone(v)
    if math.abs(v) < AXIS_DEAD_ZONE then return 0 end
    -- Rescale so the usable range is still [0, 1]
    local sign = v > 0 and 1 or -1
    return sign * (math.abs(v) - AXIS_DEAD_ZONE) / (1 - AXIS_DEAD_ZONE)
end

-- ============================================================================
-- Camera Controls
-- ============================================================================

local function UpdateCameraInput(state)
    local speed     = state.moveSpeed
    local lookSpeed = state.lookSpeed

    -- WASD / left analogue: forward/back + strafe
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.STICK_LEFT)

    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    if TEN.Input.IsKeyHeld(ActionID.W) or lsY < -AXIS_DEAD_ZONE then
        local s = (lsY < -AXIS_DEAD_ZONE) and (-lsY * speed) or speed
        Camera.MoveForward(s)
    end
    if TEN.Input.IsKeyHeld(ActionID.S) or lsY > AXIS_DEAD_ZONE then
        local s = (lsY > AXIS_DEAD_ZONE) and (lsY * speed) or speed
        Camera.MoveBack(s)
    end
    if TEN.Input.IsKeyHeld(ActionID.A) or lsX < -AXIS_DEAD_ZONE then
        local s = (lsX < -AXIS_DEAD_ZONE) and (-lsX * speed) or speed
        Camera.Strafe(-s)
    end
    if TEN.Input.IsKeyHeld(ActionID.D) or lsX > AXIS_DEAD_ZONE then
        local s = (lsX > AXIS_DEAD_ZONE) and (lsX * speed) or speed
        Camera.Strafe(s)
    end

    -- Right analogue: rotate view
    local r2Held         = TEN.Input.IsKeyHeld(ActionID.GAMEPAD_RIGHT_TRIGGER)
    local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_RIGHT)

    
    local rs = TEN.Input.GetAnalogAxisValue(AxisID.STICK_RIGHT)
    local rsX = rs and ApplyDeadZone(rs.x) or 0
    local rsYraw = rs and ApplyDeadZone(rs.y) or 0
    if r2Held then
         if math.abs(rsYraw) > 0 then
            Camera.AdjustTargetVertical(rsYraw * speed)
        end 
    else
        if math.abs(rsX) > 0 or math.abs(rsYraw) > 0 then
            Camera.RotateView(rsX * lookSpeed, rsYraw * lookSpeed)
        end
       
    end

    -- Mouse: rotate view (unless right-click is held — that's handled by shared)
    local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
    local mx = mouse and mouse.x or 0
    local my = mouse and mouse.y or 0
    if rightClickHeld then
        if math.abs(my) > 0.001 then
            local scale = speed * Configuration.Camera.mouseSensitivity
            Camera.AdjustTargetVertical(my * scale)
        end
    else
        if math.abs(mx) > 0.001 or math.abs(my) > 0.001 then
            local scale = lookSpeed * Configuration.Camera.mouseSensitivity
            Camera.RotateView(mx * scale, my * scale)
        end
    end

end
-- ============================================================================
-- Player Controls
-- ============================================================================

local function UpdatePlayerInput(state)
    local cfg      = Configuration.Player
    local speed    = cfg.moveSpeed
    local rotSpeed = cfg.rotateSpeed

    local laraPos = Lara:GetPosition()
    local laraRot = Lara:GetRotation()

    local newPos = TEN.Vec3(laraPos.x, laraPos.y, laraPos.z)
    local newRot = TEN.Rotation(laraRot.x, laraRot.y, laraRot.z)

    -- Use the camera's look direction projected onto XZ so that W/S move
    -- Lara away from / towards the camera and A/D strafe relative to it.
    local camDir   = Camera.GetDirection()
    local fwd      = TEN.Vec3(camDir.x, 0, camDir.z)
    local fwdLen   = math.sqrt(fwd.x * fwd.x + fwd.z * fwd.z)
    if fwdLen > 0.001 then
        fwd = TEN.Vec3(fwd.x / fwdLen, 0, fwd.z / fwdLen)
    end
    local right = Camera.GetRightVector()
    right = TEN.Vec3(right.x, 0, right.z)
    local rightLen = math.sqrt(right.x * right.x + right.z * right.z)
    if rightLen > 0.001 then
        right = TEN.Vec3(right.x / rightLen, 0, right.z / rightLen)
    end

    -- WASD / left analogue: move + strafe (camera-relative, XZ only)
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.STICK_LEFT)
    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    if TEN.Input.IsKeyHeld(ActionID.W) or lsY < -AXIS_DEAD_ZONE then
        local s = (lsY < -AXIS_DEAD_ZONE) and (-lsY * speed) or speed
        newPos = Vec3Add(newPos, Vec3Scale(fwd, s))
    end
    if TEN.Input.IsKeyHeld(ActionID.S) or lsY > AXIS_DEAD_ZONE then
        local s = (lsY > AXIS_DEAD_ZONE) and (lsY * speed) or speed
        newPos = Vec3Add(newPos, Vec3Scale(fwd, -s))
    end
    if TEN.Input.IsKeyHeld(ActionID.A) or lsX < -AXIS_DEAD_ZONE then
        local s = (lsX < -AXIS_DEAD_ZONE) and (-lsX * speed) or speed
        newPos = Vec3Add(newPos, Vec3Scale(right, -s))
    end
    if TEN.Input.IsKeyHeld(ActionID.D) or lsX > AXIS_DEAD_ZONE then
        local s = (lsX > AXIS_DEAD_ZONE) and (lsX * speed) or speed
        newPos = Vec3Add(newPos, Vec3Scale(right, s))
    end

    local device = TEN.Input.GetLastInputDevice()
    if device == TEN.Input.InputDevice.GAMEPAD then
        -- Right analogue X: rotate character Y; right analogue Y: move up/down
        local r2Held         = TEN.Input.IsKeyHeld(ActionID.GAMEPAD_RIGHT_TRIGGER)
        local rs = TEN.Input.GetAnalogAxisValue(AxisID.STICK_RIGHT)
        local rsX = rs and ApplyDeadZone(rs.x) or 0
        local rsY = rs and ApplyDeadZone(rs.y) or 0
        if r2Held and math.abs(rsY) > 0 then
            newPos = TEN.Vec3(newPos.x, newPos.y + rsY * speed, newPos.z)
        else
            newRot = TEN.Rotation(laraRot.x, laraRot.y + rsX * rotSpeed, laraRot.z)
        end
    else
        -- Mouse X: rotate character Y; mouse Y: move up/down (right-click held)
        local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_RIGHT)
        local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
        local mx = mouse and mouse.x or 0
        local my = mouse and mouse.y or 0
        local scale = Configuration.Camera.mouseSensitivity * 2

        if rightClickHeld and math.abs(my) > 0.001 then
            scale = scale / 2
            newPos = TEN.Vec3(newPos.x, newPos.y + my * scale * speed, newPos.z)
        else
            newRot = TEN.Rotation(laraRot.x, laraRot.y + mx * scale * rotSpeed, laraRot.z)
        end
    end
    

    local posChanged = newPos.x ~= laraPos.x or newPos.y ~= laraPos.y or newPos.z ~= laraPos.z
    local rotChanged = newRot.y ~= laraRot.y

    if posChanged or rotChanged then
        if posChanged then Lara:SetPosition(newPos) end
        if rotChanged then Lara:SetRotation(newRot) end
        pcall(function() Lara:ResetHair() end)
    end
end

-- ============================================================================
-- Light Controls
-- ============================================================================

local function UpdateLightInput(state)
    local speed    = Configuration.Camera.defaultMoveSpeed
    local lightPos = state.lightPos

    -- Camera-relative XZ axes (same normalization as player mode)
    local camDir = Camera.GetDirection()
    local fwd    = TEN.Vec3(camDir.x, 0, camDir.z)
    local fwdLen = math.sqrt(fwd.x * fwd.x + fwd.z * fwd.z)
    if fwdLen > 0.001 then
        fwd = TEN.Vec3(fwd.x / fwdLen, 0, fwd.z / fwdLen)
    end
    local right    = Camera.GetRightVector()
    right          = TEN.Vec3(right.x, 0, right.z)
    local rightLen = math.sqrt(right.x * right.x + right.z * right.z)
    if rightLen > 0.001 then
        right = TEN.Vec3(right.x / rightLen, 0, right.z / rightLen)
    end

    -- WASD / left analogue: XZ movement relative to camera direction
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.STICK_LEFT)
    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    if TEN.Input.IsKeyHeld(ActionID.W) or lsY < -AXIS_DEAD_ZONE then
        local s = (lsY < -AXIS_DEAD_ZONE) and (-lsY * speed) or speed
        lightPos = Vec3Add(lightPos, Vec3Scale(fwd, s))
    end
    if TEN.Input.IsKeyHeld(ActionID.S) or lsY > AXIS_DEAD_ZONE then
        local s = (lsY > AXIS_DEAD_ZONE) and (lsY * speed) or speed
        lightPos = Vec3Add(lightPos, Vec3Scale(fwd, -s))
    end
    if TEN.Input.IsKeyHeld(ActionID.A) or lsX < -AXIS_DEAD_ZONE then
        local s = (lsX < -AXIS_DEAD_ZONE) and (-lsX * speed) or speed
        lightPos = Vec3Add(lightPos, Vec3Scale(right, -s))
    end
    if TEN.Input.IsKeyHeld(ActionID.D) or lsX > AXIS_DEAD_ZONE then
        local s = (lsX > AXIS_DEAD_ZONE) and (lsX * speed) or speed
        lightPos = Vec3Add(lightPos, Vec3Scale(right, s))
    end

    -- Right analogue Y: move light up/down (unless R2 held — that's camera adjust)
    local r2Held = TEN.Input.IsKeyHeld(ActionID.GAMEPAD_RIGHT_TRIGGER)
    local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_RIGHT)

    if r2Held then
        local rs = TEN.Input.GetAnalogAxisValue(AxisID.STICK_RIGHT)
        local rsY = rs and ApplyDeadZone(rs.y) or 0
        if math.abs(rsY) > 0 then
            lightPos = TEN.Vec3(lightPos.x, lightPos.y + rsY * speed, lightPos.z)
        end
    end

    -- Mouse Y: move light up/down (unless right-click held — that's camera adjust)
    if rightClickHeld then
        local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
        local my = mouse and mouse.y or 0
        if math.abs(my) > 0.001 then
            local scale = Configuration.Camera.mouseSensitivity * 2
            lightPos = TEN.Vec3(lightPos.x, lightPos.y + my * scale * speed, lightPos.z)
        end
    end

    state.lightPos = lightPos
end

-- ============================================================================
-- Public: Always-on update — called every freeze frame regardless of UI state.
-- ============================================================================

function Input.Update()
    local state = States.Get()
    local mode  = States.GetMode()

    -- Mode-specific movement
    if mode == States.Mode.CAMERA then
        UpdateCameraInput(state)
    elseif mode == States.Mode.PLAYER then
        UpdatePlayerInput(state)
    elseif mode == States.Mode.LIGHT then
        UpdateLightInput(state)
    end
end

return Input
