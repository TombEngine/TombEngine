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

local function DrawBackground(alpha)
    if Settings.Background.enable then
        local capped    = math.min(alpha, Settings.Background.alpha)
        local bgColor   = Utilities.ColorCombine(Settings.Background.color, capped)
        local bgSprite  = TEN.View.DisplaySprite(
            Settings.Background.objectID,
            Settings.Background.spriteID,
            Settings.Background.position,
            Settings.Background.rotation,
            Settings.Background.scale,
            bgColor
        )
        bgSprite:Draw(BG_LAYER, Settings.Background.alignMode, Settings.Background.scaleMode, Settings.Background.blendMode)
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