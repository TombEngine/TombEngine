--- Internal file used by the RingInventory module.
-- @module RingInventory.Animation
-- @local

-- ============================================================================
-- ANIMATION FUNCTIONS
-- ============================================================================

--External Modules
local Constants = require("Engine.RingInventory.Constants")
local Interpolate = require("Engine.RingInventory.Interpolate")
local InventoryData = require("Engine.RingInventory.InventoryData")
local Ring = require("Engine.RingInventory.Ring")
local Settings = require("Engine.RingInventory.Settings")
local Utilities = require("Engine.RingInventory.Utilities")

--Constants
local ITEM_START = TEN.Vec3(0, 200, 512)
local ITEM_END = TEN.Vec3(0, -25, 400)
local PROGRESS_COMPLETE = 1

--Variables to be cleaned up
local itemRotation = TEN.Rotation(0, 0, 0)
local itemRotationOld = TEN.Rotation(0, 0, 0)
local itemStartPos = ITEM_START

--Animation functions
local Animation = {}

function Animation.SaveItemData(selectedItem)
    if not selectedItem then return end

    local displayItem = selectedItem:GetDisplayItem()
    itemRotationOld = Utilities.CopyRotation(displayItem:GetRotation())
    itemRotation = Utilities.CopyRotation(selectedItem:GetRotation())
end

function Animation.SetItemStartPos(selectedItem)
    if not selectedItem then return end
    local displayItem = selectedItem:GetDisplayItem()
    if displayItem then
        itemStartPos = displayItem:GetPosition()
    end
end

function Animation.Clear(prefix, motionTable)
    for _, motion in ipairs(motionTable) do
        local id = prefix..motion.key
        Interpolate.Clear(id)
    end
end

function Animation.PerformBatchMotion(prefix, motionTable, time, clearProgress, selectedRing, item, reverse)
    local interpolated = {}
    local allComplete = true
    local ringName = selectedRing and selectedRing:GetType() or nil
    
    for _, motion in ipairs(motionTable) do
        local id = prefix..motion.key
        local interp = {output = motion.start, progress = PROGRESS_COMPLETE}
        
        if motion.start ~= motion.finish then
            local startVal = reverse and motion.finish or motion.start
            local endVal = reverse and motion.start or motion.finish
            interp = Interpolate.Calculate(id, startVal, endVal, time, Interpolate.Easing.Smoothstep)
        end
        
        interpolated[motion.key] = interp
        
        if interp.progress < PROGRESS_COMPLETE then
            allComplete = false
        end
    end
    
    if selectedRing and (interpolated.ringCenter or interpolated.ringRadius or interpolated.ringAngle) then
        local center = interpolated.ringCenter and interpolated.ringCenter.output or Ring.CENTERS[ringName]
        local radius = interpolated.ringRadius and interpolated.ringRadius.output or Ring.RING_RADIUS
        local angle = interpolated.ringAngle and interpolated.ringAngle.output or 0
        selectedRing:Translate(center, radius, angle)
    end
    
    if selectedRing and interpolated.ringColor then
        selectedRing:Color(interpolated.ringColor.output, item)
    end
    
    if interpolated.camera then
        TEN.View.DisplayItem.SetCameraPosition(interpolated.camera.output, false)
    end
    
    if interpolated.target then
        TEN.View.DisplayItem.SetTargetPosition(interpolated.target.output, false)
    end
    
    if item then
        local displayItem = item:GetDisplayItem()
        if interpolated.itemColor then
            displayItem:SetColor(interpolated.itemColor.output)
        end
        if interpolated.itemPosition then
            displayItem:SetPosition(Utilities.OffsetY(interpolated.itemPosition.output, item.yOffset))
        end
        if interpolated.itemScale then
            displayItem:SetScale(Vec3(interpolated.itemScale.output))
        end
        if interpolated.itemRotation then
            displayItem:SetRotation(interpolated.itemRotation.output)
        end
    end
    
    if allComplete then
        if clearProgress then
            Animation.Clear(prefix, motionTable)
        end
        return true
    end
end

