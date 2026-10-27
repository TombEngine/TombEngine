--- Internal file used by the RingInventory module.
-- @module RingInventory.InventoryItem
-- @local

-- ============================================================================
-- INVENTORY ITEM METHODS
-- ============================================================================
local Utilities = require("Engine.RingInventory.Utilities")

local InventoryItem = {}
InventoryItem.__index = InventoryItem

function InventoryItem:GetObjectID()
    return self.objectID
end

function InventoryItem:GetName()
    return self.name
end

function InventoryItem:GetType()
    return self.type
end

function InventoryItem:GetYOffset()
    return self.yOffset
end

function InventoryItem:GetScale()
    return self.scale
end

function InventoryItem:GetRotation()
    return self.rotation
end

function InventoryItem:GetOrientation()
    return self.orientation
end

function InventoryItem:GetMeshBits()
    return self.meshBits
end

function InventoryItem:GetCount()
    return self.count
end

function InventoryItem:GetMenuActions()
    return self.menuActions
end

function InventoryItem:GetRingType()
    return self.ringName
end

function InventoryItem:GetDisplayItem()
    return self.displayItem
end

-- ============================================================================
-- DISPLAY ITEM MANAGEMENT
-- ============================================================================

-- Create and store DisplayItem for this item
function InventoryItem:CreateDisplayItem(defaultPosition)
    local scaleFactor = Utilities.GetAspectRatioMultiplier()
    local finalScale = scaleFactor * self.scale
    self.displayItem = TEN.View.DisplayItem(
        self.objectID,
        defaultPosition,
        self.rotation,
        Vec3(finalScale),
        self.meshBits
    )
    return self.displayItem
end

-- Clear the DisplayItem reference
function InventoryItem:ClearDisplayItem()
    self.displayItem = nil
end

function InventoryItem:Draw()

    if self.displayItem then
        self.displayItem:Draw()
    end
end

-- ============================================================================
-- UTILITY METHODS
-- ============================================================================

function InventoryItem:CanCombine()
    return self.combine ~= nil
end

function InventoryItem:IsType(typeName)
    return self.type == typeName
end

function InventoryItem.New(data)
    return setmetatable(data, InventoryItem)
end

return InventoryItem