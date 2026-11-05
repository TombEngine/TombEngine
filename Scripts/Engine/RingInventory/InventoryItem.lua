--- Internal file used by the RingInventory module.
-- @module RingInventory.InventoryItem
-- @local

--External Modules
local Settings = require("Engine.RingInventory.Settings")

--Pointers to constant tables
local COLOR_MAP = Settings.ColorMap

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

function InventoryItem:GetDisplayItem() -- TODO Should probably disappear eventually
    return self.displayData.displayItem
end

-- ============================================================================
-- DISPLAY ITEM MANAGEMENT
-- ============================================================================

function InventoryItem:SetColor(color)
    self.displayData.color = color
    if self.displayData.displayItem then
        self.displayData.displayItem:SetColor(color)
    end
end
function InventoryItem:GetColor()
    return self.displayData.color
end

function InventoryItem:SetAdditionalPosition(position)
    self.displayData.locationAdditional.position = position
    self.displayData.displayLocationDirty = true
end
function InventoryItem:SetAdditionalRotation(rotation)
    self.displayData.locationAdditional.rotation = rotation
    self.displayData.displayLocationDirty = true
end
function InventoryItem:SetAdditionalScale(scale)
    self.displayData.locationAdditional.scale = scale
    self.displayData.displayLocationDirty = true
end

function InventoryItem:SetWorldPosition(position)
    self.displayData.locationWorld.position = position
    self.displayData.displayLocationDirty = true
end
function InventoryItem:SetWorldRotation(rotation)
    self.displayData.locationWorld.rotation = rotation
    self.displayData.displayLocationDirty = true
end
function InventoryItem:SetWorldScale(scale)
    self.displayData.locationWorld.scale = scale
    self.displayData.displayLocationDirty = true
end

function InventoryItem:GetWorldPosition()
    return self.displayData.locationWorld.position
end
function InventoryItem:GetWorldRotation()
    return self.displayData.locationWorld.rotation
end

function InventoryItem:RefreshDisplayLocation()
    if not self.displayData.displayLocationDirty then
        return
    end

    self.displayData.locationDisplay.position = (
        self.displayData.locationLocal.position
        + self.displayData.locationAdditional.position
        + self.displayData.locationWorld.position
    )
    self.displayData.locationDisplay.rotation = (
        self.displayData.locationLocal.rotation + TEN.Rotation(0, 180, 0) -- TODO check if this is really the place to flip the item, or if this should be specific to rings
        + self.displayData.locationAdditional.rotation
        + self.displayData.locationWorld.rotation
    )
    self.displayData.locationDisplay.scale = (
        self.displayData.locationLocal.scale
        * self.displayData.locationAdditional.scale
        * self.displayData.locationWorld.scale
        * Utilities.GetAspectRatioMultiplier()
    )

    if self.displayData.displayItem then
        self.displayData.displayItem:SetPosition(self.displayData.locationDisplay.position)
        self.displayData.displayItem:SetRotation(self.displayData.locationDisplay.rotation)
        self.displayData.displayItem:SetScale(self.displayData.locationDisplay.scale)
    end

    self.displayData.displayLocationDirty = false
end

-- Create and store DisplayItem for this item
function InventoryItem:CreateDisplayItem()
    self.displayData.displayItem = TEN.View.DisplayItem(
        self.objectID,
        self.displayData.locationDisplay.position,
        self.displayData.locationDisplay.rotation,
        self.displayData.locationDisplay.scale,
        self.meshBits
    )
    self.displayData.displayItem:SetColor(self.displayData.color)
end

-- Clear the DisplayItem reference
function InventoryItem:ClearDisplayItem()
    self.displayData.displayItem = nil
end

function InventoryItem:Draw()
    if not self.displayData.displayItem then
        self:CreateDisplayItem()
    end
    self:RefreshDisplayLocation()
    self.displayData.displayItem:Draw()
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
    local self = setmetatable(data, InventoryItem)
    self.rotation = Utilities.CopyRotation(data.rotation)
    self.displayData = {
        locationLocal = {
            position = TEN.Vec3(0, self.yOffset, 0),
            rotation = self.rotation,
            scale = TEN.Vec3(self.scale),
        },
        locationAdditional = {
            position = TEN.Vec3(0, 0, 0),
            rotation = TEN.Rotation(0, 0, 0),
            scale = TEN.Vec3(1, 1, 1),
        },
        locationWorld = {
            position = TEN.Vec3(0, 0, 0),
            rotation = TEN.Rotation(0, 0, 0),
            scale = TEN.Vec3(1, 1, 1),
        },
        locationDisplay = {
            position = TEN.Vec3(0, 0, 0),
            rotation = TEN.Rotation(0, 0, 0),
            scale = TEN.Vec3(1, 1, 1),
        },
        displayLocationDirty = true,
        color = data.color or COLOR_MAP.neutral,
        displayItem = nil
    }
    self:RefreshDisplayLocation()

    return self
end

return InventoryItem