--- State management for the PhotoMode module.
-- Tracks the current mode (Camera, Player, Light) and all mutable photo mode state.
-- @module Engine.PhotoMode.States
-- @local

local Settings = require("Engine.PhotoMode.Settings")

local States = {}

-- ============================================================================
-- Control Modes
-- ============================================================================

States.Mode =
{
    CAMERA = 1,
    PLAYER = 2,
    LIGHT  = 3,
}

States.ModeNames = { "Camera", "Player", "Light" }

-- ============================================================================
-- Light Source Modes
-- ============================================================================

States.LightSource =
{
    MANUAL        = 1,
    FOLLOW_CAMERA = 2,
    FOLLOW_LARA   = 3,
}

-- ============================================================================
-- Runtime State (reset on entry, restored on exit)
-- ============================================================================

local State = {
    active         = false,
    entryHoldCount = 0,
    hideUI         = false,
    timeInPhotoMode   = 0,

    -- Snapshot of Lara / view state captured on entry
    snapshot = nil,

    -- Camera moveables (set by Camera module)
    cameraMesh   = nil,
    cameraTarget = nil,

    -- Current control mode
    controlMode = States.Mode.CAMERA,

    -- Camera settings
    moveSpeed         = Settings.Camera.defaultMoveSpeed,
    lookSpeed         = Settings.Camera.defaultLookSpeed,
    collisionOn       = true,
    limitCameraDistance = Settings.Camera.defaultLimitDistance,
    maxCameraDistance   = Settings.Camera.defaultMaxDistance,

    -- Lens
    fov  = Settings.Lens.defaultFOV,
    roll = Settings.Lens.defaultRoll,

    -- Pose
    animIndex = 1,

    -- Light
    lightEnabled    = Settings.Light.defaultEnabled,
    lightSource     = States.LightSource.MANUAL,
    lightPos        = TEN.Vec3(0, 0, 0),
    lightIntensity   = Settings.Light.defaultIntensity,
    lightRadius     = Settings.Light.defaultRadius,
    lightShadows    = Settings.Light.defaultShadows,
    lightColorIndex = 1,

    -- Filters
    filterIndex    = 1,
    filterStrength = 1.0,
    tintIndex      = 1,
    tintIntensity  = 0,

    -- Outfit / Weapons
    appliedSkin        = false,
    appliedSkinnedMesh = false,
    hiddenMeshes       = {},
    swappedWeaponMeshes = {},
    outfitIndex        = 1,
    weaponIndex        = 1,

    -- Frame overlay
    frameIndex = 1, -- index into Settings.Frames.presets (1 = None)

    -- Expressions
    expressionIndex         = 1,
    swappedExpressionMeshes = {},

    -- Depth of Field
    dofMode          = Settings.DepthOfField.defaultMode,
    dofFocusDistance = Settings.DepthOfField.defaultFocusDistance,
    dofRange         = Settings.DepthOfField.defaultRange,
    dofStrength      = Settings.DepthOfField.defaultStrength,

    -- Entry camera state (for Reset Camera)
    entryCamPos    = nil,
    entryTargetPos = nil,
    entryFov       = Settings.Lens.defaultFOV,
    entryRoll      = Settings.Lens.defaultRoll,
    entryLight     = nil,

    -- Accessory
    accessoryIndex = 1,
    accessoryMesh  = nil,

    -- Gun Flash
    gunflashEnabled = false,
}

-- ============================================================================
-- Accessors
-- ============================================================================

function States.Get()
    return State
end

function States.IsActive()
    return State.active
end

function States.SetActive(v)
    State.active = v
end

function States.GetMode()
    return State.controlMode
end

function States.SetMode(mode)
    State.controlMode = mode
end

function States.GetModeName()
    return States.ModeNames[State.controlMode] or "Unknown"
end

-- ============================================================================
-- Snapshot Capture / Restore
-- ============================================================================

