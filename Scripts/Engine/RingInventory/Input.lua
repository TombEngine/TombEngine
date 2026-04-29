--- Internal file used by the RingInventory module.
-- @module RingInventory.Input
-- @local

-- ============================================================================
-- INPUT HANDLING
-- ============================================================================

--External Modules
local Examine = require("Engine.RingInventory.Examine")
local InputHelpers = require("Engine.RingInventory.InputHelpers")
local ItemMenu = require("Engine.RingInventory.ItemMenu")
local InventoryData = require("Engine.RingInventory.InventoryData")
local InventoryStates = require("Engine.RingInventory.InventoryStates")
local Ring = require("Engine.RingInventory.Ring")
local Settings = require("Engine.RingInventory.Settings")

--Pointers to tables
local INVENTORY_MODE = InventoryStates.MODE
local RING = Ring.TYPE
local SOUND_MAP = Settings.SoundMap

local timer = 0

local Inputs = {}

local function GetHeldHorizontalDirection()
    local isLeftHeld = TEN.Input.IsKeyHeld(TEN.Input.ActionID.LEFT)
    local isRightHeld = TEN.Input.IsKeyHeld(TEN.Input.ActionID.RIGHT)

    if isLeftHeld and not isRightHeld then
        return -1
    end

    if isRightHeld and not isLeftHeld then
        return 1
    end

    return 0
end

local function DoLeftKey(ring)
    if InventoryStates.StartRingNavigation(ring, -1) then
        TEN.Sound.PlaySound(SOUND_MAP.menuRotate)
    end

end

local function DoRightKey(ring)
    if InventoryStates.StartRingNavigation(ring, 1) then
        TEN.Sound.PlaySound(SOUND_MAP.menuRotate)
    end

end

