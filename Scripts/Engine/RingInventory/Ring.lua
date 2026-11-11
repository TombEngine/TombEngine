--- Internal file used by the RingInventory module. Manage Rings and items stored in them.
-- @module RingInventory.Ring
-- @local

--External Modules
local Constants = require("Engine.RingInventory.Constants")
local RingLight = require("Engine.RingInventory.RingLight")
local Settings = require("Engine.RingInventory.Settings")
local Utilities = require("Engine.RingInventory.Utilities")
local InventoryData
-- require("Addons.print_r")

--Pointer to tables

-- Ring Class - represents a single ring in the inventory
local Ring = {}
Ring.__index = Ring

-- Class Constants
Ring.RING_POSITION_PRIMARY = Vec3(0, 200, 1024) -- Position on the screen of whatever ring is currently focused
Ring.RING_POSITION_PRIMARY_OFFSET = Constants.RING_OFFSET
Ring.RING_POSITION_SECONDARY = Vec3(0, 230, 1024)
Ring.RING_RADIUS = -Constants.RING_RADIUS

Ring.TYPE =
{
	PUZZLE = "PUZZLE_RING",
    MAIN = "MAIN_RING",
	OPTIONS = "OPTIONS_RING",
	COMBINE = "COMBINE_RING",
	AMMO = "AMMO_RING"
}

--Pointers to specific rings
Ring.PRIMARY_TYPES = {
    Ring.TYPE.PUZZLE,
    Ring.TYPE.MAIN,
    Ring.TYPE.OPTIONS
}
Ring.DEFAULT_TYPE = Ring.TYPE.MAIN
Ring.COMBINE_TYPE = Ring.TYPE.COMBINE
Ring.AMMO_TYPE = Ring.TYPE.AMMO


--Internal variables for arithmetics, based on the order of the rings defined above
Ring.PRIMARY_TYPES__TYPE_TO_ORDER = {}
Ring.PRIMARY_TYPES__ORDER_TO_TYPE = {}
for order, ringName in pairs(Ring.PRIMARY_TYPES) do
    Ring.PRIMARY_TYPES__TYPE_TO_ORDER[ringName] = order
    Ring.PRIMARY_TYPES__ORDER_TO_TYPE[order] = ringName
end

-- Default center positions for different ring types
Ring.CENTERS = {}
-- Center position for main rings
local ringPosition = Ring.RING_POSITION_PRIMARY
local base = Ring.PRIMARY_TYPES__TYPE_TO_ORDER[Ring.DEFAULT_TYPE]
for order, ringName in ipairs(Ring.PRIMARY_TYPES) do
    local offset = Ring.RING_POSITION_PRIMARY_OFFSET * (order - base)
    local position = ringPosition + offset

    Ring.CENTERS[ringName] = position
end
-- Center position for secondary rings
-- (ie. every ring that has not had its position defined based on their order, which is done right above for main rings only)
for _, ringName in pairs(Ring.TYPE) do
    Ring.CENTERS[ringName] = Ring.CENTERS[ringName] or Ring.RING_POSITION_SECONDARY
end


-- Constructor
function Ring.Create(ringType)
    local self = setmetatable({}, Ring)
    
    -- Instance variables
    self.type = ringType
    self.items = {}
    self.itemsPositions = {}
    self.itemsRotations = {}
    self.selectedItemIndex = 1
    self.previousItemIndex = 1
    self.position = Vec3(0, 0, 0)
    self.previousPosition = Vec3(0, 0, 0)
    self.slice = 0
    self.visible = true
    
    self.currentAngle = 0
    self.previousAngle = 0
    self.targetAngle = 0
    
    Ring.RING_RADIUS = -Constants.RING_RADIUS * Utilities.GetAspectRatioMultiplier()

    return self
end

function Ring.GetCenterByType(name)
    if Ring.IsPrimaryRingType(name) then
        return Ring.CENTERS[name]
    else
        return Ring.RING_POSITION_SECONDARY
    end
end

function Ring.IsPrimaryRingType(ringType)
    return Utilities.Contains(Ring.PRIMARY_TYPES, ringType)
end


function Ring:IsPrimaryRing()
    return Ring.IsPrimaryRingType(self.type)
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
    self.itemsPositions = {}
    self.itemsRotations = {}
    self.selectedItemIndex = 1
    self.currentAngle = 0
    self.previousAngle = 0
    self.targetAngle = 0
end

function Ring:GetCenter()
    return Ring.GetCenterByType(self.type)
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

function Ring:SetAngleToSelectedItem()
    local itemIndex = self:GetSelectedItemIndex()
    local angle = Utilities.GetSelectedRingAngle(itemIndex, self:GetItemCount())

    self:SetCurrentAngle(angle)
    self:SetTargetAngle(angle)
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
function Ring:Translate(center, radius, rotationOffset)
    center = center or self.position
    radius = radius or Ring.RING_RADIUS
    rotationOffset = rotationOffset or 0
    
    local itemCount = #self.items
    if itemCount == 0 then return end

    for i = 1, itemCount do
        local currentItem = self.items[i]

        local angleDeg = (360 / itemCount) * (i - 1) + rotationOffset
        local position = center:Translate(TEN.Rotation(0, angleDeg, 0), radius)

        local itemPosition = position
        local itemRotation = TEN.Rotation(0, angleDeg, 0)

        self.itemsPositions[currentItem.objectID] = itemPosition
        self.itemsRotations[currentItem.objectID] = itemRotation
        currentItem:SetWorldPosition(itemPosition)
        currentItem:SetWorldRotation(itemRotation)
    end
end

function Ring:GetItemPosition(item)
    return self.itemsPositions[item.objectID]
end

function Ring:GetItemRotation(item)
    return self.itemsRotations[item.objectID]
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

function Ring:OffsetPosition(offsetPosition)
    self.previousPosition = self.position 
    self.position = self.previousPosition - offsetPosition
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