--- Internal file used by the RingInventory module.
-- @module RingInventory.InventoryStates
-- @local

--External Modules
local Animation = require("Engine.RingInventory.Animation")
local Combine = require("Engine.RingInventory.Combine")
local Constants = require("Engine.RingInventory.Constants")
local Examine = require("Engine.RingInventory.Examine")
local Menu = require("Engine.RingInventory.Menu")
local InventoryData = require("Engine.RingInventory.InventoryData")
local Interpolate = require("Engine.RingInventory.Interpolate")
local ItemSpin = require("Engine.RingInventory.ItemSpin")
local PickupData = require("Engine.RingInventory.PickupData")
local Ring = require("Engine.RingInventory.Ring")
local Settings = require("Engine.RingInventory.Settings")
local Statistics = require("Engine.RingInventory.Statistics")
local Save = require("Engine.RingInventory.Save")
local Sprites = require("Engine.RingInventory.Sprites")
local Text = require("Engine.RingInventory.Text")
local Utilities = require("Engine.RingInventory.Utilities")
local WeaponMode = require("Engine.RingInventory.WeaponMode")

local ItemMenu
local Inputs

local InventoryStates = {}

-- Enums
InventoryStates.MODE = 
{
    INVENTORY            = 1,
    INVENTORY_OPENING    = 2,
    INVENTORY_EXIT       = 3,
    RING_OPENING         = 4,
    RING_CLOSING         = 5,
    RING_CHANGE          = 6,
    RING_ROTATE          = 7,
    ITEM_SELECT          = 8,
    ITEM_SELECTED        = 9,
    ITEM_DESELECT        = 10,
    ITEM_USE             = 11,
    STATISTICS_OPEN      = 12,
    STATISTICS           = 13,
    STATISTICS_CLOSE     = 14,
    EXAMINE_OPEN         = 15,
    EXAMINE              = 16,
    EXAMINE_CLOSE        = 17,
    COMBINE_SETUP        = 18,
    COMBINE_RING_OPENING = 19,
    COMBINE              = 20,
    COMBINE_SUCCESS      = 21,
    COMBINE_CLOSE        = 22,
    SEPARATE             = 23,
    AMMO_SELECT_OPEN     = 24,
    AMMO_SELECT          = 25,
    AMMO_SELECT_CLOSE    = 26,
    SAVE_SETUP           = 27,
    SAVE_MENU            = 28,
    SAVE_CLOSE           = 29,
    WEAPON_MODE_SETUP    = 30,
    WEAPON_MODE          = 31,
    WEAPON_MODE_CLOSE    = 32,
}

local INVENTORY_TEXT_CHANNELS =
{
    "ITEM_LABEL_PRIMARY",
    "ITEM_LABEL_SECONDARY",
    "HEADER",
    "SUB_HEADER",
    "CONTROLS_SELECT",
    "CONTROLS_BACK"
}

local ITEM_LABEL_CHANNELS =
{
    "ITEM_LABEL_PRIMARY",
    "ITEM_LABEL_SECONDARY"
}

local ITEM_USE_TEXT_CHANNELS =
{
    "ITEM_LABEL_PRIMARY",
    "ITEM_LABEL_SECONDARY",
    "HEADER",
    "SUB_HEADER"
}

-- Pointers to tables
local ANIM_SETTINGS = Settings.Animation
local COLOR_MAP = Settings.ColorMap
local SOUND_MAP = Settings.SoundMap
local UI_RING_FADE_SPEED = Utilities.GetAlphaLerpFactor(ANIM_SETTINGS.transitionSpeed) * 1.5

-- Variables
local inventoryClosed = false
local inventoryMode = InventoryStates.MODE.INVENTORY_OPENING
local previousMode = nil
local performAction = false
local onEnter = true
local timeInMenu = 0
local combineCloseTargetObjectID = nil

InventoryStates.GetActionCheck = function()
    return performAction
end

InventoryStates.SetActionCheck = function(check)
    performAction = check
end

InventoryStates.SetInventoryClosed = function(status)
    inventoryClosed = status
end

InventoryStates.GetInventoryClosed = function()
    return inventoryClosed
