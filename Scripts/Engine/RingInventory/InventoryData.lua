--- Internal file used by the RingInventory module.
-- @module RingInventory.InventoryData
-- @local

--External Modules
local Constants = require("Engine.RingInventory.Constants")
local Ring = require("Engine.RingInventory.Ring")
local InventoryItem = require("Engine.RingInventory.InventoryItem")
local ItemSpin = require("Engine.RingInventory.ItemSpin")
local Settings = require("Engine.RingInventory.Settings")
local PickupData = require("Engine.RingInventory.PickupData")
local Utilities = require("Engine.RingInventory.Utilities")

--Pointers to tables
local COLOR_MAP = Settings.ColorMap
local RING_CENTER = Ring.CENTERS
local RING_TYPE = Ring.TYPE
local TYPE = PickupData.TYPE
local RING = Ring.TYPE

-- Inventory Module
local InventoryData = {}

-- Instance data (private)
local rings = {}
local selectedRingType = RING.MAIN
local previousRingType = RING.MAIN
local chosenItem = nil
local openAtItem = nil
local combineCount = 0

--Variables
local gameflowOverrides = nil

-- Reset all data (useful for level changes)
function InventoryData.Reset()
    InventoryData.ClearAll()
    rings = {}
    selectedRingType = RING.MAIN
    previousRingType = RING.MAIN
    chosenItem = nil
    openAtItem = nil
    gameflowOverrides = nil
end

-- Get or create a ring by type
function InventoryData.GetRing(ringType)
    if not rings[ringType] then
        local center = RING_CENTER[ringType]
        rings[ringType] = Ring.Create(ringType, center)
    end
    return rings[ringType]
end

-- Check if a ring exists
function InventoryData.HasRing(ringType)
    return rings[ringType] ~= nil
end

-- Get all rings
function InventoryData.GetAllRings()
    return rings
end

-- Get currently selected ring
function InventoryData.GetSelectedRing()
    return InventoryData.GetRing(selectedRingType)
end

-- Get selected ring type
function InventoryData.GetSelectedRingType()
    return selectedRingType
end

-- Get previous ring type
function InventoryData.GetPreviousRingType()
    return previousRingType
end

-- Switch to a different ring
function InventoryData.SwitchToRingType(ringType)
    local isValid = false
    for _, value in pairs(RING_TYPE) do
        if value == ringType then
            isValid = true
            break
        end
    end
    
    if not isValid then
        return false
    end
    
    previousRingType = selectedRingType
    selectedRingType = ringType

    local ring = InventoryData.GetRing(ringType)

    ItemSpin.StartSpin(ring)
    return true
end

-- Return to previous ring
function InventoryData.ReturnToPreviousRing()
    if previousRingType then
        local temp = selectedRingType
        selectedRingType = previousRingType
        previousRingType = temp
        return true
    end
    return false
end

-- Setup secondary ring (combine, ammo, etc.)
function InventoryData.SetupSecondaryRing(ringType, item, keepCurrentRing)
    local currentRing = InventoryData.GetSelectedRing()
    item = item or (currentRing and currentRing:GetSelectedItem())
    
    -- Get or create the new ring
    local newRing = InventoryData.GetRing(ringType)
    
    InventoryData.Construct(ringType, item)
    
    if not keepCurrentRing then
        InventoryData.SwitchToRingType(ringType)
    end

    -- Special handling for ammo ring
    if ringType == RING.AMMO then
        local weaponSlot = PickupData.WEAPON_SET[item:GetObjectID()].slot
        local ammoType = Lara:GetAmmoType(weaponSlot)
        local objectID = PickupData.AMMO_TYPE_TO_OBJECT[ammoType]

        newRing:SetSelectedItemByID(objectID)

        local itemIndex = newRing:GetSelectedItemIndex()
        local angle = Utilities.GetSelectedRingAngle(itemIndex, newRing:GetItemCount())
    
        newRing:SetCurrentAngle(angle)
        newRing:SetTargetAngle(angle)
    end
    
    return newRing
end

-- Find item across all rings
function InventoryData.FindItem(objectID)
    for ringType, ring in pairs(rings) do
        local item = ring:GetItem(objectID)
        if item then
            return item, ringType, ring
        end
    end
    return nil, nil, nil
end

-- Remove a ring
function InventoryData.RemoveRing(ringType)
    if rings[ringType] then
        ItemSpin.ClearRingState(rings[ringType])
        rings[ringType] = nil
        
        -- If we removed the selected ring, switch to main
        if selectedRingType == ringType then
            selectedRingType = RING.MAIN
        end
        return true
    end
    return false
