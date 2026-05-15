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
local InputHelpers = require("Engine.PhotoMode.InputHelpers")
local Menu     = require("Engine.PhotoMode.Menu")
local Settings = require("Engine.PhotoMode.Settings")
local States   = require("Engine.PhotoMode.States")
require("Engine.PhotoMode.Strings")

LevelFuncs.Engine.PhotoMode = LevelFuncs.Engine.PhotoMode or {}

local PhotoMode = {}

-- Guards LevelFuncs registration to once per Lua session (resets on level/savegame reload).
local _callbacksRegistered = false
local _photoModeExited = false
local _spriteAnim      = {}   -- per-sprite lerp state: { sizeW, sizeH, r, g, b }

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

local function ColorCombine(color, alpha)
    return TEN.Color(color.r, color.g, color.b, alpha)
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
local LIGHT_SRC_NAMES  = Settings.Light.sourceNames
local FILTER_NAMES     = BuildNames(Settings.Filters.presets)
local TINT_NAMES       = BuildNames(Settings.Filters.tints)
local COLOR_NAMES      = BuildNames(Settings.Light.colorPresets)
local ANIM_NAMES       = BuildNames(Settings.Animations)
local FRAME_NAMES      = BuildNames(Settings.Frames.presets)
local EXPRESSION_NAMES = BuildNames(Settings.Expressions)

-- Outfit and weapon name lists are built dynamically each entry to respect
-- per-outfit unlock flags and live inventory checks.
local _outfitMenuMap        = {}  -- [menuOptionIdx] -> real Settings.Outfits index
local _outfitMenuMapReverse = {}  -- [real Settings.Outfits index] -> menuOptionIdx
local _weaponMenuMap        = {}  -- [menuOptionIdx] -> real Settings.Weapons index
local _weaponMenuMapReverse = {}  -- [real Settings.Weapons index] -> menuOptionIdx

local function BuildFilteredOutfitNames()
    _outfitMenuMap        = {}
    _outfitMenuMapReverse = {}
    local names = {}
    for i, outfit in ipairs(Settings.Outfits) do
        if outfit.unlocked ~= false then
            local idx             = #names + 1
            names[idx]            = outfit.name
            _outfitMenuMap[idx]   = i
            _outfitMenuMapReverse[i] = idx
        end
    end
    return names
end

local function BuildFilteredWeaponNames()
    _weaponMenuMap        = {}
    _weaponMenuMapReverse = {}
    local names = {}
    for i, weapon in ipairs(Settings.Weapons) do
        local show = false
        if weapon.name == "Default" then
            show = true  -- Default always shown.
        elseif weapon.pickupObjID == nil then
            show = true  -- No inventory check configured.
        else
            local ok, count = pcall(TEN.Inventory.GetItemCount, weapon.pickupObjID)
            show = ok and count > 0
        end
        if show then
            local idx             = #names + 1
            names[idx]            = weapon.name
            _weaponMenuMap[idx]   = i
            _weaponMenuMapReverse[i] = idx
        end
    end
    return names
end

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
    if Settings.Sunglasses.enabled == false then return nil end
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

local MENU_CHARACTER = "pm_character"
local MENU_EFFECTS   = "pm_effects"
local MENU_FILTERS   = "pm_filters"
local MENU_LIGHT     = "pm_light"
local MENU_UI        = "pm_ui"

