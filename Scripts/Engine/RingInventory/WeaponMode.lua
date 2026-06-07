--- Internal file used by the RingInventory module. Handles weapon mode functions for ring inventory
-- @module RingInventory.WeaponMode
-- @local

--External Modules
local Menu = require("Engine.RingInventory.Menu")
local Settings = require("Engine.RingInventory.Settings")

--Pointers to constant tables
local COLOR_MAP = Settings.ColorMap

--Module Start
local WeaponMode = {}

function WeaponMode.ChangeWeaponMode()
    local index = Menu.Get("WeaponModeMenu"):GetCurrentItemIndex()
    Lara:SetWeaponMode(TEN.Objects.WeaponType.HK, index)

    local InventoryStates = require("Engine.RingInventory.InventoryStates")
    InventoryStates.SetMode(InventoryStates.MODE.WEAPON_MODE_CLOSE)
end

function WeaponMode.CreateWeaponModeMenu(itemData)
    local PickupData = require("Engine.RingInventory.PickupData")

    local weaponModes = {}
    
    for _, entry in ipairs(PickupData.WEAPON_MODE_LOOKUP) do
        if entry.weapon == itemData:GetObjectID() then
            table.insert(weaponModes, 
			{
                itemName = entry.string,
                actionBit = nil,
                options = nil,
                currentOption = 1
            })
        end
    end
    
    local modeIndex = Lara:GetWeaponMode(TEN.Objects.WeaponType.HK)
    local weaponModeMenu = Menu.Create("WeaponModeMenu", nil, weaponModes, "Engine.RingInventory.ChangeWeaponMode", nil, Menu.Type.ITEMS_ONLY)
    
    weaponModeMenu:SetItemsPosition(TEN.Vec2(50, 35))
    weaponModeMenu:SetVisibility(true)
    weaponModeMenu:SetLineSpacing(5.3)
    weaponModeMenu:SetItemsFont(COLOR_MAP.plainText, 0.9)
    weaponModeMenu:SetItemsTranslate(true)
    weaponModeMenu:SetCurrentItem(modeIndex)
    weaponModeMenu:SetTitle(nil, COLOR_MAP.headerText, nil, nil, true)
end

function WeaponMode.Show()
    Menu.AddActive("WeaponModeMenu")
end

function WeaponMode.Hide()
    Menu.RemoveActive("WeaponModeMenu")
end

-- ============================================================================
-- PUBLIC API (LevelFuncs.Engine.RingInventory)
-- ============================================================================
LevelFuncs.Engine.RingInventory = LevelFuncs.Engine.RingInventory or {}
LevelFuncs.Engine.RingInventory.ChangeWeaponMode = WeaponMode.ChangeWeaponMode

return WeaponMode