-- Camera management for the PhotoMode module.
-- Handles creation of null-mesh camera objects, initial placement,
-- attaching/detaching the object camera, and camera movement.

local Settings = require("Engine.PhotoMode.Settings")
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

    local camMesh, _   = GetOrCreate(Settings.Camera.meshName)
    local camTarget, _ = GetOrCreate(Settings.Camera.targetName)

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

    local cfg = Settings.Camera

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
            Settings.Camera.meshIndex,
            state.cameraTarget,
            Settings.Camera.targetIndex
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
    if Settings.Camera.limitDistance and state.snapshot and state.snapshot.laraPos then
        local origin = state.snapshot.laraPos
        local dx = newCam.x - origin.x
        local dy = newCam.y - origin.y
        local dz = newCam.z - origin.z
        local distSq = dx * dx + dy * dy + dz * dz
        local maxDist = Settings.Camera.maxDistance
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

-- Rotate the camera view freely (yaw = horizontal, pitch = vertical).
function Camera.RotateView(yawDeg, pitchDeg)
    local state  = States.Get()
    local camPos = state.cameraMesh:GetPosition()
    local tgtPos = state.cameraTarget:GetPosition()

    local offset  = TEN.Vec3(tgtPos.x - camPos.x, tgtPos.y - camPos.y, tgtPos.z - camPos.z)
    local rotated = offset:Rotate(TEN.Rotation(pitchDeg, yawDeg, 0))
    local newTgt  = TEN.Vec3(camPos.x + rotated.x, camPos.y + rotated.y, camPos.z + rotated.z)

    state.cameraMesh:SetPosition(camPos)
    state.cameraTarget:SetPosition(newTgt)
end

return Camera