function Animation.Inventory(mode, selectedRing, selectedItem)
    local InventoryStates = require("Engine.RingInventory.InventoryStates")
    local INVENTORY_MODE = InventoryStates.MODE
    
    local ringAnimation =
    {
        {key = "ringRadius", start = 0, finish = Ring.RING_RADIUS},
        {key = "ringAngle", start = selectedRing:GetCurrentAngle() - 360, finish = selectedRing:GetCurrentAngle()},
        {key = "ringCenter", start = selectedRing:GetPosition(), finish = selectedRing:GetPosition()},
        {key = "camera", start = Constants.CAMERA_START, finish = Constants.CAMERA_END},
        {key = "target", start = Constants.TARGET_START, finish = Constants.TARGET_END},
    }
    
    local examineAnimation =
    {
        {key = "itemPosition", start = itemStartPos, finish = ITEM_END},
        {key = "itemRotation", start = itemRotationOld, finish = itemRotation},
        {key = "ringFade", start = Constants.ALPHA_MAX, finish = Constants.ALPHA_MIN},
    }
    
    local combineRingAnimation =
    {
        ringAnimation[1],
        ringAnimation[2]
    }

    if mode == INVENTORY_MODE.RING_OPENING then
        if Animation.PerformBatchMotion("RingOpening", ringAnimation, Settings.Animation.inventoryAnimTime, true, selectedRing) then
            LevelVars.Engine.RingInventory.InventoryOpenFreeze = true
            return true
        end
    elseif mode == INVENTORY_MODE.RING_CLOSING then
        if Animation.PerformBatchMotion("RingClosing", ringAnimation, Settings.Animation.inventoryAnimTime, true, selectedRing, nil, true) then
            return true
        end
    elseif mode == INVENTORY_MODE.RING_CHANGE then
        local allMotionComplete = true
        
        local rings = InventoryData.GetAllRings()
        for ringType, ring in pairs(rings) do   

            local targetAngle = ring:GetCurrentAngle()

            local ringChange = 
            {
            {key = "ringAngle", start = targetAngle, finish = targetAngle + 360 },
            {key = "ringCenter", start = ring:GetPreviousPosition(), finish = ring:GetPosition()},
            }
            if not Animation.PerformBatchMotion("RingChange"..ringType, ringChange, Settings.Animation.inventoryAnimTime, true, ring) then
                allMotionComplete = false
            end
        end

        if allMotionComplete then
            return true
        end
    elseif mode == INVENTORY_MODE.RING_ROTATE then
        local ringRotate = Interpolate.Calculate("RingRotateAngle",
            selectedRing:GetPreviousAngle(), selectedRing:GetTargetAngle(),
            Settings.Animation.inventoryAnimTime / 1.5, Interpolate.Easing.Smoothstep)

        selectedRing:SetCurrentAngle(ringRotate.output)
        selectedRing:Translate(selectedRing:GetPosition(), Ring.RING_RADIUS, ringRotate.output)

        if ringRotate.progress >= PROGRESS_COMPLETE then
            Interpolate.Clear("RingRotateAngle")
            return true
        end
    elseif mode == INVENTORY_MODE.STATISTICS_OPEN or 
           mode == INVENTORY_MODE.SAVE_SETUP or 
           mode == INVENTORY_MODE.COMBINE_SETUP or 
           mode == INVENTORY_MODE.COMBINE_SUCCESS then
        if Animation.PerformBatchMotion("ExamineOpen", examineAnimation, Settings.Animation.inventoryAnimTime, true, selectedRing, selectedItem) then
            return true
        end
    elseif mode == INVENTORY_MODE.ITEM_SELECT then
        local allMotionComplete = true
        
        if not Animation.PerformBatchMotion("ExamineOpen", examineAnimation, Settings.Animation.inventoryAnimTime, false, selectedRing, selectedItem) then
            allMotionComplete = false
        end

        local ammoRing = InventoryData.GetRing(Ring.TYPE.AMMO)
        local ringAnimation =
        {
            {key = "ringRadius", start = 0, finish = Ring.RING_RADIUS},
            {key = "ringAngle", start = ammoRing:GetCurrentAngle(), finish = ammoRing:GetCurrentAngle()},
            {key = "ringCenter", start = ammoRing:GetPosition(), finish = ammoRing:GetPosition()},
        }

        if not Animation.PerformBatchMotion("CombineRingOpening", ringAnimation, Settings.Animation.inventoryAnimTime, true, ammoRing) then
            allMotionComplete = false
        end

        if allMotionComplete then
            Animation.Clear("ExamineOpen", examineAnimation)
            Animation.Clear("CombineRingOpening", combineRingAnimation)
            return true
        end
    elseif mode == INVENTORY_MODE.STATISTICS_CLOSE or 
           mode == INVENTORY_MODE.SAVE_CLOSE or 
           mode == INVENTORY_MODE.ITEM_DESELECT or
           mode == INVENTORY_MODE.COMBINE_CLOSE then
        if Animation.PerformBatchMotion("ExamineClose", examineAnimation, Settings.Animation.inventoryAnimTime, true, selectedRing, selectedItem, true) then
            return true
        end
    elseif mode == INVENTORY_MODE.COMBINE_RING_OPENING then
        if Animation.PerformBatchMotion("CombineRingOpening", combineRingAnimation, Settings.Animation.inventoryAnimTime, true, selectedRing) then
            return true
        end
    elseif mode == INVENTORY_MODE.ITEM_USE then
        
        if InventoryData.IsItemChosen() then
            if not Animation.PerformBatchMotion("ItemDeselect", examineAnimation, Settings.Animation.inventoryAnimTime, true, selectedRing, selectedItem, true) then
                return false
            end
        end
        
        return true
    end
end

return Animation