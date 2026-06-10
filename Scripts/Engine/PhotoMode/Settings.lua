-- ldignore
-- Settings for the PhotoMode module.
-- Reads colors from RingInventory.Settings where available.

local Accessories = require("Engine.PhotoMode.Accessories")
local Expressions = require("Engine.PhotoMode.Expressions")
local Frames = require("Engine.PhotoMode.Frames")
local Outfits = require("Engine.PhotoMode.Outfits")
local Poses = require("Engine.PhotoMode.Poses")

local Settings = {}

-- ============================================================================
-- Colors (inherited from Ring Inventory where possible)
-- ============================================================================

Settings.ColorMap =
{
    plainText = TEN.Flow.GetSettings().UI.plainTextColor,
    headerText = TEN.Flow.GetSettings().UI.headerTextColor,
    optionText = TEN.Flow.GetSettings().UI.optionTextColor,
    neutral      = Color(255, 255, 255, 255),
    dimmed       = Color(120, 120, 120, 255),
    highlight    = Color(255, 255, 80, 255),
}

-- ============================================================================
-- Sounds
-- ============================================================================

Settings.SoundMap =
{
    menuRotate = 108,
    menuSelect = 109,
    menuChoose = 111,
    menuOpen = 109,
    menuClose = 109,
}

-- ============================================================================
-- Animation
-- ============================================================================

Settings.Animation =
{
    transitionSpeed = 50,
    fadeSpeed        = 50,
}

-- ============================================================================
-- Camera Defaults
-- ============================================================================

Settings.Camera =
{
    meshName         = "pm_CameraMesh",
    targetName       = "pm_CameraTarget",
    meshIndex        = 0,
    targetIndex      = 0,
    defaultMoveSpeed = 64,
    minMoveSpeed     = 8,
    maxMoveSpeed     = 512,
    moveSpeedStep    = 8,
    defaultLookSpeed  = 8.0,
    minLookSpeed      = 0.5,
    maxLookSpeed      = 5.0,
    lookSpeedStep     = 0.5,
    mouseSensitivity  = 32,
    offsetForward    = -512,
    offsetUp         = -256,
    targetForward    = 512,
    targetUp         = -256,

    -- Distance limit from Lara's entry position (snap.laraPos)
    defaultLimitDistance = false,
    defaultMaxDistance   = 4096,
    minMaxDistance       = 512,
    maxMaxDistance       = 16384,
    distanceStep         = 512,
}

-- ============================================================================
-- Lens Defaults
-- ============================================================================

Settings.Lens =
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

Settings.DepthOfField =
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

Settings.Player =
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
Settings.ColorPalette =
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

Settings.Light =
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
    colorPresets   = Settings.ColorPalette,
    sourceNames = { "Manual", "Follow Camera", "Follow Lara" },
}

-- ============================================================================
-- Filter / Tint Presets
-- ============================================================================

Settings.Filters =
{
    presets =
    {
        { name = "Off",        mode = TEN.View.PostProcessMode.NONE },
        { name = "Monochrome", mode = TEN.View.PostProcessMode.MONOCHROME },
        { name = "Negative",   mode = TEN.View.PostProcessMode.NEGATIVE },
        { name = "Exclusion",  mode = TEN.View.PostProcessMode.EXCLUSION },
    },
    tints                = Settings.ColorPalette,
    defaultTintIntensity = 0.0,
    minTintIntensity     = 0.0,
    maxTintIntensity     = 1.0,
    tintIntensityStep    = 0.05,
}


-- ============================================================================
-- Frames (full-screen sprite overlays)
-- ============================================================================

Settings.Frames =
{
    objectID  = TEN.Objects.ObjID.PHOTOMODE_FRAMES,
    position  = TEN.Vec2(50, 50),
    rotation  = 0,
    scale     = TEN.Vec2(100, 100),
    alignMode = TEN.View.AlignMode.CENTER,
    scaleMode = TEN.View.ScaleMode.STRETCH,
    blendMode = TEN.Effects.BlendID.ALPHA_BLEND,
    color     = TEN.Color(255, 255, 255),
    alpha     = 255,
    presets   = Frames
}

-- ============================================================================
-- Outfit / Weapon Presets
-- ============================================================================

Settings.Outfits = Outfits

