-- ldignore
-- Camera management for the PhotoMode module.
-- Handles creation of null-mesh camera objects, initial placement,
-- attaching/detaching the object camera, and camera movement.

local Configuration = require("Engine.PhotoMode.Configuration")
local States   = require("Engine.PhotoMode.States")

local Camera = {}

-- ============================================================================
-- Helpers
-- ============================================================================

local UP = TEN.Vec3(0, -1, 0) -- negative Y is up in TEN

local function IsInsideSolid(pos)
    local ok, probe = pcall(TEN.Collision.Probe, pos)
    if not ok then return false end
    return probe:IsInsideSolidGeometry()
end

-- ============================================================================
-- Create / Destroy
-- ============================================================================

local function IsValidPosition(pos)
    local ok, probe = pcall(TEN.Collision.Probe, pos)
    return ok and probe ~= nil
end

local function CreateAtLara(name)
    local pos  = Lara:GetPosition()
    local rot  = TEN.Rotation(0, 0, 0)
    local room = Lara:GetRoomNumber()
    local ok, mov = pcall(TEN.Objects.Moveable,
        TEN.Objects.ObjID.CAMERA_TARGET, name, pos, rot, room)
    if ok and mov then
        mov:Enable()
        return mov
    end
    return nil
end

local function GetOrCreate(name)
    if TEN.Objects.IsNameInUse(name) then
        local mov = TEN.Objects.GetMoveableByName(name)
        if mov then return mov, false end
        -- Name registered but moveable is invalid (was OOB-killed); recreate it.
        TEN.Util.PrintLog("PhotoMode: Recreating invalid object '" .. name .. "'.", TEN.Util.LogLevel.WARNING)
    end
    return CreateAtLara(name), true
end

function Camera.Init()
    local state = States.Get()

    local camMesh, _   = GetOrCreate(Configuration.Camera.meshName)
    local camTarget, _ = GetOrCreate(Configuration.Camera.targetName)

    state.cameraMesh   = camMesh
    state.cameraTarget = camTarget

    if not camMesh or not camTarget then
        TEN.Util.PrintLog("PhotoMode: Failed to create camera objects.", TEN.Util.LogLevel.ERROR)
        return false
    end

    return true
end

local function LaraOffsetPosition(forwardOffset, upOffset)
    local pos = Lara:GetPosition()
    local rot = Lara:GetRotation()
    local forward = TEN.Vec3(0, 0, 1):Rotate(rot)
    return TEN.Vec3(
        pos.x + forward.x * forwardOffset,
        pos.y + upOffset,
        pos.z + forward.z * forwardOffset
    )
end

function Camera.PlaceInitial()
    local state = States.Get()
    if not state.cameraMesh or not state.cameraTarget then return end

    local cfg = Configuration.Camera

    -- Try to start from the current game camera; fall back to Lara-based offsets
    -- if those positions are out of the level bounds.
    local camPos    = TEN.View.GetCameraPosition()
    local targetPos = TEN.View.GetCameraTarget()

    if not IsValidPosition(camPos) then
        TEN.Util.PrintLog("PhotoMode: Camera position out of bounds, falling back to Lara offset.", TEN.Util.LogLevel.WARNING)
        camPos = LaraOffsetPosition(cfg.offsetForward, cfg.offsetUp)
    end

    if not IsValidPosition(targetPos) then
        TEN.Util.PrintLog("PhotoMode: Camera target out of bounds, falling back to Lara offset.", TEN.Util.LogLevel.WARNING)
        targetPos = LaraOffsetPosition(cfg.targetForward, cfg.targetUp)
    end

    state.cameraMesh:SetPosition(camPos)
    state.cameraTarget:SetPosition(targetPos)

    state.entryCamPos    = camPos
    state.entryTargetPos = targetPos

    if state.snapshot then
        state.snapshot.camPos    = camPos
        state.snapshot.targetPos = targetPos
    end
end

function Camera.Attach()
    local state = States.Get()
    if state.cameraMesh and state.cameraTarget then
        state.cameraMesh:AttachObjCamera(
            Configuration.Camera.meshIndex,
            state.cameraTarget,
            Configuration.Camera.targetIndex
        )
    end
end

function Camera.Detach()
    pcall(TEN.View.ResetObjCamera)
end

function Camera.Reset()
    local state = States.Get()
    if state.entryCamPos and state.cameraMesh then
        state.cameraMesh:SetPosition(state.entryCamPos)
    end
    if state.entryTargetPos and state.cameraTarget then
        state.cameraTarget:SetPosition(state.entryTargetPos)
    end
end

-- ============================================================================
-- Direction helpers (public for Input module)
-- ============================================================================

function Camera.GetDirection()
    local state = States.Get()
    if not state.cameraMesh or not state.cameraTarget then
        return TEN.Vec3(0, 0, 1)
    end
    return state.cameraMesh:GetPosition():Direction(state.cameraTarget:GetPosition())
end

