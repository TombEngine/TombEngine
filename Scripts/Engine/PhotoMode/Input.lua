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
local Settings = require("Engine.PhotoMode.Settings")
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
-- Shared: camera vertical adjust + scroll dolly (active in every mode)
-- ============================================================================

local function UpdateSharedCameraInput(state)
    local speed = state.moveSpeed

    -- Scroll wheel dolly
    if TEN.Input.IsKeyHit(ActionID.MOUSE_SCROLL_UP) then
        Camera.MoveForward(speed * 2)
    end
    if TEN.Input.IsKeyHit(ActionID.MOUSE_SCROLL_DOWN) then
        Camera.MoveBack(speed * 2)
    end

    -- R2 (gamepad) or right mouse button (keyboard) + vertical axis
    -- -> adjust camera target up/down
    local r2Held         = TEN.Input.IsKeyHeld(ActionID.SPRINT)
    local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_RIGHT)

    if r2Held then
        local rs = TEN.Input.GetAnalogAxisValue(AxisID.CAMERA_VERTICAL)
        local rsY = ApplyDeadZone(rs and rs.y or 0)
        if math.abs(rsY) > 0 then
            Camera.AdjustTargetVertical(rsY * speed)
        end
    end

    if rightClickHeld then
        local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
        local my = mouse and mouse.y or 0
        if math.abs(my) > 0.001 then
            local scale = state.lookSpeed * Settings.Camera.mouseSensitivity
            Camera.AdjustTargetVertical(my * scale)
        end
    end
end

-- ============================================================================
-- Camera Controls
-- ============================================================================

