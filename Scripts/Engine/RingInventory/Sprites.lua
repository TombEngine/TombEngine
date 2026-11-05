--- Internal file used by the RingInventory module.
-- @module RingInventory.Sprites
-- @local

--External Modules
local Interpolate = require("Engine.RingInventory.Interpolate")
local InventoryData
local Ring
local Settings = require("Engine.RingInventory.Settings")
local Utilities = require("Engine.RingInventory.Utilities")

--Pointers to tables
local COLOR_MAP = Settings.ColorMap

--CONSTANTS
local BG_LAYER = 0
local ALPHA_SPEED = Settings.Animation.transitionSpeed

-- Background state
local bgAlpha  = 0
local bgTarget = 0

-- Arrows state
local arrowUpAlpha    = 0
local arrowUpTarget   = 0
local arrowDownAlpha  = 0
local arrowDownTarget = 0
local arrowVisible = true

local Sprites = {}

-- ============================================================
--  Private
-- ============================================================

local function DrawArrow(list, alpha)
    for _, entry in ipairs(list) do
        local entrySprite = TEN.View.DisplaySprite(
            TEN.Objects.ObjID.INVENTORY_SPRITES,
            0,
            entry[2],
            entry[1],
            Vec2(3, 3),
            Utilities.ColorCombine(COLOR_MAP.arrowColor, alpha)
        )
        entrySprite:Draw(-8, View.AlignMode.CENTER, View.ScaleMode.FIT, TEN.Effects.BlendID.ALPHA_BLEND)
    end
end

local function DrawArrows()
    local arrowsUp =
    {
        {0, Vec2(5, 5)},
        {0, Vec2(95, 5)},
    }

    local arrowsDown =
    {
        {180, Vec2(5, 88)},
        {180, Vec2(95, 88)},
    }

    if arrowUpAlpha > 0 then
        DrawArrow(arrowsUp, arrowUpAlpha)
    end

    if arrowDownAlpha > 0 then
        DrawArrow(arrowsDown, arrowDownAlpha)
    end

end