function Camera.GetRightVector()
    local dir = Camera.GetDirection()
    local right = dir:Cross(UP)
    if right:Length() < 0.001 then
        return TEN.Vec3(1, 0, 0)
    end
    return right:Normalize()
end

-- ============================================================================
-- Movement (called from Input module)
-- ============================================================================

-- Apply both positions atomically.  Always sets BOTH moveables so
-- the engine re-evaluates the object camera view.
local function ApplyPositions(newCam, newTgt)
    local state = States.Get()
    if state.collisionOn then
        if IsInsideSolid(newCam) or IsInsideSolid(newTgt) then return false end
    end

    -- Distance limit: prevent camera moving beyond maxCameraDistance from Lara's entry position.
    if Configuration.Camera.limitDistance and state.snapshot and state.snapshot.laraPos then
        local origin = state.snapshot.laraPos
        local dx = newCam.x - origin.x
        local dy = newCam.y - origin.y
        local dz = newCam.z - origin.z
        local distSq = dx * dx + dy * dy + dz * dz
        local maxDist = Configuration.Camera.maxDistance
        if distSq > maxDist * maxDist then return false end
    end

    state.cameraMesh:SetPosition(newCam)
    state.cameraTarget:SetPosition(newTgt)
    return true
end

function Camera.MoveForward(speed)
    local state = States.Get()
    local dir    = Camera.GetDirection()
    local newCam = state.cameraMesh:GetPosition():Translate(dir, speed)
    local newTgt = state.cameraTarget:GetPosition():Translate(dir, speed)
    ApplyPositions(newCam, newTgt)
end

function Camera.MoveBack(speed)
    Camera.MoveForward(-speed)
end

function Camera.Strafe(speed)
    local state  = States.Get()
    local right  = Camera.GetRightVector()
    local newCam = state.cameraMesh:GetPosition():Translate(right, speed)
    local newTgt = state.cameraTarget:GetPosition():Translate(right, speed)
    ApplyPositions(newCam, newTgt)
end

function Camera.OrbitHorizontal(angle)
    local state  = States.Get()
    local camPos = state.cameraMesh:GetPosition()
    local tgtPos = state.cameraTarget:GetPosition()
    local offset = TEN.Vec3(tgtPos.x - camPos.x, tgtPos.y - camPos.y, tgtPos.z - camPos.z)
    local rotated = offset:Rotate(TEN.Rotation(0, angle, 0))
    local newTgt  = TEN.Vec3(camPos.x + rotated.x, camPos.y + rotated.y, camPos.z + rotated.z)
    -- Set both so the engine refreshes the object camera
    state.cameraMesh:SetPosition(camPos)
    state.cameraTarget:SetPosition(newTgt)
end

function Camera.AdjustTargetVertical(speed)
    local state  = States.Get()
    local camPos = state.cameraMesh:GetPosition()
    local tgtPos = state.cameraTarget:GetPosition()
    local newCam = TEN.Vec3(camPos.x, camPos.y + speed, camPos.z)
    local newTgt = TEN.Vec3(tgtPos.x, tgtPos.y + speed, tgtPos.z)
    state.cameraMesh:SetPosition(newCam)
    state.cameraTarget:SetPosition(newTgt)
end

-- Maximum elevation angle (degrees from horizontal) the look-at vector is
-- allowed to reach.  Staying below 90° prevents the camera from flipping.
local PITCH_LIMIT = 88.0

-- Rotate the camera view freely (yaw = horizontal, pitch = vertical).
-- Uses spherical coordinates to guarantee no gimbal flip regardless of input speed.
function Camera.RotateView(yawDeg, pitchDeg)
    local state  = States.Get()
    local camPos = state.cameraMesh:GetPosition()
    local tgtPos = state.cameraTarget:GetPosition()

    local ox = tgtPos.x - camPos.x
    local oy = tgtPos.y - camPos.y
    local oz = tgtPos.z - camPos.z

    -- Keep the camera-to-target distance constant.
    local dist = math.sqrt(ox * ox + oy * oy + oz * oz)
    if dist < 0.001 then return end

    local hDist = math.sqrt(ox * ox + oz * oz)

    -- Decompose into spherical angles (radians).
    local currentYaw   = math.atan(ox, oz)
    local currentPitch = math.atan(oy, hDist)

    -- Apply deltas and clamp pitch.
    local limitRad = math.rad(PITCH_LIMIT)
    local newYaw   = currentYaw   + math.rad(yawDeg)
    local newPitch = math.max(-limitRad, math.min(limitRad, currentPitch + math.rad(pitchDeg)))

    -- Reconstruct offset from spherical coordinates.
    local cosP = math.cos(newPitch)
    local newTgt = TEN.Vec3(
        camPos.x + dist * cosP * math.sin(newYaw),
        camPos.y + dist * math.sin(newPitch),
        camPos.z + dist * cosP * math.cos(newYaw))

    state.cameraMesh:SetPosition(camPos)
    state.cameraTarget:SetPosition(newTgt)
end

return Camera