end

InventoryStates.SetMode = function(mode)
    previousMode = inventoryMode
    inventoryMode = mode
    return true
end

InventoryStates.GetMode = function()
    return inventoryMode
end

InventoryStates.GetPreviousMode = function()
    return previousMode
end

InventoryStates.IsMode = function(mode)
    return inventoryMode == mode
end

local CreateAmmoRing = function(item)
    if PickupData.WEAPON_SET[item:GetObjectID()] then
        local ammoRing = InventoryData.SetupSecondaryRing(Ring.TYPE.AMMO, InventoryData.GetChosenItem(), true)
        ammoRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemSelected)
        ItemSpin.StartSpin(ammoRing)
        Text.SetItemLabel(ammoRing:GetSelectedItem())
    end
end

local ShowAmmoRing = function(item)
    if PickupData.WEAPON_SET[item:GetObjectID()] then
        local ammoRing = InventoryData.GetRing(Ring.TYPE.AMMO)
        ammoRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemSelected)
        ItemSpin.StartSpin(ammoRing)
        Text.SetItemLabel(ammoRing:GetSelectedItem())
    end
end

local HideAmmoRing = function(item)
    if PickupData.WEAPON_SET[item:GetObjectID()] then
        local ammoRing = InventoryData.GetRing(Ring.TYPE.AMMO)
        ammoRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden)
        ItemSpin.StopSelectedItemSpin(ammoRing)
    end
end

local ShowSelectedAmmoName = function(weaponItem)
    if not weaponItem or weaponItem:GetType() ~= PickupData.TYPE.WEAPON then
        Text.HideItemSubLabel()
        return
    end

    local itemObjectID = weaponItem:GetObjectID()

    if not itemObjectID then
        Text.HideItemSubLabel()
        return
    end

    local weaponSlot = PickupData.WEAPON_SET[itemObjectID].slot

    local ammoType = Lara:GetAmmoType(weaponSlot)
    
    if not ammoType then
        Text.HideItemSubLabel()
        return
    end
    
    local objectID = PickupData.AMMO_TYPE_TO_OBJECT[ammoType]
    
    if not objectID then return end
    
    local base = PickupData.GetProperties(objectID)
    local data = InventoryData.BuildItem(base)
 
    Text.SetItemSubLabel(data)
end

local UpdateActionLabel = function(itemSelected, override, transitionType)
    local string = nil

    if  itemSelected and ItemMenu.IsSingleItemAction(itemSelected) then
        local itemActions = itemSelected:GetMenuActions()
        for _, entry in ipairs(PickupData.ItemActionFlags) do
            if ItemMenu.HasItemAction(itemActions, entry.bit) then
                string = Flow.GetString(entry.string)
                break
            end
        end
    elseif override then
        string = Flow.GetString(override)
    else
        string = Flow.GetString("actions_select")
    end

    local actionString = Input.GetActionBinding(ActionID.SELECT)..": "..string

    Text.SetText("CONTROLS_SELECT", actionString, true, transitionType)
end

local BeginSaveSetup = function(selectedRing, selectedItem, instantOpen)
    Sprites.ShowBackground()
    if Save.IsLoadMenu() then
        Text.SetText("HEADER", "load_game", true, instantOpen and Text.TRANSITION.NONE or nil)
    else
        Text.SetText("HEADER", "save_game", true, instantOpen and Text.TRANSITION.NONE or nil)
    end
    ItemMenu.Hide()
    Text.Hide("ITEM_LABEL_PRIMARY")
    Text.Hide("ITEM_LABEL_SECONDARY")
    Text.Hide("CONTROLS_SELECT")
    Text.Hide("CONTROLS_BACK")
    selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden, UI_RING_FADE_SPEED)
    ItemSpin.StopSelectedItemSpin(selectedRing)
    Sprites.HideArrows()
    if InventoryData.IsItemChosen() then
        ItemMenu.Hide()
        HideAmmoRing(selectedItem)
    end

    Save.CreateSaveMenu()
    Save.Show(instantOpen)

    onEnter = true
    inventoryMode = InventoryStates.MODE.SAVE_MENU
end

