--- Internal file used by the RingInventory module.
-- @module RingInventory.ItemMenu
-- @local

--External Modules
local Menu = require("Engine.RingInventory.Menu")
local PickupData = require("Engine.RingInventory.PickupData")
local InventoryStates= require("Engine.RingInventory.InventoryStates")
local Save = require("Engine.RingInventory.Save")
local Settings = require("Engine.RingInventory.Settings")

--Pointers to tables
local COLOR_MAP = Settings.ColorMap
local INVENTORY_MODE = InventoryStates.MODE

local ItemMenu = {}

function ItemMenu.HasItemAction(packedFlags, flag)
    return (packedFlags & flag) ~= 0
end

local function HasChooseAmmo(menuActions)
    for _, flag in ipairs(PickupData.CHOOSE_AMMO_FLAGS) do
        if ItemMenu.HasItemAction(menuActions, flag) then
            return true
        end
    end
    return false
end

function ItemMenu.IsSingleItemAction(item)
    local flags = item:GetMenuActions()

    if flags == 0 then return false end

    -- Single flag set
    if (flags & (flags - 1)) == 0 then return true end

    return false
end

function ItemMenu.ParseAction(menuActions)
    if ItemMenu.HasItemAction(menuActions, ItemAction.USE) or ItemMenu.HasItemAction(menuActions, ItemAction.EQUIP) then
        ItemMenu.Hide()
        InventoryStates.SetMode(INVENTORY_MODE.ITEM_USE)
    elseif ItemMenu.HasItemAction(menuActions, ItemAction.EXAMINE) then
        InventoryStates.SetMode(INVENTORY_MODE.EXAMINE_OPEN)
    elseif ItemMenu.HasItemAction(menuActions, ItemAction.COMBINE) then
        local InventoryData = require("Engine.RingInventory.InventoryData")
        local combineItemCount = InventoryData.GetCombineItemsCount()
        if combineItemCount > 1 then 
            InventoryStates.SetMode(INVENTORY_MODE.COMBINE_SETUP)
        else
            TEN.Sound.PlaySound(Settings.SoundMap.playerNo)
        end
    elseif ItemMenu.HasItemAction(menuActions, ItemAction.STATISTICS) then
        InventoryStates.SetMode(INVENTORY_MODE.STATISTICS_OPEN)
    elseif ItemMenu.HasItemAction(menuActions, ItemAction.SAVE) then
        Save.SetSaveMenu()
        InventoryStates.SetMode(INVENTORY_MODE.SAVE_SETUP)
    elseif ItemMenu.HasItemAction(menuActions, ItemAction.LOAD) then
        Save.SetLoadMenu()
        InventoryStates.SetMode(INVENTORY_MODE.SAVE_SETUP)
    elseif ItemMenu.HasItemAction(menuActions, ItemAction.SEPARATE) then
        InventoryStates.SetMode(INVENTORY_MODE.SEPARATE)
    elseif ItemMenu.HasItemAction(menuActions, ItemAction.CHOOSE_AMMO_HK) then
        InventoryStates.SetMode(INVENTORY_MODE.WEAPON_MODE_SETUP)
    elseif HasChooseAmmo(menuActions) then
        InventoryStates.SetMode(INVENTORY_MODE.AMMO_SELECT_OPEN)
    end
end

function ItemMenu.DoItemAction()
    local menu = Menu.Get("menuActions")
    if not menu then return end

    local selectedItem = menu:GetCurrentItem()
    if selectedItem and selectedItem.actionBit then
        ItemMenu.ParseAction(selectedItem.actionBit)
    end
end

function ItemMenu.Create(item)
    local menuActions = {}
    
    local itemMenuActions = item:GetMenuActions()

    for _, entry in ipairs(PickupData.ItemActionFlags) do
        if ItemMenu.HasItemAction(itemMenuActions, entry.bit) then
            local allowInsert = true
            
            if entry.bit == ItemAction.COMBINE then
                local InventoryData = require("Engine.RingInventory.InventoryData")
                local itemCount = InventoryData.GetCombineItemsCount()
                allowInsert = (itemCount > 1)
            end
            
            if allowInsert then
                table.insert(menuActions, {
                    itemName = entry.string,
                    actionBit = entry.bit,
                    options = nil,
                    currentOption = 1
                })
            end
        end
    end
    
    local itemMenu = Menu.Create("menuActions", nil, menuActions, "Engine.RingInventory.DoItemAction", nil, Menu.Type.ITEMS_ONLY)
    
    itemMenu:SetItemsPosition(Vec2(50, 38))
    itemMenu:SetVisibility(true)
    itemMenu:SetLineSpacing(5.3)
    itemMenu:SetItemsFont(COLOR_MAP.plainText, 0.9)
    itemMenu:SetItemsTranslate(true)
    itemMenu:SetTitlePosition(Vec2(50, 4))
end

function ItemMenu.Show()
    Menu.AddActive("menuActions")
end

function ItemMenu.Hide()
    Menu.RemoveActive("menuActions")
end

-- ============================================================================
-- PUBLIC API (LevelFuncs.Engine.RingInventory)
-- ============================================================================
LevelFuncs.Engine.RingInventory = LevelFuncs.Engine.RingInventory or {}
LevelFuncs.Engine.RingInventory.DoItemAction = ItemMenu.DoItemAction

return ItemMenu