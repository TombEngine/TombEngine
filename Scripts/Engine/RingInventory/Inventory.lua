---
-- This module enables classic ring inventory in TEN levels. 
-- Example usage:
--
--	local RingInventory = require("Engine.RingInventory.Inventory")
--
-- Global ring inventory settings.
-- Settings is composed of several sub-tables, and each section of the Settings documentation corresponds to one of these sub-tables.
-- These configuration groups are located in *Settings.lua* script file inside RingInventory folder.
--
-- It is possible to change settings on a per-level basis via @{RingInventory.GetSettings} and @{RingInventory.SetSettings} functions, but keep in mind that
-- _Settings.lua is reread every time the level is reloaded_. Therefore, you need to implement custom settings management in your level script
-- if you want to override global settings.
-- @luautil RingInventory

--External Modules
local Constants = require("Engine.RingInventory.Constants")
local InventoryData = require("Engine.RingInventory.InventoryData")
local InventoryStates
local ItemSpin = require("Engine.RingInventory.ItemSpin")
local RingLight = require("Engine.RingInventory.RingLight")
local Save = require("Engine.RingInventory.Save")
local Settings = require("Engine.RingInventory.Settings")
local Statistics = require("Engine.RingInventory.Statistics")
local Strings = require("Engine.RingInventory.Strings")
local Utilities = require("Engine.RingInventory.Utilities")

--Pointers to tables
local COLOR_MAP = Settings.ColorMap

--Local Variables
local inventoryDelay = 0

local inventorySetup = true
local inventoryOpen = false
local inventoryRunning = false

LevelFuncs.Engine.RingInventory = LevelFuncs.Engine.RingInventory or {}

-- ============================================================================
-- MAIN FUNCTIONS
-- ============================================================================
LevelFuncs.Engine.RingInventory.UpdateInventory = function()
    if not inventoryRunning then
        return
    end

    if not InventoryStates then
        InventoryStates = require("Engine.RingInventory.InventoryStates")
    end

    RingLight.Update()
    ItemSpin.Update()
    InventoryStates.Update()

end

LevelFuncs.Engine.RingInventory.RunInventory = function()
    if not InventoryStates then
        InventoryStates = require("Engine.RingInventory.InventoryStates")
    end

    if inventorySetup then
        LevelVars.Engine.RingInventory = {}
        inventoryOpen = false
        InventoryStates.SetInventoryClosed(false)
        inventoryRunning = false
        TEN.View.SetPostProcessMode(View.PostProcessMode.NONE)
        TEN.View.SetPostProcessStrength(1)
        TEN.View.SetPostProcessTint(COLOR_MAP.itemSelected)
        local settings = TEN.Flow.GetSettings()
        settings.Gameplay.enableInventory = false
        TEN.Flow.SetSettings(settings)
        
        inventorySetup = false
    end
    
    local playerHp = Lara:GetHP() > 0
    local isNotUsingBinoculars = TEN.View.GetCameraType() ~= CameraType.BINOCULARS
    
    if (TEN.Input.IsKeyHit(TEN.Input.ActionID.INVENTORY) or TEN.Inventory.GetFocusedItem() ~= Constants.NO_VALUE) and 
       not inventoryOpen and 
       playerHp and 
       isNotUsingBinoculars then
        inventoryOpen = true
        local focusedItem = TEN.Inventory.GetFocusedItem()
        if focusedItem == Constants.NO_VALUE then
            local savedItem = LevelVars.Engine.RingInventory.lastFocusedItem or Constants.NO_VALUE
            focusedItem = InventoryData.LoadFocusedItem(savedItem)
        end
        InventoryData.SetOpenAtItem(focusedItem)
        inventoryDelay = 0
    end
    
    if (TEN.Input.IsKeyHit(TEN.Input.ActionID.SAVE) or TEN.Inventory.GetFocusedItem() ~= Constants.NO_VALUE) and 
       not inventoryOpen and 
       playerHp and 
       isNotUsingBinoculars then
        inventoryOpen = true
        Save.SetQuickSaveStatus(true)
        Save.SetSaveMenu()
        inventoryDelay = 0
    end
    
    if (TEN.Input.IsKeyHit(TEN.Input.ActionID.LOAD) or TEN.Inventory.GetFocusedItem() ~= Constants.NO_VALUE) and 
       not inventoryOpen and 
       isNotUsingBinoculars then
        inventoryOpen = true
        Save.SetQuickSaveStatus(true)
        Save.SetLoadMenu()
        inventoryDelay = 0
    end

    if (Statistics.IsEndStatisticsEnabled()) and 
       not inventoryOpen and 
       isNotUsingBinoculars then
        inventoryOpen = true
        inventoryDelay = 0
    end
    
    if inventoryOpen == true then
        local requiredDelay = 2

        inventoryDelay = inventoryDelay + 1
        
        if Settings.Background.enable ~= true then
            TEN.View.SetPostProcessMode(View.PostProcessMode.MONOCHROME)
            TEN.View.SetPostProcessStrength(COLOR_MAP.background.a / Constants.ALPHA_MAX)
            TEN.View.SetPostProcessTint(COLOR_MAP.background)
        end
        
        if inventoryDelay >= requiredDelay then
            TEN.View.DisplayItem.SetCameraPosition(Constants.CAMERA_START)
            TEN.View.DisplayItem.SetTargetPosition(Constants.TARGET_START)
            TEN.View.DisplayItem.SetFOV(Constants.FOV)
            TEN.View.DisplayItem.SetAmbientLight(COLOR_MAP.inventoryAmbient)
            inventoryRunning = true
            inventoryOpen = false
            Flow.SetFreezeMode(Flow.FreezeMode.FULL)
        end
    end
    
    if InventoryStates.GetInventoryClosed() then
        if Settings.Background.enable ~= true then
            TEN.View.SetPostProcessMode(View.PostProcessMode.NONE)
            TEN.View.SetPostProcessStrength(1)
            TEN.View.SetPostProcessTint(COLOR_MAP.itemSelected)
        end
        InventoryStates.SetInventoryClosed(false)
        inventoryRunning = false
    end