local UpdateInventoryTextsForSelectedItem = function(selectedItem, itemTransitionType, controlsTransitionType)
    Text.SetItemLabel(selectedItem, itemTransitionType)
    UpdateActionLabel(selectedItem, nil, controlsTransitionType or itemTransitionType)
    ShowSelectedAmmoName(selectedItem)
end

local UpdateBackLabel = function(label)
    local backstring

    if label then
        backstring = label
    else
        backstring = "close"
    end

    local string = Input.GetActionBinding(ActionID.DESELECT)..": "..Flow.GetString(backstring)
    Text.SetText("CONTROLS_BACK", string, true)
end

InventoryStates.StartRingNavigation = function(ring, direction)
    if not ring or ring:GetItemCount() <= 1 then
        return false
    end

    if direction < 0 then
        ring:SelectNext()
    else
        ring:SelectPrevious()
    end

    ring:CalculateRotation(direction)
    Interpolate.Clear("RingRotateAngle")

    local transitionType = direction < 0 and Text.TRANSITION.SWIPE_RIGHT or Text.TRANSITION.SWIPE_LEFT
    local selectedItem = ring:GetSelectedItem()

    UpdateInventoryTextsForSelectedItem(selectedItem, transitionType, Text.TRANSITION.CROSSFADE)

    if inventoryMode ~= InventoryStates.MODE.RING_ROTATE then
        InventoryStates.SetMode(InventoryStates.MODE.RING_ROTATE)
    end

    return true
end

InventoryStates.StartRingChange = function(targetRingType, offsetDirection)
    if not targetRingType then
        return
    end

    InventoryData.SwitchToRingType(targetRingType)
    InventoryData.OffsetAll(offsetDirection)

    local selectedRing = InventoryData.GetSelectedRing()
    local selectedItem = selectedRing:GetSelectedItem()
    UpdateInventoryTextsForSelectedItem(selectedItem, Text.TRANSITION.CROSSFADE)

    InventoryStates.SetMode(InventoryStates.MODE.RING_CHANGE)
end

local CreateStateContext = function()
    local context = {}

    context.RefreshSelection = function(self)
        self.selectedRing = InventoryData.GetSelectedRing()
        self.selectedItem = self.selectedRing:GetSelectedItem()
    end

    context:RefreshSelection()
    return context
end

local HideTextChannels = function(channels)
    for _, channelName in ipairs(channels) do
        Text.Hide(channelName)
    end
end

local SetInventoryOverviewText = function(selectedItem, itemTransitionType, controlsTransitionType)
    Text.SetText("HEADER", "actions_inventory", true)
    UpdateInventoryTextsForSelectedItem(selectedItem, itemTransitionType, controlsTransitionType)
    UpdateBackLabel()
end

local RestoreInventoryView = function(selectedRing, selectedItem, startSelectedItemSpin, ringFadeSpeed)
    SetInventoryOverviewText(selectedItem)
    selectedRing:Color(COLOR_MAP.itemDeselected, COLOR_MAP.itemSelected, ringFadeSpeed)
    Sprites.ShowArrows()

    if startSelectedItemSpin then
        ItemSpin.StartSelectedItemSpin(selectedRing)
    end
end

local ResetInventorySession = function()
    local selectedRing = InventoryData.GetSelectedRing()
    if selectedRing then
        local selectedItem = selectedRing:GetSelectedItem()
        if selectedItem then
            InventoryData.SaveFocusedItem(selectedItem)
        end
    end

    Examine.Clear()
    InventoryData.ClearDisplayItems()
    InventoryData.Reset()
    Sprites.Clear()
    TEN.Inventory.SetFocusedItem(Constants.NO_VALUE)
    Interpolate.ClearAll()
    ItemSpin.Reset()
    Menu.DeleteAll()
    InventoryStates.SetMode(InventoryStates.MODE.INVENTORY_OPENING)
    InventoryData.SwitchToRingType(Ring.TYPE.MAIN)
    TEN.View.DisplayItem.ResetCamera()
    Text.DestroyAll()
    timeInMenu = 0
    InventoryData.SetChosenItem()
    InventoryStates.SetInventoryClosed(true)
    InventoryData.ClearAll()
    Flow.SetFreezeMode(Flow.FreezeMode.NONE)
