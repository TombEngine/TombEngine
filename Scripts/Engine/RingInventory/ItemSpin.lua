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
    if not ring then return end

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
        ItemSpin.rings[ringName].selectedItemEnabled = true
        ItemSpin.rings[ringName].firstFrame = true
        ItemSpin.rings[ringName].itemStates = {}
    end
end

-- Stop spinning for a ring
function ItemSpin.StopSpin(ring)
    if not ring then return end
    local ringName = ring:GetType()
    if ItemSpin.rings[ringName] then
        ItemSpin.rings[ringName].enabled = false
    end
end

-- Start spinning the selected item
function ItemSpin.StartSelectedItemSpin(ring)
    if not ring then return end
    local ringName = ring:GetType()
    if ItemSpin.rings[ringName] then
        ItemSpin.rings[ringName].selectedItemEnabled = true
    end
end

-- Stop spinning the selected item
function ItemSpin.StopSelectedItemSpin(ring)
    if not ring then return end
    local ringName = ring:GetType()
    if ItemSpin.rings[ringName] then
        ItemSpin.rings[ringName].selectedItemEnabled = false
    end
end

function ItemSpin.ClearRingState(ring)
    if not ring then return end

    local ringName = ring:GetType()
    local items = ring:GetItems() or {}

    for _, item in ipairs(items) do
        if item and item.objectID then
            local spinKey = ringName .. "Spin" .. item:GetObjectID()
            LevelVars.Engine.InterpolateProgress[spinKey] = nil
        end
    end

    ItemSpin.rings[ringName] = nil
end

-- Update all spinning items - call once per frame
function ItemSpin.Update()
    for _, ringState in pairs(ItemSpin.rings) do
        if ringState.enabled then
            ItemSpin.UpdateRing(ringState)
        end
    end
end

local function CalculateSpinbackDuration(angleDiff)
    local duration = math.abs(angleDiff) / (ItemSpin.SPINBACK_SPEED * 30)
    return math.max(duration, 1 / 30)
end

-- Update spinning for a single ring
function ItemSpin.UpdateRing(ringState)
    local ring = ringState.ring
    if not ring then return end

    local items = ring:GetItems()
    if not items or #items == 0 then return end

    local selectedItem = ring:GetSelectedItem()
    local previousItem = ring:GetPreviousItem()
    local ringAngle = ring:GetTargetAngle()
    local itemCount = #items
    local itemStates = ringState.itemStates

    local isFirstFrame = ringState.firstFrame
    if isFirstFrame then
        ringState.firstFrame = false
    end

    for i = 1, itemCount do
        local item = items[i]
        if item and item.objectID then
            local displayItem = item:GetDisplayItem()
            if displayItem then
                local currentRotation = displayItem:GetRotation()
                local id = item:GetObjectID()
                local spinKey = ringState.ring:GetType() .. "Spin" .. id

                if selectedItem and item == selectedItem then
                    -- Clear state and interpolation so next deselection starts fresh
                    itemStates[id] = nil
                    LevelVars.Engine.InterpolateProgress[spinKey] = nil

                    -- Selected item spins continuously
                    if ring:IsAtTargetAngle() and ringState.selectedItemEnabled then
                        local newY = (currentRotation.y + ItemSpin.ROTATION_SPEED) % 360
                        displayItem:SetRotation(Rotation(currentRotation.x, newY, currentRotation.z))
                    end
                else
                    local targetAngle = Utilities.GetRingItemAngle(i, itemCount, ringAngle)
                    local isJustDeselected = (not isFirstFrame) and previousItem and item == previousItem

                    -- Initialize state on first frame
                    if not itemStates[id] then
                        local angleDiff = Utilities.GetShortestAngleDelta(currentRotation.y, targetAngle)
                        itemStates[id] =
                        {
                            startAngle = currentRotation.y,
                            angleDiff = angleDiff,
                            lastTarget = targetAngle,
                            isSpinback = isJustDeselected,
                            spinbackDuration = CalculateSpinbackDuration(angleDiff)
                        }
                    end

                    local state = itemStates[id]

                    -- If target changed (ring rotated), restart animation from current position
                    if math.abs(targetAngle - state.lastTarget) > 0.1 then
                        local spinbackFinished = state.isSpinback and 
                            math.abs(Utilities.GetShortestAngleDelta(currentRotation.y, targetAngle)) <= ItemSpin.SPINBACK_SPEED

                        -- Keep spinback if still in progress, otherwise re-evaluate
                        local isSpinback = (state.isSpinback and not spinbackFinished) or isJustDeselected

                        local angleDiff = Utilities.GetShortestAngleDelta(currentRotation.y, targetAngle)
                        state.startAngle = currentRotation.y
                        state.angleDiff = angleDiff
                        state.lastTarget = targetAngle
                        state.isSpinback = isSpinback
                        state.spinbackDuration = CalculateSpinbackDuration(angleDiff)

                        LevelVars.Engine.InterpolateProgress[spinKey] = nil

                    end

                    if state.isSpinback then
                        local result = Interpolate.Calculate(spinKey, 0, 1, state.spinbackDuration, Interpolate.Easing.Smoothstep)
                        local newAngle = state.startAngle + state.angleDiff * result.output
                        displayItem:SetRotation(Rotation(currentRotation.x, newAngle, currentRotation.z))

                        if result.progress >= 1 then
                            state.isSpinback = false
                            state.startAngle = targetAngle
                            state.angleDiff = 0
                            LevelVars.Engine.InterpolateProgress[spinKey] = nil
                            displayItem:SetRotation(Rotation(currentRotation.x, targetAngle, currentRotation.z))
                        end
                    else
                        local result = Interpolate.Calculate(spinKey, 0, 1, ItemSpin.ALIGNMENT_SPEED, Interpolate.Easing.Smoothstep)
                        local newAngle = state.startAngle + state.angleDiff * result.output
                        displayItem:SetRotation(Rotation(currentRotation.x, newAngle, currentRotation.z))
                    end
                end
            end
        end
    end
end

-- Check if a ring is currently spinning
function ItemSpin.IsSpinning(ring)
    if not ring then return false end
    local ringName = ring:GetType()
    local ringState = ItemSpin.rings[ringName]
    return ringState and ringState.enabled
end

-- Reset all spinning state
function ItemSpin.Reset()
    ItemSpin.rings = {}
end

function ItemSpin.SnapToTargetAngles(ring)
    if not ring then return end

    local items = ring:GetItems()
    if not items then return end

    local selectedItem = ring:GetSelectedItem()
    local ringAngle = ring:GetTargetAngle()
    local itemCount = #items
    
    for i = 1, itemCount do
        local item = items[i]
        if item and item.objectID and not (selectedItem and item == selectedItem) then
            local displayItem = item:GetDisplayItem()
            if displayItem then
                local rot = displayItem:GetRotation()
                displayItem:SetRotation(Rotation(rot.x, Utilities.GetRingItemAngle(i, itemCount, ringAngle), rot.z))
            end
        end
    end
end

return ItemSpin
