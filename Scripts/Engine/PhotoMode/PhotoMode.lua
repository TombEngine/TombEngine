---
-- PhotoMode
-- An in-game photo mode for Tomb Engine.
--
-- The player freezes the game, moves a free camera, adjusts Lara's pose and
-- appearance, sets up a light, applies post-process filters, and can overlay
-- frame sprites before taking an in-game screenshot.
--
-- All user-facing content (poses, outfits, expressions, accessories, frames)
-- lives in small data files inside Engine/PhotoMode/ so you can extend the
-- system without touching this file.
--
-- In your level script, require this module once:
-- <br><br>
-- <i>local PhotoMode = require("Engine.PhotoMode.PhotoMode")</i>
-- <br><br>
-- The module self-registers its POSTLOOP and PREFREEZE callbacks, so no further wiring is needed.
--
-- Make sure the following WAD objects exist in your wad:
-- <br><br>
--        PHOTOMODE_SPRITES  — sprite sheet for menus
--        PHOTOMODE_FRAMES   — frame overlay sprite sheet
--        PHOTOMODE_ANIMS    — object that holds all custom photo-mode poses
--        CAMERA_TARGET      — used internally to drive the object camera
-- 
-- It is possible to change settings on a per-level basis via @{PhotoMode.GetSettings} and @{PhotoMode.SetSettings} functions, but keep in mind that
-- _Settings.lua is reread every time the level is reloaded_. Therefore, you need to implement custom settings management in your level script
-- if you want to override global settings.
--
-- <h3><b>Controls</b></h3>
-- The active header tab (cycle with Q / E or shoulder buttons) determines which
-- subject (camera, character or light) the movement controls apply to.  The controls remain functional even when the UI is hidden.
--
-- <table class="function_list">
-- <tr>
--   <th>Action</th>
--   <th>Keyboard</th>
--   <th>Controller</th>
-- </tr>
-- <tr>
--   <td>Enter Photo Mode</td>
--   <td>F3</td>
--   <td>Press Left Stick + Right Stick</td>
-- </tr>
-- <tr>
--   <td>Move forward / back</td>
--   <td>W / S</td>
--   <td>Left stick Y</td>
-- </tr>
-- <tr>
--   <td>Strafe left / right</td>
--   <td>A / D</td>
--   <td>Left stick X</td>
-- </tr>
-- <tr>
--   <td>Rotate camera view</td>
--   <td>Mouse (move freely)</td>
--   <td>Right stick</td>
-- </tr>
-- <tr>
--   <td>Rotate character Y</td>
--   <td>Mouse X</td>
--   <td>Right stick X</td>
-- </tr>
-- <tr>
--   <td>Move subject vertically</td>
--   <td>Hold RMB + Mouse Y</td>
--   <td>Hold RT + Right stick Y</td>
-- </tr>
-- <tr>
--   <td>Navigate menu items</td>
--   <td>Up / Down</td>
--   <td>D-pad </td>
-- </tr>
-- <tr>
--   <td>Change option value</td>
--   <td>Left / Right</td>
--   <td>D-pad Left / Right</td>
-- </tr>
-- <tr>
--   <td>Confirm (Accept)</td>
--   <td>Enter / Action</td>
--   <td>Cross/A</td>
-- </tr>
-- <tr>
--   <td>Switch tabs</td>
--   <td>Q / E</td>
--   <td>LS / RS</td>
-- </tr>
-- <tr>
--   <td>Toggle UI visibility</td>
--   <td>Look (NumPad 0)</td>
--   <td>LT</td>
-- </tr>
-- <tr>
--   <td>Exit Photo Mode</td>
--   <td>Inventory (Escape)</td>
--   <td>Circle/B</td>
-- </tr>
-- </table>
--
-- @luautil PhotoMode

--- Accessories
-- @section Accessories
-- Accessories are drawn on top of Lara. They are defined in <i>Engine/PhotoMode/Accessories.lua</i> file. To add new accessories add a row to the accessory presets table in that file. The first entry should always be a "None" sentinel (objID = nil, meshIndices = {}) so the player can clear accessories.
-- To hide the Accessory option entirely set Settings.Character.accessoriesEnabled = false via @{PhotoMode.SetSettings}.
-- @usage
--    {
--      name = "None",  objID = nil, meshIndices = {}
--    },
--    {
--      name = "Sunglasses",
--      objID       = TEN.Objects.ObjID.ACTOR1_SPEECH_HEAD1,
--      meshIndices = { 14 }, 
--    },
--    { 
--      name = "Beret",
--      objID       = TEN.Objects.ObjID.ANIMATING5,
--      meshIndices = { 14 },
--    }
--

--- Display name shown in the selector.
-- @tfield string name Display name for this acccessory.

--- The object to source the accessory meshes from.
-- @tfield Objects.ObjID objID Object that provides the accessory meshes.

--- Mesh slot indices on the accessory moveable to make visible.
-- @tfield table meshIndices Array of mesh slot indices to display on the accessory moveable.
-- @usage
-- meshIndices = { 0, 4, 9 } — only the listed slot indices are shown

--- Expressions
-- @section Expressions
-- Expressions swap one or more of Lara's classic mesh slots with meshes sourced from another object. Use multiple indices to swap more than one mesh at once. They are defined in <i>Engine/PhotoMode/Expressions.lua</i> file.
-- @usage
--    {
--      name = "Default", objID = nil, meshIndices = {}
--    },
--    {
--      name = "Scream",
--      objID       = TEN.Objects.ObjID.LARA_SCREAM,
--      meshIndices = { 14 },   -- head slot
--    }

--- Display name shown in the selector.
-- @tfield string name Display name for this expression.

