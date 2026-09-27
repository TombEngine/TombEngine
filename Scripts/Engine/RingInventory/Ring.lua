--- Internal file used by the RingInventory module. Manage Rings and items stored in them.
-- @module RingInventory.Ring
-- @local

--External Modules
local Constants = require("Engine.RingInventory.Constants")
local RingLight = require("Engine.RingInventory.RingLight")
local Settings = require("Engine.RingInventory.Settings")
local Utilities = require("Engine.RingInventory.Utilities")

--Pointer to tables

-- Ring Class - represents a single ring in the inventory
local Ring = {}
Ring.__index = Ring

-- Class Constants
local RING_POSITION_OFFSET = 1000
Ring.RING_RADIUS = -Constants.RING_RADIUS

Ring.TYPE =
{
	PUZZLE = 1,
    MAIN = 2,
	OPTIONS = 3,
	COMBINE = 4,
	AMMO = 5
}

-- Default center positions for different ring types
Ring.CENTERS =
{
    [Ring.TYPE.PUZZLE] = Vec3(0, -800, 1024),
    [Ring.TYPE.MAIN] = Vec3(0, 200, 1024),
    [Ring.TYPE.OPTIONS] = Vec3(0, 1200, 1024),
    [Ring.TYPE.COMBINE] = Vec3(0, 230, 1024),
    [Ring.TYPE.AMMO] = Vec3(0, 230, 1024)
}

-- Constructor
function Ring.Create(ringType, centerPosition)
    local self = setmetatable({}, Ring)
    
    -- Instance variables
    self.type = ringType
    self.items = {}
    self.selectedItemIndex = 1
    self.previousItemIndex = 1
    self.position = centerPosition or Vec3(0, 0, 0)
    self.previousPosition = centerPosition or Vec3(0, 0, 0)
    self.slice = 0
    self.visible = true
    
    self.currentAngle = 0
    self.previousAngle = 0
    self.targetAngle = 0
    
    Ring.RING_RADIUS = -Constants.RING_RADIUS * Utilities.GetAspectRatioMultiplier()

    return self
end

-- Recalculate slice based on item count
function Ring:RecalculateSlice()
    if #self.items > 0 then
        self.slice = 360 / #self.items
    else
        self.slice = 0
    end
end

-- Add item to this ring
function Ring:AddItem(item)
    table.insert(self.items, item)
    self:RecalculateSlice()
end