end

local ReturnToPreviousModeOrInventory = function()
    if previousMode then
        inventoryMode = previousMode
    else
        InventoryStates.SetMode(InventoryStates.MODE.INVENTORY)
    end
end

-- ============================================================================
-- STATE HANDLER FUNCTIONS
-- ============================================================================

local HandleInventoryOpening = function(state)
    TEN.View.SetPostProcessMode(View.PostProcessMode.NONE)
    Text.Setup()

    if Save.IsQuickSaveEnabled() then
        BeginSaveSetup(state.selectedRing, state.selectedItem, true)
        return
    end

    if Statistics.IsEndStatisticsEnabled() then
        Sprites.ShowBackground()
        InventoryStates.SetMode(InventoryStates.MODE.STATISTICS_OPEN)
        return
    end

    Text.SetText("HEADER", "actions_inventory", true)
    TEN.Sound.PlaySound(SOUND_MAP.inventoryOpen)
    InventoryData.Construct()
    InventoryData.OpenAtItem(InventoryData.GetOpenAtItem(), true)

    state:RefreshSelection()
    UpdateInventoryTextsForSelectedItem(state.selectedItem)
    UpdateBackLabel()

    inventoryMode = InventoryStates.MODE.RING_OPENING
end

local HandleInventoryExit = function()
    ResetInventorySession()
end

local HandleRingOpening = function(state)
    if onEnter then
        Sprites.ShowBackground()
        Sprites.ShowArrows()
        state.selectedRing:Color(COLOR_MAP.itemDeselected, COLOR_MAP.itemSelected)
        ItemSpin.StartSpin(state.selectedRing)
        onEnter = false
    end

    if Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        state:RefreshSelection()
        SetInventoryOverviewText(state.selectedItem)
        onEnter = true
        InventoryData.ColorAll(COLOR_MAP.itemDeselected, COLOR_MAP.itemSelected, true)
        InventoryStates.SetMode(InventoryStates.MODE.INVENTORY)
    end
end

local HandleRingClosing = function(state)
    if onEnter then
        HideTextChannels(INVENTORY_TEXT_CHANNELS)
        InventoryData.ColorAll(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden)
        Sprites.HideBackground()
        Sprites.HideArrows()
        InventoryData.SetVisibility(false, true)
        onEnter = false
    end

    if ANIM_SETTINGS.skipRingClose or Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        onEnter = true
        InventoryStates.SetMode(InventoryStates.MODE.INVENTORY_EXIT)
    end
end

local HandleRingRotate = function(state)
    if not Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        return
    end

    state.selectedRing:SetCurrentAngle(state.selectedRing:GetTargetAngle())
    ReturnToPreviousModeOrInventory()
end

local HandleRingChange = function(state)
    if Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        ItemSpin.StartSpin(state.selectedRing)
        InventoryStates.SetMode(InventoryStates.MODE.INVENTORY)
    end
end

local HandleExamineOpen = function(state)
    Animation.SaveItemData(state.selectedItem)
    ItemMenu.Hide()
    Text.SetText("HEADER", "examine", true)
    HideTextChannels(ITEM_LABEL_CHANNELS)
    state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden, UI_RING_FADE_SPEED)
    Examine.Show(state.selectedItem)
    Text.Hide("CONTROLS_SELECT")
    UpdateBackLabel("back")
    Sprites.HideArrows()
    HideAmmoRing(state.selectedItem)
    InventoryStates.SetMode(InventoryStates.MODE.EXAMINE)
end

local HandleExamine = function(state)
    if InventoryStates.GetActionCheck() then
        Examine.ResetExamine(state.selectedItem)
        InventoryStates.SetActionCheck(false)
    end
end

local HandleExamineClose = function(state)
    local isItemChosen = InventoryData.IsItemChosen()
    Examine.Hide()

    if isItemChosen then
        ItemMenu.Show()
        Text.SetText("HEADER", state.selectedItem:GetName(), true)
        ShowAmmoRing(state.selectedItem)
        state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemSelected)
        UpdateActionLabel()
        UpdateBackLabel("actions_deselect")
        InventoryStates.SetMode(InventoryStates.MODE.ITEM_SELECTED)
        return
    end

    RestoreInventoryView(state.selectedRing, state.selectedItem)
    InventoryStates.SetMode(InventoryStates.MODE.INVENTORY)
