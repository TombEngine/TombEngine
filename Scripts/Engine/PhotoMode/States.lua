-- ldignore
-- State management for the PhotoMode module.
-- Tracks the current mode (Camera, Player, Light) and all mutable photo mode state.

local Configuration = require("Engine.PhotoMode.Configuration")

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

local State = 
{
    active         = false,
    entryHoldCount = 0,
    hideUI         = false,
    hideCharacter  = false,
    timeInPhotoMode   = 0,

    -- Snapshot of Lara / view state captured on entry
    snapshot = nil,

    -- Camera moveables (set by Camera module)
    cameraMesh   = nil,
    cameraTarget = nil,

    -- Current control mode
    controlMode = States.Mode.CAMERA,

    -- Camera settings
    moveSpeed         = Configuration.Camera.defaultMoveSpeed,
    lookSpeed         = Configuration.Camera.defaultLookSpeed,
    collisionOn       = true,
    limitCameraDistance = Configuration.Camera.defaultLimitDistance,
    maxCameraDistance   = Configuration.Camera.defaultMaxDistance,

    -- Lens
    fov  = Configuration.Lens.defaultFOV,
    roll = Configuration.Lens.defaultRoll,

    -- Pose
    animIndex = 1,

    -- Light
    lightEnabled    = Configuration.Light.defaultEnabled,
    lightSource     = States.LightSource.MANUAL,
    lightPos        = TEN.Vec3(0, 0, 0),
    lightIntensity  = Configuration.Light.defaultIntensity,
    lightRadius     = Configuration.Light.defaultRadius,
    lightShadows    = Configuration.Light.defaultShadows,
    lightColorIndex = 1,

    -- Filters
    filterIndex    = 1,
    filterStrength = 1.0,
    tintIndex      = 1,
    tintIntensity  = 0,

    -- Outfit / Weapons
    appliedSkin         = false,
    appliedSkinnedMesh  = false,
    hiddenMeshes        = {},
    swappedWeaponMeshes = {},
    outfitIndex         = 1,
    weaponIndex         = 1,

    -- Frame overlay
    frameIndex = 1, -- index into Configuration.Frames.presets (1 = None)

    -- Expressions
    expressionIndex         = 1,
    swappedExpressionMeshes = {},

    -- Depth of Field
    dofMode          = Configuration.DepthOfField.defaultMode,
    dofFocusDistance = Configuration.DepthOfField.defaultFocusDistance,
    dofRange         = Configuration.DepthOfField.defaultRange,
    dofStrength      = Configuration.DepthOfField.defaultStrength,

    -- Entry camera state (for Reset Camera)
    entryCamPos    = nil,
    entryTargetPos = nil,
    entryFov       = Configuration.Lens.defaultFOV,
    entryRoll      = Configuration.Lens.defaultRoll,
    entryLight     = nil,

    -- Accessory
    accessoryIndex = 1,
    accessoryMesh  = nil,

    -- Gun Flash
    gunflashEnabled = false,

    -- Screenshot
    screenshotPending    = false,
    screenshotHideFrames = 0,
    screenshotMessage    = nil,
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

    --save all settings in case user overrides settings for outfits via onSelect
    snap.settings = TEN.Flow.GetSettings()

    snap.laraPos      = Lara:GetPosition()
    snap.laraRot      = Lara:GetRotation()
    snap.laraVelocity = Lara:GetVelocity()
    snap.laraAnim     = Lara:GetAnim()
    snap.laraAnimSlot = Lara:GetAnimSlot()
    snap.laraFrame    = Lara:GetFrame()
    snap.laraState    = Lara:GetState()
    snap.handStatus   = Lara:GetHandStatus()

    snap.fov  = TEN.View.GetFOV()
    snap.roll = TEN.View.GetRoll()
    snap.dofMode, snap.dofFocusDistance, snap.dofRange, snap.dofStrength = TEN.View.GetDOF()
    snap.postProcessMode, snap.postProcessStrength = TEN.View.GetPostProcess()
    snap.postProcessTint = TEN.View.GetPostProcessTint()

    -- Holster state
    local left, right, back = Lara:GetHolsterWeaponTypes()
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
    local skinObject, meshIndex = Lara:GetSkinnedMesh()
    snap.skinnedMeshObject = skinObject ~= nil and skinObject or nil
    snap.skinnedMeshIndex = meshIndex ~= nil and meshIndex or nil
    -- Capture classic skin state (SetSkin parameters)
    local skinTable = Lara:GetSkin()
    if skinTable then
        snap.skin = skinTable
    end

    -- Capture per-mesh swap state for all 15 Lara meshes (0-14)
    snap.meshSwaps = {}
    for i = 0, 14 do
        local swapped, sourceObjID = Lara:GetMeshSwapped(i)
        if swapped and sourceObjID then
            snap.meshSwaps[#snap.meshSwaps + 1] = { index = i, sourceObjID = sourceObjID }
        end
    end

    -- Capture per-mesh visibility state for all 15 Lara meshes (0-14).
    snap.meshVisible = {}
    for i = 0, 14 do
        local vis = Lara:GetMeshVisible(i)
        snap.meshVisible[i] = vis ~= false
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
    Lara:SetAnim(snap.laraAnim, snap.laraAnimSlot)
    Lara:SetFrame(snap.laraFrame)
    Lara:SetState(snap.laraState)
    Lara:SetHandStatus(snap.handStatus)
    
    -- Restore classic skin to entry state.
    if snap.skin then
        Lara:SetSkin(snap.skin[1], snap.skin[2], snap.skin[3], snap.skin[4], snap.skin[5])
    end

    -- Restore skinned mesh to entry state.
    if snap.skinnedMeshObject then
        Lara:SwapSkinnedMesh(snap.skinnedMeshObject, snap.skinnedMeshIndex)
    else
        Lara:ClearSkinnedMesh()
    end

    -- Restore per-mesh visibility to entry state.
    if snap.meshVisible then
        for i = 0, 14 do
            Lara:SetMeshVisible(i, snap.meshVisible[i] ~= false)
        end
    end

    -- Undo weapon and expression mesh swaps, then re-apply entry swaps.
    for _, meshIdx in ipairs(State.swappedWeaponMeshes) do
        Lara:UnswapMesh(meshIdx)
    end
    for _, meshIdx in ipairs(State.swappedExpressionMeshes) do
        Lara:UnswapMesh(meshIdx)
    end
    if snap.meshSwaps then
        for _, entry in ipairs(snap.meshSwaps) do
            Lara:SwapMesh(entry.index, entry.sourceObjID, entry.index)
        end
    end

    -- Restore holster state
    Lara:SetHolsterWeaponTypes(snap.holsterLeft, snap.holsterRight, snap.holsterBack)

    --Reset Hair
    Lara:ResetHair()

    --Clear any trailing gun flashes
    Lara:ClearGunFlashes()

    -- Reset post-process and camera settings
    TEN.View.SetDOF(snap.dofMode, snap.dofFocusDistance, snap.dofRange, snap.dofStrength)
    TEN.View.SetFOV(snap.fov)
    TEN.View.SetRoll(snap.roll)
    TEN.View.SetPostProcess(snap.postProcessMode, snap.postProcessStrength)
    TEN.View.SetPostProcessTint(snap.postProcessTint)
end

-- ============================================================================
-- Reset to Entry Defaults
-- ============================================================================

function States.ResetToEntry()
    State.controlMode             = States.Mode.CAMERA
    State.moveSpeed               = Configuration.Camera.defaultMoveSpeed
    State.lookSpeed               = Configuration.Camera.defaultLookSpeed
    State.collisionOn             = true
    State.limitCameraDistance     = Configuration.Camera.defaultLimitDistance
    State.maxCameraDistance       = Configuration.Camera.defaultMaxDistance
    State.fov                     = State.snapshot and State.snapshot.fov or Configuration.Lens.defaultFOV
    State.roll                    = Configuration.Lens.defaultRoll
    State.animIndex               = 1
    State.lightEnabled            = Configuration.Light.defaultEnabled
    State.lightSource             = States.LightSource.MANUAL
    State.lightRadius             = Configuration.Light.defaultRadius
    State.lightColorIndex         = 1
    State.filterIndex             = 1
    State.filterStrength          = 1.0
    State.tintIndex               = 1
    State.tintIntensity           = 0.0
    State.hideUI                  = false
    State.hideCharacter           = false
    State.appliedSkin             = false
    State.appliedSkinnedMesh      = false
    State.hiddenMeshes            = {}
    State.swappedWeaponMeshes     = {}
    State.outfitIndex             = 1
    State.weaponIndex             = 1
    State.expressionIndex         = 1
    State.swappedExpressionMeshes = {}
    State.dofMode                 = Configuration.DepthOfField.defaultMode
    State.dofFocusDistance        = Configuration.DepthOfField.defaultFocusDistance
    State.dofRange                = Configuration.DepthOfField.defaultRange
    State.dofStrength             = Configuration.DepthOfField.defaultStrength
    State.frameIndex              = 1
    State.accessoryIndex          = 1
    State.gunflashEnabled         = false
    State.entryHoldCount          = 0
end

return States
