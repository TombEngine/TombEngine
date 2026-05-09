--- PhotoMode entry point.
-- Orchestrates all sub-modules: Camera, States, Input, Menu, Frames.
-- Uses the RingInventory-style Menu with header tabs to provide a polished UI.
--
-- To use in a level script:
--
--    local PhotoMode = require("Engine.PhotoMode.PhotoMode")
--
-- Entry is triggered by holding Walk + Inventory for N frames.
-- While active the game is frozen (SPECTATOR mode) and the object camera is used.
--
-- @module Engine.PhotoMode.PhotoMode
-- @local

local Camera   = require("Engine.PhotoMode.Camera")
local Frames   = require("Engine.PhotoMode.Frames")
local Input    = require("Engine.PhotoMode.Input")
local Menu     = require("Engine.PhotoMode.Menu")
local Settings = require("Engine.PhotoMode.Settings")
local States   = require("Engine.PhotoMode.States")
require("Engine.PhotoMode.Strings")

LevelFuncs.Engine.PhotoMode = LevelFuncs.Engine.PhotoMode or {}

local PhotoMode = {}

-- Guards LevelFuncs registration to once per Lua session (resets on level/savegame reload).
local _callbacksRegistered = false
local _photoModeExited = false

-- ============================================================================
-- Helpers
-- ============================================================================

local function Clamp(v, lo, hi)
    return math.max(lo, math.min(hi, v))
end

local function Round(val, decimals)
    local mult = 10 ^ (decimals or 0)
    return math.floor(val * mult + 0.5) / mult
end

-- ============================================================================
-- Option name builders (for selector-type options)
-- ============================================================================