local function Draw9SliceBackground(x, y, width, height, cornerW, cornerH, objID, sprites, color, layer, blendMode)
    -- Calculate inner width/height (Percentages)
    local innerW = width - (cornerW * 2)
    local innerH = height - (cornerH * 2)

    -- Row Y positions
    local yTop    = y
    local yMid    = y + cornerH
    local yBottom = y + height - cornerH

    -- Column X positions
    local xLeft  = x
    local xMid   = x + cornerW
    local xRight = x + width - cornerW

    -- 1. Corners (Fixed percent aspect)
    TEN.View.DisplaySprite(objID, sprites[1], TEN.Vec2(xLeft, yTop), 0, TEN.Vec2(cornerW, cornerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)
    TEN.View.DisplaySprite(objID, sprites[3], TEN.Vec2(xRight, yTop), 0, TEN.Vec2(cornerW, cornerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)
    TEN.View.DisplaySprite(objID, sprites[7], TEN.Vec2(xLeft, yBottom), 0, TEN.Vec2(cornerW, cornerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)
    TEN.View.DisplaySprite(objID, sprites[9], TEN.Vec2(xRight, yBottom), 0, TEN.Vec2(cornerW, cornerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)

    -- 2. Edges (Stretched)
    TEN.View.DisplaySprite(objID, sprites[2], TEN.Vec2(xMid, yTop), 0, TEN.Vec2(innerW, cornerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)
    TEN.View.DisplaySprite(objID, sprites[8], TEN.Vec2(xMid, yBottom), 0, TEN.Vec2(innerW, cornerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)
    TEN.View.DisplaySprite(objID, sprites[4], TEN.Vec2(xLeft, yMid), 0, TEN.Vec2(cornerW, innerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)
    TEN.View.DisplaySprite(objID, sprites[6], TEN.Vec2(xRight, yMid), 0, TEN.Vec2(cornerW, innerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)

    -- 3. Center
    TEN.View.DisplaySprite(objID, sprites[5], TEN.Vec2(xMid, yMid), 0, TEN.Vec2(innerW, innerH), color):Draw(layer, TEN.View.AlignMode.TOP_LEFT, TEN.View.ScaleMode.STRETCH, blendMode)
end

local function DrawBackground(alpha)
    if Settings.Background.enable then
        local bgSettings = Settings.Background
        local capped    = math.min(alpha, bgSettings.alpha)
        local bgColor   = Utilities.ColorCombine(bgSettings.color, capped)
        
        if bgSettings.sliced then
            local screenSize = TEN.Util.PercentToScreen(TEN.Vec2(100, 100))
            local screenWidth = screenSize.x
            local screenHeight = screenSize.y
            local blendMode = bgSettings.blendMode
            
            -- Overall dimensions (Percentages)
            local width = bgSettings.scale.x
            local height = bgSettings.scale.y
            local x = bgSettings.position.x - (width / 2)
            local y = bgSettings.position.y - (height / 2)
            
            -- Convert corner pixel size to percentage of screen width and height
            local cornerPixels = bgSettings.slicedCornerSize or 16
            local cornerW = (cornerPixels / screenWidth) * 100
            local cornerH = (cornerPixels / screenHeight) * 100
            
            Draw9SliceBackground(
                x, y, width, height, cornerW, cornerH,
                bgSettings.slicedObjectID or bgSettings.objectID, 
                bgSettings.slicedSpriteIDs or {2, 3, 4, 5, 6, 7, 8, 9, 10},
                bgColor,     
                BG_LAYER,
                blendMode
            )
        else
            local bgSprite = TEN.View.DisplaySprite(
                bgSettings.objectID,
                bgSettings.spriteID,
                bgSettings.position,
                bgSettings.rotation,
                bgSettings.scale,
                bgColor
            )
            bgSprite:Draw(BG_LAYER, bgSettings.alignMode, bgSettings.scaleMode, bgSettings.blendMode)
        end
    end
end

-- ============================================================
--  Background controls
-- ============================================================

function Sprites.ShowBackground()
    bgTarget = 255
end

function Sprites.HideBackground()
    bgTarget = 0
end

-- ============================================================
--  Arrows controls
-- ============================================================

function Sprites.ShowArrows()
    arrowVisible = true
    arrowUpTarget   = 255
    arrowDownTarget = 255
end

function Sprites.HideArrows()
    arrowVisible = false
    arrowUpTarget   = 0
    arrowDownTarget = 0
end

function Sprites.ShowArrowsUp()
    arrowUpTarget = 255
end

function Sprites.HideArrowsUp()
    arrowUpTarget = 0
end

function Sprites.ShowArrowsDown()
    arrowDownTarget = 255
end

function Sprites.HideArrowsDown()
    arrowDownTarget = 0
end

-- ============================================================
--  Update
-- ============================================================

function Sprites.Update(selectedRing)
    if not Ring then
        Ring = require("Engine.RingInventory.Ring")
    end

    if not InventoryData then
        InventoryData = require("Engine.RingInventory.InventoryData")
    end

    local selectedRingType = selectedRing:GetType()

    if arrowVisible then

        local isPuzzle  = selectedRingType == Ring.TYPE.PUZZLE
        local isMain    = selectedRingType == Ring.TYPE.MAIN
        local isOptions = selectedRingType == Ring.TYPE.OPTIONS
 
        -- Up arrow: visible only from MAIN (if PUZZLE has items) or OPTIONS
        if isOptions then
            arrowUpTarget = 255
        elseif isMain then
            arrowUpTarget = InventoryData.GetRing(Ring.TYPE.PUZZLE):IsEmpty() and 0 or 255
        else
            arrowUpTarget = 0
        end

        -- Down arrow: visible only from MAIN (if OPTIONS has items) or PUZZLE
        if isPuzzle then
            arrowDownTarget = 255
        elseif isMain then
            arrowDownTarget = InventoryData.GetRing(Ring.TYPE.OPTIONS):IsEmpty() and 0 or 255
        else
            arrowDownTarget = 0
        end

    end
  
    bgAlpha        = Interpolate.StepAlpha(bgAlpha, bgTarget, ALPHA_SPEED)
    arrowUpAlpha   = Interpolate.StepAlpha(arrowUpAlpha, arrowUpTarget, ALPHA_SPEED)
    arrowDownAlpha = Interpolate.StepAlpha(arrowDownAlpha, arrowDownTarget, ALPHA_SPEED)
end

-- ============================================================
--  Draw — call after Update
-- ============================================================

function Sprites.Draw()
    if bgAlpha > 0 then
        DrawBackground(bgAlpha)
    end

    DrawArrows()
end

-- ============================================================
--  Clear — snap both to hidden instantly
-- ============================================================

function Sprites.Clear()
    bgAlpha         = 0
    bgTarget        = 0
    arrowUpAlpha    = 0
    arrowUpTarget   = 0
    arrowDownAlpha  = 0
    arrowDownTarget = 0
    arrowVisible = false
end

return Sprites