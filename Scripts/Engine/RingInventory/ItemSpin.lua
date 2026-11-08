--- Internal file used by the RingInventory module.
-- @module RingInventory.ItemSpin
-- @local

local Settings = require("Engine.RingInventory.Settings")
local Interpolate = require("Engine.RingInventory.Interpolate")
local Utilities = require("Engine.RingInventory.Utilities")

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
            firstFrame = true,
            itemStates = {}
        }
    else
        ItemSpin.rings[ringName].enabled = true
        ItemSpin.rings[ringName].firstFrame = true
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

    local selectedItem = ring:GetSelectedItem()
    local itemCount = #items
    local itemStates = ringState.itemStates

    local isFirstFrame = ringState.firstFrame
    if isFirstFrame then
        ringState.firstFrame = false
    end

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

        if ringState.enabled and ringState.selectedItemEnabled and selectedItem and item == selectedItem then
            -- Selected item spins continuously
            state.currentSpinAngle = (state.currentSpinAngle + ItemSpin.ROTATION_SPEED) % 360
            state.isSpinning = true
            -- Clear potential residual spinback data, if the item was in the middle of a spinback before selection
            if state.spinback then
                state.spinback = nil
                Interpolate.Clear(spinKey)
            end
        else
            local wasSpinning = state.isSpinning
            state.isSpinning = false
            local isJustDeselected = wasSpinning
            local targetAngle = 0

            -- Initialize spin back animation if this item was just deselected
            if isJustDeselected then
                local angleDiff = Utilities.GetShortestAngleDelta(state.currentSpinAngle, targetAngle)
                state.spinback = {
                    startAngle = state.currentSpinAngle,
                    angleDiff = angleDiff,
                    duration = CalculateSpinbackDuration(angleDiff)
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