Settings.Weapons =
{
    { name = "Default", objID = TEN.Objects.ObjID.LARA_SKIN, meshIndices = {}, weaponType = TEN.Objects.WeaponType.NONE, type = "none" },
    { name = TEN.Flow.GetString("pistols"),  objID = TEN.Objects.ObjID.PISTOLS_ANIM, meshIndices = {10, 13}, weaponType = TEN.Objects.WeaponType.PISTOLS, pickupObjID = TEN.Objects.ObjID.PISTOLS_ITEM, type = "holsters" },
    { name = TEN.Flow.GetString("pistols").."(Right)",  objID = TEN.Objects.ObjID.PISTOLS_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.NONE, pickupObjID = TEN.Objects.ObjID.PISTOLS_ITEM,type = "right" },
    { name = TEN.Flow.GetString("pistols").."(Left)",  objID = TEN.Objects.ObjID.PISTOLS_ANIM, meshIndices = {13}, weaponType = TEN.Objects.WeaponType.NONE, pickupObjID = TEN.Objects.ObjID.PISTOLS_ITEM,type = "left" },
    { name = TEN.Flow.GetString("shotgun"),  objID = TEN.Objects.ObjID.SHOTGUN_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.SHOTGUN, pickupObjID = TEN.Objects.ObjID.SHOTGUN_ITEM,type = "back" },
    { name = TEN.Flow.GetString("uzis"),  objID = TEN.Objects.ObjID.UZI_ANIM, meshIndices = {10, 13}, weaponType = TEN.Objects.WeaponType.UZIS, pickupObjID = TEN.Objects.ObjID.UZI_ITEM,type = "holsters" },
    { name = TEN.Flow.GetString("revolver"),  objID = TEN.Objects.ObjID.REVOLVER_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.REVOLVER, pickupObjID = TEN.Objects.ObjID.REVOLVER_ITEM,type = "right" },
    { name = TEN.Flow.GetString("hk"),  objID = TEN.Objects.ObjID.HK_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.HK, pickupObjID = TEN.Objects.ObjID.HK_ITEM,type = "back" },
    { name = TEN.Flow.GetString("crossbow"),  objID = TEN.Objects.ObjID.CROSSBOW_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.CROSSBOW, pickupObjID = TEN.Objects.ObjID.CROSSBOW_ITEM,type = "back" },
    { name = TEN.Flow.GetString("harpoon_gun"),  objID = TEN.Objects.ObjID.HARPOON_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.HARPOON_GUN, pickupObjID = TEN.Objects.ObjID.HARPOON_ITEM,type = "back" },
    { name = TEN.Flow.GetString("grenade_launcher"),  objID = TEN.Objects.ObjID.GRENADE_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.GRENADE_LAUNCHER, pickupObjID = TEN.Objects.ObjID.GRENADE_GUN_ITEM,type = "back" },
    { name = TEN.Flow.GetString("rocket_launcher"),  objID = TEN.Objects.ObjID.ROCKET_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.ROCKET_LAUNCHER, pickupObjID = TEN.Objects.ObjID.ROCKET_LAUNCHER_ITEM,type = "back" },
    { name = TEN.Flow.GetString("flares"),  objID = TEN.Objects.ObjID.FLARE_ANIM, meshIndices = {13}, weaponType = TEN.Objects.WeaponType.FLARE, pickupObjID = TEN.Objects.ObjID.FLARE_INV_ITEM, type = "none" },
    { name = TEN.Flow.GetString("crowbar"),  objID = TEN.Objects.ObjID.LARA_CROWBAR_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.NONE, pickupObjID = TEN.Objects.ObjID.CROWBAR_ITEM, type = "none" },
}

Settings.Expressions = Expressions
Settings.Animations = Poses

-- ============================================================================
-- Accessories
-- ============================================================================

Settings.Accessories =
{
    -- Name of the moveable spawned in the level to host accessory mesh swaps.
    meshName = "pm_Sunglasses",
    -- Base object used when spawning the moveable (any Lara-compatible skeleton).
    baseObjID = TEN.Objects.ObjID.PHOTOMODE_ANIMS,
    -- Set to false to hide the Accessory option entirely.
    enabled  = true,
    presets  = Accessories,
}

-- ============================================================================
-- Entry
-- ============================================================================

Settings.Entry =
{
    holdFrames = 1,  -- Walk + Inventory held for N frames to enter
}
-- ============================================================================
-- Header Sprites
-- ============================================================================
-- One sprite per header tab drawn below the header labels.
-- Selected tab  = sizeActive  + colorActive  (fully highlighted).
-- Other tabs    = sizeInactive + colorInactive (dimmed).

Settings.HeaderSprites =
{
    objectID      = TEN.Objects.ObjID.PHOTOMODE_SPRITES, -- object that owns the sprites
    spriteIDs     = { 0, 1, 2, 3, 4 },                    -- one sprite index per header tab
    position      = TEN.Vec2(16, 15),                      -- center of the sprite row (percent)
    spacing       = 6,                                     -- percent spacing between sprites
    sizeActive    = TEN.Vec2(6, 6),                        -- size when selected
    sizeInactive  = TEN.Vec2(4, 4),                        -- size when not selected
    colorActive   = TEN.Color(255, 255, 255),
    colorInactive = TEN.Color(100, 100, 100),
    rotation  = 0,
    alignMode = TEN.View.AlignMode.CENTER,
    scaleMode = TEN.View.ScaleMode.FIT,
    blendMode = TEN.Effects.BlendID.ALPHA_BLEND,
    layer     = -4,
}
return Settings