end

-- Color all rings
function InventoryData.ColorAll(color, selectedItemColor, omitSelectedRing)
    for ringType, ring in pairs(rings) do
        if not (omitSelectedRing and ringType == selectedRingType) then
            ring:Color(color, selectedItemColor)
        end
    end
end

-- Offset all rings
function InventoryData.OffsetAll(direction)
    for ringType, ring in pairs(rings) do
        ring:OffsetPosition(direction)
    end
end

-- Hide all rings
function InventoryData.SetVisibility(visible, omitSelectedRing)
    for ringType, ring in pairs(rings) do
        if not (omitSelectedRing and ringType == selectedRingType) then
            ring:SetVisibility(visible)
        end
    end
end

-- Iterator for all rings
function InventoryData.IterateRings()
    return pairs(rings)
end

-- Read gameflow overrides for items
local ReadGameflow = function()
    local overrides = {}
    for _, itemID in ipairs(TEN.Flow.GetCurrentLevel().objects) do
        if itemID.objectID then
            local id = TEN.Inventory.ConvertInventoryItemToObject(itemID.objectID)
            overrides[id] = 
            { 
                item = id,
                yOffset = itemID.yOffset,
                scale = itemID.scale,
                rotation = itemID.rotation,
                menuActions = itemID.action,
                name = itemID.nameKey,
                meshBits = itemID.meshBits,
                orientation = itemID.axis
            }
        end
    end
    return overrides
end

-- Build item with gameflow overrides
function InventoryData.BuildItem(data)
    gameflowOverrides = gameflowOverrides or ReadGameflow()
    data.count = TEN.Inventory.GetItemCount(data.objectID)
    
    local override = gameflowOverrides[data.objectID] or {}
    
    if override.yOffset ~= nil then data.yOffset = override.yOffset end
    if override.scale ~= nil then data.scale = override.scale end
    if override.rotation ~= nil then data.rotation = override.rotation end
    if override.menuActions ~= nil then data.menuActions = override.menuActions end
    if override.name ~= nil then data.name = override.name end
    if override.meshBits ~= nil then data.meshBits = override.meshBits end
    if override.orientation ~= nil then data.orientation = override.orientation end
    if override.type ~= nil then data.type = override.type end
    if override.combine ~= nil then data.combine = override.combine end

    --add metatable for inventoryItem methods
    InventoryItem.New(data)
    
    return data
end

-- Construct rings with items from game data
function InventoryData.Construct(ringType, selectedWeapon)
    local selectedWeaponID = selectedWeapon and selectedWeapon:GetObjectID()
    local items = PickupData.CONSTANTS
    
    if ringType == RING.AMMO or ringType == RING.COMBINE then
        InventoryData.Clear(ringType)
    else
        InventoryData.ClearAll()
    end
    
    for _, itemRow in ipairs(items) do
        local itemData = PickupData.ConvertRowData(itemRow)
        itemData.rotation = Utilities.CopyRotation(itemData.rotation)
        local data = InventoryData.BuildItem(itemData)

        local objectID = data:GetObjectID()
        
        if data:GetType() == TYPE.AMMO and ringType ~= RING.AMMO then
            local weaponPresent = TEN.Inventory.GetItemCount(PickupData.AMMO_SET[objectID].weapon)
            if weaponPresent ~= 0 then
                goto continue
            end
        end
        
        if data.type == TYPE.WEAPON then
            if Lara:GetLaserSight(PickupData.WEAPON_SET[objectID].slot) then
                data.meshBits = PickupData.WEAPON_LASERSIGHT_DATA[objectID].MESHBITS
                data.name = PickupData.WEAPON_LASERSIGHT_DATA[objectID].NAME
                data.menuActions = PickupData.WEAPON_LASERSIGHT_DATA[objectID].FLAGS
            end
        end
        
        local ammoRing = false
        local shouldInsert = false
        
        if ringType == RING.COMBINE then
            if data.combine == true then
                data.ringName = RING.COMBINE
                
                if chosenItem:GetObjectID() == objectID then
                    goto continue
                end
                
                if data:GetType() == TYPE.WEAPON and Lara:GetLaserSight(PickupData.WEAPON_SET[objectID].slot) then
                    goto continue
                end
                
                shouldInsert = (data:GetCount() ~= 0)
            else
                goto continue
            end
        elseif ringType == RING.AMMO then
            if data.type == TYPE.AMMO and PickupData.WEAPON_AMMO_LOOKUP[selectedWeaponID] and Utilities.Contains(PickupData.WEAPON_AMMO_LOOKUP[selectedWeaponID], objectID) then
                data.ringName = RING.AMMO
                ammoRing = true
                shouldInsert = true
            else
                goto continue
            end
        else
            shouldInsert = (data.count ~= 0)
        end
        
        if (objectID == TEN.Objects.ObjID.LASERSIGHT_ITEM) then
            if Lara:GetLaserSight(TEN.Objects.WeaponType.CROSSBOW) or 
               Lara:GetLaserSight(TEN.Objects.WeaponType.REVOLVER) or 
               Lara:GetLaserSight(TEN.Objects.WeaponType.HK) then
                shouldInsert = false
            end
        end
        
        if shouldInsert or ammoRing then
            -- Get or create the ring for this item
            local ring = InventoryData.GetRing(data.ringName)
            
            -- Create display item
            local displayItem = data:CreateDisplayItem(Ring.CENTERS[data.ringName])
            displayItem:SetColor(COLOR_MAP.itemHidden)
            -- Add item to ring
            ring:AddItem(data)

            if not ringType and data.combine then
                combineCount = combineCount + 1
            end

        end
        
        ::continue::
    end
    
    -- Initialize ring positions
    if ringType then
        local ring = InventoryData.GetRing(ringType)
        ring:SetPosition(RING_CENTER[ringType])
    else
        for ringType, ring in pairs(rings) do
            ring:SetPosition(RING_CENTER[ringType])
        end
    end
