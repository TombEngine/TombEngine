-- ldignore
-- Configuration for the PhotoMode module.

local Accessories = require("Engine.PhotoMode.Accessories")
local Expressions = require("Engine.PhotoMode.Expressions")
local Frames = require("Engine.PhotoMode.Frames")
local Outfits = require("Engine.PhotoMode.Outfits")
local Poses = require("Engine.PhotoMode.Poses")
local Settings = require("Engine.PhotoMode.Settings")
local Weapons = require("Engine.PhotoMode.Weapons")

local Configuration = {}

-- ============================================================================
-- Colors
-- ============================================================================

Configuration.ColorMap = Settings.ColorMap

-- ============================================================================
-- Sounds
-- ============================================================================

Configuration.SoundMap = Settings.SoundMap

-- ============================================================================
-- Animation
-- ============================================================================

Configuration.Animation =
{
    fadeSpeed        = 50,
}

-- ============================================================================
-- Camera Defaults
-- ============================================================================

Configuration.Camera =
{
    meshName         = "pm_CameraMesh",
    targetName       = "pm_CameraTarget",
    meshIndex        = 0,
    targetIndex      = 0,
    defaultMoveSpeed = 50,
    defaultLookSpeed  = 8.0,
    mouseSensitivity  = 32,
    offsetForward    = -512,
    offsetUp         = -256,
    targetForward    = 512,
    targetUp         = -256,

    defaultLimitDistance = Settings.Camera.limitCameraDistance,
    defaultMaxDistance   = Settings.Camera.distance,
}

-- ============================================================================
-- Lens Defaults
-- ============================================================================

Configuration.Lens =
{
    defaultFOV  = 90,
    minFOV      = 30,
    maxFOV      = 120,
    fovStep     = 1,
    defaultRoll = 0,
    minRoll     = -180,
    maxRoll     = 180,
    rollStep    = 5,
}

-- ============================================================================
-- Depth of Field
-- ============================================================================

Configuration.DepthOfField =
{
    -- Mode selector: index into modes table (1 = Off / NONE)
    modes =
    {
        { name = "Off",   mode = TEN.View.DOFMode.NONE  },
        { name = "Full",  mode = TEN.View.DOFMode.FULL  },
        { name = "Front", mode = TEN.View.DOFMode.FRONT },
        { name = "Back",  mode = TEN.View.DOFMode.BACK  },
    },
    defaultMode          = 1,     -- index into modes (1 = Off)

    -- Focus distance: world units to the sharp focal plane
    defaultFocusDistance = 1536,
    minFocusDistance     = 64,
    maxFocusDistance     = 8192,
    focusDistanceStep    = 64,

    -- Range: width of the sharp focus region in world units
    defaultRange         = 2048,
    minRange             = 64,
    maxRange             = 8192,
    rangeStep            = 64,

    -- Strength: maximum bokeh radius (clamped to [0, 1])
    defaultStrength      = 0.5,
    minStrength          = 0.0,
    maxStrength          = 1.0,
    strengthStep         = 0.05,
}

-- ============================================================================
-- Player
-- ============================================================================

Configuration.Player =
{
    moveSpeed    = 64,
    rotateSpeed  = 2,
}

-- ============================================================================
-- Light Defaults
-- ============================================================================

-- Shared 32-colour palette used by both the light colour picker and the tint
-- picker.  Colours are evenly spaced across the HSV hue wheel (S=1, V=1),
-- matching the 32-swatch rainbow strip sprite left-to-right.
Configuration.ColorPalette =
{
    { color = TEN.Color(255, 255, 255) },
    { color = TEN.Color(255, 0, 0) }, --  1  Red
    { color = TEN.Color(255, 45, 0) }, --  2  Red-orange
    { color = TEN.Color(255, 99, 0) }, --  3  Orange
    { color = TEN.Color(255, 150, 0) }, --  4  Dark orange
    { color = TEN.Color(255, 199, 0) }, --  5  Amber
    { color = TEN.Color(255, 248, 0) }, --  6  Yellow
    { color = TEN.Color(215, 255, 0) }, --  7  Yellow-green
    { color = TEN.Color(167, 255, 0) }, --  8  Chartreuse
    { color = TEN.Color(116, 255, 0) }, --  9  Spring green
    { color = TEN.Color(64, 255, 0) }, -- 10  Green
    { color = TEN.Color(8, 255, 0) }, -- 11  Bright green
    { color = TEN.Color(0, 255, 27) }, -- 12  Green
    { color = TEN.Color(0, 255, 81) }, -- 13  Cyan-green
    { color = TEN.Color(0, 255, 133) }, -- 14  Teal-green
    { color = TEN.Color(0, 255, 183) }, -- 15  Teal
    { color = TEN.Color(0, 255, 231) }, -- 16  Cyan-teal
    { color = TEN.Color(0, 231, 255) }, -- 17  Cyan
    { color = TEN.Color(0, 183, 255) }, -- 18  Sky cyan
    { color = TEN.Color(0, 133, 255) }, -- 19  Sky blue
    { color = TEN.Color(0, 81, 255) }, -- 20  Azure
    { color = TEN.Color(0, 27, 255) }, -- 21  Blue
    { color = TEN.Color(8, 0, 255) }, -- 22  Deep blue
    { color = TEN.Color(64, 0, 255) }, -- 23  Blue-violet
    { color = TEN.Color(116, 0, 255) }, -- 24  Violet-blue
    { color = TEN.Color(167, 0, 255) }, -- 25  Violet
    { color = TEN.Color(215, 0, 255) }, -- 26  Purple-violet
    { color = TEN.Color(248, 0, 248) }, -- 27  Purple-magenta
    { color = TEN.Color(255, 0, 199) }, -- 28  Magenta
    { color = TEN.Color(255, 0, 150) }, -- 29  Pink-magenta
    { color = TEN.Color(255, 0, 99) }, -- 30  Hot pink
    { color = TEN.Color(255, 0,  45) }, -- 31  Deep pink
    { color = TEN.Color(255, 0,  0) }, -- 32  Red-pink
}