end

local HandleItemSelect = function(state)
    if onEnter then
        ItemSpin.StopSelectedItemSpin(state.selectedRing)
        Animation.SaveItemData(state.selectedItem)
        Animation.SetItemStartPos(state.selectedItem)
        InventoryData.SetChosenItem(state.selectedItem)
        state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemSelected)
        Text.SetText("HEADER", state.selectedItem:GetName(), true)
        HideTextChannels(ITEM_LABEL_CHANNELS)
        UpdateActionLabel()
        UpdateBackLabel("actions_deselect")
        ItemMenu.Create(state.selectedItem)
        ItemMenu.Show()
        Sprites.HideArrows()
        CreateAmmoRing(state.selectedItem)
        onEnter = false
    end

    if Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        onEnter = true
        InventoryStates.SetMode(InventoryStates.MODE.ITEM_SELECTED)
    end
end

local HandleItemDeselect = function(state)
    if onEnter then
        SetInventoryOverviewText(state.selectedItem)
        ItemMenu.Hide()
        state.selectedRing:Color(COLOR_MAP.itemDeselected, COLOR_MAP.itemSelected)

        local ammoRing = InventoryData.GetRing(Ring.TYPE.AMMO)
        ItemSpin.StopSpin(ammoRing)
        ammoRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden)
        Sprites.ShowArrows()
        onEnter = false
    end

    if Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        InventoryData.SetChosenItem(nil)
        ItemSpin.StartSelectedItemSpin(state.selectedRing)
        onEnter = true
        InventoryStates.SetMode(InventoryStates.MODE.INVENTORY)
    end
end

local HandleItemUse = function(state)
    if onEnter then
        HideTextChannels(ITEM_USE_TEXT_CHANNELS)
        InventoryData.ColorAll(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden, true)
        onEnter = false
    end

    if ANIM_SETTINGS.skipRingClose or Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        onEnter = true
        TEN.Inventory.UseItem(state.selectedItem.objectID)
        InventoryStates.SetMode(InventoryStates.MODE.RING_CLOSING)
    end
end

local HandleStatisticsOpen = function(state)
    if onEnter then
        if not InventoryData.IsItemChosen() then
            Animation.SaveItemData(state.selectedItem)
            Animation.SetItemStartPos(state.selectedItem)
        end
        Text.Hide("ITEM_LABEL_PRIMARY")
        Text.SetText("HEADER", "statistics", true)
        Sprites.HideArrows()
        UpdateBackLabel("back")
        ItemSpin.StopSelectedItemSpin(state.selectedRing)
        Statistics.SetupStats()
        Statistics.Show()
        state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemSelected)

        if InventoryData.IsItemChosen() then
            ItemMenu.Hide()
            HideAmmoRing(state.selectedItem)
        end

        if Settings.Statistics.gameStats and not Statistics.IsEndStatisticsEnabled() then
            UpdateActionLabel(nil, "game_statistics")
        else
            Text.Hide("CONTROLS_SELECT")
        end

        onEnter = false
    end

    if InventoryData.IsItemChosen() or Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        onEnter = true
        inventoryMode = InventoryStates.MODE.STATISTICS
    end
end

local HandleStatistics = function()
    if InventoryStates.GetActionCheck() and not Statistics.IsEndStatisticsEnabled()then
        Statistics.ToggleType(Text.TRANSITION.SWIPE_RIGHT)

        if Statistics.GetType() then
            UpdateActionLabel(nil, "level_statistics")
        else
            UpdateActionLabel(nil, "game_statistics")
        end

        InventoryStates.SetActionCheck(false)
    end
end