end

local InventoryModule = {}

---Get settings tables for RingInventory.
-- @function RingInventory.GetSettings
-- @treturn Settings Current settings table
InventoryModule.GetSettings = function()
    return Utilities.CopyTable(Settings)
end

---Set settings tables for RingInventory.
-- @function RingInventory.SetSettings
-- @tparam Settings newSettings Required settings table
InventoryModule.SetSettings = function(newSettings)
    for section, values in pairs(newSettings) do
        if Settings[section] ~= nil then
            for setting, value in pairs(values) do
                if Settings[section][setting] ~= nil then
                    Settings[section][setting] = value
                end
            end
        end
    end
end

--- Background
-- @section Background
-- These settings control the inventory background sprite display.
-- @usage
-- -- In the level's lua file
-- local settings = RingInventory.GetSettings()
-- settings.Background.enable = false
-- RingInventory.SetSettings(settings)

--- Whether the background sprite is rendered.
-- @tfield[opt=true] bool enable If true, the background sprite will be displayed behind the inventory.

--- The object ID used to source the background sprite.
-- @tfield[opt=TEN.Objects.ObjID.DIARY_ENTRY_SPRITES] Objects.ObjID objectID Object ID for the background's sprite.

--- The sprite index within the object to use as the background.
-- @tfield[opt=0] int spriteID Sprite ID from the specified object for the background's sprite.