local function BuildNames(list, key)
    local t = {}
    for _, v in ipairs(list) do t[#t + 1] = v[key or "name"] end
    return t
end

local DOF_MODE_NAMES   = BuildNames(Settings.DepthOfField.modes)
local CTRL_MODE_NAMES  = States.ModeNames
local LIGHT_SRC_NAMES  = Settings.Light.sourceNames
local FILTER_NAMES     = BuildNames(Settings.Filters.presets)
local TINT_NAMES       = BuildNames(Settings.Filters.tints)
local COLOR_NAMES      = BuildNames(Settings.Light.colorPresets)
local ANIM_NAMES       = BuildNames(Settings.Animations)
local OUTFIT_NAMES     = BuildNames(Settings.Outfits)
local WEAPON_NAMES     = BuildNames(Settings.Weapons)
local FRAME_NAMES      = BuildNames(Settings.Frames.presets)
local EXPRESSION_NAMES = BuildNames(Settings.Expressions)

-- ============================================================================
-- LevelFuncs callbacks for menu option changes
-- ============================================================================

-- We register thin callback stubs in LevelFuncs so Menu can call them by name.

LevelFuncs.Engine.PhotoMode.OnExit = function()
    PhotoMode.Exit()
end

-- ============================================================================
-- Apply Functions (setters triggered by option changes)
-- ============================================================================

local function ApplyFOV(state)
    TEN.View.SetFOV(state.fov)
end

local function ApplyRoll(state)
    TEN.View.SetRoll(state.roll)
end

local function ApplyFilter(state)
    local preset = Settings.Filters.presets[state.filterIndex]
    if preset then
        TEN.View.SetPostProcessMode(preset.mode)
    end
end

local function ApplyFilterStrength(state)
    TEN.View.SetPostProcessStrength(state.filterStrength)
end

local function ApplyTint(state)
    local preset = Settings.Filters.tints[state.tintIndex]
    if preset then
        TEN.View.SetPostProcessTint(preset.color)
    end
end

local function ResetCurrentOutfit(state)
    local snap = state.snapshot

    -- Restore skinned mesh to entry state.
    if state.appliedSkinnedMesh then
        if snap and snap.skinnedMeshIndex then
            pcall(function() Lara:SetSkinnedMesh(snap.skinnedMeshIndex) end)
        else
            pcall(function() Lara:ClearSkinnedMesh() end)
        end
    end

    -- Restore classic skin to entry state.
    if state.appliedSkin and snap and snap.skin then
        pcall(function() Lara:SetSkin(snap.skin[1], snap.skin[2], snap.skin[3], snap.skin[4], snap.skin[5]) end)
    end

    -- Restore mesh visibility to entry state.
    if #state.hiddenMeshes > 0 and snap and snap.meshVisible then
        for i = 0, 14 do
            pcall(function() Lara:SetMeshVisible(i, snap.meshVisible[i] ~= false) end)
        end
    end

    if snap.settings then
        TEN.Flow.SetSettings(snap.settings)
    end

    state.appliedSkin        = false
    state.appliedSkinnedMesh = false
    state.hiddenMeshes       = {}
end

local function ApplyOutfit(state)
    ResetCurrentOutfit(state)

    local preset = Settings.Outfits[state.outfitIndex]

    -- Default: ResetCurrentOutfit already restored entry state.
    if not preset or (not preset.skin and not preset.skinnedMesh and not preset.meshVisible) then
        pcall(function() Lara:ResetHair() end)
        return
    end

    -- Apply classic skin change.
    if preset.skin then
        local s = preset.skin
        pcall(function() Lara:SetSkin(s[1], s[2], s[3], s[4], s[5]) end)
        state.appliedSkin = true
    end

    -- Apply skinned mesh change.
    if preset.skinnedMesh then
        if preset.skinnedMesh == "clear" then
            pcall(function() Lara:ClearSkinnedMesh() end)
        else
            pcall(function() Lara:SwapSkinnedMesh(preset.skinnedMesh, preset.skinnedMeshIndex) end)
        end
        state.appliedSkinnedMesh = true
    end

    -- Apply mesh visibility.
    if preset.meshVisible then
        state.hiddenMeshes = {}
        local mv = preset.meshVisible
        if mv == "all" then
            -- keep all visible
        elseif type(mv) == "table" then
            local keep = {}
            for _, idx in ipairs(mv) do keep[idx] = true end
            for i = 0, 14 do
                if not keep[i] then
                    pcall(function() Lara:SetMeshVisible(i, false) end)
                    state.hiddenMeshes[#state.hiddenMeshes + 1] = i
                end
            end
        else
            -- "none" → hide all classic meshes
            for i = 0, 14 do
                pcall(function() Lara:SetMeshVisible(i, false) end)
                state.hiddenMeshes[#state.hiddenMeshes + 1] = i
            end
        end
    end

    -- Re-apply weapon and expression mesh swaps (SetSkin resets classic meshes).
    for _, meshIdx in ipairs(state.swappedWeaponMeshes) do
        local wp = Settings.Weapons[state.weaponIndex]
        if wp and wp.objID then
            pcall(function() Lara:SwapMesh(meshIdx, wp.objID, meshIdx) end)
        end
    end
    for _, meshIdx in ipairs(state.swappedExpressionMeshes) do
        local ep = Settings.Expressions[state.expressionIndex]
        if ep and ep.objID then
            pcall(function() Lara:SwapMesh(meshIdx, ep.objID, meshIdx) end)
        end
    end

    -- Execute outfit-specific hook if provided.
    if preset.onEnter and type(preset.onEnter) == "function" then
        pcall(preset.onEnter)
    end

    pcall(function() Lara:ResetHair() end)
end

local function ApplyWeapon(state)
    -- Unswap previously applied weapon meshes
    for _, meshIdx in ipairs(state.swappedWeaponMeshes) do
        pcall(function() Lara:UnswapMesh(meshIdx) end)
    end
    state.swappedWeaponMeshes = {}

    local preset = Settings.Weapons[state.weaponIndex]
    if preset and preset.objID and preset.meshIndices then
        for _, meshIdx in ipairs(preset.meshIndices) do
            pcall(function() Lara:SwapMesh(meshIdx, preset.objID, meshIdx) end)
            state.swappedWeaponMeshes[#state.swappedWeaponMeshes + 1] = meshIdx
        end
    end

    -- Adjust holster slots based on which visual slots the weapon occupies.
    -- Clear the slots that are now visually shown as drawn; retain the rest.
    -- For "none" (default), restore entry snapshot holster state.
    pcall(function()
        local slot = preset and preset.type or "none"
        local snap = state.snapshot
        if slot == "holsters" then
            -- Pistols in both hand holsters: clear left + right, leave back alone
            Lara:SetHolsterWeapon(TEN.Objects.WeaponType.NONE, TEN.Objects.WeaponType.NONE, nil)
        elseif slot == "right" then
            -- Weapon in right holster only: clear right, leave left + back alone
            Lara:SetHolsterWeapon(nil, TEN.Objects.WeaponType.NONE, nil)
        elseif slot == "back" then
            -- Weapon on back: clear back, leave left + right alone
            Lara:SetHolsterWeapon(nil, nil, TEN.Objects.WeaponType.NONE)
        elseif slot == "left" then
            -- Weapon in left holster only: clear left, leave right + back alone
            Lara:SetHolsterWeapon(TEN.Objects.WeaponType.NONE, nil, nil)
        else
            -- No weapon / default: restore entry holster state
            if snap then
                Lara:SetHolsterWeapon(snap.holsterLeft, snap.holsterRight, snap.holsterBack)
            end
        end
    end)
    pcall(function() Lara:ResetHair() end)
end

local function ApplyPosePreset(state)
    local preset = Settings.Animations[state.animIndex]
    if preset then
        pcall(function() Lara:SetAnim(preset.animNumber, preset.objID) end)
        pcall(function() Lara:SetFrame(preset.frameNumber) end)
    end
    pcall(function() Lara:ResetHair() end)
end

local function ApplyExpression(state)
    for _, meshIdx in ipairs(state.swappedExpressionMeshes) do
        pcall(function() Lara:UnswapMesh(meshIdx) end)
    end
    state.swappedExpressionMeshes = {}

    local preset = Settings.Expressions[state.expressionIndex]
    if preset and preset.objID and preset.meshIndices then
        for _, meshIdx in ipairs(preset.meshIndices) do
            pcall(function() Lara:SwapMesh(meshIdx, preset.objID, meshIdx) end)
            state.swappedExpressionMeshes[#state.swappedExpressionMeshes + 1] = meshIdx
        end
    end
    pcall(function() Lara:ResetHair() end)
end

local function GetOrCreateSunglasses(state)
    local name = Settings.Sunglasses.meshName
    if state.sunglassesMesh then return state.sunglassesMesh end
    if TEN.Objects.IsNameInUse(name) then
        local mov = TEN.Objects.GetMoveableByName(name)
        state.sunglassesMesh = mov
        return mov
    end
    local pos  = Lara:GetPosition()
    local rot  = Lara:GetRotation()
    local room = Lara:GetRoomNumber()
    local ok, mov = pcall(TEN.Objects.Moveable, Settings.Sunglasses.objID, name, pos, rot, room)
    if ok and mov then
        mov:Enable()
        pcall(function() mov:SetColor(TEN.Color(255, 255, 255, 0)) end)
        state.sunglassesMesh = mov
        return mov
    end
    return nil
end

local function ApplySunglasses(state)
    local mov = GetOrCreateSunglasses(state)
    if not mov then return end
    if state.sunglassesEnabled then
        pcall(function() mov:SetPosition(Lara:GetPosition()) end)
        pcall(function() mov:SetRotation(Lara:GetRotation()) end)
        pcall(function() mov:SetAnim(Lara:GetAnim(), Lara:GetAnimSlot()) end)
        pcall(function() mov:SetFrame(Lara:GetFrame()) end)
        pcall(function() mov:SetColor(TEN.Color(255, 255, 255, 255)) end)
    else
        pcall(function() mov:SetColor(TEN.Color(255, 255, 255, 0)) end)
    end
end

local function UpdateSunglasses(state)
    if not state.sunglassesEnabled or not state.sunglassesMesh then return end
    pcall(function() state.sunglassesMesh:SetPosition(Lara:GetPosition()) end)
    pcall(function() state.sunglassesMesh:SetRotation(Lara:GetRotation()) end)
    pcall(function() state.sunglassesMesh:SetAnim(Lara:GetAnim(), Lara:GetAnimSlot()) end)
    pcall(function() state.sunglassesMesh:SetFrame(Lara:GetFrame()) end)
end

local function UpdateGunFlash(state)
    if not state.gunflashEnabled then return end
    local preset = Settings.Weapons[state.weaponIndex]
    if not preset or preset.weaponType == TEN.Objects.WeaponType.NONE then return end

    if preset.weaponType == TEN.Objects.WeaponType.FLARE then
        local flare = TEN.Flow.GetSettings().Flare
        local position = Lara:GetJointPosition(preset.meshIndices[1], flare.offset)
        pcall(function() TEN.Effects.EmitLight(position, flare.color, flare.range) end)
    else
        pcall(function() Lara:SpawnGunFlash(preset.weaponType) end)
    end
end

local function ApplyDOF(state)
    local cfg = Settings.DepthOfField
    local modePreset = cfg.modes[state.dofMode]
    local mode = modePreset and modePreset.mode or TEN.View.DOFMode.NONE
    if mode == TEN.View.DOFMode.NONE then
        pcall(function() TEN.View.SetDOF(TEN.View.DOFMode.NONE) end)
    else
        pcall(function() TEN.View.SetDOF(mode, state.dofFocusDistance, state.dofRange, state.dofStrength) end)
    end
end

-- ============================================================================
-- Reset Functions
-- ============================================================================

local function ResetCamera()
    Camera.Reset()
end

local function ResetCharacter()
    local state = States.Get()
    if state.snapshot then
        pcall(function() Lara:SetAnim(state.snapshot.laraAnim, state.snapshot.laraAnimSlot) end)
        pcall(function() Lara:SetFrame(state.snapshot.laraFrame) end)
    end
    state.animIndex = 1

    ResetCurrentOutfit(state)

    -- Undo weapon and expression mesh swaps.
    for _, meshIdx in ipairs(state.swappedWeaponMeshes) do
        pcall(function() Lara:UnswapMesh(meshIdx) end)
    end
    state.swappedWeaponMeshes = {}

    for _, meshIdx in ipairs(state.swappedExpressionMeshes) do
        pcall(function() Lara:UnswapMesh(meshIdx) end)
    end
    state.swappedExpressionMeshes = {}

    -- Re-apply entry mesh swaps and holster state.
    local snap = state.snapshot
    if snap then
        if snap.meshSwaps then
            for _, entry in ipairs(snap.meshSwaps) do
                pcall(function() Lara:SwapMesh(entry.index, entry.sourceObjID, entry.index) end)
            end
        end
        pcall(function() Lara:SetHolsterWeapon(snap.holsterLeft, snap.holsterRight, snap.holsterBack) end)
    end

    state.outfitIndex     = 1
    state.weaponIndex     = 1
    state.expressionIndex = 1
    state.sunglassesEnabled = false
    state.gunflashEnabled   = false
    ApplySunglasses(state)
end

local function ResetEffects()
    local state = States.Get()
    state.fov  = state.entryFov
    state.roll = state.entryRoll
    ApplyFOV(state)
    ApplyRoll(state)

    state.filterIndex    = 1
    state.filterStrength = 1.0
    state.tintIndex      = 1
    ApplyFilter(state)
    ApplyFilterStrength(state)
    ApplyTint(state)

    state.frameIndex       = 1
    state.dofMode          = Settings.DepthOfField.defaultMode
    state.dofFocusDistance = Settings.DepthOfField.defaultFocusDistance
    state.dofRange         = Settings.DepthOfField.defaultRange
    state.dofStrength      = Settings.DepthOfField.defaultStrength
    ApplyDOF(state)
end

local function ResetLight()
    local state = States.Get()
    if state.entryLight then
        state.lightEnabled    = state.entryLight.enabled
        state.lightSource     = state.entryLight.source
        state.lightPos        = TEN.Vec3(state.entryLight.pos.x, state.entryLight.pos.y, state.entryLight.pos.z)
        state.lightRadius     = state.entryLight.radius
        state.lightShadows    = state.entryLight.shadows
        state.lightColorIndex = state.entryLight.colorIndex
    end
end

local function PlaceLightAtCamera()
    local state = States.Get()
    if state.cameraMesh then
        local cp = state.cameraMesh:GetPosition()
        state.lightPos = TEN.Vec3(cp.x, cp.y, cp.z)
        state.lightSource = States.LightSource.MANUAL
    end
end

local function PlaceLightAtLara()
    local state = States.Get()
    local lp = Lara:GetPosition()
    state.lightPos = TEN.Vec3(lp.x, lp.y - 256, lp.z)
    state.lightSource = States.LightSource.MANUAL
end

-- ============================================================================
-- Menu Construction
-- ============================================================================

-- Each header maps to one menu. Menu items use ITEMS_AND_OPTIONS type so
-- left/right changes values while forward/back navigates items.

local MENU_CAMERA    = "pm_camera"
local MENU_CHARACTER = "pm_character"
local MENU_EFFECTS   = "pm_effects"
local MENU_LIGHT     = "pm_light"
local MENU_UI        = "pm_ui"

local function NumberRange(min, max, step, format)
    local opts = {}
    local val = min
    while val <= max + step * 0.01 do
        if format then
            opts[#opts + 1] = format(val)
        else
            opts[#opts + 1] = tostring(math.floor(val))
        end
        val = val + step
    end
    return opts
end

local function BoolOptions()
    return { "OFF", "ON" }
end

local function BoolToIndex(v)
    return v and 2 or 1
end

local function IndexToBool(i)
    return i == 2
end

local function ValueToOptionIndex(value, min, step)
    return math.floor((value - min) / step + 0.5) + 1
end

local function OptionIndexToValue(index, min, step)
    return min + (index - 1) * step
end

local function BuildAllMenus()
    local state = States.Get()
    local cfg   = Settings
    local acceptString = TEN.Flow.GetString("pm_press")

    Menu.DeleteAll()

    -- ================================================================
    -- Helper to create and configure a menu
    -- ================================================================
    local function CreateMenu(menuName, items, acceptFunc, optionChangeFunc)
        local menu = Menu.Create(menuName, "", items,
            acceptFunc, "Engine.PhotoMode.OnExit", Menu.Type.ITEMS_AND_OPTIONS)
        menu:SetItemsPosition(Vec2(15, 25))
        menu:SetOptionsPosition(Vec2(55, 25))
        menu:SetTitle("", nil, 0)
        menu:SetItemsTranslate(true)
        if optionChangeFunc then
            for _, item in ipairs(items) do
                item.onOptionChange = optionChangeFunc
            end
        end
        return menu
    end

    -- ================================================================
    -- Accept callbacks (one per menu, handles button/[Press] items)
    -- ================================================================

    if not _callbacksRegistered then
    _callbacksRegistered = true

    LevelFuncs.Engine.PhotoMode.OnCameraAccept = function()
        local m = Menu.Get(MENU_CAMERA)
        if not m then return end
        if m:GetCurrentItemIndex() == 5 then ResetCamera() end
    end

    LevelFuncs.Engine.PhotoMode.OnCharacterAccept = function()
        local m = Menu.Get(MENU_CHARACTER)
        if not m then return end
        if m:GetCurrentItemIndex() == 7 then
            ResetCharacter()
            m:SetOptionIndexForItem(1, state.animIndex)
            m:SetOptionIndexForItem(2, state.outfitIndex)
            m:SetOptionIndexForItem(3, state.weaponIndex)
            m:SetOptionIndexForItem(4, state.expressionIndex)
            m:SetOptionIndexForItem(5, BoolToIndex(state.sunglassesEnabled))
            m:SetOptionIndexForItem(6, BoolToIndex(state.gunflashEnabled))
        end
    end

    LevelFuncs.Engine.PhotoMode.OnEffectsAccept = function()
        local m = Menu.Get(MENU_EFFECTS)
        if not m then return end
        if m:GetCurrentItemIndex() == 11 then
            ResetEffects()
            m:SetOptionIndexForItem(1, ValueToOptionIndex(state.fov, cfg.Lens.minFOV, cfg.Lens.fovStep))
            m:SetOptionIndexForItem(2, ValueToOptionIndex(state.roll, cfg.Lens.minRoll, cfg.Lens.rollStep))
            m:SetOptionIndexForItem(3, state.filterIndex)
            m:SetOptionIndexForItem(4, ValueToOptionIndex(state.filterStrength, 0, 0.05))
            m:SetOptionIndexForItem(5, state.tintIndex)
            m:SetOptionIndexForItem(6, state.frameIndex)
            m:SetOptionIndexForItem(7, state.dofMode)
            m:SetOptionIndexForItem(8, ValueToOptionIndex(state.dofFocusDistance, cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.focusDistanceStep))
            m:SetOptionIndexForItem(9, ValueToOptionIndex(state.dofRange, cfg.DepthOfField.minRange, cfg.DepthOfField.rangeStep))
            m:SetOptionIndexForItem(10, ValueToOptionIndex(state.dofStrength, cfg.DepthOfField.minStrength, cfg.DepthOfField.strengthStep))
        end
    end

    LevelFuncs.Engine.PhotoMode.OnLightAccept = function()
        local m = Menu.Get(MENU_LIGHT)
        if not m then return end
        local idx = m:GetCurrentItemIndex()
        if idx == 5 then PlaceLightAtCamera()
        elseif idx == 6 then PlaceLightAtLara()
        elseif idx == 7 then
            ResetLight()
            m:SetOptionIndexForItem(1, BoolToIndex(state.lightEnabled))
            m:SetOptionIndexForItem(2, state.lightSource)
            m:SetOptionIndexForItem(3, ValueToOptionIndex(state.lightRadius, cfg.Light.minRadius, cfg.Light.radiusStep))
            m:SetOptionIndexForItem(4, BoolToIndex(state.lightShadows))
            m:SetOptionIndexForItem(5, state.lightColorIndex)
        end
    end

    LevelFuncs.Engine.PhotoMode.OnUIAccept = function()
        local m = Menu.Get(MENU_UI)
        if not m then return end
        if m:GetCurrentItemIndex() == 2 then PhotoMode.Exit() end
    end

    -- ================================================================
    -- Option change callbacks (sync state when left/right changes value)
    -- ================================================================

    LevelFuncs.Engine.PhotoMode.OnCameraOptionChange = function()
        local m = Menu.Get(MENU_CAMERA)
        if not m then return end
        local idx = m:GetCurrentItemIndex()
        if idx == 1 then state.controlMode = m:GetCurrentOptionIndex()
        elseif idx == 2 then state.moveSpeed = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Camera.minMoveSpeed, cfg.Camera.moveSpeedStep)
        elseif idx == 3 then state.lookSpeed = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Camera.minLookSpeed, cfg.Camera.lookSpeedStep)
        elseif idx == 4 then state.collisionOn = IndexToBool(m:GetCurrentOptionIndex())
        end
    end

    LevelFuncs.Engine.PhotoMode.OnCharacterOptionChange = function()
        local m = Menu.Get(MENU_CHARACTER)
        if not m then return end
        local idx = m:GetCurrentItemIndex()
        if idx == 1 then
            state.animIndex = m:GetCurrentOptionIndex()
            ApplyPosePreset(state)
        elseif idx == 2 then
            state.outfitIndex = m:GetCurrentOptionIndex()
            ApplyOutfit(state)
        elseif idx == 3 then
            state.weaponIndex = m:GetCurrentOptionIndex()
            ApplyWeapon(state)
        elseif idx == 4 then
            state.expressionIndex = m:GetCurrentOptionIndex()
            ApplyExpression(state)
        elseif idx == 5 then
            state.sunglassesEnabled = IndexToBool(m:GetCurrentOptionIndex())
            ApplySunglasses(state)
        elseif idx == 6 then
            state.gunflashEnabled = IndexToBool(m:GetCurrentOptionIndex())
        end
    end

    LevelFuncs.Engine.PhotoMode.OnEffectsOptionChange = function()
        local m = Menu.Get(MENU_EFFECTS)
        if not m then return end
        local idx = m:GetCurrentItemIndex()
        if idx == 1 then
            state.fov = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Lens.minFOV, cfg.Lens.fovStep)
            ApplyFOV(state)
        elseif idx == 2 then
            state.roll = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Lens.minRoll, cfg.Lens.rollStep)
            ApplyRoll(state)
        elseif idx == 3 then
            state.filterIndex = m:GetCurrentOptionIndex()
            ApplyFilter(state)
        elseif idx == 4 then
            state.filterStrength = OptionIndexToValue(m:GetCurrentOptionIndex(), 0, 0.05)
            ApplyFilterStrength(state)
        elseif idx == 5 then
            state.tintIndex = m:GetCurrentOptionIndex()
            ApplyTint(state)
        elseif idx == 6 then
            state.frameIndex = m:GetCurrentOptionIndex()
        elseif idx == 7 then
            state.dofMode = m:GetCurrentOptionIndex()
            ApplyDOF(state)
        elseif idx == 8 then
            state.dofFocusDistance = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.focusDistanceStep)
            ApplyDOF(state)
        elseif idx == 9 then
            state.dofRange = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.DepthOfField.minRange, cfg.DepthOfField.rangeStep)
            ApplyDOF(state)
        elseif idx == 10 then
            state.dofStrength = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.DepthOfField.minStrength, cfg.DepthOfField.strengthStep)
            ApplyDOF(state)
        end
    end

    LevelFuncs.Engine.PhotoMode.OnLightOptionChange = function()
        local m = Menu.Get(MENU_LIGHT)
        if not m then return end
        local idx = m:GetCurrentItemIndex()
        if idx == 1 then state.lightEnabled = IndexToBool(m:GetCurrentOptionIndex())
        elseif idx == 2 then state.lightSource = m:GetCurrentOptionIndex()
        elseif idx == 3 then state.lightRadius = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Light.minRadius, cfg.Light.radiusStep)
        elseif idx == 4 then state.lightColorIndex = m:GetCurrentOptionIndex()
        end
    end

    LevelFuncs.Engine.PhotoMode.OnUIOptionChange = function()
        local m = Menu.Get(MENU_UI)
        if not m then return end
        if m:GetCurrentItemIndex() == 1 then
            state.hideUI = IndexToBool(m:GetCurrentOptionIndex())
        end
    end

    end -- _callbacksRegistered

    -- ================================================================
    -- CAMERA menu
    -- ================================================================
    CreateMenu(MENU_CAMERA, {
        { itemName = "pm_mode",            options = CTRL_MODE_NAMES, currentOption = state.controlMode },
        { itemName = "pm_move_speed",      options = NumberRange(cfg.Camera.minMoveSpeed, cfg.Camera.maxMoveSpeed, cfg.Camera.moveSpeedStep),
          currentOption = ValueToOptionIndex(state.moveSpeed, cfg.Camera.minMoveSpeed, cfg.Camera.moveSpeedStep) },
        { itemName = "pm_look_speed",      options = NumberRange(cfg.Camera.minLookSpeed, cfg.Camera.maxLookSpeed, cfg.Camera.lookSpeedStep,
              function(v) return string.format("%.1f", v) end),
          currentOption = ValueToOptionIndex(state.lookSpeed, cfg.Camera.minLookSpeed, cfg.Camera.lookSpeedStep) },
        { itemName = "pm_collision",       options = BoolOptions(), currentOption = BoolToIndex(state.collisionOn) },
        { itemName = "pm_reset",           options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnCameraAccept", "Engine.PhotoMode.OnCameraOptionChange")

    -- ================================================================
    -- CHARACTER menu
    -- ================================================================
    CreateMenu(MENU_CHARACTER, {
        { itemName = "pm_animation",  options = ANIM_NAMES,       currentOption = state.animIndex },
        { itemName = "pm_outfit",     options = OUTFIT_NAMES,     currentOption = state.outfitIndex },
        { itemName = "pm_weapons",    options = WEAPON_NAMES,     currentOption = state.weaponIndex },
        { itemName = "pm_expression", options = EXPRESSION_NAMES, currentOption = state.expressionIndex },
        { itemName = "pm_sunglasses", options = BoolOptions(),     currentOption = BoolToIndex(state.sunglassesEnabled) },
        { itemName = "pm_gunflash",   options = BoolOptions(),     currentOption = BoolToIndex(state.gunflashEnabled) },
        { itemName = "pm_reset",      options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnCharacterAccept", "Engine.PhotoMode.OnCharacterOptionChange")

    -- ================================================================
    -- EFFECTS menu
    -- ================================================================
    CreateMenu(MENU_EFFECTS, {
        { itemName = "pm_fov",        options = NumberRange(cfg.Lens.minFOV, cfg.Lens.maxFOV, cfg.Lens.fovStep),
          currentOption = ValueToOptionIndex(state.fov, cfg.Lens.minFOV, cfg.Lens.fovStep) },
        { itemName = "pm_roll",       options = NumberRange(cfg.Lens.minRoll, cfg.Lens.maxRoll, cfg.Lens.rollStep),
          currentOption = ValueToOptionIndex(state.roll, cfg.Lens.minRoll, cfg.Lens.rollStep) },
        { itemName = "pm_preset",     options = FILTER_NAMES, currentOption = state.filterIndex },
        { itemName = "pm_strength",   options = NumberRange(0, 1.0, 0.05, function(v) return string.format("%.2f", v) end),
          currentOption = ValueToOptionIndex(state.filterStrength, 0, 0.05) },
        { itemName = "pm_tint",       options = TINT_NAMES, currentOption = state.tintIndex },
        { itemName = "pm_frame_overlay",  options = FRAME_NAMES, currentOption = state.frameIndex },
        { itemName = "pm_dof_mode",        options = DOF_MODE_NAMES, currentOption = state.dofMode },
        { itemName = "pm_dof_focus",       options = NumberRange(cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.maxFocusDistance, cfg.DepthOfField.focusDistanceStep),
          currentOption = ValueToOptionIndex(state.dofFocusDistance, cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.focusDistanceStep) },
        { itemName = "pm_dof_range",       options = NumberRange(cfg.DepthOfField.minRange, cfg.DepthOfField.maxRange, cfg.DepthOfField.rangeStep),
          currentOption = ValueToOptionIndex(state.dofRange, cfg.DepthOfField.minRange, cfg.DepthOfField.rangeStep) },
        { itemName = "pm_dof_strength",    options = NumberRange(cfg.DepthOfField.minStrength, cfg.DepthOfField.maxStrength, cfg.DepthOfField.strengthStep,
              function(v) return string.format("%.2f", v) end),
          currentOption = ValueToOptionIndex(state.dofStrength, cfg.DepthOfField.minStrength, cfg.DepthOfField.strengthStep) },
        { itemName = "pm_reset",           options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnEffectsAccept", "Engine.PhotoMode.OnEffectsOptionChange")

    -- ================================================================
    -- LIGHT menu
    -- ================================================================
    CreateMenu(MENU_LIGHT, {
        { itemName = "pm_enabled",      options = BoolOptions(), currentOption = BoolToIndex(state.lightEnabled) },
        { itemName = "pm_source",       options = LIGHT_SRC_NAMES, currentOption = state.lightSource },
        { itemName = "pm_radius",       options = NumberRange(cfg.Light.minRadius, cfg.Light.maxRadius, cfg.Light.radiusStep),
          currentOption = ValueToOptionIndex(state.lightRadius, cfg.Light.minRadius, cfg.Light.radiusStep) },
        { itemName = "pm_color",        options = COLOR_NAMES, currentOption = state.lightColorIndex },
        { itemName = "pm_place_camera", options = { acceptString }, currentOption = 1 },
        { itemName = "pm_place_lara",   options = { acceptString }, currentOption = 1 },
        { itemName = "pm_reset",        options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnLightAccept", "Engine.PhotoMode.OnLightOptionChange")

    -- ================================================================
    -- UI menu
    -- ================================================================
    CreateMenu(MENU_UI, {
        { itemName = "pm_hide_ui", options = BoolOptions(), currentOption = BoolToIndex(state.hideUI) },
        { itemName = "pm_exit",    options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnUIAccept", "Engine.PhotoMode.OnUIOptionChange")

    -- ================================================================
    -- Set up headers (STEP_LEFT / STEP_RIGHT to navigate)
    -- ================================================================
    Menu.SetHeaders({
        { name = "pm_header_camera",    menuName = MENU_CAMERA },
        { name = "pm_header_character", menuName = MENU_CHARACTER },
        { name = "pm_header_effects",   menuName = MENU_EFFECTS },
        { name = "pm_header_light",     menuName = MENU_LIGHT },
        { name = "pm_header_ui",        menuName = MENU_UI },
    })

    Menu.SetHeaderSpacing(15)
    -- Activate the first header's menu
    Menu.SetActiveHeader(1)
end

-- ============================================================================
-- Light Emission (every frame while active)
-- ============================================================================

local function UpdateLightEmission()
    local state = States.Get()
    if not state.lightEnabled then return end

    local lightPos = state.lightPos

    if state.lightSource == States.LightSource.FOLLOW_CAMERA and state.cameraMesh then
        lightPos = state.cameraMesh:GetPosition()
    elseif state.lightSource == States.LightSource.FOLLOW_LARA then
        local lp = Lara:GetPosition()
        lightPos = TEN.Vec3(lp.x, lp.y - 256, lp.z)
    end

    local lightColor = Settings.Light.colorPresets[state.lightColorIndex].color

    pcall(function()
        TEN.Effects.EmitLight(lightPos, lightColor, state.lightRadius, state.lightShadows, Settings.Light.lightName)
    end)
end

-- ============================================================================
-- Entry / Exit
-- ============================================================================

function PhotoMode.Enter()
    if States.IsActive() then return end

    -- Capture snapshot
    local snap = States.CaptureSnapshot()
    if not snap then return end

    -- Create camera objects
    if not Camera.Init() then return end

    -- Place camera relative to Lara
    Camera.PlaceInitial()

    -- Reset state to defaults
    States.ResetToEntry()
    States.SetActive(true)

    local state = States.Get()
    state.entryFov  = snap.fov
    state.entryRoll = 0
    state.animIndex = 1

    -- Store entry light state
    local camPos = state.cameraMesh:GetPosition()
    state.entryLight = {
        enabled    = state.lightEnabled,
        source     = state.lightSource,
        pos        = TEN.Vec3(camPos.x, camPos.y, camPos.z),
        radius     = state.lightRadius,
        shadows    = state.lightShadows,
        colorIndex = state.lightColorIndex,
    }
    state.lightPos = TEN.Vec3(camPos.x, camPos.y, camPos.z)

    -- Build menus
    BuildAllMenus()

    -- Freeze the game
    TEN.Flow.SetFreezeMode(TEN.Flow.FreezeMode.SPECTATOR)

    -- Attach object camera
    Camera.Attach()

    TEN.Input.ClearAllKeys()
    TEN.Util.PrintLog("PhotoMode: Entered.", TEN.Util.LogLevel.INFO)
end

function PhotoMode.Exit()
    if not States.IsActive() then return end

    -- Restore snapshot
    States.RestoreSnapshot()

    -- Detach camera
    Camera.Detach()

    -- Stop light
    pcall(function()
        local state = States.Get()
        TEN.Effects.EmitLight(state.lightPos, TEN.Color(0, 0, 0), 0, false, Settings.Light.lightName)
    end)

    -- Hide sunglasses
    pcall(function()
        local state = States.Get()
        if state.sunglassesMesh then
            state.sunglassesMesh:SetColor(TEN.Color(255, 255, 255, 0))
            state.sunglassesEnabled = false
        end
    end)

    -- Clean up menus
    Menu.DeleteAll()

    -- Clear frames
    Frames.Clear()

    -- Unfreeze
    TEN.Flow.SetFreezeMode(TEN.Flow.FreezeMode.NONE)

    States.SetActive(false)
    States.Get().snapshot = nil
    States.Get().entryHoldCount = 0

    TEN.Input.ClearAllKeys()
    TEN.Util.PrintLog("PhotoMode: Exited.", TEN.Util.LogLevel.INFO)
end

function PhotoMode.IsActive()
    return States.IsActive()
end

-- ============================================================================
-- Header drawing position
-- ============================================================================

local HEADER_POS   = TEN.Vec2(50, 15)
local HEADER_SCALE = 1.0

-- ============================================================================
-- Callbacks
-- ============================================================================

LevelFuncs.Engine.PhotoMode.OnLoop = function()
    if States.IsActive() then return end

    if _photoModeExited then
        TEN.Input.ClearAllKeys()
        _photoModeExited = false
    end

    local state = States.Get()
    local walkHeld = TEN.Input.IsKeyHeld(TEN.Input.ActionID.Q)
    local invHeld  = TEN.Input.IsKeyHeld(TEN.Input.ActionID.INVENTORY)

    if walkHeld or invHeld then
        state.entryHoldCount = state.entryHoldCount + 1
        if state.entryHoldCount >= Settings.Entry.holdFrames then
            state.entryHoldCount = 0
            TEN.Input.ClearAllKeys()
            PhotoMode.Enter()
        end
    else
        state.entryHoldCount = 0
    end
end

LevelFuncs.Engine.PhotoMode.OnFreeze = function()
    if not States.IsActive() then return end

    local state = States.Get()

    -- Toggle UI with LOOK key
    if TEN.Input.IsKeyHit(TEN.Input.ActionID.LOOK) then
        state.hideUI = not state.hideUI
    end

    -- Exit with Inventory key (always available)
    if TEN.Input.IsKeyHit(TEN.Input.ActionID.INVENTORY) then
        PhotoMode.Exit()
        _photoModeExited = true
        return
    end

    if state.hideUI then
        -- UI hidden: movement controls only
        Input.Update()
    else
        -- UI visible: menu handles input (includes header nav via STEP_LEFT/RIGHT)
        Menu.UpdateActiveMenus()
    end

    if not States.IsActive() then return end

    -- Attach camera every frame
    Camera.Attach()

    -- Emit light
    UpdateLightEmission()

    -- Update sunglasses position to follow joint
    UpdateSunglasses(state)

    -- Emit gun flash if enabled
    UpdateGunFlash(state)

    -- Update and draw frames
    Frames.Update()
    Frames.Draw()

    -- Draw UI (menus + headers) unless hidden
    if not state.hideUI then
        -- Draw header bar
        local headerAlpha = 255
        -- Use the alpha from the active menu for consistency
        local activeMenuName = Menu.GetActiveHeaderMenu()
        if activeMenuName then
            local m = Menu.Get(activeMenuName)
            if m and m.IsVisible and not m:IsVisible() then
                headerAlpha = 0
            end
        end
        Menu.DrawHeaders(HEADER_POS, HEADER_SCALE, headerAlpha)
        Menu.DrawActiveMenus()

        -- Draw mode indicator at top
        local modeText = TEN.Flow.GetString("pm_mode_prefix") .. States.GetModeName()
        local modePos  = TEN.Util.PercentToScreen(TEN.Vec2(50, 10))
        local modeStr  = TEN.Strings.DisplayString(
            modeText, modePos, 0.9,
            Settings.ColorMap.dimmed, false,
            { Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER }
        )
        TEN.Strings.ShowString(modeStr, 1 / 30)

        -- Draw control hint at bottom
        local helpPos  = TEN.Util.PercentToScreen(TEN.Vec2(50, 92))
        local helpStr  = TEN.Strings.DisplayString(
            "pm_help", helpPos, 0.65,
            Settings.ColorMap.dimmed, true,
            { Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER }
        )
        TEN.Strings.ShowString(helpStr, 1 / 30)
    end
end

-- ============================================================================
-- Register Callbacks
-- ============================================================================

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.POSTLOOP,  LevelFuncs.Engine.PhotoMode.OnLoop)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PREFREEZE, LevelFuncs.Engine.PhotoMode.OnFreeze)

return PhotoMode