local HandleStatisticsClose = function(state)
    local isItemChosen = InventoryData.IsItemChosen()

    if onEnter then
        Statistics.Hide()

        if not isItemChosen then
            RestoreInventoryView(state.selectedRing, state.selectedItem, true)
        end

        onEnter = false
    end

    if Statistics.IsEndStatisticsEnabled() or isItemChosen  or Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        onEnter = true

        if isItemChosen then
            ItemMenu.Show()
            UpdateActionLabel()
            Text.SetText("HEADER", state.selectedItem:GetName(), true)
            ShowAmmoRing(state.selectedItem)
            InventoryStates.SetMode(InventoryStates.MODE.ITEM_SELECTED)
        else
            if Statistics.IsEndStatisticsEnabled() then
                InventoryStates.SetMode(InventoryStates.MODE.INVENTORY_EXIT)
                Statistics.SetEndStatistics(false)
            else
                InventoryStates.SetMode(InventoryStates.MODE.INVENTORY)
            end
        end
    end
end

local HandleSaveSetup = function(state)
    if onEnter then
        BeginSaveSetup(state.selectedRing, state.selectedItem, false)
    end
end

local HandleSaveClose = function(state)
    if onEnter then
        Save.Hide()

        if not InventoryData.IsItemChosen() then
            RestoreInventoryView(state.selectedRing, state.selectedItem, true)
        end

        onEnter = false
    end

    if InventoryData.IsItemChosen() then
        ItemMenu.Show()
        ShowAmmoRing(state.selectedItem)
        Text.SetText("HEADER", state.selectedItem:GetName(), true)
        UpdateActionLabel()
        UpdateBackLabel()
        state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemSelected)
        onEnter = true
        inventoryMode = InventoryStates.MODE.ITEM_SELECTED
    elseif Save.IsSaveSelected() or Save.IsQuickSaveEnabled() then
        Save.ClearSaveSelected()
        Save.SetQuickSaveStatus(false)
        onEnter = true
        inventoryMode = InventoryStates.MODE.INVENTORY_EXIT
    else
        onEnter = true
        inventoryMode = InventoryStates.MODE.INVENTORY
    end
end

local HandleCombineSetup = function(state)
    if onEnter then
        ItemMenu.Hide()
        Animation.SaveItemData(state.selectedItem)
        Animation.SetItemStartPos(state.selectedItem)
        ItemSpin.StopSelectedItemSpin(state.selectedRing)
        state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemSelected)
        Sprites.HideArrows()
        onEnter = false
    end

    if InventoryData.IsItemChosen() or Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        onEnter = true
        InventoryData.SetChosenItem(state.selectedItem)
        HideAmmoRing(state.selectedItem)

        local combineRing = InventoryData.SetupSecondaryRing(Ring.TYPE.COMBINE)
        combineRing:Color(COLOR_MAP.itemDeselected, COLOR_MAP.itemSelected)
        Text.SetText("HEADER", state.selectedItem.name, true)
        Text.SetText("SUB_HEADER", "combine_with", true)
        inventoryMode = InventoryStates.MODE.COMBINE_RING_OPENING
    end
end

local HandleCombineRingOpening = function(state)
    if Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        Text.SetItemLabel(state.selectedItem)
        UpdateActionLabel(state.selectedItem)
        inventoryMode = InventoryStates.MODE.COMBINE
    end
end

local HandleCombine = function(state)
    if not InventoryStates.GetActionCheck() then
        return
    end

    if Combine.CombineItems(InventoryData.GetChosenItem(), state.selectedItem) then
        TEN.Sound.PlaySound(SOUND_MAP.menuCombine)
        Animation.SaveItemData(state.selectedItem)
        Animation.SetItemStartPos(state.selectedItem)
        inventoryMode = InventoryStates.MODE.COMBINE_SUCCESS
    else
        TEN.Sound.PlaySound(SOUND_MAP.playerNo)
        InventoryStates.SetActionCheck(false)
    end
end

local HandleCombineSuccess = function(state)
    if not Animation.Inventory(inventoryMode, state.selectedRing, state.selectedItem) then
        return
    end

    state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden, UI_RING_FADE_SPEED)
    local chosenItem = InventoryData.GetChosenItem()
    if chosenItem then
        local _, ringType = InventoryData.FindItem(chosenItem:GetObjectID())
        if ringType then
            InventoryData.GetRing(ringType):Color(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden, UI_RING_FADE_SPEED)
        end
    end

    inventoryMode = InventoryStates.MODE.COMBINE_CLOSE
