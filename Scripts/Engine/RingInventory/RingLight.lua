--- Internal file used by the RingInventory module. Manages ring lighting with selection tracking.
-- @module RingInventory.RingLight
-- @local

--External Modules
local Interpolate = require("Engine.RingInventory.Interpolate")
local Settings = require("Engine.RingInventory.Settings")
local Utilities = require("Engine.RingInventory.Utilities")

local RingLighting = {}

-- Constants
local FADE_SPEED = 0.1
local UI_FADE_SPEED = Utilities.GetAlphaLerpFactor(Settings.Animation.transitionSpeed)

-- State tracking per ring: { ringType = { ring, color, selectedItemColor, previousSelectedItem } }
RingLighting.rings = {}

-- Item fade states: { itemID = { item, targetColor, isFading } }
RingLighting.itemStates = {}

-- Initialize a ring for lighting management
function RingLighting.InitializeRing(ring, color, selectedItemColor)
    if not ring then return end
    
    local ringType = ring:GetType()
    
    RingLighting.rings[ringType] =
    {
        ring = ring,
        color = color,
        selectedItemColor = selectedItemColor,
        previousSelectedItem = nil,
        fadeSpeed = UI_FADE_SPEED
    }
end

-- Update all rings - check for selection changes and update colors
function RingLighting.Update()
    for ringType, ringState in pairs(RingLighting.rings) do
        if not ringState.ring then goto continue end
        
        local currentSelectedItem = ringState.ring:GetSelectedItem()
        local currentSelectedID = currentSelectedItem and currentSelectedItem:GetObjectID() or nil
        local previousSelectedID = ringState.previousSelectedItem and ringState.previousSelectedItem:GetObjectID() or nil
        
        -- If selection changed, update colors
        if currentSelectedID ~= previousSelectedID then
            RingLighting.UpdateRingSelection(ringState)
            ringState.previousSelectedItem = currentSelectedItem
        end
        
        ::continue::
    end
    
    -- Process all fading items
    RingLighting.ProcessItemFades()
end

-- Update colors when selection changes in a ring
function RingLighting.UpdateRingSelection(ringState)
    if not ringState.ring then return end
    
    local ring = ringState.ring
    local selectedItem = ring:GetSelectedItem()
    
    for i = 1, #ring.items do
        local currentItem = ring.items[i]
        local displayItem = currentItem:GetDisplayItem()
        
        if not displayItem then goto continue end
        
        local targetColor
        
        if selectedItem and selectedItem:GetObjectID() == currentItem:GetObjectID() then
            -- This is now the selected item
            targetColor = ringState.selectedItemColor
        else
            -- Non-selected item
            targetColor = ringState.color
        end
        
        RingLighting.FadeItem(currentItem, targetColor, ringState.fadeSpeed)
        
        ::continue::
    end
end

-- Queue an item for fading
function RingLighting.FadeItem(item, targetColor, fadeSpeed)
    if not item or not item:GetDisplayItem() then return end
    
    local itemID = item:GetObjectID()
    
    RingLighting.itemStates[itemID] =
    {
        item = item,
        targetColor = targetColor,
        isFading = true,
        fadeSpeed = fadeSpeed or UI_FADE_SPEED
    }
end

-- Process all item fades for one frame
function RingLighting.ProcessItemFades()
    for itemID, state in pairs(RingLighting.itemStates) do
        if not state.item or not state.item:GetDisplayItem() then
            RingLighting.itemStates[itemID] = nil
            goto continue
        end
        
        if state.isFading then
            local displayItem = state.item:GetDisplayItem()
            local currentColor = displayItem:GetColor()
            local interpolatedColor = Interpolate.Lerp(currentColor, state.targetColor, 1 - (1 - (state.fadeSpeed or FADE_SPEED)) ^ 2, Interpolate.Easing.Linear)
            displayItem:SetColor(interpolatedColor)
            
            -- Check if fade is complete
            local threshold = 2
            if math.abs(state.targetColor.r - interpolatedColor.r) < threshold and
               math.abs(state.targetColor.g - interpolatedColor.g) < threshold and
               math.abs(state.targetColor.b - interpolatedColor.b) < threshold and
               math.abs(state.targetColor.a - interpolatedColor.a) < threshold then
                displayItem:SetColor(state.targetColor)
                state.isFading = false
            end
        end
        
        ::continue::
    end
end

-- Set colors for a ring (call once when ring colors change)
function RingLighting.SetRingColors(ring, color, selectedItemColor, fadeSpeed)
    if not ring then return end
    
    local ringType = ring:GetType()
    
    RingLighting.InitializeRing(ring, color, selectedItemColor)
    RingLighting.rings[ringType].fadeSpeed = fadeSpeed or FADE_SPEED
    
    -- Update all items with new colors
    RingLighting.UpdateRingSelection(RingLighting.rings[ringType])
end

-- Reset all lighting
function RingLighting.Reset()
    RingLighting.rings = {}
    RingLighting.itemStates = {}
end

return RingLighting