end

-- Open inventory at specific item
function InventoryData.OpenAtItem(itemID, repositionRings)
    if itemID == Constants.NO_VALUE then
        return
    end
    
    local item, ringType, ring = InventoryData.FindItem(itemID)
    
    if not (ringType and ring) then
        return
    end
    
    InventoryData.SwitchToRingType(ringType)
    
    -- Set selected item
    ring:SetSelectedItemByID(itemID)
    
    local itemIndex = ring:GetSelectedItemIndex()
    local angle = Utilities.GetSelectedRingAngle(itemIndex, ring:GetItemCount())
    
    -- Store angle (you may want to make these instance variables)
    ring:SetCurrentAngle(angle)
    ring:SetTargetAngle(angle)
    
    if repositionRings then
        local ringPosition = RING_CENTER[RING.MAIN]
        selectedRingType = ringType
        
        for rType, r in pairs(rings) do
            local offset = (rType - selectedRingType) * 1000  -- RING_POSITION_OFFSET
            local position = Vec3(ringPosition.x, ringPosition.y + offset, ringPosition.z)
            r:SetPosition(position)
            r:Translate(position, Ring.RING_RADIUS, angle)
        end
    end
end

-- Get count of combinable items present
function InventoryData.GetCombineItemsCount()
    return combineCount
end

-- Clear a specific ring or all rings
function InventoryData.Clear(ringType)
    local ring = InventoryData.GetRing(ringType)
    
    if ring then
        ring:Clear()

    -- If we cleared the selected ring, switch to main
        if selectedRingType == ringType then
            selectedRingType = RING.MAIN
        end
    end
end

function InventoryData.ClearDisplayItems()
    for _, ring in pairs(rings) do
        ring:ClearDisplayItems()
    end
end

function InventoryData.ClearAll()
    for ringType, ring in pairs(rings) do
        ring:Clear()
    end

    combineCount = 0
end

function InventoryData.DrawAllRings()
    for ringType, ring in pairs(rings) do
        ring:Draw()
    end
end

-- Get count of rings
function InventoryData.GetRingCount()
    local count = 0
    for _ in pairs(rings) do
        count = count + 1
    end
    return count
end

--Get item selected objectID
function InventoryData.GetChosenItem()
    return chosenItem 
end

--Set item selected objectID
function InventoryData.SetChosenItem(item)
    chosenItem = item
    return true
end

--Check if an item is chosen
function InventoryData.IsItemChosen()
    return chosenItem ~= nil
end

--Get open at item objectID
function InventoryData.GetOpenAtItem()
    return openAtItem
end

--Set open at item objectID
function InventoryData.SetOpenAtItem(objectID)
    openAtItem = objectID
    return true
end