Configuration.Light =
{
    defaultRadius  = 8,
    minRadius      = 1,
    maxRadius      = 20,
    radiusStep     = 1,
    defaultIntensity = 0.5,
    minIntensity     = 0.0,
    maxIntensity     = 1.0,
    intensityStep    = 0.05,
    defaultEnabled = false,
    lightName      = "PHOTO_MODE_LIGHT",
    colorPresets   = Configuration.ColorPalette,
    sourceNames = { "Manual", "Follow Camera", "Follow Lara" },
}

-- ============================================================================
-- Filter / Tint Presets
-- ============================================================================

Configuration.Filters =
{
    presets =
    {
        { name = "Off",        mode = TEN.View.PostProcessMode.NONE },
        { name = "Monochrome", mode = TEN.View.PostProcessMode.MONOCHROME },
        { name = "Negative",   mode = TEN.View.PostProcessMode.NEGATIVE },
        { name = "Exclusion",  mode = TEN.View.PostProcessMode.EXCLUSION },
    },
    tints                = Configuration.ColorPalette,
    defaultTintIntensity = 0.0,
    minTintIntensity     = 0.0,
    maxTintIntensity     = 1.0,
    tintIntensityStep    = 0.05,
}


-- ============================================================================
-- Frames (full-screen sprite overlays)
-- ============================================================================

Configuration.Frames =
{
    objectID  = TEN.Objects.ObjID.PHOTOMODE_FRAMES,
    position  = TEN.Vec2(50, 50),
    rotation  = 0,
    scale     = TEN.Vec2(100, 100),
    alignMode = TEN.View.AlignMode.CENTER,
    scaleMode = TEN.View.ScaleMode.STRETCH,
    blendMode = TEN.Effects.BlendID.ALPHA_BLEND,
    color     = Settings.ColorMap.neutral,
    alpha     = 255,
    presets   = Frames
}

-- ============================================================================
-- Outfit / Weapon Presets
-- ============================================================================

Configuration.Outfits = Outfits
Configuration.Weapons = Weapons
Configuration.Expressions = Expressions
Configuration.Animations = Poses

-- ============================================================================
-- Accessories
-- ============================================================================

Configuration.Accessories =
{
    meshName = "pm_Sunglasses",
    baseObjID = TEN.Objects.ObjID.PHOTOMODE_ANIMS,
    presets  = Accessories,
}

-- ============================================================================
-- Entry
-- ============================================================================

Configuration.Entry =
{
    holdFrames = 1,  -- Walk + Inventory held for N frames to enter
}
-- ============================================================================
-- Header Sprites
-- ============================================================================
-- One sprite per header tab drawn below the header labels.
-- Selected tab  = sizeActive  + colorActive  (fully highlighted).
-- Other tabs    = sizeInactive + colorInactive (dimmed).

Configuration.HeaderSprites =
{
    objectID      = TEN.Objects.ObjID.PHOTOMODE_SPRITES, -- object that owns the sprites
    spriteIDs     = { 0, 1, 2, 3, 4 },                    -- one sprite index per header tab
    position      = TEN.Vec2(16, 15),                      -- center of the sprite row (percent)
    spacing       = 6,                                     -- percent spacing between sprites
    sizeActive    = TEN.Vec2(6, 6),                        -- size when selected
    sizeInactive  = TEN.Vec2(4, 4),                        -- size when not selected
    colorActive   = Settings.ColorMap.neutral,
    colorInactive = Settings.ColorMap.dimmed,
    rotation  = 0,
    alignMode = TEN.View.AlignMode.CENTER,
    scaleMode = TEN.View.ScaleMode.FIT,
    blendMode = TEN.Effects.BlendID.ALPHA_BLEND,
    layer     = -4,
}
return Configuration