local function UpdateCameraInput(state)
    local speed     = state.moveSpeed
    local lookSpeed = state.lookSpeed

    -- WASD / left analogue: forward/back + strafe
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.MOVE)
    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    if TEN.Input.IsKeyHeld(ActionID.FORWARD) or lsY < -AXIS_DEAD_ZONE then
        local s = (lsY < -AXIS_DEAD_ZONE) and (-lsY * speed) or speed
        Camera.MoveForward(s)
    end
    if TEN.Input.IsKeyHeld(ActionID.BACK) or lsY > AXIS_DEAD_ZONE then
        local s = (lsY > AXIS_DEAD_ZONE) and (lsY * speed) or speed
        Camera.MoveBack(s)
    end
    if TEN.Input.IsKeyHeld(ActionID.STEP_LEFT) or lsX < -AXIS_DEAD_ZONE then
        local s = (lsX < -AXIS_DEAD_ZONE) and (-lsX * speed) or speed
        Camera.Strafe(-s)
    end
    if TEN.Input.IsKeyHeld(ActionID.STEP_RIGHT) or lsX > AXIS_DEAD_ZONE then
        local s = (lsX > AXIS_DEAD_ZONE) and (lsX * speed) or speed
        Camera.Strafe(s)
    end

    -- Right analogue: rotate view
    local r2Held         = TEN.Input.IsKeyHeld(ActionID.SPRINT)
    local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_RIGHT)

    if not r2Held then
        local rs = TEN.Input.GetAnalogAxisValue(AxisID.CAMERA_HORIZONTAL)
        local rsX = rs and ApplyDeadZone(rs.x) or 0
        local rsYraw = rs and ApplyDeadZone(rs.y) or 0
        if math.abs(rsX) > 0 or math.abs(rsYraw) > 0 then
            Camera.RotateView(rsX * lookSpeed, rsYraw * lookSpeed)
        end
    end

    -- Mouse: rotate view (unless right-click is held — that's handled by shared)
    if not rightClickHeld then
        local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
        local mx = mouse and mouse.x or 0
        local my = mouse and mouse.y or 0
        if math.abs(mx) > 0.001 or math.abs(my) > 0.001 then
            local scale = lookSpeed * Settings.Camera.mouseSensitivity
            Camera.RotateView(mx * scale, my * scale)
        end
    end
end

-- ============================================================================
-- Player Controls
-- ============================================================================

local function UpdatePlayerInput(state)
    local cfg      = Settings.Player
    local speed    = cfg.moveSpeed
    local rotSpeed = cfg.rotateSpeed

    local laraPos = Lara:GetPosition()
    local laraRot = Lara:GetRotation()
    local fwd     = ForwardFromYaw(laraRot.y)

    local newPos = TEN.Vec3(laraPos.x, laraPos.y, laraPos.z)
    local newRot = TEN.Rotation(laraRot.x, laraRot.y, laraRot.z)

    -- WASD / left analogue: move + strafe
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.MOVE)
    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    if TEN.Input.IsKeyHeld(ActionID.FORWARD) or lsY < -AXIS_DEAD_ZONE then
        local s = (lsY < -AXIS_DEAD_ZONE) and (-lsY * speed) or speed
        newPos = Vec3Add(newPos, Vec3Scale(fwd, s))
    end
    if TEN.Input.IsKeyHeld(ActionID.BACK) or lsY > AXIS_DEAD_ZONE then
        local s = (lsY > AXIS_DEAD_ZONE) and (lsY * speed) or speed
        newPos = Vec3Add(newPos, Vec3Scale(fwd, -s))
    end
    if TEN.Input.IsKeyHeld(ActionID.STEP_LEFT) or lsX < -AXIS_DEAD_ZONE then
        local s = (lsX < -AXIS_DEAD_ZONE) and (-lsX * speed) or speed
        local right = RightFromYaw(laraRot.y)
        newPos = Vec3Add(newPos, Vec3Scale(right, -s))
    end
    if TEN.Input.IsKeyHeld(ActionID.STEP_RIGHT) or lsX > AXIS_DEAD_ZONE then
        local s = (lsX > AXIS_DEAD_ZONE) and (lsX * speed) or speed
        local right = RightFromYaw(laraRot.y)
        newPos = Vec3Add(newPos, Vec3Scale(right, s))
    end

    -- Right analogue X: rotate character Y; right analogue Y: move up/down
    local r2Held         = TEN.Input.IsKeyHeld(ActionID.SPRINT)
    local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_RIGHT)

    if not r2Held then
        local rs = TEN.Input.GetAnalogAxisValue(AxisID.CAMERA_HORIZONTAL)
        local rsX = rs and ApplyDeadZone(rs.x) or 0
        local rsY = rs and ApplyDeadZone(rs.y) or 0
        if math.abs(rsX) > 0 then
            newRot = TEN.Rotation(laraRot.x, laraRot.y + rsX * rotSpeed, laraRot.z)
        end
        if math.abs(rsY) > 0 then
            newPos = TEN.Vec3(newPos.x, newPos.y + rsY * speed, newPos.z)
        end
    end

    -- Mouse X: rotate character Y; mouse Y: move up/down (unless right-click held)
    if not rightClickHeld then
        local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
        local mx = mouse and mouse.x or 0
        local my = mouse and mouse.y or 0
        local scale = Settings.Camera.mouseSensitivity * 0.2
        if math.abs(mx) > 0.001 then
            newRot = TEN.Rotation(laraRot.x, laraRot.y + mx * scale * rotSpeed, laraRot.z)
        end
        if math.abs(my) > 0.001 then
            newPos = TEN.Vec3(newPos.x, newPos.y + my * scale * speed, newPos.z)
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
    local speed    = Settings.Camera.defaultMoveSpeed
    local lightPos = state.lightPos
    local dir      = Camera.GetDirection()
    local right    = Camera.GetRightVector()

    -- WASD / left analogue: XZ movement relative to camera direction
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.MOVE)
    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    if TEN.Input.IsKeyHeld(ActionID.FORWARD) or lsY < -AXIS_DEAD_ZONE then
        local s = (lsY < -AXIS_DEAD_ZONE) and (-lsY * speed) or speed
        lightPos = Vec3Add(lightPos, Vec3Scale(TEN.Vec3(dir.x, 0, dir.z), s))
    end
    if TEN.Input.IsKeyHeld(ActionID.BACK) or lsY > AXIS_DEAD_ZONE then
        local s = (lsY > AXIS_DEAD_ZONE) and (lsY * speed) or speed
        lightPos = Vec3Add(lightPos, Vec3Scale(TEN.Vec3(dir.x, 0, dir.z), -s))
    end
    if TEN.Input.IsKeyHeld(ActionID.STEP_LEFT) or lsX < -AXIS_DEAD_ZONE then
        local s = (lsX < -AXIS_DEAD_ZONE) and (-lsX * speed) or speed
        lightPos = Vec3Add(lightPos, Vec3Scale(right, -s))
    end
    if TEN.Input.IsKeyHeld(ActionID.STEP_RIGHT) or lsX > AXIS_DEAD_ZONE then
        local s = (lsX > AXIS_DEAD_ZONE) and (lsX * speed) or speed
        lightPos = Vec3Add(lightPos, Vec3Scale(right, s))
    end

    -- Right analogue Y: move light up/down (unless R2 held — that's camera adjust)
    local r2Held         = TEN.Input.IsKeyHeld(ActionID.SPRINT)
    local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_RIGHT)

    if not r2Held then
        local rs = TEN.Input.GetAnalogAxisValue(AxisID.CAMERA_VERTICAL)
        local rsY = rs and ApplyDeadZone(rs.y) or 0
        if math.abs(rsY) > 0 then
            lightPos = TEN.Vec3(lightPos.x, lightPos.y + rsY * speed, lightPos.z)
        end
    end

    -- Mouse Y: move light up/down (unless right-click held — that's camera adjust)
    if not rightClickHeld then
        local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
        local my = mouse and mouse.y or 0
        if math.abs(my) > 0.001 then
            local scale = Settings.Camera.mouseSensitivity * 2
            lightPos = TEN.Vec3(lightPos.x, lightPos.y + my * scale, lightPos.z)
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

    -- Shared: scroll dolly + R2/right-click camera vertical adjust
    UpdateSharedCameraInput(state)

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