--- The object to source swapped meshes from (nil restores Lara's default expression).
-- @tfield Objects.ObjID objID Object to source the swapped meshes from.

--- Mesh slot indices on Lara to swap.
-- @tfield table meshIndices table of mesh slot indices to swap. Use multiple indices to swap more than one mesh at once.
-- @usage
-- meshIndices = { 0, 4, 9 } — only the listed slot indices are swapped

--- Frames
-- @section Frames
-- Frames are full-screen sprites drawn from the PHOTOMODE_FRAMES object. A spriteID of -1 means "no frame". The first entry should always be "None". They are defined in <i>Engine/PhotoMode/Frames.lua</i> file.
-- @usage
--    { name = "None",            spriteID = -1 },
--    { name = "Cinematic Bars",  spriteID = 0 },
--    { name = "My Custom Frame", spriteID = 6, scaleMode = TEN.View.ScaleMode.FIT },

--- Display name shown in the Frames selector.
-- @tfield string name Display name for this frame overlay preset.

--- Sprite index in the PHOTOMODE_FRAMES object (-1 for no frame).
-- @tfield int spriteID Sprite index from PHOTOMODE_FRAMES. Use -1 for no overlay.

--- Scale mode to set for the frame overlay.
-- @tfield[opt=TEN.View.ScaleMode.STRETCH] View.ScaleMode scaleMode Scale mode for the frame overlay.

--- Outfits
-- @section Outfits
-- Outfits can change Lara's Outfit. Both the classic skins and skinned mesh can be used.  They are defined in <i>Engine/PhotoMode/Outfits.lua</i> file. The first entry is always "Default". Set unlocked = false to hide an outfit until the player earns it, then call @{PhotoMode.UnlockOutfit} to reveal it. Unlocks are saved in GlobalVars.Engine.PhotoModeOutfits.
-- @usage
-- --Classic skin swap (uses Lara:SetSkin):
--
--    { 
--      name = "Classic TR4",
--      skin = 
--          {
--          TEN.Objects.ObjID.ANIMATING1,   -- skin
--          TEN.Objects.ObjID.ANIMATING2,   -- skinJoints
--          TEN.Objects.ObjID.ANIMATING3,   -- skinScream
--          TEN.Objects.ObjID.ANIMATING4,   -- hair1
--          -- hair2 omitted → unchanged
--          },
--      meshVisible = "all",
--    },
--
-- --skinned mesh swap (uses Lara:SwapSkinnedMesh):
--
--    {
--      name             = "Remastered",
--      skinnedMesh      = TEN.Objects.ObjID.ANIMATING14,
--      skinnedMeshIndex = 0,      -- optional sub-index
--      meshVisible      = "none", -- hide classic meshes so only GPU mesh shows
--      unlocked         = false,
--      onEnter = function() --function to call when outfit is selected
--      local s = TEN.Flow.GetSettings()
--       s.Hair[1].offset = Vec3(-4, 3, -28)
--       TEN.Flow.SetSettings(s)
--      end
--    }
--

--- Display name shown in the selector.
-- @tfield string name Display name for this outfit.

--- Array of up to 5 ObjIDs for classic skin swap via Lara:SetSkin().
-- @tfield[opt=nil] table skin Array of up to 5 ObjIDs: skin, skinJoints, skinScream, hair1, hair2. Nil entries leave that slot unchanged.
-- @usage
--      skin = 
--          {
--          TEN.Objects.ObjID.ANIMATING1,   -- skin
--          TEN.Objects.ObjID.ANIMATING2,   -- skinJoints
--          TEN.Objects.ObjID.ANIMATING3,   -- skinScream
--          TEN.Objects.ObjID.ANIMATING4,   -- hair1
--          },

--- ObjID for skinned mesh swap, or the string "clear" to disable GPU skinning.
-- @tfield[opt=nil] Objects.ObjID skinnedMesh ObjID passed to Lara:SwapSkinnedMesh(), or "clear" to call Lara:ClearSkinnedMesh().
-- @usage
-- skinnedMesh = TEN.Objects.ObjID.ANIMATING14,

--- Optional sub-index passed to Lara:SwapSkinnedMesh().
-- @tfield[opt=nil] int skinnedMeshIndex Optional sub-index for SwapSkinnedMesh.
-- @usage
-- skinnedMeshIndex = 0,      -- optional sub-index

--- Controls classic mesh visibility: "all", "none", or a table of visible slot indices.
-- @tfield[opt=nil] string|table meshVisible "all" keeps all meshes visible, "none" hides all, or a table of indices keeps only those slots visible.
-- @usage
-- meshVisible values:
--    "all"       — all classic mesh slots remain visible
--    "none"      — all classic mesh slots are hidden
--    { 0, 4, 9 } — only the listed slot indices stay visible; rest are hidden

--- Optional function called after the outfit is applied.
-- @tfield[opt=nil] function onEnter Hook function executed after applying this outfit. Can be used to change hair offsets.
-- @usage
-- onEnter = function() --function to call when outfit is selected
--      local s = TEN.Flow.GetSettings()
--      s.Hair[1].offset = Vec3(-4, 3, -28)
--      TEN.Flow.SetSettings(s)
-- end
--

--- Whether the outfit is visible in the selector menu.
-- @tfield[opt=true] bool unlocked true or nil makes the outfit visible; false hides it in selection until @{PhotoMode.UnlockOutfit} is called.

--- Clear all unlocked outfits so they no longer appear in the photo mode outfit selector.
-- @function PhotoMode.ClearOutfits
-- @usage
-- PhotoMode.ClearOutfits()

--- Unlock a named outfit so it appears in the photo mode outfit selector. The outfit remains unlocked in all levels.
-- @function PhotoMode.UnlockOutfit
-- @tparam string name The outfit name string as defined in Outfits.lua.
-- @usage
-- PhotoMode.UnlockOutfit("Secret Wetsuit")

--- Poses
-- @section Poses
-- Poses are defined in the <i>Engine/PhotoMode/Poses.lua</i> file. Each pose applies an animation from the PHOTOMODE_ANIMS object or any other object in the level. The first entry is always "Default" — it restores Lara's entry animation.
-- @usage
--    { 
--      name = "Victory",
--      objID       = TEN.Objects.ObjID.PHOTOMODE_ANIMS,
--      animNumber  = 42,    -- animation slot inside PHOTOMODE_ANIMS
--      frameNumber = 0,     -- starting frame (0 = first frame)
--    },
--
-- --You can also reference any other object's animations
--
--    { 
--      name = "Running",
--      objID       = TEN.Objects.ObjID.LARA,
--      animNumber  = 17,
--      frameNumber = 0,
--    },

--- Display name shown in the selector.
-- @tfield string name Display name for this pose.

--- The object that contains the animation to apply for this pose.
-- @tfield Objects.ObjID objID The object that contains the animation to apply for this pose.

--- The animation slot number inside that object (0-based).
-- @tfield int animNumber The animation slot number inside that object (0-based).

--- The starting frame number for that animation (0-based).
-- @tfield int frameNumber The frame number for that animation to set as pose (0-based).

--- Settings
-- @section Settings
-- Settings can be managed by the two functions @{PhotoMode.GetSettings} and @{PhotoMode.SetSettings}. See those functions for details.

---Set settings tables for PhotoMode.
-- @function PhotoMode.SetSettings
-- @tparam Settings newSettings Required settings table
-- @usage
-- -- In the level's lua file
-- local settings = PhotoMode.GetSettings()
-- settings.Character.accessoriesEnabled = false
-- PhotoMode.SetSettings(settings)

---Get settings tables for PhotoMode.
-- @function PhotoMode.GetSettings
-- @treturn Settings Current settings table

--- Settings.Camera
-- @section Settings.Camera
-- Camera-related settings.

--- Whether to limit camera distance from Lara to prevent clipping through level geometry. Enabling this will cause the camera to stop moving further away once it reaches the configured distance, but it will not push the camera back if the player moves closer after exceeding the limit.
-- @tfield bool limitCameraDistance true to enable camera distance limiting, false to allow unlimited camera distance.

--- Maximum camera distance from Lara when Settings.Camera.limitCameraDistance is enabled. The camera will stop moving further away once it reaches this distance.
-- @tfield int distance Maximum camera distance from Lara when Settings.Camera.limitCameraDistance is enabled, measured in game units.

--- All four DOF items (Mode, Focus Distance, Focus Range, Blur Strength) can be hidden at once by setting depthOfFieldEnabled = false. This is useful for projects that do not use the DOF post-process effect and want a cleaner Camera menu.
-- @tfield bool depthOfFieldEnabled true to enable depth of field, false to disable it.

--- Settings.Character
-- @section Settings.Character
-- Settings related to character customization.

--- Whether the Accessories menu is enabled. Accessories are mesh swaps parented to a hidden moveable that mirrors Lara's animation, allowing them to move naturally with her skeleton. Set this to false to hide the Accessories menu and disable the accessory system entirely.
-- @tfield bool accessoriesEnabled true to enable the Accessories menu, false to hide it and disable the accessory option in Character menu.

--- Whether to show all weapon options in the Character menu regardless of inventory.
-- @tfield bool allWeapons set this to false to hide unavailable weapons.

--- Whether the Outfits menu is enabled.
-- @tfield bool outfitsEnabled true to enable the Outfits menu, false to hide it and disable the outfit option in Character menu.

--- Settings.ColorMap
-- @section Settings.ColorMap
-- These settings define the colors used throughout the inventory UI.
-- Colors are of type @{Color}.
-- @usage
-- -- Example of changing the text color
-- -- In the level's lua file
-- local settings = PhotoMode.GetSettings()
-- settings.ColorMap.plainTextColor = TEN.Color(200, 180, 60, 255)
-- PhotoMode.SetSettings(settings)

--- Color used for standard body text in the inventory.
-- @tfield[opt=Flow.GetSettings().UI.plainTextColor] Color plainText Applied to descriptive text.

--- Color used for section headers and titles.
-- @tfield[opt=Flow.GetSettings().UI.headerTextColor] Color headerText Applied to inventory category headings and titles.

--- Color used for selectable option text.
-- @tfield[opt=Flow.GetSettings().UI.optionTextColor] Color optionText Applied to text entries.

--- Color used to render the neutral sprites.
-- @tfield[opt=Color(255&#44; 255&#44; 255&#44; 255)] Color neutral Tint applied to the sprites.

--- Color used to render the dimmed sprites.
-- @tfield[opt=Color(120&#44; 120&#44; 120&#44; 255)] Color dimmed Tint applied to the sprites.


--- Settings.SoundMap
-- @section Settings.SoundMap
-- These settings map inventory UI events to sound effect IDs.
-- Sound IDs correspond to entries in the game's sound catalogue.
-- @usage
-- -- Example of overriding the photo mode open sound
-- -- In the level's lua file
-- local settings = PhotoMode.GetSettings()
-- settings.SoundMap.menuOpen = 42
-- PhotoMode.SetSettings(settings)

--- Sound played when rotating the inventory ring.
-- @tfield[opt=108] int menuRotate Sound effect ID triggered while scrolling through photo mode.

--- Sound played when hovering over or highlighting a menu option.
-- @tfield[opt=109] int menuSelect Sound effect ID triggered on item selection highlight.

--- Sound played when confirming a menu choice.
-- @tfield[opt=111] int menuChoose Sound effect ID triggered when the player confirms a selected action.

--- Sound played when the inventory is opened.
-- @tfield[opt=109] int menuOpen Sound effect ID triggered when the photo modeis opened.

--- Sound played when the inventory is closed.
-- @tfield[opt=109] int menuClose Sound effect ID triggered when the photo modeis closed.


-- Photo Module Start
local Borders       = require("Engine.PhotoMode.SpriteBorders")
local Camera        = require("Engine.PhotoMode.Camera")
local Configuration = require("Engine.PhotoMode.Configuration")
local Input         = require("Engine.PhotoMode.Input")
local Menu          = require("Engine.PhotoMode.Menu")
local Settings      = require("Engine.PhotoMode.Settings")
local States        = require("Engine.PhotoMode.States")
require("Engine.PhotoMode.Strings")

LevelFuncs.Engine.PhotoMode = LevelFuncs.Engine.PhotoMode or {}

local PhotoMode = {}

-- Guards LevelFuncs registration to once per Lua session (resets on level/savegame reload).
local _callbacksRegistered = false
local _photoModeExited     = false
local _spriteAnim          = {}   -- per-sprite lerp state: { sizeW, sizeH, r, g, b }

-- ============================================================================
-- Helpers
-- ============================================================================
local CopyTable = function(original)
    local copy = {}
    for k, v in pairs(original) do
        if type(v) == "table" then
            copy[k] = Utilities.CopyTable(v)
        else
            copy[k] = v
        end
    end
    return copy
end

local function ColorCombine(color, alpha)
    return TEN.Color(color.r, color.g, color.b, alpha)
end

function PhotoMode.UnlockOutfit(name)
    for _, outfit in ipairs(Configuration.Outfits) do
        if outfit.name == name then
            GlobalVars.Engine.PhotoModeOutfits = GlobalVars.Engine.PhotoModeOutfits or {}
            GlobalVars.Engine.PhotoModeOutfits[name] = true
            return
        end
    end
end

function PhotoMode.ClearOutfits()
    GlobalVars.Engine.PhotoModeOutfits = {}
end

PhotoMode.GetSettings = function()
    return CopyTable(Configuration)
end

PhotoMode.SetSettings = function(newSettings)
    for section, values in pairs(newSettings) do
        if Configuration[section] ~= nil then
            for setting, value in pairs(values) do
                if Configuration[section][setting] ~= nil then
                    Configuration[section][setting] = value
                end
            end
        end
    end
end
-- ============================================================================
-- Option name builders (for selector-type options)
-- ============================================================================

local function BuildNames(list, key)
    local t = {}
    for _, v in ipairs(list) do t[#t + 1] = v[key or "name"] end
    return t
end

local DOF_MODE_NAMES   = BuildNames(Configuration.DepthOfField.modes)
local LIGHT_SRC_NAMES  = Configuration.Light.sourceNames
local FILTER_NAMES     = BuildNames(Configuration.Filters.presets)
local FRAME_NAMES      = BuildNames(Configuration.Frames.presets)
local ANIM_NAMES       = BuildNames(Configuration.Animations)
local EXPRESSION_NAMES = BuildNames(Configuration.Expressions)
local ACCESSORY_NAMES  = BuildNames(Configuration.Accessories.presets)

-- Color selectors use blank option labels — the actual color is shown via sprite strip.
local TINT_NAMES  = (function() local t={} for i=1,#Configuration.Filters.tints       do t[i]="" end return t end)()
local COLOR_NAMES = (function() local t={} for i=1,#Configuration.Light.colorPresets  do t[i]="" end return t end)()

-- Outfit and weapon name lists are built dynamically each entry to respect
-- per-outfit unlock flags and live inventory checks.
local _outfitMenuMap        = {}  -- [menuOptionIdx] -> real Configuration.Outfits index
local _outfitMenuMapReverse = {}  -- [real Configuration.Outfits index] -> menuOptionIdx
local _weaponMenuMap        = {}  -- [menuOptionIdx] -> real Configuration.Weapons index
local _weaponMenuMapReverse = {}  -- [real Configuration.Weapons index] -> menuOptionIdx

local function BuildFilteredOutfitNames()
    _outfitMenuMap        = {}
    _outfitMenuMapReverse = {}
    local names           = {}
    for i, outfit in ipairs(Configuration.Outfits) do
        local unlocked = outfit.unlocked
        if unlocked == false then
            local photoModeOutfits = GlobalVars.Engine and GlobalVars.Engine.PhotoModeOutfits
            unlocked = photoModeOutfits ~= nil and photoModeOutfits[outfit.name] == true
        end

        if unlocked ~= false then  -- allows nil and true, blocks false
            local idx                = #names + 1
            names[idx]               = outfit.name
            _outfitMenuMap[idx]      = i
            _outfitMenuMapReverse[i] = idx
        end
    end
    return names
end

local function BuildFilteredWeaponNames()
    _weaponMenuMap        = {}
    _weaponMenuMapReverse = {}
    local names = {}
    for i, weapon in ipairs(Configuration.Weapons) do
        local show = false
        if weapon.name == "Default" then
            show = true  -- Default always shown.
        elseif weapon.pickupObjID == nil then
            show = true  -- No inventory check configured.
        else
            local count = TEN.Inventory.GetItemCount(weapon.pickupObjID)
            show = count > 0 or Settings.Character.allWeapons  -- Show if in inventory or if allWeapons setting is enabled.
        end
        if show then
            local idx                = #names + 1
            names[idx]               = weapon.name
            _weaponMenuMap[idx]      = i
            _weaponMenuMapReverse[i] = idx
        end
    end
    return names
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

local function ApplyDOF(state)
    local cfg = Configuration.DepthOfField
    local modePreset = cfg.modes[state.dofMode]
    local mode = modePreset and modePreset.mode or TEN.View.DOFMode.NONE
    if mode == TEN.View.DOFMode.NONE then
        TEN.View.SetDOF(TEN.View.DOFMode.NONE)
    else
        TEN.View.SetDOF(mode, state.dofFocusDistance, state.dofRange, state.dofStrength)
    end
end

local function ApplyFilter(state)
    local preset = Configuration.Filters.presets[state.filterIndex]
    if preset then
        TEN.View.SetPostProcessMode(preset.mode)
    end
end

local function ApplyFilterStrength(state)
    TEN.View.SetPostProcessStrength(state.filterStrength)
end

local function ApplyTint(state)
    local preset = Configuration.Filters.tints[state.tintIndex]
    if preset then
        local i = state.tintIntensity or 0
        local c = preset.color
        local blended = TEN.Color(
            math.floor(128 + (c.r - 128) * i),
            math.floor(128 + (c.g - 128) * i),
            math.floor(128 + (c.b - 128) * i))
        TEN.View.SetPostProcessTint(blended)
    end
end

local function ResetCurrentOutfit(state)
    local snap = state.snapshot

    -- Restore skinned mesh to entry state.
    if state.appliedSkinnedMesh then
        if snap and snap.skinnedMeshObject then
            Lara:SwapSkinnedMesh(snap.skinnedMeshObject, snap.skinnedMeshIndex)
        else
            Lara:ClearSkinnedMesh()
        end
    end

    -- Restore classic skin to entry state.
    if state.appliedSkin and snap and snap.skin then
        Lara:SetSkin(snap.skin[1], snap.skin[2], snap.skin[3], snap.skin[4], snap.skin[5])
    end

    -- Restore mesh visibility to entry state.
    if #state.hiddenMeshes > 0 and snap and snap.meshVisible then
        for i = 0, 14 do
            Lara:SetMeshVisible(i, snap.meshVisible[i] ~= false)
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

    local preset = Configuration.Outfits[state.outfitIndex]

    -- Default: ResetCurrentOutfit already restored entry state.
    if not preset or (not preset.skin and not preset.skinnedMesh and not preset.meshVisible) then
        Lara:ResetHair()
        return
    end

    -- Apply classic skin change.
    if preset.skin then
        local s = preset.skin
        Lara:SetSkin(s[1], s[2], s[3], s[4], s[5])
        state.appliedSkin = true
    end

    -- Apply skinned mesh change.
    if preset.skinnedMesh then
        if preset.skinnedMesh == "clear" then
            Lara:ClearSkinnedMesh()
        else
            Lara:SwapSkinnedMesh(preset.skinnedMesh, preset.skinnedMeshIndex)
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
                    Lara:SetMeshVisible(i, false)
                    state.hiddenMeshes[#state.hiddenMeshes + 1] = i
                end
            end
        else
            -- "none" → hide all classic meshes
            for i = 0, 14 do
                Lara:SetMeshVisible(i, false)
                state.hiddenMeshes[#state.hiddenMeshes + 1] = i
            end
        end
    end

    -- Re-apply weapon and expression mesh swaps (SetSkin resets classic meshes).
    for _, meshIdx in ipairs(state.swappedWeaponMeshes) do
        local wp = Configuration.Weapons[state.weaponIndex]
        if wp and wp.objID then
            Lara:SwapMesh(meshIdx, wp.objID, meshIdx)
        end
    end
    for _, meshIdx in ipairs(state.swappedExpressionMeshes) do
        local ep = Configuration.Expressions[state.expressionIndex]
        if ep and ep.objID then
            Lara:SwapMesh(meshIdx, ep.objID, meshIdx)
        end
    end

    -- Execute outfit-specific hook if provided.
    if preset.onEnter and type(preset.onEnter) == "function" then
        preset.onEnter()
    end

    Lara:ResetHair()
end

local function ApplyWeapon(state)

    local snap = state.snapshot
    if not snap then return end

        -- Unswap previously applied weapon meshes
    for _, meshIdx in ipairs(state.swappedWeaponMeshes) do
        Lara:UnswapMesh(meshIdx)
    end

    if Configuration.Weapons[state.weaponIndex].name == "Default" then
        if snap.meshSwaps then
            
            local entry10, entry13

            for _, entry in ipairs(snap.meshSwaps) do
                if entry.index == 10 then entry10 = entry end
                if entry.index == 13 then entry13 = entry end
            end

            if entry10 then
                Lara:SwapMesh(entry10.index, entry10.sourceObjID, entry10.index)
            end

            if entry13 then
                Lara:SwapMesh(entry13.index, entry13.sourceObjID, entry13.index)
            end

            Lara:SetHolsterWeapon(snap.holsterLeft, snap.holsterRight, snap.holsterBack)
            Lara:ResetHair()
        end
        return
    end

    state.swappedWeaponMeshes = {}

    local preset = Configuration.Weapons[state.weaponIndex]
    if preset and preset.objID and preset.meshIndices then
        for _, meshIdx in ipairs(preset.meshIndices) do
            Lara:SwapMesh(meshIdx, preset.objID, meshIdx)
            state.swappedWeaponMeshes[#state.swappedWeaponMeshes + 1] = meshIdx
        end
    end

    -- Adjust holster slots based on which visual slots the weapon occupies.
    -- Clear the slots that are now visually shown as drawn; retain the rest.
    -- For "none" (default), restore entry snapshot holster state.
    local slot = preset and preset.type or "none"

    if snap then
        Lara:SetHolsterWeapon(snap.holsterLeft, snap.holsterRight, snap.holsterBack)
    end

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

    Lara:ResetHair()
end

local function ApplyPosePreset(state)
    local preset = Configuration.Animations[state.animIndex]
    if preset then

        if preset.name == "Default" then
            if state.snapshot then
                Lara:SetAnim(state.snapshot.laraAnim, state.snapshot.laraAnimSlot)
                Lara:SetFrame(state.snapshot.laraFrame)
            end
        else
            Lara:SetAnim(preset.animNumber, preset.objID)
            Lara:SetFrame(preset.frameNumber)
        end
    end
    Lara:ResetHair()
end

local function ApplyExpression(state)
    for _, meshIdx in ipairs(state.swappedExpressionMeshes) do
        Lara:UnswapMesh(meshIdx)
    end
    state.swappedExpressionMeshes = {}

    local preset = Configuration.Expressions[state.expressionIndex]
    if preset and preset.objID and preset.meshIndices then
        for _, meshIdx in ipairs(preset.meshIndices) do
            Lara:SwapMesh(meshIdx, preset.objID, meshIdx)
            state.swappedExpressionMeshes[#state.swappedExpressionMeshes + 1] = meshIdx
        end
    end
    Lara:ResetHair()
end

local function GetOrCreateAccessoryMesh(state)
    if Configuration.Accessories.enabled == false then return nil end
    local name = Configuration.Accessories.meshName
    if state.accessoryMesh then return state.accessoryMesh end
    if TEN.Objects.IsNameInUse(name) then
        local mov = TEN.Objects.GetMoveableByName(name)
        state.accessoryMesh = mov
        return mov
    end
    local pos  = Lara:GetPosition()
    local rot  = Lara:GetRotation()
    local room = Lara:GetRoomNumber()
    local mov = TEN.Objects.Moveable(Configuration.Accessories.baseObjID, name, pos, rot, room)
    if mov then
        mov:Enable()
        mov:SetColor(TEN.Color(255, 255, 255, 0))
        state.accessoryMesh = mov
        return mov
    end
    return nil
end

local function ApplyAccessory(state)
    local mov = GetOrCreateAccessoryMesh(state)
    if not mov then return end
    local preset = Configuration.Accessories.presets[state.accessoryIndex]
    local hasPreset = preset and preset.objID ~= nil
    -- Swap meshes based on the selected preset
    if hasPreset then

        for i = 0, 14 do
            mov:SetMeshVisible(i, false)
        end

        for _, meshIdx in ipairs(preset.meshIndices) do
            mov:SwapMesh(meshIdx, preset.objID, meshIdx)
            mov:SetMeshVisible(meshIdx, true)
        end
        mov:SetPosition(Lara:GetPosition())
        mov:SetRotation(Lara:GetRotation())
        mov:SetAnim(Lara:GetAnim(), Lara:GetAnimSlot())
        mov:SetFrame(Lara:GetFrame())
        mov:SetColor(TEN.Color(255, 255, 255, 255))
    else
        -- Restore default meshes and hide the moveable
        for i = 0, 14 do
            mov:SetMeshVisible(i, false)
            mov:UnswapMesh(i)
        end
        mov:SetColor(TEN.Color(255, 255, 255, 0))
    end
end

local function UpdateAccessoryMesh(state)
    local preset = Configuration.Accessories.presets[state.accessoryIndex]
    if not preset or preset.objID == nil then return end
    if not state.accessoryMesh then return end
    state.accessoryMesh:SetPosition(Lara:GetPosition())
    state.accessoryMesh:SetRotation(Lara:GetRotation())
    state.accessoryMesh:SetAnim(Lara:GetAnim(), Lara:GetAnimSlot())
    state.accessoryMesh:SetFrame(Lara:GetFrame())
end

local function UpdateGunFlash(state)
    
    local preset = Configuration.Weapons[state.weaponIndex]
    if not state.gunflashEnabled or state.hideCharacter or not preset then 
        Lara:ClearGunFlash()
        return
    end

    local handStatus = Lara:GetHandStatus()

    if preset.weaponType == TEN.Objects.WeaponType.FLARE then
        local flare = TEN.Flow.GetSettings().Flare
        local position = Lara:GetJointPosition(preset.meshIndices[1], flare.offset)
        TEN.Effects.EmitLight(position, flare.color, flare.range)
    elseif preset.name =="Default" and (handStatus ~= TEN.Objects.HandStatus.BUSY and handStatus ~= TEN.Objects.HandStatus.FREE) then
        Lara:SpawnGunFlash(Lara:GetWeaponType(), TEN.Objects.WeaponFlashMode.AUTO)
    elseif preset.gunFlash then
        if preset.gunFlash ~= TEN.Objects.WeaponFlashMode.AUTO then
            Lara:ClearGunFlash()
        end
        Lara:SpawnGunFlash(preset.weaponType, preset.gunFlash)
    else
        Lara:ClearGunFlash()
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
        Lara:SetAnim(state.snapshot.laraAnim, state.snapshot.laraAnimSlot)
        Lara:SetFrame(state.snapshot.laraFrame)
        Lara:SetPosition(state.snapshot.laraPos)
        Lara:SetRotation(state.snapshot.laraRot)
    end
    state.animIndex = 1

    ResetCurrentOutfit(state)

    -- Undo weapon and expression mesh swaps.
    for _, meshIdx in ipairs(state.swappedWeaponMeshes) do
        Lara:UnswapMesh(meshIdx)
    end
    state.swappedWeaponMeshes = {}

    for _, meshIdx in ipairs(state.swappedExpressionMeshes) do
        Lara:UnswapMesh(meshIdx)
    end
    state.swappedExpressionMeshes = {}

    -- Re-apply entry mesh swaps and holster state.
    local snap = state.snapshot
    if snap then
        if snap.meshSwaps then
            for _, entry in ipairs(snap.meshSwaps) do
                Lara:SwapMesh(entry.index, entry.sourceObjID, entry.index)
            end
        end
        Lara:SetHolsterWeapon(snap.holsterLeft, snap.holsterRight, snap.holsterBack)
    end

    state.outfitIndex     = 1
    state.weaponIndex     = 1
    state.expressionIndex = 1
    state.accessoryIndex  = 1
    state.gunflashEnabled = false
    ApplyAccessory(state)

    Lara:ResetHair()

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
    state.tintIntensity  = Configuration.Filters.defaultTintIntensity
    ApplyFilter(state)
    ApplyFilterStrength(state)
    ApplyTint(state)

    state.frameIndex       = 1
    state.dofMode          = Configuration.DepthOfField.defaultMode
    state.dofFocusDistance = Configuration.DepthOfField.defaultFocusDistance
    state.dofRange         = Configuration.DepthOfField.defaultRange
    state.dofStrength      = Configuration.DepthOfField.defaultStrength
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
        state.lightIntensity  = state.entryLight.intensity
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
    local cfg   = Configuration
    local acceptString = TEN.Flow.GetString("pm_press")

    Menu.DeleteAll()

    -- ================================================================
    -- Helper to create and configure a menu
    -- ================================================================
    local function CreateMenu(menuName, items, acceptFunc, optionChangeFunc, titleText)
        local menu = Menu.Create(menuName, "", items,
            acceptFunc, nil, Menu.Type.ITEMS_AND_OPTIONS)
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
            m:SetOptionIndexForItemName("pm_accessory",   state.accessoryIndex)
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
                mf:SetOptionIndexForItemName("pm_preset",         state.filterIndex)
                mf:SetOptionIndexForItemName("pm_strength",      ValueToOptionIndex(state.filterStrength, 0, 0.05))
                mf:SetOptionIndexForItemName("pm_tint",          state.tintIndex)
                mf:SetOptionIndexForItemName("pm_tint_intensity", ValueToOptionIndex(state.tintIntensity, cfg.Filters.minTintIntensity, cfg.Filters.tintIntensityStep))
                mf:SetOptionIndexForItemName("pm_frame_overlay", state.frameIndex)
            end
        end
    end

    LevelFuncs.Engine.PhotoMode.OnLightAccept = function()
        local m = Menu.Get(MENU_LIGHT)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_place_light" then
            local opt = m:GetCurrentOptionIndex()
            if opt == 1 then PlaceLightAtCamera() else PlaceLightAtLara() end
        elseif name == "pm_reset" then
            ResetLight()
            m:SetOptionIndexForItemName("pm_enabled",   BoolToIndex(state.lightEnabled))
            m:SetOptionIndexForItemName("pm_source",    state.lightSource)
            m:SetOptionIndexForItemName("pm_radius",    ValueToOptionIndex(state.lightRadius,    cfg.Light.minRadius,    cfg.Light.radiusStep))
            m:SetOptionIndexForItemName("pm_intensity", ValueToOptionIndex(state.lightIntensity, cfg.Light.minIntensity, cfg.Light.intensityStep))
            m:SetOptionIndexForItemName("pm_color",     state.lightColorIndex)
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
        elseif name == "pm_accessory" then
            state.accessoryIndex = m:GetCurrentOptionIndex()
            ApplyAccessory(state)
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
        if     name == "pm_enabled"   then state.lightEnabled    = IndexToBool(m:GetCurrentOptionIndex())
        elseif name == "pm_source"    then state.lightSource     = m:GetCurrentOptionIndex()
        elseif name == "pm_radius"    then state.lightRadius     = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Light.minRadius, cfg.Light.radiusStep)
        elseif name == "pm_intensity" then state.lightIntensity  = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Light.minIntensity, cfg.Light.intensityStep)
        elseif name == "pm_color"     then state.lightColorIndex = m:GetCurrentOptionIndex()
        end
    end

    LevelFuncs.Engine.PhotoMode.OnUIOptionChange = function()
        local m = Menu.Get(MENU_UI)
        if not m then return end
        local name = m:GetCurrentItem() and m:GetCurrentItem().itemName
        if name == "pm_hide_ui" then
            state.hideUI = IndexToBool(m:GetCurrentOptionIndex())
        elseif name == "pm_hide_character" then
            state.hideCharacter = IndexToBool(m:GetCurrentOptionIndex())
            Lara:SetVisible(not state.hideCharacter)
            state.accessoryMesh:SetVisible(not state.hideCharacter)
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
            m:SetOptionIndexForItemName("pm_tint_intensity", ValueToOptionIndex(state.tintIntensity, cfg.Filters.minTintIntensity, cfg.Filters.tintIntensityStep))
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
        elseif name == "pm_tint_intensity" then
            state.tintIntensity = OptionIndexToValue(m:GetCurrentOptionIndex(), cfg.Filters.minTintIntensity, cfg.Filters.tintIntensityStep)
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
    local characterItems =
    {
        { itemName = "pm_animation",  options = ANIM_NAMES,       currentOption = state.animIndex },
    }

    if Settings.Character.outfitsEnabled ~= false then
        characterItems[#characterItems + 1] = { itemName = "pm_outfit", options = outfitNames, currentOption = _outfitMenuMapReverse[state.outfitIndex] or 1 }
    end
    characterItems[#characterItems + 1] = { itemName = "pm_weapons",    options = weaponNames,      currentOption = _weaponMenuMapReverse[state.weaponIndex] or 1 }
    characterItems[#characterItems + 1] = { itemName = "pm_expression", options = EXPRESSION_NAMES, currentOption = state.expressionIndex }
    if Settings.Character.accessoriesEnabled ~= false then
        characterItems[#characterItems + 1] = { itemName = "pm_accessory", options = ACCESSORY_NAMES, currentOption = state.accessoryIndex }
    end
    characterItems[#characterItems + 1] = { itemName = "pm_gunflash", options = BoolOptions(),    currentOption = BoolToIndex(state.gunflashEnabled) }
    characterItems[#characterItems + 1] = { itemName = "pm_reset",    options = { acceptString }, currentOption = 1 }
    CreateMenu(MENU_CHARACTER, characterItems,
        "Engine.PhotoMode.OnCharacterAccept", "Engine.PhotoMode.OnCharacterOptionChange", "pm_header_character")

    -- ================================================================
    -- EFFECTS menu (Lens + Depth of Field)
    -- ================================================================
    local effectsItems =
    {
        { itemName = "pm_fov",  options = NumberRange(cfg.Lens.minFOV, cfg.Lens.maxFOV, cfg.Lens.fovStep),
          currentOption = ValueToOptionIndex(state.fov, cfg.Lens.minFOV, cfg.Lens.fovStep)},
        { itemName = "pm_roll", options = NumberRange(cfg.Lens.minRoll, cfg.Lens.maxRoll, cfg.Lens.rollStep),
          currentOption = ValueToOptionIndex(state.roll, cfg.Lens.minRoll, cfg.Lens.rollStep)},
    }

    if Settings.Camera.depthOfFieldEnabled ~= false then
        effectsItems[#effectsItems + 1] = { itemName = "pm_dof_mode", options = DOF_MODE_NAMES, currentOption = state.dofMode }
        effectsItems[#effectsItems + 1] = { itemName = "pm_dof_focus", options = NumberRange(cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.maxFocusDistance, cfg.DepthOfField.focusDistanceStep),
          currentOption = ValueToOptionIndex(state.dofFocusDistance, cfg.DepthOfField.minFocusDistance, cfg.DepthOfField.focusDistanceStep)}
        effectsItems[#effectsItems + 1] = { itemName = "pm_dof_range", options = NumberRange(cfg.DepthOfField.minRange, cfg.DepthOfField.maxRange, cfg.DepthOfField.rangeStep),
          currentOption = ValueToOptionIndex(state.dofRange, cfg.DepthOfField.minRange, cfg.DepthOfField.rangeStep)}
        effectsItems[#effectsItems + 1] = { itemName = "pm_dof_strength", options = NumberRange(cfg.DepthOfField.minStrength, cfg.DepthOfField.maxStrength, cfg.DepthOfField.strengthStep,
              function(v) return string.format("%.2f", v) end),
          currentOption = ValueToOptionIndex(state.dofStrength, cfg.DepthOfField.minStrength, cfg.DepthOfField.strengthStep)}
    end
    effectsItems[#effectsItems + 1] = { itemName = "pm_reset", options = { acceptString }, currentOption = 1 }
    CreateMenu(MENU_EFFECTS, effectsItems,
        "Engine.PhotoMode.OnEffectsAccept", "Engine.PhotoMode.OnEffectsOptionChange", "pm_header_effects")

    -- ================================================================
    -- FILTERS menu (Post-process + Frame)
    -- ================================================================
    CreateMenu(MENU_FILTERS,
    {
        { itemName = "pm_frame_overlay", options = FRAME_NAMES, currentOption = state.frameIndex },
        { itemName = "pm_preset",        options = FILTER_NAMES, currentOption = state.filterIndex },
        { itemName = "pm_strength",      options = NumberRange(0, 1.0, 0.05, function(v) return string.format("%.2f", v) end),
          currentOption = ValueToOptionIndex(state.filterStrength, 0, 0.05)},
        { itemName = "pm_tint",          options = TINT_NAMES, currentOption = state.tintIndex },
        { itemName = "pm_tint_intensity", options = NumberRange(cfg.Filters.minTintIntensity, cfg.Filters.maxTintIntensity, cfg.Filters.tintIntensityStep,
              function(v) return string.format("%.2f", v) end),
          currentOption = ValueToOptionIndex(state.tintIntensity, cfg.Filters.minTintIntensity, cfg.Filters.tintIntensityStep)},
        { itemName = "pm_reset",         options = { acceptString }, currentOption = 1 },
    },
    "Engine.PhotoMode.OnFiltersAccept", "Engine.PhotoMode.OnFiltersOptionChange", "pm_header_filters")

    -- ================================================================
    -- LIGHT menu
    -- ================================================================
    CreateMenu(MENU_LIGHT,
    {
        { itemName = "pm_enabled",      options = BoolOptions(), currentOption = BoolToIndex(state.lightEnabled) },
        { itemName = "pm_source",       options = LIGHT_SRC_NAMES, currentOption = state.lightSource },
        { itemName = "pm_radius",       options = NumberRange(cfg.Light.minRadius, cfg.Light.maxRadius, cfg.Light.radiusStep),
          currentOption = ValueToOptionIndex(state.lightRadius, cfg.Light.minRadius, cfg.Light.radiusStep)},
        { itemName = "pm_color",        options = COLOR_NAMES, currentOption = state.lightColorIndex },
        { itemName = "pm_intensity",    options = NumberRange(cfg.Light.minIntensity, cfg.Light.maxIntensity, cfg.Light.intensityStep,
              function(v) return string.format("%.2f", v) end),
          currentOption = ValueToOptionIndex(state.lightIntensity, cfg.Light.minIntensity, cfg.Light.intensityStep)},
        { itemName = "pm_place_light",  options = { "Camera", "Lara" }, currentOption = 1 },
        { itemName = "pm_reset",        options = { acceptString }, currentOption = 1 },
    },
    "Engine.PhotoMode.OnLightAccept", "Engine.PhotoMode.OnLightOptionChange", "pm_header_light")

    -- ================================================================
    -- UI menu
    -- ================================================================
    CreateMenu(MENU_UI,
    {
        { itemName = "pm_hide_ui",        options = BoolOptions(), currentOption = BoolToIndex(state.hideUI) },
        { itemName = "pm_hide_character",  options = BoolOptions(), currentOption = BoolToIndex(state.hideCharacter) },
        { itemName = "pm_exit",            options = { acceptString }, currentOption = 1 },
    },
    "Engine.PhotoMode.OnUIAccept", "Engine.PhotoMode.OnUIOptionChange", "pm_header_ui")

    -- ================================================================
    -- Set up headers (STEP_LEFT / STEP_RIGHT to navigate)
    -- ================================================================
    Menu.SetHeaders(
    {
        { name = "",   menuName = MENU_EFFECTS, hideText = true },
        { name = "", menuName = MENU_CHARACTER, hideText = true },
        { name = "",     menuName = MENU_LIGHT, hideText = true },
        { name = "",   menuName = MENU_FILTERS, hideText = true },
        { name = "",        menuName = MENU_UI, hideText = true },
    })

    Menu.SetHeaderSpacing(15)
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

    local lightColor = Configuration.Light.colorPresets[state.lightColorIndex].color
    local i = state.lightIntensity
    local modifiedColor = TEN.Color(
        math.min(255, math.floor(lightColor.r * i)),
        math.min(255, math.floor(lightColor.g * i)),
        math.min(255, math.floor(lightColor.b * i)))

    TEN.Effects.EmitLight(lightPos, modifiedColor, state.lightRadius, state.lightShadows, Configuration.Light.lightName)
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
    state.entryLight =
    {
        enabled    = state.lightEnabled,
        source     = state.lightSource,
        pos        = TEN.Vec3(camPos.x, camPos.y, camPos.z),
        radius     = state.lightRadius,
        shadows    = state.lightShadows,
        intensity  = state.lightIntensity,
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
    local state = States.Get()
    TEN.Effects.EmitLight(state.lightPos, TEN.Color(0, 0, 0), 0, false, Configuration.Light.lightName)

    -- Hide accessory mesh
    if state.accessoryMesh then
        state.accessoryMesh:SetColor(TEN.Color(255, 255, 255, 0))
        state.accessoryIndex = 1
    end

    -- Restore Lara visibility
    Lara:SetVisible(true)

    -- Clean up menus
    Menu.DeleteAll()

    -- Clear frames
    Borders.Clear()

    -- Unfreeze
    TEN.Flow.SetFreezeMode(TEN.Flow.FreezeMode.NONE)

    States.SetActive(false)
    States.Get().snapshot = nil
    States.Get().entryHoldCount = 0
    States.Get().timeInPhotoMode = 0

    TEN.Sound.PlaySound(Configuration.SoundMap.menuClose)
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
-- Color Selector Sprite
-- For pm_color (MENU_LIGHT) and pm_tint (MENU_FILTERS):
--   * When the item is focused  → draw the rainbow strip (sprite 6) + cursor (sprite 7).
--   * When the item is not focused → draw a small tinted square swatch (COLOR_SWATCH_SPRITE)
--     at the option column position so the current colour is always visible.
-- Each entry uses its own menu's alpha so fades work correctly on both menus.
-- ============================================================================

local function DrawColorSelector()
    local state = States.Get()

        -- Color selector strip constants (percent-screen coords, must match DrawBackSprites layout)
    local COLOR_STRIP_X      = 23.2
    local COLOR_STRIP_Y      = 33.8
    local COLOR_STRIP_W      = 14.0
    local COLOR_STRIP_H      = 8.0
    -- Sprite index in PHOTOMODE_SPRITES used as a small color swatch next to
    -- pm_color / pm_tint items when the rainbow strip is not expanded.
    -- Must be a plain white (or neutral) square sprite in your WAD.
    local COLOR_SWATCH_SPRITE = 8

    -- Descriptor table for each colour-picking item
    local entries =
    {
        { menuName = MENU_LIGHT,   itemName = "pm_color", palette  = Configuration.Light.colorPresets,  colorIndex = state.lightColorIndex },
        { menuName = MENU_FILTERS, itemName = "pm_tint",  palette  = Configuration.Filters.tints,       colorIndex = state.tintIndex },
    }

    local activeMenuName = Menu.GetActiveHeaderMenu()
    local activeM        = Menu.Get(activeMenuName)
    local activeItemName = activeM and activeM:GetCurrentItemName() or nil
    local menuAlpha      = activeM:GetAlpha()
    for _, e in ipairs(entries) do
        
        if menuAlpha < 1 then goto nextEntry end
        local a = math.floor(menuAlpha)

        local m = Menu.Get(e.menuName)
        if not m then goto nextEntry end

        local isActive = (e.menuName == activeMenuName)
        local isActiveItem =(activeItemName == e.itemName)

        if isActive and isActiveItem then
            local strip = TEN.View.DisplaySprite(TEN.Objects.ObjID.PHOTOMODE_SPRITES, 6,
                TEN.Vec2(COLOR_STRIP_X, COLOR_STRIP_Y), 0,
                TEN.Vec2(COLOR_STRIP_W, COLOR_STRIP_H), TEN.Color(255, 255, 255, a))
            if strip then
                strip:Draw(-3,
                    TEN.View.AlignMode.CENTER,
                    TEN.View.ScaleMode.FIT,
                    TEN.Effects.BlendID.ALPHA_BLEND)
            end

            local anchors = strip:GetAnchors(TEN.View.AlignMode.CENTER, TEN.View.ScaleMode.FIT)

            local numOpts = #e.palette
            local optIdx  = m:GetCurrentOptionIndex()

            local cursorX = anchors.CENTER_LEFT.x + (optIdx - 1) / numOpts * (anchors.CENTER_RIGHT.x - anchors.CENTER_LEFT.x)
            local cursorY = anchors.CENTER_LEFT.y

            local cursor = TEN.View.DisplaySprite(
                TEN.Objects.ObjID.PHOTOMODE_SPRITES, 7,
                TEN.Vec2(cursorX, cursorY), 0,
                TEN.Vec2(COLOR_STRIP_W, COLOR_STRIP_H), TEN.Color(255, 255, 255, a))
            if cursor then
                cursor:Draw(-2,
                    TEN.View.AlignMode.CENTER_LEFT,
                    TEN.View.ScaleMode.FIT,
                    TEN.Effects.BlendID.ALPHA_BLEND)
            end
        elseif isActive then
            -- Small tinted swatch at the option column position
            local pos = TEN.Vec2(COLOR_STRIP_X, COLOR_STRIP_Y)
            local col = e.palette[e.colorIndex]
            if pos and col then
                local swatch = TEN.View.DisplaySprite(
                    TEN.Objects.ObjID.PHOTOMODE_SPRITES, COLOR_SWATCH_SPRITE,
                    pos, 0,
                    TEN.Vec2(3, 3), TEN.Color(col.color.r, col.color.g, col.color.b, a))
                if swatch then
                    swatch:Draw(-2,
                        TEN.View.AlignMode.CENTER,
                        TEN.View.ScaleMode.FIT,
                        TEN.Effects.BlendID.ALPHA_BLEND)
                end
            end
        end

        ::nextEntry::
    end
end

-- ============================================================================
-- Header Sprites
-- ============================================================================

local function DrawHeaderSprites(alpha)
    local cfg = Configuration.HeaderSprites
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
        local sprite = TEN.View.DisplaySprite(cfg.objectID, spriteID, TEN.Vec2(x, posY), cfg.rotation or 0, size, color)
        if sprite then
            sprite:Draw(cfg.layer or -4, cfg.alignMode or TEN.View.AlignMode.CENTER, cfg.scaleMode or TEN.View.ScaleMode.FIT, cfg.blendMode or TEN.Effects.BlendID.ALPHA_BLEND)
        end
    end
end

local function DrawBackSprites(alpha)

    local color = ColorCombine(Configuration.ColorMap.dimmed, math.floor(alpha))
    local sprite = TEN.View.DisplaySprite(TEN.Objects.ObjID.PHOTOMODE_SPRITES, 5, TEN.Vec2(1.5, 11), 0, TEN.Vec2(29, 38.5), color)
    if sprite then
        sprite:Draw(-4, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, TEN.Effects.BlendID.ALPHA_BLEND)
    end

end

local function DrawTitle(alpha)
    local modeText = TEN.Flow.GetString("photo_mode")
    local modePos  = TEN.Util.PercentToScreen(TEN.Vec2(16, 9))
    local modeStr  = TEN.Strings.DisplayString(modeText, modePos, 0.8, ColorCombine(Configuration.ColorMap.headerText, alpha), false, { TEN.Strings.DisplayStringOption.SHADOW, TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.VERTICAL_CENTER })
    TEN.Strings.ShowString(modeStr, 1 / 30)
end

local function DrawModeText(alpha)
    local modeText = TEN.Flow.GetString("pm_mode_prefix") .. States.GetModeName()
    local modePos  = TEN.Util.PercentToScreen(TEN.Vec2(16, 46))
    local modeStr  = TEN.Strings.DisplayString(modeText, modePos, 0.6, ColorCombine(Configuration.ColorMap.neutral, alpha), false, { TEN.Strings.DisplayStringOption.SHADOW, TEN.Strings.DisplayStringOption.CENTER })
    TEN.Strings.ShowString(modeStr, 1 / 30)
end

local function DrawHelpText(alpha)

    local mode    = States.GetMode()

    -- Pick the string key for the current control mode
    local device = TEN.Input.GetLastInputDevice()
    local suffix = (device == TEN.Input.InputDevice.GAMEPAD) and "_gamepad" or ""

    local modeKeys = {
        [States.Mode.PLAYER] = "pm_help_character",
        [States.Mode.LIGHT]  = "pm_help_light",
        default              = "pm_help_camera"
    }

    local modeKey = (modeKeys[mode] or modeKeys.default) .. suffix
    local helpKey = (device == TEN.Input.InputDevice.GAMEPAD) and "pm_help_nav_gamepad" or "pm_help_nav"
    
    -- Line 1: mode-specific movement hints
    local helpPos1 = TEN.Util.PercentToScreen(TEN.Vec2(50, 90))
    local helpStr1 = TEN.Strings.DisplayString(modeKey, helpPos1, 0.6, ColorCombine(Configuration.ColorMap.neutral, alpha), true, { TEN.Strings.DisplayStringOption.SHADOW, TEN.Strings.DisplayStringOption.CENTER })
    TEN.Strings.ShowString(helpStr1, 1 / 30)

    -- Line 2: universal navigation hints
    local helpPos2 = TEN.Util.PercentToScreen(TEN.Vec2(50, 94))
    local helpStr2 = TEN.Strings.DisplayString(helpKey, helpPos2, 0.6, ColorCombine(Configuration.ColorMap.neutral, alpha), true, { TEN.Strings.DisplayStringOption.SHADOW, TEN.Strings.DisplayStringOption.CENTER })
    TEN.Strings.ShowString(helpStr2, 1 / 30)
end

-- ============================================================================
-- Callbacks
-- ============================================================================

LevelFuncs.Engine.PhotoMode.OnLoop = function()

    local isTitle = Flow.GetCurrentLevelIndex() == 0

    if isTitle then return end

    if States.IsActive() then return end

    if _photoModeExited then
        _photoModeExited = false
    end

    local state = States.Get()
    local keySet1 = TEN.Input.IsKeyHeld(TEN.Input.ActionID.F3)
    local keySet2  = TEN.Input.IsKeyHeld(TEN.Input.ActionID.GAMEPAD_LEFT_STICK) and TEN.Input.IsKeyHeld(TEN.Input.ActionID.GAMEPAD_RIGHT_STICK)

    if keySet1 or keySet2 then
        state.entryHoldCount = state.entryHoldCount + 1
        if state.entryHoldCount >= Configuration.Entry.holdFrames then
            state.entryHoldCount = 0
            TEN.Sound.PlaySound(Configuration.SoundMap.menuOpen)
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

    local device = TEN.Input.GetLastInputDevice()
    local hideKey = (device == TEN.Input.InputDevice.GAMEPAD) and TEN.Input.ActionID.GAMEPAD_LEFT_TRIGGER or TEN.Input.ActionID.LOOK

    -- Toggle UI with Look key or L2
    if (TEN.Input.IsKeyHit(hideKey) or (TEN.Input.IsKeyHit(TEN.Input.ActionID.INVENTORY) and state.hideUI)) then
        state.hideUI = not state.hideUI
        -- Sync the menu item so it shows Off when the UI is restored
        local mUI = Menu.Get(MENU_UI)
        if mUI then
            mUI:SetOptionIndexForItemName("pm_hide_ui", BoolToIndex(state.hideUI))
        end
        return
    end

    if (TEN.Input.IsKeyHit(TEN.Input.ActionID.INVENTORY) or TEN.Input.IsKeyHit(TEN.Input.ActionID.DESELECT)) and not state.hideUI then
        PhotoMode.Exit()
        _photoModeExited = true
        return
    end

    -- Derive control mode from which header tab is active.
    local activeMenu = Menu.GetActiveHeaderMenu()
    state.controlMode = HEADER_CONTROL_MODE[activeMenu] or States.Mode.CAMERA

    -- Movement controls are always active (UI visible or hidden).
    Input.Update()

    if not state.hideUI then
        -- Menu navigation when UI is visible.
        Menu.UpdateActiveMenus()
    end

    if not States.IsActive() then return end

    -- Attach camera every frame
    Camera.Attach()

    -- Emit light
    UpdateLightEmission()

    -- Update accessory mesh position to follow Lara
    UpdateAccessoryMesh(state)

    -- Emit gun flash if enabled
    UpdateGunFlash(state)

    -- Update and draw frames
    Borders.Update()
    Borders.Draw()

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
        DrawColorSelector()
        DrawModeText(headerAlpha)
        DrawHelpText(headerAlpha)
        DrawTitle(headerAlpha)
        Menu.DrawHeaders(HEADER_POS, HEADER_SCALE, headerAlpha)
        Menu.DrawActiveMenus()
    end
end

-- ============================================================================
-- Register Callbacks
-- ============================================================================

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.POSTLOOP,  LevelFuncs.Engine.PhotoMode.OnLoop)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PREFREEZE, LevelFuncs.Engine.PhotoMode.OnFreeze)

return PhotoMode
