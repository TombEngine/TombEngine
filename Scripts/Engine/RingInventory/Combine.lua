--- Internal file used by the RingInventory module.
-- @module RingInventory.Combine
-- @local

-- ============================================================================
-- Combine - Handles combine functions and data for ring inventory
-- ============================================================================

--Pointers to tables
local PickupData = require("Engine.RingInventory.PickupData")
local TYPE = PickupData.TYPE

--Combine functions
local Combine = {}

--Variables
local combineResult = nil

function Combine.PerformWaterskinCombine(flag)
    local smallRaw = Lara:GetWaterSkinStatus(false)
    local bigRaw = Lara:GetWaterSkinStatus(true)
    
    local smallLiters = (smallRaw > 0) and (smallRaw - 1) or 0
    local bigLiters = (bigRaw > 0) and (bigRaw - 1) or 0
    
    local smallCapacity = 3 - smallLiters
    local bigCapacity = 5 - bigLiters
    
    if flag then
        if bigRaw > 1 and smallCapacity > 0 then
            local transfer = math.min(bigLiters, smallCapacity)
            smallLiters = smallLiters + transfer
            bigLiters = bigLiters - transfer
            
            Lara:SetWaterSkinStatus(smallLiters + 1, false)
            Lara:SetWaterSkinStatus(bigLiters + 1, true)
            
            combineResult = (smallLiters + 1) + (TEN.Objects.ObjID.WATERSKIN1_EMPTY - 1)
            return true
        end
    else
        if smallRaw > 1 and bigCapacity > 0 then
            local transfer = math.min(smallLiters, bigCapacity)
            bigLiters = bigLiters + transfer
            smallLiters = smallLiters - transfer
            
            Lara:SetWaterSkinStatus(smallLiters + 1, false)
            Lara:SetWaterSkinStatus(bigLiters + 1, true)
            
            combineResult = (bigLiters + 1) + (TEN.Objects.ObjID.WATERSKIN2_EMPTY - 1)
            return true
        end
    end
    
    return false
end

function Combine.CombineItems(data1, data2)
    local item1 = data1:GetObjectID()
    local item2 = data2:GetObjectID()
    
    if data1.type == TYPE.WATERSKIN and data2.type == TYPE.WATERSKIN then
        if (item1 >= TEN.Objects.ObjID.WATERSKIN1_EMPTY and
            item1 <= TEN.Objects.ObjID.WATERSKIN1_3 and
            item2 >= TEN.Objects.ObjID.WATERSKIN2_EMPTY and
            item2 <= TEN.Objects.ObjID.WATERSKIN2_5) then
            if (Combine.PerformWaterskinCombine(false)) then
                return true
            end
        elseif(item2 >= TEN.Objects.ObjID.WATERSKIN1_EMPTY and
            item2 <= TEN.Objects.ObjID.WATERSKIN1_3 and
            item1 >= TEN.Objects.ObjID.WATERSKIN2_EMPTY and
            item1 <= TEN.Objects.ObjID.WATERSKIN2_5) then
            if (Combine.PerformWaterskinCombine(true)) then
                return true
            end
        end
    end
    
    for _, combo in ipairs(PickupData.combineTable) do
        local a, b, result = combo[1], combo[2], combo[3]
        
        if (item1 == a and item2 == b) or (item1 == b and item2 == a) then
            local count1 = TEN.Inventory.GetItemCount(item1)
            local count2 = TEN.Inventory.GetItemCount(item2)
            local selectedAmmo = nil

            if count1 == 0 or count2 == 0 then
                return false
            end

            if PickupData.WEAPON_LASERSIGHT_DATA[result] and
               PickupData.WEAPON_SET[result] and
               PickupData.WEAPON_SET[result].slot then
                Lara:SetLaserSight(PickupData.WEAPON_SET[result].slot, true)
                selectedAmmo = Lara:GetAmmoType(PickupData.WEAPON_SET[result].slot)
            end
            
            TEN.Inventory.TakeItem(item1, 1)
            TEN.Inventory.TakeItem(item2, 1)
            TEN.Inventory.GiveItem(result, 1)
            
            if PickupData.WEAPON_LASERSIGHT_DATA[result] and
               PickupData.WEAPON_SET[result] then
                Lara:SetAmmoType(selectedAmmo)
            end

            combineResult = result
            return true
        end
    end
    
    return false
end

function Combine.SeparateItems(item)
    local itemObjectID = item:GetObjectID()
    
    for _, combo in ipairs(PickupData.combineTable) do
        local a, b, result = combo[1], combo[2], combo[3]
        
        if itemObjectID == result then
            local count = TEN.Inventory.GetItemCount(itemObjectID)
            local selectedAmmo = nil

            if count == 0 then
                return false
            end
            
            if PickupData.WEAPON_LASERSIGHT_DATA[result] and
               PickupData.WEAPON_SET[result] and
               PickupData.WEAPON_SET[result].slot then
                Lara:SetLaserSight(PickupData.WEAPON_SET[result].slot, false)
                selectedAmmo = Lara:GetAmmoType(PickupData.WEAPON_SET[result].slot)
            end
            
            TEN.Inventory.TakeItem(itemObjectID, 1)
            TEN.Inventory.GiveItem(a, 1)
            TEN.Inventory.GiveItem(b, 1)
            
            if PickupData.WEAPON_LASERSIGHT_DATA[result] and
               PickupData.WEAPON_SET[result] then
                Lara:SetAmmoType(selectedAmmo)
            end

            combineResult = a
            return true
        end
    end
    
    return false
end

function Combine.GetResults()
    return combineResult
end

function Combine.ClearResults()
    combineResult = nil
end

return Combine