end

local HandleCombineClose = function(state)
    local chosenItem = InventoryData.GetChosenItem()
    local transitionItem = Combine.GetResults() and state.selectedItem or chosenItem or state.selectedItem

    if onEnter then
        combineCloseTargetObjectID = Combine.GetResults() or (chosenItem and chosenItem:GetObjectID())

        Animation.SaveItemData(transitionItem)
        Text.Hide("SUB_HEADER")
        Text.SetText("HEADER", "actions_inventory", true)
        ItemMenu.Hide()
        InventoryStates.SetActionCheck(false)
        state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden, UI_RING_FADE_SPEED)
        Sprites.HideArrows()
        onEnter = false
    end

    if Animation.Inventory(inventoryMode, state.selectedRing, transitionItem) then
        InventoryData.SetOpenAtItem(combineCloseTargetObjectID)
        InventoryData.SetChosenItem(nil)
        Combine.ClearResults()
        InventoryData.RemoveRing(Ring.TYPE.COMBINE)
        InventoryData.Construct()
        InventoryData.OpenAtItem(InventoryData.GetOpenAtItem(), true)

        state:RefreshSelection()
        InventoryData.ColorAll(COLOR_MAP.itemDeselected, COLOR_MAP.itemSelected)
        ItemSpin.SnapToTargetAngles(state.selectedRing)
        ItemSpin.StartSelectedItemSpin(state.selectedRing)
        SetInventoryOverviewText(state.selectedItem)
        Sprites.ShowArrows()

        combineCloseTargetObjectID = nil
        onEnter = true
        inventoryMode = InventoryStates.MODE.INVENTORY
    end
end

local HandleSeparate = function(state)
    Combine.SeparateItems(state.selectedItem)
    InventoryData.SetOpenAtItem(Combine.GetResults())
    ItemMenu.Hide()
    Text.SetText("HEADER", "actions_inventory", true)
    state.selectedRing:Color(COLOR_MAP.itemHidden, COLOR_MAP.itemHidden, UI_RING_FADE_SPEED)
    inventoryMode = InventoryStates.MODE.COMBINE_CLOSE
end

local HandleAmmoSelectOpen = function(state)
    Animation.SaveItemData(state.selectedItem)

    local ammoRing = InventoryData.GetRing(Ring.TYPE.AMMO)
    ammoRing:Color(COLOR_MAP.itemDeselected, COLOR_MAP.itemSelected)
    InventoryData.SwitchToRingType(Ring.TYPE.AMMO)
    ItemMenu.Hide()
    UpdateActionLabel(ammoRing:GetSelectedItem())
    UpdateBackLabel("back")
    Text.SetText("SUB_HEADER", "choose_ammo", true)
    inventoryMode = InventoryStates.MODE.AMMO_SELECT
end

local HandleAmmoSelect = function(state)
    if InventoryStates.GetActionCheck() then
        local ammo = PickupData.AMMO_SET[state.selectedItem.objectID]
        Lara:SetAmmoType(ammo.slot)
        inventoryMode = InventoryStates.MODE.AMMO_SELECT_CLOSE
    end
end

local HandleAmmoSelectClose = function(state)
    InventoryStates.SetActionCheck(false)
    InventoryData.ReturnToPreviousRing()
    InventoryData.GetRing(Ring.TYPE.AMMO):Color(COLOR_MAP.itemHidden, COLOR_MAP.itemSelected)
    Text.Hide("SUB_HEADER")
    UpdateActionLabel()
    UpdateBackLabel("actions_deselect")
    ItemMenu.Show()
    Text.SetItemLabel(state.selectedItem)
    inventoryMode = InventoryStates.MODE.ITEM_SELECTED
end

local HandleWeaponModeSetup = function(state)
    WeaponMode.CreateWeaponModeMenu(state.selectedItem)
    WeaponMode.Show()
    ItemMenu.Hide()
    UpdateBackLabel("back")
    inventoryMode = InventoryStates.MODE.WEAPON_MODE
