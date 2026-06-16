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

local Camera        = require("Engine.PhotoMode.Camera")
local Configuration = require("Engine.PhotoMode.Configuration")
local Constants     = require("Engine.PhotoMode.Constants")
local States        = require("Engine.PhotoMode.States")

local ActionID = TEN.Input.ActionID
local AxisID   = TEN.Input.AxisID
local Input    = {}

-- Dead-zone threshold for analogue axes.
local AXIS_DEAD_ZONE = Constants.AXIS_DEAD_ZONE

-- ============================================================================
-- Helpers
-- ============================================================================

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

    -- WASD / left analogue: forward/back + strafe.
    -- Inputs are combined into a single normalised translation so that
    -- diagonal speed matches cardinal speed and collision / distance
    -- limiting are evaluated exactly once per frame on the final delta.
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.STICK_LEFT)
    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    local moveFwd   = 0
    local moveRight = 0

    if TEN.Input.IsKeyHeld(ActionID.W) or lsY < -AXIS_DEAD_ZONE then
        moveFwd = (lsY < -AXIS_DEAD_ZONE) and -lsY or 1
    end
    if TEN.Input.IsKeyHeld(ActionID.S) or lsY > AXIS_DEAD_ZONE then
        moveFwd = (lsY > AXIS_DEAD_ZONE) and -lsY or -1
    end
    if TEN.Input.IsKeyHeld(ActionID.A) or lsX < -AXIS_DEAD_ZONE then
        moveRight = (lsX < -AXIS_DEAD_ZONE) and lsX or -1
    end
    if TEN.Input.IsKeyHeld(ActionID.D) or lsX > AXIS_DEAD_ZONE then
        moveRight = (lsX > AXIS_DEAD_ZONE) and lsX or 1
    end

    if moveFwd ~= 0 or moveRight ~= 0 then
        local dir   = Camera.GetDirection()
        local right = Camera.GetRightVector()

        local combined = TEN.Vec3(
            dir.x * moveFwd + right.x * moveRight,
            dir.y * moveFwd + right.y * moveRight,
            dir.z * moveFwd + right.z * moveRight)

        local len = combined:Length()
        if len > Constants.EPSILON then
            combined = combined:Normalize()
            local moveAmount = speed * len
            if moveAmount > speed then
                moveAmount = speed
            end
            Camera.Move(combined, moveAmount)
        end
    end

    -- Right analogue: rotate view
    local r2Held         = TEN.Input.IsKeyHeld(ActionID.GAMEPAD_RIGHT_TRIGGER)
    local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_RIGHT)

    
    local rs = TEN.Input.GetAnalogAxisValue(AxisID.STICK_RIGHT)
    local rsX = rs and ApplyDeadZone(rs.x) or 0
    local rsYraw = rs and ApplyDeadZone(rs.y) or 0
    if r2Held then
         if math.abs(rsX) > 0 or math.abs(rsYraw) > 0 then
            Camera.AdjustTargetVertical(rsYraw * speed)
            Camera.RotateView(rsX * lookSpeed, 0)
        end 
    else
        if math.abs(rsX) > 0 or math.abs(rsYraw) > 0 then
            Camera.RotateView(rsX * lookSpeed, rsYraw * lookSpeed)
        end
    end

    -- Mouse: rotate view, or translate vertically when a mouse button is held.
    local leftClickHeld  = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_LEFT)
    local clickHeld      = leftClickHeld or rightClickHeld
    local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
    local mx = mouse and mouse.x or 0
    local my = mouse and mouse.y or 0
    if clickHeld then
        if math.abs(my) > Constants.EPSILON then
            local vertScale = speed * Configuration.Camera.mouseSensitivity
            local lookScale = lookSpeed * Configuration.Camera.mouseSensitivity
            Camera.AdjustTargetVertical(my * vertScale)
            Camera.RotateView(mx * lookScale, 0)
        end
    else
        if math.abs(mx) > Constants.EPSILON or math.abs(my) > Constants.EPSILON then
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
    if fwdLen > Constants.EPSILON then
        fwd = TEN.Vec3(fwd.x / fwdLen, 0, fwd.z / fwdLen)
    end
    local right = Camera.GetRightVector()
    right = TEN.Vec3(right.x, 0, right.z)
    local rightLen = math.sqrt(right.x * right.x + right.z * right.z)
    if rightLen > Constants.EPSILON then
        right = TEN.Vec3(right.x / rightLen, 0, right.z / rightLen)
    end

    -- WASD / left analogue: move + strafe (camera-relative, XZ only)
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.STICK_LEFT)
    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    if TEN.Input.IsKeyHeld(ActionID.W) or lsY < -AXIS_DEAD_ZONE then
        local s = (lsY < -AXIS_DEAD_ZONE) and (-lsY * speed) or speed
        newPos = newPos + (fwd * s)
    end
    if TEN.Input.IsKeyHeld(ActionID.S) or lsY > AXIS_DEAD_ZONE then
        local s = (lsY > AXIS_DEAD_ZONE) and (lsY * speed) or speed
        newPos = newPos + (fwd * -s)
    end
    if TEN.Input.IsKeyHeld(ActionID.A) or lsX < -AXIS_DEAD_ZONE then
        local s = (lsX < -AXIS_DEAD_ZONE) and (-lsX * speed) or speed
        newPos = newPos + (right * -s)
    end
    if TEN.Input.IsKeyHeld(ActionID.D) or lsX > AXIS_DEAD_ZONE then
        local s = (lsX > AXIS_DEAD_ZONE) and (lsX * speed) or speed
        newPos = newPos + (right * s)
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
        -- Mouse: rotate character Y, or move when a mouse button is held.
        -- LMB + mouse Y/X = horizontal (camera-relative XZ).
        -- RMB + mouse Y   = vertical.
        local leftClickHeld  = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_LEFT)
        local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_RIGHT)
        local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
        local mx = mouse and mouse.x or 0
        local my = mouse and mouse.y or 0
        local scale = Configuration.Camera.mouseSensitivity * 2

        if leftClickHeld and (math.abs(mx) > Constants.EPSILON or math.abs(my) > Constants.EPSILON) then
            -- Horizontal movement: mouse Y = forward/back, mouse X = strafe.
            local hScale = scale * speed / 2
            newPos = newPos - (fwd * my * hScale)
            newPos = newPos + (right * mx * hScale)
        elseif rightClickHeld and math.abs(my) > Constants.EPSILON then
            -- Vertical movement.
            newPos = TEN.Vec3(newPos.x, newPos.y + my * scale / 2 * speed, newPos.z)
        else
            -- Y-axis rotation.
            newRot = TEN.Rotation(laraRot.x, laraRot.y + mx * scale * rotSpeed, laraRot.z)
        end
    end
    

    local posChanged = newPos.x ~= laraPos.x or newPos.y ~= laraPos.y or newPos.z ~= laraPos.z
    local rotChanged = newRot.y ~= laraRot.y

    if posChanged or rotChanged then
        if posChanged then Lara:SetPosition(newPos) end
        if rotChanged then Lara:SetRotation(newRot) end
        Lara:ResetHair()
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
    if fwdLen > Constants.EPSILON then
        fwd = TEN.Vec3(fwd.x / fwdLen, 0, fwd.z / fwdLen)
    end
    local right    = Camera.GetRightVector()
    right          = TEN.Vec3(right.x, 0, right.z)
    local rightLen = math.sqrt(right.x * right.x + right.z * right.z)
    if rightLen > Constants.EPSILON then
        right = TEN.Vec3(right.x / rightLen, 0, right.z / rightLen)
    end

    -- WASD / left analogue: XZ movement relative to camera direction
    local ls = TEN.Input.GetAnalogAxisValue(AxisID.STICK_LEFT)
    local lsX = ls and ApplyDeadZone(ls.x) or 0
    local lsY = ls and ApplyDeadZone(ls.y) or 0

    if TEN.Input.IsKeyHeld(ActionID.W) or lsY < -AXIS_DEAD_ZONE then
        local s = (lsY < -AXIS_DEAD_ZONE) and (-lsY * speed) or speed
        lightPos = lightPos + (fwd * s)
    end
    if TEN.Input.IsKeyHeld(ActionID.S) or lsY > AXIS_DEAD_ZONE then
        local s = (lsY > AXIS_DEAD_ZONE) and (lsY * speed) or speed
        lightPos = lightPos + (fwd * -s)
    end
    if TEN.Input.IsKeyHeld(ActionID.A) or lsX < -AXIS_DEAD_ZONE then
        local s = (lsX < -AXIS_DEAD_ZONE) and (-lsX * speed) or speed
        lightPos = lightPos + (right * -s)
    end
    if TEN.Input.IsKeyHeld(ActionID.D) or lsX > AXIS_DEAD_ZONE then
        local s = (lsX > AXIS_DEAD_ZONE) and (lsX * speed) or speed
        lightPos = lightPos + (right * s)
    end

    -- Right analogue Y: move light up/down (unless R2 held — that's camera adjust)
    local r2Held = TEN.Input.IsKeyHeld(ActionID.GAMEPAD_RIGHT_TRIGGER)

    if r2Held then
        local rs = TEN.Input.GetAnalogAxisValue(AxisID.STICK_RIGHT)
        local rsY = rs and ApplyDeadZone(rs.y) or 0
        if math.abs(rsY) > 0 then
            lightPos = TEN.Vec3(lightPos.x, lightPos.y + rsY * speed, lightPos.z)
        end
    end

    -- Mouse: move light.
    -- LMB + mouse Y/X = horizontal (camera-relative XZ).
    -- RMB + mouse Y   = vertical.
    local leftClickHeld  = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_LEFT)
    local rightClickHeld = TEN.Input.IsKeyHeld(ActionID.MOUSE_CLICK_RIGHT)
    local mouse = TEN.Input.GetAnalogAxisValue(AxisID.MOUSE)
    local mx = mouse and mouse.x or 0
    local my = mouse and mouse.y or 0
    local scale = Configuration.Camera.mouseSensitivity * 2

    if leftClickHeld and (math.abs(mx) > Constants.EPSILON or math.abs(my) > Constants.EPSILON) then
        -- Horizontal movement: mouse Y = forward/back, mouse X = strafe.
        lightPos = lightPos - (fwd * my * scale * speed)
        lightPos = lightPos + (right * mx * scale * speed)
    end

    if rightClickHeld and math.abs(my) > Constants.EPSILON then
        -- Vertical movement.
        lightPos = TEN.Vec3(lightPos.x, lightPos.y + my * scale * speed, lightPos.z)
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