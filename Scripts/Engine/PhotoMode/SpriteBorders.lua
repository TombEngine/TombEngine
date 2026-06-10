-- ldignore
-- Frame overlay for the PhotoMode module.
-- Draws a full-screen sprite as a decorative frame using the same
-- technique as RingInventory.Sprites background drawing.

local Settings = require("Engine.PhotoMode.Settings")
local States   = require("Engine.PhotoMode.States")

local FrameSprites = {}

local ALPHA_SPEED = Settings.Animation.fadeSpeed

local currentAlpha = 0
local targetAlpha  = 0

-- ============================================================================
-- Public API
-- ============================================================================

function FrameSprites.Show()
    targetAlpha = Settings.Frames.alpha
end

function FrameSprites.Hide()
    targetAlpha = 0
end

function FrameSprites.SetAlpha(alpha)
    targetAlpha = alpha
end

-- ============================================================================
-- Update (call each frame)
-- ============================================================================

function FrameSprites.Update()
    local state = States.Get()
    local preset = Settings.Frames.presets[state.frameIndex]

    -- If preset is "None" (spriteID == -1), fade out
    if not preset or preset.spriteID < 0 then
        targetAlpha = 0
    else
        targetAlpha = Settings.Frames.alpha
    end

    -- Step alpha toward target
    if currentAlpha < targetAlpha then
        currentAlpha = math.min(currentAlpha + ALPHA_SPEED, targetAlpha)
    elseif currentAlpha > targetAlpha then
        currentAlpha = math.max(currentAlpha - ALPHA_SPEED, targetAlpha)
    end
end

-- ============================================================================
-- Draw (call after Update)
-- ============================================================================

function FrameSprites.Draw()
    if currentAlpha <= 0 then return end

    local state  = States.Get()
    local preset = Settings.Frames.presets[state.frameIndex]
    if not preset or preset.spriteID < 0 then return end

    local cfg = Settings.Frames

    local frameColor = TEN.Color(cfg.color.r, cfg.color.g, cfg.color.b, math.floor(currentAlpha))

    local sprite = TEN.View.DisplaySprite(
        cfg.objectID,
        preset.spriteID,
        cfg.position,
        cfg.rotation,
        cfg.scale,
        frameColor
    )

    -- Draw on a layer in front of the scene but behind the UI text
    sprite:Draw(-5, cfg.alignMode, cfg.scaleMode, cfg.blendMode)
end

-- ============================================================================
-- Clear (instant reset)
-- ============================================================================

function FrameSprites.Clear()
    currentAlpha = 0
    targetAlpha  = 0
end

return FrameSprites