end

local HandleWeaponModeClose = function()
    WeaponMode.Hide()
    UpdateBackLabel("actions_deselect")
    ItemMenu.Show()
    inventoryMode = InventoryStates.MODE.ITEM_SELECTED
end

local STATE_HANDLERS =
{
    [InventoryStates.MODE.INVENTORY] = function() end,
    [InventoryStates.MODE.INVENTORY_OPENING] = HandleInventoryOpening,
    [InventoryStates.MODE.INVENTORY_EXIT] = HandleInventoryExit,
    [InventoryStates.MODE.RING_OPENING] = HandleRingOpening,
    [InventoryStates.MODE.RING_CLOSING] = HandleRingClosing,
    [InventoryStates.MODE.RING_ROTATE] = HandleRingRotate,
    [InventoryStates.MODE.RING_CHANGE] = HandleRingChange,
    [InventoryStates.MODE.EXAMINE_OPEN] = HandleExamineOpen,
    [InventoryStates.MODE.EXAMINE] = HandleExamine,
    [InventoryStates.MODE.EXAMINE_CLOSE] = HandleExamineClose,
    [InventoryStates.MODE.ITEM_SELECT] = HandleItemSelect,
    [InventoryStates.MODE.ITEM_SELECTED] = function() end,
    [InventoryStates.MODE.ITEM_DESELECT] = HandleItemDeselect,
    [InventoryStates.MODE.ITEM_USE] = HandleItemUse,
    [InventoryStates.MODE.STATISTICS_OPEN] = HandleStatisticsOpen,
    [InventoryStates.MODE.STATISTICS] = HandleStatistics,
    [InventoryStates.MODE.STATISTICS_CLOSE] = HandleStatisticsClose,
    [InventoryStates.MODE.SAVE_SETUP] = HandleSaveSetup,
    [InventoryStates.MODE.SAVE_MENU] = function() end,
    [InventoryStates.MODE.SAVE_CLOSE] = HandleSaveClose,
    [InventoryStates.MODE.COMBINE_SETUP] = HandleCombineSetup,
    [InventoryStates.MODE.COMBINE_RING_OPENING] = HandleCombineRingOpening,
    [InventoryStates.MODE.COMBINE] = HandleCombine,
    [InventoryStates.MODE.COMBINE_SUCCESS] = HandleCombineSuccess,
    [InventoryStates.MODE.COMBINE_CLOSE] = HandleCombineClose,
    [InventoryStates.MODE.SEPARATE] = HandleSeparate,
    [InventoryStates.MODE.AMMO_SELECT_OPEN] = HandleAmmoSelectOpen,
    [InventoryStates.MODE.AMMO_SELECT] = HandleAmmoSelect,
    [InventoryStates.MODE.AMMO_SELECT_CLOSE] = HandleAmmoSelectClose,
    [InventoryStates.MODE.WEAPON_MODE_SETUP] = HandleWeaponModeSetup,
    [InventoryStates.MODE.WEAPON_MODE] = function() end,
    [InventoryStates.MODE.WEAPON_MODE_CLOSE] = HandleWeaponModeClose
}

local UpdateInventoryFrame = function(selectedRing)
    Statistics.UpdateIngameTime()

    Examine.Update()
    Examine.Draw()
    Menu.UpdateActiveMenus()
    Menu.DrawActiveMenus()
    InventoryData.SetItemRotations(timeInMenu)
    Inputs.Update(inventoryMode, timeInMenu)
    InventoryData.DrawAllRings()
    Text.Update()
    Text.DrawAll()
    Sprites.Update(selectedRing)
    Sprites.Draw()
end

InventoryStates.Update = function()
    timeInMenu = timeInMenu + 1
	
    if not Inputs then
        Inputs = require("Engine.RingInventory.Input")
    end

    if not ItemMenu then
        ItemMenu = require("Engine.RingInventory.ItemMenu")
    end

    local state = CreateStateContext()
    local handler = STATE_HANDLERS[inventoryMode]

    if handler then
        handler(state)
    end

    UpdateInventoryFrame(state.selectedRing)
end

return InventoryStates