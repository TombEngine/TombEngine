--- Internal file used by the RingInventory module.
-- @module RingInventory.ItemSpin
-- @local

local Settings = require("Engine.RingInventory.Settings")
local Interpolate = require("Engine.RingInventory.Interpolate")
local Utilities = require("Engine.RingInventory.Utilities")
local Ring
local InventoryData

local ItemSpin = {}

ItemSpin.ROTATION_SPEED = 5 -- Degrees per frame, continuous spin for selected item
ItemSpin.SPINBACK_SPEED = 5 -- Degrees per frame, spinback for just-deselected item
ItemSpin.ALIGNMENT_SPEED = Settings.Animation.itemAnimTime -- Seconds, for ring rotation alignment

ItemSpin.rings = {}

-- Start spinning for a ring
function ItemSpin.StartSpin(ring)
    local ringName = ring:GetType()

    if not ItemSpin.rings[ringName] then
        ItemSpin.rings[ringName] = {
            enabled = true,
            selectedItemEnabled = true,
            ring = ring,
            itemStates = {}
        }
    else
        ItemSpin.rings[ringName].enabled = true
    end
end

-- Stop spinning for a ring
function ItemSpin.StopSpin(ring)
    local ringName = ring:GetType()
    if ItemSpin.rings[ringName] then
        ItemSpin.rings[ringName].enabled = false
    end
end

-- Start spinning the selected item
function ItemSpin.StartSelectedItemSpin(ring)
    local ringName = ring:GetType()
    if ItemSpin.rings[ringName] then
        ItemSpin.rings[ringName].selectedItemEnabled = true
    end
end

-- Stop spinning the selected item
function ItemSpin.StopSelectedItemSpin(ring)
    local ringName = ring:GetType()
    if ItemSpin.rings[ringName] then
        ItemSpin.rings[ringName].selectedItemEnabled = false
    end
end

function ItemSpin.ClearRingState(ring)
    local ringName = ring:GetType()
    local items = ring:GetItems() or {}

    for _, item in ipairs(items) do
        if item and item.objectID then
            local spinKey = ringName .. "Spin" .. item:GetObjectID()
            Interpolate.Clear(spinKey)
        end
    end

    ItemSpin.rings[ringName] = nil
end

-- Update all spinning items - call once per frame
function ItemSpin.Update()
    for _, ringState in pairs(ItemSpin.rings) do
        ItemSpin.UpdateRing(ringState)
    end
end

local function CalculateSpinbackDuration(angleDiff)
    local duration = math.abs(angleDiff) / (ItemSpin.SPINBACK_SPEED * 30)
    return math.max(duration, 1 / 30)
end

-- Update spinning for a single ring
function ItemSpin.UpdateRing(ringState)
    local ring = ringState.ring

    local items = ring:GetItems()
    if #items == 0 then return end

    -- TEMPORARY HACK: we have 2 types of spinback: slow spinback = item deselected / fast spinback = item chosen.
    -- Currently, to determine if we should even initiate a spinback, we rely on whether this ring's state was set to Spin
    -- or not spin anymore.
    -- But this interferes with the desired result: when an weapon item is selected (ITEM_SELECTED mode) and we open its Ammo
    -- ring, and then we exit it, we (logically) set ringState.enabled to false, because we don't want the ammo to spin unless
    -- we currently have the focus on the ammo ring itself (implying it's ready for user input to select another ammo item).
    -- This is falsely registered as "item deselected" because the ring itself is not spinning anymore, so it triggers a slow
    -- spinback (as if something was deselected) instead of a fast one (returning to the item menu, with this weapon and this
    -- ammo chosen).
    -- The hack = if we're handling an ammo ring, we'll just lie and say the ring is always selected (in a way, it's true: when
    -- "exiting" the Ammo selection ring, it still remains the selected ring amongst the secondary rings... even if it's the only
    -- one. Just like the Main ring is still the selected ring amongst the primary rings when we enter the item select menu).
    -- Ideally: we would just have a way to know if a ring is the selected ring in its own "area" (primary or secondary).
    if not InventoryData then
        InventoryData = require("Engine.RingInventory.InventoryData")
    end
    if not Ring then
        Ring = require("Engine.RingInventory.Ring")
    end

    local isSelectedRing = ring == InventoryData.GetSelectedRing()
    if not Utilities.Contains(Ring.PRIMARY_TYPES, ring:GetType()) then
        -- The hack.
        -- We're targeting the ammo ring primarily because that's where the "issue" was visible, but the combine ring would also be
        -- affected... if we weren't resetting the inventory everytime we exit this ring (possible improvement for the near future).
        isSelectedRing = true
    end

    local selectedItem = ring:GetSelectedItem()
    local itemCount = #items
    local itemStates = ringState.itemStates

    for i = 1, itemCount do
        local item = items[i]
        local id = item:GetObjectID()
        local spinKey = ringState.ring:GetType() .. "Spin" .. id
        if not itemStates[id] then
            itemStates[id] = {
                currentSpinAngle = 0,
                isSpinning = false,
                spinback = nil
            }
        end
        local state = itemStates[id]

        local wasSpinning = state.isSpinning

        local isRingSpinEnabled = ringState.enabled and ringState.selectedItemEnabled
        local isItemSelected = selectedItem and item == selectedItem and isSelectedRing

        local shouldSpin = isRingSpinEnabled and isItemSelected

        if shouldSpin then
            -- Selected item spins continuously
            state.currentSpinAngle = (state.currentSpinAngle + ItemSpin.ROTATION_SPEED) % 360
            state.isSpinning = true
            -- Clear potential residual spinback data, if the item was in the middle of a spinback before selection
            if state.spinback then
                state.spinback = nil
                Interpolate.Clear(spinKey)
            end
        else
            state.isSpinning = false

            local shouldSpinbackBegin = wasSpinning
            local shouldSpinbackBeFast = isItemSelected

            -- Initialize spin back animation if this item was just deselected
            if shouldSpinbackBegin then
                local targetAngle = 0
                local angleDiff = Utilities.GetShortestAngleDelta(state.currentSpinAngle, targetAngle)
                state.spinback = {
                    startAngle = state.currentSpinAngle,
                    angleDiff = angleDiff,
                    duration = shouldSpinbackBeFast and ItemSpin.ALIGNMENT_SPEED or CalculateSpinbackDuration(angleDiff)
                }
            end

            if state.spinback then
                local result = Interpolate.Calculate(spinKey, 0, 1, state.spinback.duration, Interpolate.Easing.Smoothstep)
                state.currentSpinAngle = state.spinback.startAngle + (state.spinback.angleDiff * result.output)

                -- Spinback finished: clear its animation data
                if result.progress >= 1 then
                    state.spinback = nil
                    Interpolate.Clear(spinKey)
                end
            end
        end

        item:SetAdditionalRotation(TEN.Rotation(0, state.currentSpinAngle, 0))
    end
end

-- Reset all spinning state
function ItemSpin.Reset()
    ItemSpin.rings = {}
end

return ItemSpin