function InventoryData.LoadFocusedItem(savedID)
    local waterskinFamilies =
    {
        [TEN.Objects.ObjID.WATERSKIN1_EMPTY] = 
        {
            TEN.Objects.ObjID.WATERSKIN1_EMPTY,
            TEN.Objects.ObjID.WATERSKIN1_1,
            TEN.Objects.ObjID.WATERSKIN1_2,
            TEN.Objects.ObjID.WATERSKIN1_3,
        },
        [TEN.Objects.ObjID.WATERSKIN2_EMPTY] = 
        {
            TEN.Objects.ObjID.WATERSKIN2_EMPTY,
            TEN.Objects.ObjID.WATERSKIN2_1,
            TEN.Objects.ObjID.WATERSKIN2_2,
            TEN.Objects.ObjID.WATERSKIN2_3,
            TEN.Objects.ObjID.WATERSKIN2_4,
            TEN.Objects.ObjID.WATERSKIN2_5,
        },
    }

    local family = waterskinFamilies[savedID]
    if family then
        for _, variantID in ipairs(family) do
            if TEN.Inventory.GetItemCount(variantID) > 0 then
                return variantID
            end
        end
        return Constants.NO_VALUE
    end

    return savedID
end

function InventoryData.SaveFocusedItem(item)
    local id = item:GetObjectID()

    if item:IsType(PickupData.TYPE.WATERSKIN) then
        
        local isSmall = (id >= TEN.Objects.ObjID.WATERSKIN1_EMPTY and id <= TEN.Objects.ObjID.WATERSKIN1_3)
        local isLarge = (id >= TEN.Objects.ObjID.WATERSKIN2_EMPTY and id <= TEN.Objects.ObjID.WATERSKIN2_5)

        if isSmall then
            LevelVars.Engine.RingInventory.lastFocusedItem = TEN.Objects.ObjID.WATERSKIN1_EMPTY
        elseif isLarge then
            LevelVars.Engine.RingInventory.lastFocusedItem = TEN.Objects.ObjID.WATERSKIN2_EMPTY

        end
    else
        LevelVars.Engine.RingInventory.lastFocusedItem = id
    end
end

local function CalculateCompassNeedle(timeCount)
    local needleOrient = Rotation(0, -Lara:GetRotation().y, 0)
    local wibble = math.sin((timeCount % 0x40) / 0x3F * (2 * math.pi)) * 3
    needleOrient.y = needleOrient.y + wibble
    return needleOrient
end

local function CalculateStopWatchHands()
    local angles = {}
    local Stats = require("Engine.RingInventory.Statistics")
    local levelTime = Flow.GetStatistics(Stats.GetType()).timeTaken
    angles.hour_hand_angle = Rotation(0, 0, -(levelTime.h / 12) * 360)
    angles.minute_hand_angle = Rotation(0, 0, -(levelTime.m / 60) * 360)
    angles.second_hand_angle = Rotation(0, 0, -(levelTime.s / 60) * 360)
    return angles
end

function InventoryData.SetItemRotations(timeCount)
    local angles = CalculateStopWatchHands()
    local stopwatch = InventoryData.FindItem(TEN.Objects.ObjID.STOPWATCH_ITEM)
    if stopwatch then
        local displayItem = stopwatch:GetDisplayItem()
        displayItem:SetJointRotation(4, angles.hour_hand_angle)
        displayItem:SetJointRotation(5, angles.minute_hand_angle)
        displayItem:SetJointRotation(6, angles.second_hand_angle)
    end

    local compass = InventoryData.FindItem(TEN.Objects.ObjID.COMPASS_ITEM)
    if compass then
        local displayItem = compass:GetDisplayItem()
        displayItem:SetJointRotation(1, CalculateCompassNeedle(timeCount))
    end

    local Examine = require("Engine.RingInventory.Examine")
    if Examine.item and (Examine.item:GetObjectID() == TEN.Objects.ObjID.STOPWATCH_ITEM or Examine.item:GetObjectID() == TEN.Objects.ObjID.COMPASS_ITEM) then
        local displayItem = Examine.item
        if Examine.item:GetObjectID() == TEN.Objects.ObjID.STOPWATCH_ITEM then
            displayItem:SetJointRotation(4, angles.hour_hand_angle)
            displayItem:SetJointRotation(5, angles.minute_hand_angle)
            displayItem:SetJointRotation(6, angles.second_hand_angle)
        else
            displayItem:SetJointRotation(1, CalculateCompassNeedle(timeCount))
        end
    end

end

return InventoryData