function States.CaptureSnapshot()
    local snap = {}

    --save all settings in case user overrides settings for outfits via onEnter
    snap.settings = TEN.Flow.GetSettings()

    snap.laraPos      = Lara:GetPosition()
    snap.laraRot      = Lara:GetRotation()
    snap.laraVelocity = Lara:GetVelocity()
    snap.laraAnim     = Lara:GetAnim()
    snap.laraAnimSlot = Lara:GetAnimSlot()
    snap.laraFrame    = Lara:GetFrame()
    snap.laraState    = Lara:GetState()

    snap.fov  = TEN.View.GetFOV()
    snap.roll = TEN.View.GetRoll()
    snap.dofMode, snap.dofFocusDistance, snap.dofRange, snap.dofStrength = TEN.View.GetDOF()
    snap.postProcessMode, snap.postProcessStrength = TEN.View.GetPostProcess()
    snap.postProcessTint = TEN.View.GetPostProcessTint()

    -- Holster state
    local left, right, back = Lara:GetHolsterWeapon()
    snap.holsterLeft  = left
    snap.holsterRight = right
    snap.holsterBack  = back

    snap.filterIndex    = 1
    snap.filterStrength = 1.0
    snap.tintIndex      = 1
    snap.tintIntensity  = 0.0
    snap.camPos         = nil
    snap.targetPos      = nil
    snap.hideUI         = false
    -- Capture skinned mesh state (GPU skinning slot)
    local ok, skinObject, meshIndex = pcall(function() return Lara:GetSkinnedMesh() end)
    snap.skinnedMeshObject = (ok and skinObject ~= nil) and skinObject or nil
    snap.skinnedMeshIndex = (ok and meshIndex ~= nil) and meshIndex or nil
    -- Capture classic skin state (SetSkin parameters)
    local okSkin, skinTable = pcall(function() return Lara:GetSkin() end)
    if okSkin then
        snap.skin = skinTable
    end

    -- Capture per-mesh swap state for all 15 Lara meshes (0-14)
    snap.meshSwaps = {}
    for i = 0, 14 do
        local ok, swapped, sourceObjID = pcall(function() return Lara:GetMeshSwapped(i) end)
        if ok and swapped and sourceObjID then
            snap.meshSwaps[#snap.meshSwaps + 1] = { index = i, sourceObjID = sourceObjID }
        end
    end

    -- Capture per-mesh visibility state for all 15 Lara meshes (0-14).
    snap.meshVisible = {}
    for i = 0, 14 do
        local ok, vis = pcall(function() return Lara:GetMeshVisible(i) end)
        snap.meshVisible[i] = not (ok and vis == false)
    end

    State.snapshot = snap
    return snap
end

function States.RestoreSnapshot()
    local snap = State.snapshot
    if not snap then return end

    TEN.Flow.SetSettings(snap.settings)

    Lara:SetPosition(snap.laraPos)
    Lara:SetRotation(snap.laraRot)
    Lara:SetVelocity(snap.laraVelocity)
    pcall(function() Lara:SetAnim(snap.laraAnim, snap.laraAnimSlot) end)
    pcall(function() Lara:SetFrame(snap.laraFrame) end)
    pcall(function() Lara:SetState(snap.laraState) end)

    -- Restore classic skin to entry state.
    if snap.skin then
        pcall(function() Lara:SetSkin(snap.skin[1], snap.skin[2], snap.skin[3], snap.skin[4], snap.skin[5]) end)
    end

    -- Restore skinned mesh to entry state.
    if snap.skinnedMeshObject then
        pcall(function() Lara:SwapSkinnedMesh(snap.skinnedMeshObject, snap.skinnedMeshIndex) end)
    else
        pcall(function() Lara:ClearSkinnedMesh() end)
    end

    -- Restore per-mesh visibility to entry state.
    if snap.meshVisible then
        for i = 0, 14 do
            pcall(function() Lara:SetMeshVisible(i, snap.meshVisible[i] ~= false) end)
        end
    end

    -- Undo weapon and expression mesh swaps, then re-apply entry swaps.
    for _, meshIdx in ipairs(State.swappedWeaponMeshes) do
        pcall(function() Lara:UnswapMesh(meshIdx) end)
    end
    for _, meshIdx in ipairs(State.swappedExpressionMeshes) do
        pcall(function() Lara:UnswapMesh(meshIdx) end)
    end
    if snap.meshSwaps then
        for _, entry in ipairs(snap.meshSwaps) do
            pcall(function() Lara:SwapMesh(entry.index, entry.sourceObjID, entry.index) end)
        end
    end

    -- Restore holster state
    pcall(function()
        Lara:SetHolsterWeapon(snap.holsterLeft, snap.holsterRight, snap.holsterBack)
    end)

    --Reset Hair
    pcall(function() Lara:ResetHair() end)

    -- Reset post-process and camera settings
    pcall(function()
        TEN.View.SetDOF(snap.dofMode, snap.dofFocusDistance, snap.dofRange, snap.dofStrength)
        TEN.View.SetFOV(snap.fov)
        TEN.View.SetRoll(snap.roll)
        TEN.View.SetPostProcess(snap.postProcessMode, snap.postProcessStrength)
        TEN.View.SetPostProcessTint(snap.postProcessTint)
    end)
end

-- ============================================================================
-- Reset to Entry Defaults
-- ============================================================================

function States.ResetToEntry()
    State.controlMode         = States.Mode.CAMERA
    State.moveSpeed           = Settings.Camera.defaultMoveSpeed
    State.lookSpeed           = Settings.Camera.defaultLookSpeed
    State.collisionOn         = true
    State.limitCameraDistance = Settings.Camera.defaultLimitDistance
    State.maxCameraDistance   = Settings.Camera.defaultMaxDistance
    State.fov           = State.snapshot and State.snapshot.fov or Settings.Lens.defaultFOV
    State.roll          = Settings.Lens.defaultRoll
    State.animIndex     = 1
    State.lightEnabled  = Settings.Light.defaultEnabled
    State.lightSource   = States.LightSource.MANUAL
    State.lightRadius   = Settings.Light.defaultRadius
    State.lightColorIndex = 1
    State.filterIndex   = 1
    State.filterStrength = 1.0
    State.tintIndex     = 1
    State.tintIntensity = 0.0
    State.hideUI             = false
    State.appliedSkin        = false
    State.appliedSkinnedMesh = false
    State.hiddenMeshes       = {}
    State.swappedWeaponMeshes = {}
    State.outfitIndex                  = 1
    State.weaponIndex         = 1
    State.expressionIndex         = 1
    State.swappedExpressionMeshes = {}
    State.dofMode          = Settings.DepthOfField.defaultMode
    State.dofFocusDistance = Settings.DepthOfField.defaultFocusDistance
    State.dofRange         = Settings.DepthOfField.defaultRange
    State.dofStrength      = Settings.DepthOfField.defaultStrength
    State.frameIndex        = 1
    State.accessoryIndex    = 1
    State.gunflashEnabled   = false
    State.entryHoldCount    = 0
end

return States