-- Maps the active header's menu name to the input control mode used when UI is hidden.
local HEADER_CONTROL_MODE = {
    [MENU_CHARACTER] = States.Mode.PLAYER,
    [MENU_EFFECTS]   = States.Mode.CAMERA,
    [MENU_FILTERS]   = States.Mode.CAMERA,
    [MENU_LIGHT]     = States.Mode.LIGHT,
    [MENU_UI]        = States.Mode.CAMERA,
}

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
    return { "Off", "On" }
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
    local function CreateMenu(menuName, items, acceptFunc, optionChangeFunc, titleText)
        local menu = Menu.Create(menuName, "", items,
            acceptFunc, "Engine.PhotoMode.OnExit", Menu.Type.ITEMS_AND_OPTIONS)
        menu:SetItemsPosition(Vec2(2, 23))
        menu:SetItemsFont(nil, 0.6)
        menu:SetOptionsFont(nil, 0.6)
        menu:SetLineSpacing(3)
        menu:SetOptionsPosition(Vec2(23, 23))
        menu:SetTitle(titleText, nil, 0.6, nil, true)
        menu:SetTitlePosition(Vec2(16, 19))
        menu:SetItemsTranslate(true)
        menu:SetWrapAroundItems(false)
        menu:SetWrapAroundOptions(false)
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

    LevelFuncs.Engine.PhotoMode.OnCharacterAccept = function()
        local m = Menu.Get(MENU_CHARACTER)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_reset" then
            ResetCharacter()
            m:SetOptionIndexForItemName("pm_animation",  state.animIndex)
            m:SetOptionIndexForItemName("pm_outfit",     _outfitMenuMapReverse[state.outfitIndex] or 1)
            m:SetOptionIndexForItemName("pm_weapons",    _weaponMenuMapReverse[state.weaponIndex] or 1)
            m:SetOptionIndexForItemName("pm_expression", state.expressionIndex)
            m:SetOptionIndexForItemName("pm_sunglasses", BoolToIndex(state.sunglassesEnabled))
            m:SetOptionIndexForItemName("pm_gunflash",   BoolToIndex(state.gunflashEnabled))
        end
    end

    LevelFuncs.Engine.PhotoMode.OnEffectsAccept = function()
        local m = Menu.Get(MENU_EFFECTS)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_reset" then
            ResetEffects()
            ResetCamera()
            m:SetOptionIndexForItemName("pm_fov",          ValueToOptionIndex(state.fov, cfg.Lens.minFOV, cfg.Lens.fovStep))
            m:SetOptionIndexForItemName("pm_roll",         ValueToOptionIndex(state.roll, cfg.Lens.minRoll, cfg.Lens.rollStep))
            m:SetOptionIndexForItemName("pm_dof_mode",     state.dofMode)
            m:SetOptionIndexForItemName("pm_dof_focus",    ValueToOptionIndex(state.dofFocusDistance, cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.focusDistanceStep))
            m:SetOptionIndexForItemName("pm_dof_range",    ValueToOptionIndex(state.dofRange,         cfg.DepthOfField.minRange,         cfg.DepthOfField.rangeStep))
            m:SetOptionIndexForItemName("pm_dof_strength", ValueToOptionIndex(state.dofStrength,      cfg.DepthOfField.minStrength,      cfg.DepthOfField.strengthStep))
            local mf = Menu.Get(MENU_FILTERS)
            if mf then
                mf:SetOptionIndexForItemName("pm_preset",        state.filterIndex)
                mf:SetOptionIndexForItemName("pm_strength",      ValueToOptionIndex(state.filterStrength, 0, 0.05))
                mf:SetOptionIndexForItemName("pm_tint",          state.tintIndex)
                mf:SetOptionIndexForItemName("pm_frame_overlay", state.frameIndex)
            end
        end
    end

    LevelFuncs.Engine.PhotoMode.OnLightAccept = function()
        local m = Menu.Get(MENU_LIGHT)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_place_camera" then PlaceLightAtCamera()
        elseif name == "pm_place_lara" then PlaceLightAtLara()
        elseif name == "pm_reset" then
            ResetLight()
            m:SetOptionIndexForItemName("pm_enabled", BoolToIndex(state.lightEnabled))
            m:SetOptionIndexForItemName("pm_source",  state.lightSource)
            m:SetOptionIndexForItemName("pm_radius",  ValueToOptionIndex(state.lightRadius, cfg.Light.minRadius, cfg.Light.radiusStep))
            m:SetOptionIndexForItemName("pm_color",   state.lightColorIndex)
        end
    end

    LevelFuncs.Engine.PhotoMode.OnUIAccept = function()
        local m = Menu.Get(MENU_UI)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_exit" then PhotoMode.Exit() end
    end

    -- ================================================================
    -- Option change callbacks (sync state when left/right changes value)
    -- ================================================================

    LevelFuncs.Engine.PhotoMode.OnCharacterOptionChange = function()
        local m = Menu.Get(MENU_CHARACTER)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_animation" then
            state.animIndex = m:GetCurrentOptionIndex()
            ApplyPosePreset(state)
        elseif name == "pm_outfit" then
            state.outfitIndex = _outfitMenuMap[m:GetCurrentOptionIndex()] or 1
            ApplyOutfit(state)
        elseif name == "pm_weapons" then
            state.weaponIndex = _weaponMenuMap[m:GetCurrentOptionIndex()] or 1
            ApplyWeapon(state)
        elseif name == "pm_expression" then
            state.expressionIndex = m:GetCurrentOptionIndex()
            ApplyExpression(state)
        elseif name == "pm_sunglasses" then
            state.sunglassesEnabled = IndexToBool(m:GetCurrentOptionIndex())
            ApplySunglasses(state)
        elseif name == "pm_gunflash" then
            state.gunflashEnabled = IndexToBool(m:GetCurrentOptionIndex())
        end
    end

    LevelFuncs.Engine.PhotoMode.OnEffectsOptionChange = function()
        local m = Menu.Get(MENU_EFFECTS)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_fov" then
            state.fov = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Lens.minFOV, cfg.Lens.fovStep)
            ApplyFOV(state)
        elseif name == "pm_roll" then
            state.roll = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Lens.minRoll, cfg.Lens.rollStep)
            ApplyRoll(state)
        elseif name == "pm_dof_mode" then
            state.dofMode = m:GetCurrentOptionIndex()
            ApplyDOF(state)
        elseif name == "pm_dof_focus" then
            state.dofFocusDistance = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.focusDistanceStep)
            ApplyDOF(state)
        elseif name == "pm_dof_range" then
            state.dofRange = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.DepthOfField.minRange, cfg.DepthOfField.rangeStep)
            ApplyDOF(state)
        elseif name == "pm_dof_strength" then
            state.dofStrength = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.DepthOfField.minStrength, cfg.DepthOfField.strengthStep)
            ApplyDOF(state)
        end
    end

    LevelFuncs.Engine.PhotoMode.OnLightOptionChange = function()
        local m = Menu.Get(MENU_LIGHT)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if     name == "pm_enabled" then state.lightEnabled    = IndexToBool(m:GetCurrentOptionIndex())
        elseif name == "pm_source"  then state.lightSource     = m:GetCurrentOptionIndex()
        elseif name == "pm_radius"  then state.lightRadius     = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Light.minRadius, cfg.Light.radiusStep)
        elseif name == "pm_color"   then state.lightColorIndex = m:GetCurrentOptionIndex()
        end
    end

    LevelFuncs.Engine.PhotoMode.OnUIOptionChange = function()
        local m = Menu.Get(MENU_UI)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_hide_ui" then
            state.hideUI = IndexToBool(m:GetCurrentOptionIndex())
        end
    end

    LevelFuncs.Engine.PhotoMode.OnFiltersAccept = function()
        local m = Menu.Get(MENU_FILTERS)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_reset" then
            ResetEffects()
            m:SetOptionIndexForItemName("pm_preset",        state.filterIndex)
            m:SetOptionIndexForItemName("pm_strength",      ValueToOptionIndex(state.filterStrength, 0, 0.05))
            m:SetOptionIndexForItemName("pm_tint",          state.tintIndex)
            m:SetOptionIndexForItemName("pm_frame_overlay", state.frameIndex)
            local me = Menu.Get(MENU_EFFECTS)
            if me then
                me:SetOptionIndexForItemName("pm_fov",          ValueToOptionIndex(state.fov, cfg.Lens.minFOV, cfg.Lens.fovStep))
                me:SetOptionIndexForItemName("pm_roll",         ValueToOptionIndex(state.roll, cfg.Lens.minRoll, cfg.Lens.rollStep))
                me:SetOptionIndexForItemName("pm_dof_mode",     state.dofMode)
                me:SetOptionIndexForItemName("pm_dof_focus",    ValueToOptionIndex(state.dofFocusDistance, cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.focusDistanceStep))
                me:SetOptionIndexForItemName("pm_dof_range",    ValueToOptionIndex(state.dofRange,         cfg.DepthOfField.minRange,         cfg.DepthOfField.rangeStep))
                me:SetOptionIndexForItemName("pm_dof_strength", ValueToOptionIndex(state.dofStrength,      cfg.DepthOfField.minStrength,      cfg.DepthOfField.strengthStep))
            end
        end
    end

    LevelFuncs.Engine.PhotoMode.OnFiltersOptionChange = function()
        local m = Menu.Get(MENU_FILTERS)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_preset" then
            state.filterIndex = m:GetCurrentOptionIndex()
            ApplyFilter(state)
        elseif name == "pm_strength" then
            state.filterStrength = OptionIndexToValue(m:GetCurrentOptionIndex(), 0, 0.05)
            ApplyFilterStrength(state)
        elseif name == "pm_tint" then
            state.tintIndex = m:GetCurrentOptionIndex()
            ApplyTint(state)
        elseif name == "pm_frame_overlay" then
            state.frameIndex = m:GetCurrentOptionIndex()
        end
    end

    end -- _callbacksRegistered

    -- ================================================================
    -- CHARACTER menu
    -- ================================================================
    local outfitNames = BuildFilteredOutfitNames()
    local weaponNames = BuildFilteredWeaponNames()
    local characterItems = {
        { itemName = "pm_animation",  options = ANIM_NAMES,       currentOption = state.animIndex },
        { itemName = "pm_outfit",     options = outfitNames,      currentOption = _outfitMenuMapReverse[state.outfitIndex] or 1 },
        { itemName = "pm_weapons",    options = weaponNames,      currentOption = _weaponMenuMapReverse[state.weaponIndex] or 1 },
        { itemName = "pm_expression", options = EXPRESSION_NAMES, currentOption = state.expressionIndex },
    }
    if Settings.Sunglasses.enabled ~= false then
        characterItems[#characterItems + 1] = { itemName = "pm_sunglasses", options = BoolOptions(), currentOption = BoolToIndex(state.sunglassesEnabled) }
    end
    characterItems[#characterItems + 1] = { itemName = "pm_gunflash", options = BoolOptions(),    currentOption = BoolToIndex(state.gunflashEnabled) }
    characterItems[#characterItems + 1] = { itemName = "pm_reset",    options = { acceptString }, currentOption = 1 }
    CreateMenu(MENU_CHARACTER, characterItems,
        "Engine.PhotoMode.OnCharacterAccept", "Engine.PhotoMode.OnCharacterOptionChange", "pm_header_character")

    -- ================================================================
    -- EFFECTS menu (Lens + Depth of Field)
    -- ================================================================
    CreateMenu(MENU_EFFECTS, {
        { itemName = "pm_fov",         options = NumberRange(cfg.Lens.minFOV, cfg.Lens.maxFOV, cfg.Lens.fovStep),
          currentOption = ValueToOptionIndex(state.fov, cfg.Lens.minFOV, cfg.Lens.fovStep), accelerated = true },
        { itemName = "pm_roll",        options = NumberRange(cfg.Lens.minRoll, cfg.Lens.maxRoll, cfg.Lens.rollStep),
          currentOption = ValueToOptionIndex(state.roll, cfg.Lens.minRoll, cfg.Lens.rollStep), accelerated = true },
        { itemName = "pm_dof_mode",    options = DOF_MODE_NAMES, currentOption = state.dofMode },
        { itemName = "pm_dof_focus",   options = NumberRange(cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.maxFocusDistance, cfg.DepthOfField.focusDistanceStep),
          currentOption = ValueToOptionIndex(state.dofFocusDistance, cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.focusDistanceStep), accelerated = true },
        { itemName = "pm_dof_range",   options = NumberRange(cfg.DepthOfField.minRange, cfg.DepthOfField.maxRange, cfg.DepthOfField.rangeStep),
          currentOption = ValueToOptionIndex(state.dofRange, cfg.DepthOfField.minRange, cfg.DepthOfField.rangeStep), accelerated = true },
        { itemName = "pm_dof_strength", options = NumberRange(cfg.DepthOfField.minStrength, cfg.DepthOfField.maxStrength, cfg.DepthOfField.strengthStep,
              function(v) return string.format("%.2f", v) end),
          currentOption = ValueToOptionIndex(state.dofStrength, cfg.DepthOfField.minStrength, cfg.DepthOfField.strengthStep), accelerated = true },
        { itemName = "pm_reset",       options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnEffectsAccept", "Engine.PhotoMode.OnEffectsOptionChange", "pm_header_effects")

    -- ================================================================
    -- FILTERS menu (Post-process + Frame)
    -- ================================================================
    CreateMenu(MENU_FILTERS, {
        { itemName = "pm_preset",        options = FILTER_NAMES, currentOption = state.filterIndex },
        { itemName = "pm_strength",      options = NumberRange(0, 1.0, 0.05, function(v) return string.format("%.2f", v) end),
          currentOption = ValueToOptionIndex(state.filterStrength, 0, 0.05), accelerated = true },
        { itemName = "pm_tint",          options = TINT_NAMES, currentOption = state.tintIndex },
        { itemName = "pm_frame_overlay", options = FRAME_NAMES, currentOption = state.frameIndex },
        { itemName = "pm_reset",         options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnFiltersAccept", "Engine.PhotoMode.OnFiltersOptionChange", "pm_header_filters")

    -- ================================================================
    -- LIGHT menu
    -- ================================================================
    CreateMenu(MENU_LIGHT, {
        { itemName = "pm_enabled",      options = BoolOptions(), currentOption = BoolToIndex(state.lightEnabled) },
        { itemName = "pm_source",       options = LIGHT_SRC_NAMES, currentOption = state.lightSource },
        { itemName = "pm_radius",       options = NumberRange(cfg.Light.minRadius, cfg.Light.maxRadius, cfg.Light.radiusStep),
          currentOption = ValueToOptionIndex(state.lightRadius, cfg.Light.minRadius, cfg.Light.radiusStep), accelerated = true },
        { itemName = "pm_color",        options = COLOR_NAMES, currentOption = state.lightColorIndex },
        { itemName = "pm_place_camera", options = { acceptString }, currentOption = 1 },
        { itemName = "pm_place_lara",   options = { acceptString }, currentOption = 1 },
        { itemName = "pm_reset",        options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnLightAccept", "Engine.PhotoMode.OnLightOptionChange", "pm_header_light")

    -- ================================================================
    -- UI menu
    -- ================================================================
    CreateMenu(MENU_UI, {
        { itemName = "pm_hide_ui", options = BoolOptions(), currentOption = BoolToIndex(state.hideUI) },
        { itemName = "pm_exit",    options = { acceptString }, currentOption = 1 },
    }, "Engine.PhotoMode.OnUIAccept", "Engine.PhotoMode.OnUIOptionChange", "pm_header_ui")

    -- ================================================================
    -- Set up headers (STEP_LEFT / STEP_RIGHT to navigate)
    -- ================================================================
    Menu.SetHeaders({
        { name = "",   menuName = MENU_EFFECTS, hideText = true },
        { name = "", menuName = MENU_CHARACTER, hideText = true },
        { name = "",     menuName = MENU_LIGHT, hideText = true },
        { name = "",   menuName = MENU_FILTERS, hideText = true },
        { name = "",        menuName = MENU_UI, hideText = true },
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

    -- Reset sprite animation state so dots snap to correct positions on entry
    _spriteAnim = {}

    -- Build menus
    BuildAllMenus()

    -- Freeze the game
    TEN.Flow.SetFreezeMode(TEN.Flow.FreezeMode.SPECTATOR)

    -- Attach object camera
    Camera.Attach()

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
    States.Get().timeInPhotoMode = 0

    TEN.Input.ClearAllKeys()
    TEN.Util.PrintLog("PhotoMode: Exited.", TEN.Util.LogLevel.INFO)
end

function PhotoMode.IsActive()
    return States.IsActive()
end

-- ============================================================================
-- Header drawing position
-- ============================================================================

local HEADER_POS        = TEN.Vec2(50, 15)
local HEADER_SCALE      = 1.0
local SPRITE_ANIM_SPEED = 0.18  -- lerp factor per frame (higher = snappier)

-- ============================================================================
-- Header Sprites
-- ============================================================================

local function DrawHeaderSprites(alpha)
    local cfg = Settings.HeaderSprites
    if not cfg or not cfg.spriteIDs or alpha < 1 then return end
    local activeIdx  = Menu.GetActiveHeaderIndex()
    local count      = #cfg.spriteIDs
    local spacing    = cfg.spacing or 6
    local totalWidth = (count - 1) * spacing
    local posX       = cfg.position and cfg.position.x or 50
    local posY       = cfg.position and cfg.position.y or 21
    local startX     = posX - totalWidth / 2
    local sizeA  = cfg.sizeActive    or TEN.Vec2(5, 5)
    local sizeI  = cfg.sizeInactive  or TEN.Vec2(4, 4)
    local colorA = cfg.colorActive   or TEN.Color(255, 255, 255)
    local colorI = cfg.colorInactive or TEN.Color(100, 100, 100)
    local spd = SPRITE_ANIM_SPEED
    for i, spriteID in ipairs(cfg.spriteIDs) do
        local isActive = (i == activeIdx)
        local tW = isActive and sizeA.x  or sizeI.x
        local tH = isActive and sizeA.y  or sizeI.y
        local tR = isActive and colorA.r or colorI.r
        local tG = isActive and colorA.g or colorI.g
        local tB = isActive and colorA.b or colorI.b
        -- Initialize on first use (snap to target, no pop)
        local anim = _spriteAnim[i]
        if not anim then
            anim = { sizeW = tW, sizeH = tH, r = tR, g = tG, b = tB }
            _spriteAnim[i] = anim
        end
        -- Lerp toward target
        anim.sizeW = anim.sizeW + (tW - anim.sizeW) * spd
        anim.sizeH = anim.sizeH + (tH - anim.sizeH) * spd
        anim.r     = anim.r     + (tR - anim.r)     * spd
        anim.g     = anim.g     + (tG - anim.g)     * spd
        anim.b     = anim.b     + (tB - anim.b)     * spd
        local x     = startX + (i - 1) * spacing
        local size  = TEN.Vec2(anim.sizeW, anim.sizeH)
        local color = TEN.Color(math.floor(anim.r), math.floor(anim.g), math.floor(anim.b), math.floor(alpha))
        local ok, sprite = pcall(TEN.View.DisplaySprite,
            cfg.objectID, spriteID, TEN.Vec2(x, posY), cfg.rotation or 0, size, color)
        if ok and sprite then
            sprite:Draw(cfg.layer or -4,
                cfg.alignMode or TEN.View.AlignMode.CENTER,
                cfg.scaleMode or TEN.View.ScaleMode.FIT,
                cfg.blendMode or TEN.Effects.BlendID.ALPHA_BLEND)
        end
    end
end

local function DrawBackSprites(alpha)

    local color = ColorCombine(Settings.ColorMap.dimmed, math.floor(alpha))
    local ok, sprite = pcall(TEN.View.DisplaySprite,
        TEN.Objects.ObjID.DIARY_SPRITES, 5, TEN.Vec2(1.5, 11), 0, TEN.Vec2(29, 38.5), color)
    if ok and sprite then
        sprite:Draw(-4,
            TEN.View.AlignMode.TOP_LEFT,
            TEN.View.ScaleMode.STRETCH,
            TEN.Effects.BlendID.ALPHA_BLEND)
    end
end

local function DrawTitle(alpha)
    local modeText = TEN.Flow.GetString("photo_mode")
    local modePos  = TEN.Util.PercentToScreen(TEN.Vec2(16, 9))
    local modeStr  = TEN.Strings.DisplayString(
        modeText, modePos, 0.8,
        ColorCombine(Settings.ColorMap.headerText, alpha), false,
        { Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER, Strings.DisplayStringOption.VERTICAL_CENTER }
    )
    TEN.Strings.ShowString(modeStr, 1 / 30)
end

local function DrawModeText(alpha)
    local modeText = TEN.Flow.GetString("pm_mode_prefix") .. States.GetModeName()
    local modePos  = TEN.Util.PercentToScreen(TEN.Vec2(16, 46))
    local modeStr  = TEN.Strings.DisplayString(
        modeText, modePos, 0.6,
        ColorCombine(Settings.ColorMap.neutral, alpha), false,
        { Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER }
    )
    TEN.Strings.ShowString(modeStr, 1 / 30)
end

local function DrawHelpText(alpha)
    -- Draw control hint at bottom
    local helpPos  = TEN.Util.PercentToScreen(TEN.Vec2(50, 92))
    local helpStr  = TEN.Strings.DisplayString(
        "pm_help", helpPos, 0.65,
        ColorCombine(Settings.ColorMap.neutral, alpha), true,
        { Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER }
    )
    TEN.Strings.ShowString(helpStr, 1 / 30)
end

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
    local walkHeld = TEN.Input.IsKeyHeld(TEN.Input.ActionID.WALK)
    local lookHeld  = TEN.Input.IsKeyHeld(TEN.Input.ActionID.LOOK)

    if walkHeld and lookHeld then
        state.entryHoldCount = state.entryHoldCount + 1
        if state.entryHoldCount >= Settings.Entry.holdFrames then
            state.entryHoldCount = 0
            PhotoMode.Enter()
        end
    else
        state.entryHoldCount = 0
    end
end

LevelFuncs.Engine.PhotoMode.OnFreeze = function()
    if not States.IsActive() then return end

    local state = States.Get()

    state.timeInPhotoMode = state.timeInPhotoMode + 1

    -- Toggle UI with LOOK key
    if InputHelpers.GuiIsPulsed(TEN.Input.ActionID.LOOK, state.timeInPhotoMode) then
        state.hideUI = not state.hideUI
    end

    -- Exit with Inventory key (always available)
    if TEN.Input.IsKeyHit(TEN.Input.ActionID.INVENTORY) then
        PhotoMode.Exit()
        _photoModeExited = true
        return
    end

    -- Derive control mode from which header tab is active.
    local activeMenu = Menu.GetActiveHeaderMenu()
    state.controlMode = HEADER_CONTROL_MODE[activeMenu] or States.Mode.CAMERA

    if state.hideUI then
        -- UI hidden: movement controls only (mode determined by active tab)
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
        DrawBackSprites(headerAlpha)
        DrawHeaderSprites(headerAlpha)
        DrawModeText(headerAlpha)
        DrawHelpText(headerAlpha)
        DrawTitle(headerAlpha)
        Menu.DrawHeaders(HEADER_POS, HEADER_SCALE, headerAlpha)
        Menu.DrawActiveMenus()

    end
end

--- Unlock a named outfit so it appears in the photo mode outfit selector.
-- @param name  The outfit name string as defined in Settings.Outfits.
function PhotoMode.UnlockOutfit(name)
    for _, outfit in ipairs(Settings.Outfits) do
        if outfit.name == name then
            outfit.unlocked = true
            return
        end
    end
end

-- ============================================================================
-- Register Callbacks
-- ============================================================================

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.POSTLOOP,  LevelFuncs.Engine.PhotoMode.OnLoop)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PREFREEZE, LevelFuncs.Engine.PhotoMode.OnFreeze)

return PhotoMode