-- Remove item by object ID
function Ring:RemoveItem(objectID)
    for i, item in ipairs(self.items) do
        if item.objectID == objectID then
            table.remove(self.items, i)
            -- Adjust selected index if needed
            if self.selectedItemIndex > #self.items then
                self.selectedItemIndex = math.max(1, #self.items)
            end
            self:RecalculateSlice()
            return true
        end
    end
    return false
end

function Ring:ClearDisplayItems()
    for _, item in ipairs(self.items) do
        item:ClearDisplayItem()
    end
end

-- Clear all items
function Ring:Clear()
    self:ClearDisplayItems()
    self.items = {}
    self.selectedItemIndex = 1
    self.currentAngle = 0
    self.previousAngle = 0
    self.targetAngle = 0
end

-- Get item by object ID
function Ring:GetItem(objectID)
    for _, item in ipairs(self.items) do
        if item.objectID == objectID then
            return item
        end
    end
    return nil
end

-- Get all items
function Ring:GetItems()
    return self.items
end

-- Get item count
function Ring:GetItemCount()
    return #self.items
end

-- Get selected item
function Ring:GetSelectedItem()
    if #self.items == 0 then return nil end
    return self.items[self.selectedItemIndex]
end

-- Get selected item
function Ring:GetPreviousItem()
    if #self.items == 0 then return nil end
    return self.items[self.previousItemIndex]
end

-- Set selected item by index
function Ring:SetSelectedItemIndex(index)
    if index >= 1 and index <= #self.items then
        self.previousItemIndex = self.selectedItemIndex
        self.selectedItemIndex = index
        return true
    end
    return false
end

-- Set selected item by object ID
function Ring:SetSelectedItemByID(objectID)
    for i, item in ipairs(self.items) do
        if item.objectID == objectID then
            self.previousItemIndex = self.selectedItemIndex
            self.selectedItemIndex = i
            return true
        end
    end
    return false
end

-- Get selected item index
function Ring:GetSelectedItemIndex()
    return self.selectedItemIndex
end

-- Get previous item index
function Ring:GetPreviousItemIndex()
    return self.previousItemIndex
end

-- Navigate to next item
function Ring:SelectNext()
    if #self.items == 0 then return end
    self.previousItemIndex = self.selectedItemIndex
    self.selectedItemIndex = (self.selectedItemIndex % #self.items) + 1
end

-- Navigate to previous item
function Ring:SelectPrevious()
    if #self.items == 0 then return end
    self.previousItemIndex = self.selectedItemIndex
    self.selectedItemIndex = self.selectedItemIndex - 1
    if self.selectedItemIndex < 1 then
        self.selectedItemIndex = #self.items
    end
end

-- Translate items in a circle
function Ring:Translate(center, radius, rotationOffset, alpha)
    alpha = alpha or 1.0
    center = center or self.position
    radius = radius or Ring.RING_RADIUS
    rotationOffset = rotationOffset or 0
    
    local itemCount = #self.items
    if itemCount == 0 then return end

    for i = 1, itemCount do
        local currentItem = self.items[i].objectID
        local currentDisplayItem = self.items[i]:GetDisplayItem()
        if currentDisplayItem then
            local angleDeg = (360 / itemCount) * (i - 1) + rotationOffset
            local position = center:Translate(Rotation(0, angleDeg, 0), radius)
            currentDisplayItem:SetPosition(Utilities.OffsetY(position, self.items[i].yOffset))
        end
    end
end

-- Color items in the ring
function Ring:Color(color, selectedItemColor, fadeSpeed)
    RingLight.SetRingColors(self, color, selectedItemColor, fadeSpeed)
end

-- Set ring position
function Ring:SetPosition(position)
    self.previousPosition = self.position 
    self.position = position
end

function Ring:OffsetPosition(direction)
    self.previousPosition = self.position 
    self.position = Vec3(self.previousPosition.x, self.previousPosition.y - direction * RING_POSITION_OFFSET, self.previousPosition.z)
end

-- Get ring position
function Ring:GetPosition()
    return self.position
end

-- Get ring position
function Ring:GetPreviousPosition()
    return self.previousPosition
end

-- Get ring type
function Ring:GetType()
    return self.type
end

-- Set slice value
function Ring:SetSlice(slice)
    self.slice = slice
end

-- Get slice value
function Ring:GetSlice()
    return self.slice
end

-- Get current angle
function Ring:GetCurrentAngle()
    return self.currentAngle
end

-- Set current angle
function Ring:SetCurrentAngle(angle)
    self.currentAngle = angle
end

-- Get previous angle
function Ring:GetPreviousAngle()
    return self.previousAngle
end

-- Get target angle
function Ring:GetTargetAngle()
    return self.targetAngle
end

-- Set target angle
function Ring:SetTargetAngle(angle)
    self.previousAngle = self.targetAngle
    self.targetAngle = angle
end

-- Calculate rotation angle
function Ring:CalculateRotation(direction)
    self.previousAngle = self.currentAngle

    local baseAngle = self.targetAngle
    if math.abs(self.targetAngle - self.currentAngle) <= 0.1 then
        baseAngle = self.currentAngle
    end

    self.targetAngle = baseAngle + direction * self.slice
end

-- Set all angles at once
function Ring:SetAngles(current, previous, target)
    if current ~= nil then self.currentAngle = current end
    if previous ~= nil then self.previousAngle = previous end
    if target ~= nil then self.targetAngle = target end
end

-- Get all angles at once
function Ring:GetAngles()
    return
	{
        current = self.currentAngle,
        previous = self.previousAngle,
        target = self.targetAngle
    }
end

-- Interpolate current angle towards target
function Ring:InterpolateAngle(alpha)
    alpha = alpha or 0.1
    self.previousAngle = self.currentAngle
    self.currentAngle = self.currentAngle + (self.targetAngle - self.currentAngle) * alpha
    return self.currentAngle
end

-- Check if angle has reached target
function Ring:IsAtTargetAngle(threshold)
    threshold = threshold or 0.1
    return math.abs(self.targetAngle - self.currentAngle) < threshold
end

-- Reset all angles to zero
function Ring:ResetAngles()
    self.currentAngle = 0
    self.previousAngle = 0
    self.targetAngle = 0
end

-- Check if ring is empty
function Ring:IsEmpty()
    return #self.items == 0
end

-- Find item index by object ID
function Ring:FindItemIndex(objectID)
    for i, item in ipairs(self.items) do
        if item.objectID == objectID then
            return i
        end
    end
    return nil
end

function Ring:SetVisibility(visible)
    self.visible = visible
end

--Draw items in the ring
function Ring:Draw()
    if not self.visible then return end

    for i = 1, #self.items do
        local item = self.items[i]
        item:Draw()
    end
end

return Ring