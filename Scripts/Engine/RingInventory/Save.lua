--- Internal file used by the RingInventory module.
-- @module RingInventory.Save
-- @local

--External Modules
local Menu = require("Engine.RingInventory.Menu")
local Settings = require("Engine.RingInventory.Settings")

--Pointers to tables
local SOUND_MAP = Settings.SoundMap
local COLOR_MAP = Settings.ColorMap

local Save = {}

local saveMenu = false --Checks if to create save or load menu
local quickSave = false --checks if quicksave is enabled
local saveSelected = false --checks if saveslot has been selected
local saveSlotSelected = 1 --index of save slot selected

local function DoSave()
    local slot = Menu.Get("SaveMenu2"):GetCurrentItemIndex()
    saveSlotSelected = slot
    Flow.SaveGame(slot - 1)
    saveSelected = true
    local InventoryStates = require("Engine.RingInventory.InventoryStates")
    InventoryStates.SetMode(InventoryStates.MODE.SAVE_CLOSE)
    Save.Hide()
    return true
end

local function DoLoad()
    local slot = Menu.Get("SaveMenu2"):GetCurrentItemIndex()

    if Flow.DoesSaveGameExist(slot - 1) then
        saveSlotSelected = slot
        Flow.LoadGame(slot - 1)
        saveSelected = true
        local InventoryStates = require("Engine.RingInventory.InventoryStates")
        InventoryStates.SetMode(InventoryStates.MODE.SAVE_CLOSE)
        Save.Hide()
        return true
    else
        TEN.Sound.PlaySound(SOUND_MAP.playerNo)
        return false
    end
end

function Save.CreateSaveMenu()
    local textPosition =
    {
        Vec2(10, 12),
        Vec2(20, 12),
        Vec2(75, 12),
        Vec2(50, 12),
    }
    
    local saveFunctions = {nil, "Engine.RingInventory.DoSave", nil, nil}
    local loadFunctions = {nil, "Engine.RingInventory.DoLoad", nil, nil}
    
    local soundMap =
    {
        [1] = {select = nil, choose = nil},
        [2] = {select = SOUND_MAP.menuSelect, choose = SOUND_MAP.menuChoose},
        [3] = {select = nil, choose = nil},
        [4] = {select = nil, choose = nil}
    }
    
    local itemFlag = {Strings.DisplayStringOption.SHADOW}
    local selectedFlag = {Strings.DisplayStringOption.BLINK, Strings.DisplayStringOption.SHADOW}
    
    local itemFlags =
    {
        itemFlag,
        itemFlag,
        itemFlag,
        {Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
    }
    
    local selectedFlags =
    {
        selectedFlag,
        selectedFlag,
        selectedFlag,
        {Strings.DisplayStringOption.BLINK, Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
    }
    
    local headers = Flow.GetSaveHeaders()
    local items = {[1] = {}, [2] = {}, [3] = {}, [4] = {}}
    
    for i = 1, #headers do
        local h = headers[i]
        local itemText1, itemText2, itemText3, itemText4
        
        if h and h.Present then
            itemText1 = string.format("%03d", h.Count)
            itemText2 = string.format("%s", h.LevelName)
            itemText3 = string.format("%02d:%02d:%02d", h.Hours, h.Minutes, h.Seconds)
            itemText4 = ""
        else
            itemText1 = ""
            itemText2 = ""
            itemText3 = ""
            itemText4 = "empty"
        end
        
        table.insert(items[1], {itemName = itemText1})
        table.insert(items[2], {itemName = itemText2})
        table.insert(items[3], {itemName = itemText3})
        table.insert(items[4], {itemName = itemText4})
    end
    
    if saveMenu then
        for index in ipairs(items) do
            Menu.Create("SaveMenu"..index, nil, items[index], saveFunctions[index], nil, Menu.Type.ITEMS_ONLY)
        end
    else
        for index in ipairs(items) do
            Menu.Create("SaveMenu"..index, nil, items[index], loadFunctions[index], nil, Menu.Type.ITEMS_ONLY)
        end
    end
    
    for index = 1, 4 do
        local saveList = Menu.Get("SaveMenu"..index)
        local translate = (index == 4)
        
        saveList:SetItemsPosition(textPosition[index])
        saveList:SetVisibility(true)
        saveList:SetLineSpacing(5.3)
        saveList:SetItemsFont(COLOR_MAP.plainText, 0.9, itemFlags[index])
        saveList:SetSelectedItemFlags(selectedFlags[index])
        saveList:SetItemsTranslate(translate)
        saveList:SetSoundEffects(soundMap[index].select, soundMap[index].choose)
        saveList:SetCurrentItem(saveSlotSelected)
    end
end

function Save.Show(instant)
    for index = 1, 4 do
        Menu.AddActive("SaveMenu"..index, instant)
    end
end

function Save.Hide()
    for index = 1, 4 do
        Menu.RemoveActive("SaveMenu"..index)
    end
end

--Sets the menu to Save menu
function Save.SetSaveMenu()
    saveMenu = true
end

--Sets the menu to Load menu
function Save.SetLoadMenu()
    saveMenu = false
end

function Save.IsLoadMenu()
    return not saveMenu
end

function Save.IsSaveMenu()
    return saveMenu
end

--Check if a save has been selected
function Save.IsSaveSelected()
    return saveSelected == true
end

function Save.ClearSaveSelected()
    saveSelected = false
end

--Set quick save status. True means menu is in quick save mode.
function Save.SetQuickSaveStatus(status)
    quickSave = status
end

--Check if Quick Save is enabled
function Save.IsQuickSaveEnabled()
    return quickSave == true
end

-- ============================================================================
-- PUBLIC API (LevelFuncs.Engine.RingInventory)
-- ============================================================================
LevelFuncs.Engine.RingInventory = LevelFuncs.Engine.RingInventory or {}
LevelFuncs.Engine.RingInventory.DoSave = DoSave
LevelFuncs.Engine.RingInventory.DoLoad = DoLoad

return Save