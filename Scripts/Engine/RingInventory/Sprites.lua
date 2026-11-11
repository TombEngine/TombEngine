--- Internal file used by the RingInventory module.
-- @module RingInventory.Sprites
-- @local

--External Modules
local Constants = require("Engine.RingInventory.Constants")
local Interpolate = require("Engine.RingInventory.Interpolate")
local InventoryData
local InventoryStates
local Ring
local Settings = require("Engine.RingInventory.Settings")
local Utilities = require("Engine.RingInventory.Utilities")

--Pointers to tables
local COLOR_MAP = Settings.ColorMap

--CONSTANTS
local BG_LAYER = 0
local ALPHA_SPEED = Settings.Animation.transitionSpeed

-- Background state
local bgAlpha  = Constants.ALPHA_MIN
local bgTarget = Constants.ALPHA_MIN

-- Arrows state
local arrowUpAlpha    = Constants.ALPHA_MIN
local arrowUpTarget   = Constants.ALPHA_MIN
local arrowDownAlpha  = Constants.ALPHA_MIN
local arrowDownTarget = Constants.ALPHA_MIN
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
    bgTarget = Constants.ALPHA_MAX
end

function Sprites.HideBackground()
    bgTarget = Constants.ALPHA_MIN
end

-- ============================================================
--  Arrows controls
-- ============================================================

function Sprites.ShowArrows()
    arrowVisible = true
    Sprites.ShowArrowsUp()
    Sprites.ShowArrowsDown()
end

function Sprites.HideArrows()
    arrowVisible = false
    Sprites.HideArrowsUp()
    Sprites.HideArrowsDown()
end

function Sprites.ShowArrowsUp()
    arrowUpTarget = Constants.ALPHA_MAX
end

function Sprites.HideArrowsUp()
    arrowUpTarget = Constants.ALPHA_MIN
end

function Sprites.ShowArrowsDown()
    arrowDownTarget = Constants.ALPHA_MAX
end

function Sprites.HideArrowsDown()
    arrowDownTarget = Constants.ALPHA_MIN
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

    if not InventoryStates then
        InventoryStates = require("Engine.RingInventory.InventoryStates")
    end

    local selectedRingType = selectedRing:GetType()

    if arrowVisible then

        arrowUpTarget = InventoryStates.GetNeighbourRingType(selectedRingType, -1) and Constants.ALPHA_MAX or Constants.ALPHA_MIN
        arrowDownTarget = InventoryStates.GetNeighbourRingType(selectedRingType, 1) and Constants.ALPHA_MAX or Constants.ALPHA_MIN

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
    bgAlpha         = Constants.ALPHA_MIN
    bgTarget        = Constants.ALPHA_MIN
    arrowUpAlpha    = Constants.ALPHA_MIN
    arrowUpTarget   = Constants.ALPHA_MIN
    arrowDownAlpha  = Constants.ALPHA_MIN
    arrowDownTarget = Constants.ALPHA_MIN
    arrowVisible = false
end

return Sprites