function Inputs.Update(mode, timeInMenu)
    timer = timeInMenu

    local selectedRing = InventoryData.GetSelectedRing()
    local selectedRingType = InventoryData.GetSelectedRingType()
    local selectedItem = selectedRing:GetSelectedItem()

    if mode == INVENTORY_MODE.RING_ROTATE then
        if InputHelpers.GuiIsPulsed(TEN.Input.ActionID.LEFT, timer) then
            DoLeftKey(selectedRing)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.RIGHT, timer) then
            DoRightKey(selectedRing)
        end

        return
    end

    if mode == INVENTORY_MODE.INVENTORY then
        if InputHelpers.GuiIsPulsed(TEN.Input.ActionID.LEFT, timer) then
            DoLeftKey(selectedRing)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.RIGHT, timer) then
            DoRightKey(selectedRing)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.FORWARD, timer) and selectedRingType < RING.COMBINE then
            local targetRing = math.max(RING.PUZZLE, selectedRingType - 1)
            if targetRing ~= selectedRingType and not InventoryData.GetRing(targetRing):IsEmpty() then
                InventoryStates.StartRingChange(targetRing, -1)
                TEN.Sound.PlaySound(SOUND_MAP.menuRotate)
            end
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.BACK, timer) and selectedRingType < RING.COMBINE then
            -- Add check for the options ring here to skip it if it is empty
            local targetRing = math.min(RING.OPTIONS, selectedRingType + 1)
            if targetRing ~= selectedRingType and not InventoryData.GetRing(targetRing):IsEmpty() then
                InventoryStates.StartRingChange(targetRing, 1)
                TEN.Sound.PlaySound(SOUND_MAP.menuRotate)
            end
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.ACTION, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.SELECT, timer) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            if ItemMenu.IsSingleItemAction(selectedItem) then
                ItemMenu.ParseAction(selectedItem:GetMenuActions())
            else
                InventoryStates.SetMode(INVENTORY_MODE.ITEM_SELECT)
            end
        elseif (InputHelpers.GuiIsPulsed(TEN.Input.ActionID.INVENTORY, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.DESELECT, timer)) and LevelVars.Engine.RingInventory.InventoryOpenFreeze then
            TEN.Sound.PlaySound(SOUND_MAP.inventoryClose)
            InventoryStates.SetMode(INVENTORY_MODE.RING_CLOSING)
        end
    elseif mode == INVENTORY_MODE.COMBINE then
        if InputHelpers.GuiIsPulsed(TEN.Input.ActionID.LEFT, timer) then
            DoLeftKey(selectedRing)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.RIGHT, timer) then
            DoRightKey(selectedRing)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.ACTION, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.SELECT, timer) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            InventoryStates.SetActionCheck(true)
        elseif (InputHelpers.GuiIsPulsed(TEN.Input.ActionID.INVENTORY, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.DESELECT, timer)) then
            InventoryStates.SetMode(INVENTORY_MODE.COMBINE_CLOSE)
        end
    elseif mode == INVENTORY_MODE.STATISTICS then
        if InputHelpers.GuiIsPulsed(TEN.Input.ActionID.ACTION, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.SELECT, timer) then
            if Settings.Statistics.gameStats then
                TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
                InventoryStates.SetActionCheck(true)
            end
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.INVENTORY, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.DESELECT, timer) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            InventoryStates.SetMode(INVENTORY_MODE.STATISTICS_CLOSE)
        end
    elseif mode == INVENTORY_MODE.WEAPON_MODE then
        if (InputHelpers.GuiIsPulsed(TEN.Input.ActionID.INVENTORY, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.DESELECT, timer)) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            InventoryStates.SetMode(INVENTORY_MODE.WEAPON_MODE_CLOSE)
        end
    elseif mode == INVENTORY_MODE.SAVE_MENU then
        if (InputHelpers.GuiIsPulsed(TEN.Input.ActionID.INVENTORY, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.DESELECT, timer)) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            InventoryStates.SetMode(INVENTORY_MODE.SAVE_CLOSE)
        end
    elseif mode == INVENTORY_MODE.ITEM_SELECTED then
        if (InputHelpers.GuiIsPulsed(TEN.Input.ActionID.INVENTORY, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.DESELECT, timer)) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            InventoryStates.SetMode(INVENTORY_MODE.ITEM_DESELECT)
        end
    elseif mode == INVENTORY_MODE.AMMO_SELECT then
        if InputHelpers.GuiIsPulsed(TEN.Input.ActionID.LEFT, timer) then
            DoLeftKey(selectedRing)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.RIGHT, timer) then
            DoRightKey(selectedRing)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.ACTION, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.SELECT, timer) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            InventoryStates.SetActionCheck(true)
        elseif (InputHelpers.GuiIsPulsed(TEN.Input.ActionID.INVENTORY, timer) or InputHelpers.GuiIsPulsed(TEN.Input.ActionID.DESELECT, timer)) then
            TEN.Sound.PlaySound(SOUND_MAP.inventoryClose)
            InventoryStates.SetMode(INVENTORY_MODE.AMMO_SELECT_CLOSE)
        end
    elseif mode == INVENTORY_MODE.EXAMINE then     
        if TEN.Input.IsKeyHeld(TEN.Input.ActionID.FORWARD) then
            Examine.ModifyRotation(1, 0, 0)
        elseif TEN.Input.IsKeyHeld(TEN.Input.ActionID.BACK) then
            Examine.ModifyRotation(-1, 0, 0)
        elseif TEN.Input.IsKeyHeld(TEN.Input.ActionID.LEFT) then
            Examine.ModifyRotation(0, 1, 0)
        elseif TEN.Input.IsKeyHeld(TEN.Input.ActionID.RIGHT) then
            Examine.ModifyRotation(0, -1, 0)
        elseif TEN.Input.IsKeyHeld(TEN.Input.ActionID.SPRINT) then
            Examine.ModifyScale(1)
        elseif TEN.Input.IsKeyHeld(TEN.Input.ActionID.CROUCH) then
            Examine.ModifyScale(-1)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.JUMP, timer) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            InventoryStates.SetActionCheck(true)
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.ACTION, timer) then
            Examine.ToggleText()
        elseif InputHelpers.GuiIsPulsed(TEN.Input.ActionID.INVENTORY, timer) then
            TEN.Sound.PlaySound(SOUND_MAP.menuChoose)
            InventoryStates.SetMode(INVENTORY_MODE.EXAMINE_CLOSE)
        end
    end
end

function Inputs.GetHeldRingDirection()
    return GetHeldHorizontalDirection()
end

return Inputs