--- Tint color applied to the background sprite.
-- @tfield[opt=TEN.Color(255&#44; 255&#44; 255)] Color color Color of background's sprite.

--- Screen position of the background sprite's anchor point, in percent.
-- @tfield[opt=TEN.Vec2(50&#44; 50)] Vec2 position X,Y position of the background sprite in screen percent (0-100).

--- Rotation of the background sprite in degrees.
-- @tfield[opt=0] float rotation Rotation of the background's sprite (0-360), in degrees.

--- Scale of the background sprite as a percentage of screen size.
-- @tfield[opt=TEN.Vec2(100&#44; 100)] Vec2 scale X,Y Scaling factor for the background's sprite.

--- Alignment mode used when positioning the background sprite.
-- @tfield[opt=TEN.View.AlignMode.CENTER] View.AlignMode alignMode Alignment for the background's sprite.

--- Scaling mode used when sizing the background sprite.
-- @tfield[opt=TEN.View.ScaleMode.STRETCH] View.ScaleMode scaleMode Scaling for the background's sprite.

--- Blend mode used when rendering the background sprite.
-- @tfield[opt=TEN.Effects.BlendID.ALPHA_BLEND] Effects.BlendID blendMode Blending modes for the background's sprite.

--- Overall opacity of the background sprite.
-- @tfield[opt=255] int alpha Opacity value from 0 (fully transparent) to 255 (fully opaque).

--- SoundMap
-- @section SoundMap
-- These settings map inventory UI events to sound effect IDs.
-- Sound IDs correspond to entries in the game's sound catalogue.
-- @usage
-- -- Example of overriding the inventory open sound
-- -- In the level's lua file
-- local settings = RingInventory.GetSettings()
-- settings.SoundMap.inventoryOpen = 42
-- RingInventory.SetSettings(settings)

--- Sound played when Lara has no item available.
-- @tfield[opt=2] int playerNo Sound effect ID triggered when the player attempts to use an unavailable item.

--- Sound played when rotating the inventory ring.
-- @tfield[opt=108] int menuRotate Sound effect ID triggered while scrolling through inventory items.

--- Sound played when hovering over or highlighting a menu option.
-- @tfield[opt=109] int menuSelect Sound effect ID triggered on item selection highlight.

--- Sound played when confirming a menu choice.
-- @tfield[opt=111] int menuChoose Sound effect ID triggered when the player confirms a selected action.

--- Sound played when combining two inventory items.
-- @tfield[opt=114] int menuCombine Sound effect ID triggered when two compatible items are combined.

--- Sound played when the inventory is opened.
-- @tfield[opt=109] int inventoryOpen Sound effect ID triggered when the inventory ring is opened.

--- Sound played when the inventory is closed.
-- @tfield[opt=109] int inventoryClose Sound effect ID triggered when the inventory ring is closed.

--- ColorMap
-- @section ColorMap
-- These settings define the colors used throughout the inventory UI.
-- Colors are of type @{Color}.
-- @usage
-- -- Example of changing the selected item highlight color
-- -- In the level's lua file
-- local settings = RingInventory.GetSettings()
-- settings.ColorMap.itemSelected = TEN.Color(200, 180, 60, 255)
-- RingInventory.SetSettings(settings)

--- Color used for standard body text in the inventory.
-- @tfield[opt=Flow.GetSettings().UI.plainTextColor] Color plainText Applied to descriptive text.

--- Color used for section headers and titles.
-- @tfield[opt=Flow.GetSettings().UI.headerTextColor] Color headerText Applied to inventory category headings and titles.

--- Color used for selectable option text.
-- @tfield[opt=Flow.GetSettings().UI.optionTextColor] Color optionText Applied to text entries.

--- Background tint color for the inventory.
-- @tfield[opt=Color(64&#44; 64&#44; 64&#44; 128)] Color background Semi-transparent overlay color drawn behind inventory content. The alpha channel determines the strenght of the effect.

--- Ambient light color cast on inventory item models.
-- @tfield[opt=Color(255&#44; 255&#44; 128)] Color inventoryAmbient Light applied to Inventory items.

--- Color used to render hidden inventory items.
-- @tfield[opt=Color(0&#44; 0&#44; 0&#44; 0)] Color itemHidden Fully transparent; items with this color are invisible in the ring.

--- Color used to render unselected inventory items.
-- @tfield[opt=Color(32&#44; 32&#44; 32&#44; 255)] Color itemDeselected Tint applied to items that are not currently highlighted.

--- Color used to render the currently selected inventory item.
-- @tfield[opt=Color(128&#44; 128&#44; 128&#44; 255)] Color itemSelected Tint applied to the item the player has focused on.

--- Color used to render the neutral sprites.
-- @tfield[opt=Color(255&#44; 255&#44; 255&#44; 255)] Color neutral Tint applied to the sprites.

--- Animation
-- @section Animation
-- These settings determine the animations of the Inventory.
-- @usage
-- -- In the level's lua file
-- local settings = RingInventory.GetSettings()
-- settings.Animations.skipRingClose = true
-- RingInventory.SetSettings(settings)

--- Duration of the inventory ring open/close transition.
-- @tfield[opt=0.5] float inventoryAnimTime Time in seconds for the ring to animate.

--- Duration of the per-item spin or presentation animation.
-- @tfield[opt=0.2] float itemAnimTime Time in seconds for an individual item to animate into its focused pose.

--- Skip the ring collapse animation when closing the inventory.
-- @tfield[opt=false] bool skipRingClose If true, the inventory closes instantly without playing the ring-retract animation.

--- Speed at which inventory text fades in and out.
-- @tfield[opt=50] float transitionSpeed Alpha change applied per frame. Higher values cause faster fades.

--- Statistics
-- @section Statistics
-- These settings control time progression in Inventory and Game statistics display.
-- @usage
-- -- Example of disabling the game statistics screen
-- -- In the level's lua file
-- local settings = RingInventory.GetSettings()
-- settings.Statistics.gameStats = false
-- RingInventory.SetSettings(settings)

--- Progress time while in Inventory. Stopwatch hands are also moving.
-- @tfield[opt=true] bool progressTime If true, time is progressed in the inventory.

--- Display the full game statistics in Statistics mode.
-- @tfield[opt=true] bool gameStats If true, full game statistics are show in Statistics mode in Inventory.

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_FREEZE, LevelFuncs.Engine.RingInventory.UpdateInventory)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP, LevelFuncs.Engine.RingInventory.RunInventory)

return InventoryModule