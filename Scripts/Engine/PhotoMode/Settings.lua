--- Settings for the PhotoMode module.
-- Reads colors from RingInventory.Settings where available.
-- @module Engine.PhotoMode.Settings
-- @local

local RingSettings = require("Engine.RingInventory.Settings")

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
    menuSelect = RingSettings.SoundMap.menuSelect,
    menuChoose = RingSettings.SoundMap.menuChoose,
    menuRotate = RingSettings.SoundMap.menuRotate,
}

-- ============================================================================
-- Animation
-- ============================================================================

Settings.Animation =
{
    transitionSpeed = RingSettings.Animation.transitionSpeed,
    fadeSpeed        = RingSettings.Animation.transitionSpeed,
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
    defaultLookSpeed  = 2.0,
    minLookSpeed      = 0.5,
    maxLookSpeed      = 5.0,
    lookSpeedStep     = 0.5,
    mouseSensitivity  = 30,
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
    rollStep    = 1,
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
    colorPresets   =
    {
        { name = "White",   color = TEN.Color(255, 255, 255) },
        { name = "Warm",    color = TEN.Color(255, 220, 180) },
        { name = "Cool",    color = TEN.Color(180, 210, 255) },
        { name = "Red",     color = TEN.Color(255, 80, 80) },
        { name = "Green",   color = TEN.Color(80, 255, 80) },
        { name = "Blue",    color = TEN.Color(80, 80, 255) },
        { name = "Magenta", color = TEN.Color(255, 80, 255) },
    },
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
    tints =
    {
        { name = "Neutral", color = TEN.Color(128, 128, 128) },
        { name = "Warm",    color = TEN.Color(255, 160,  80) },
        { name = "Cool",    color = TEN.Color( 80, 160, 255) },
        { name = "Green",   color = TEN.Color( 80, 255,  80) },
        { name = "Magenta", color = TEN.Color(128,  40, 128) },
        { name = "Red",     color = TEN.Color(128,  40,  40) },
        { name = "Sepia",   color = TEN.Color(255, 200, 120) },
    },
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
    presets   =
    {
        { name = "None",    spriteID = -1 },
        { name = "Cinematic Bars", spriteID = 0 },
        { name = "Tomb Raider Logo", spriteID = 1 },
        { name = "Polaroid", spriteID = 2 },
        { name = "Recording", spriteID = 3 }
    },
}

-- ============================================================================
-- Outfit / Weapon Presets
-- ============================================================================

Settings.Outfits =
{
    -- Index 1: Default — restores whatever state was active on photo mode entry.
    { name = "Default" },

    -- skin:              Array of up to 5 ObjIDs → Lara:SetSkin(skin, skinJoints, skinScream, hair1, hair2).
    --                    Nil entries leave that slot unchanged.
    -- skinnedMesh:       ObjID → Lara:SwapSkinnedMesh(objID [, skinnedMeshIndex]).
    --                    "clear" → Lara:ClearSkinnedMesh() (disables GPU skin entirely).
    -- skinnedMeshIndex:  Optional sub-index for SwapSkinnedMesh.
    -- meshVisible:       Controls classic mesh visibility.
    --                    "none" or nil → hide all classic meshes.
    --                    "all"         → keep all classic meshes visible.
    --                    { i, ... }    → keep only listed indices visible, hide the rest.
    -- onEnter:           Optional function() called after the outfit is applied.
    -- unlocked:          true/nil = outfit is visible in the menu.
    --                    false    = hidden until PhotoMode.UnlockOutfit(name) is called.

    { name = "Classic TR4",
      skin = { TEN.Objects.ObjID.ANIMATING1, TEN.Objects.ObjID.ANIMATING2,
               TEN.Objects.ObjID.ANIMATING3, TEN.Objects.ObjID.ANIMATING4 },
        meshVisible = "all",
    },

        { name = "Classic TR2",
      skin = { TEN.Objects.ObjID.ANIMATING18, TEN.Objects.ObjID.ANIMATING19,
               TEN.Objects.ObjID.ANIMATING20, TEN.Objects.ObjID.ANIMATING21 },
        meshVisible = "all",
    },

    { name = "Remastered",
      skin = { TEN.Objects.ObjID.ANIMATING14, TEN.Objects.ObjID.ANIMATING15,
               TEN.Objects.ObjID.ANIMATING16, TEN.Objects.ObjID.ANIMATING17 },
        meshVisible = "all",
        onEnter = 
        function()
            local settings = TEN.Flow.GetSettings()
            settings.Hair[1].offset = Vec3(-4, 3, -28)
            TEN.Flow.SetSettings(settings)
        end
    },

    { name = "Dark Raider",
      skin = { TEN.Objects.ObjID.ANIMATING10, TEN.Objects.ObjID.ANIMATING11,
               TEN.Objects.ObjID.ANIMATING12, TEN.Objects.ObjID.ANIMATING13 },
        meshVisible = "all",
    },

    { name = "Underworld Casual",
      skin = { TEN.Objects.ObjID.ANIMATING6, TEN.Objects.ObjID.ANIMATING7,
               TEN.Objects.ObjID.ANIMATING8, TEN.Objects.ObjID.ANIMATING9 },
        meshVisible = "all",
    },

    { name = "TEN Lara",
      skinnedMesh = TEN.Objects.ObjID.LARA_EXTRA_MESH1,
      meshVisible = {10, 13},
    },

    { name = "Jeans",
      skinnedMesh = TEN.Objects.ObjID.LARA_EXTRA_MESH2,
      meshVisible = "none",
    },
}

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

Settings.Expressions =
{
    { name = "Default", objID = nil, meshIndices = {} },
    { name = "Scream", objID = TEN.Objects.ObjID.LARA_SCREAM, meshIndices = {14} },
    { name = "Talk 1", objID = TEN.Objects.ObjID.LARA_SPEECH_HEAD1, meshIndices = {14} },
    { name = "Talk 2", objID = TEN.Objects.ObjID.LARA_SPEECH_HEAD2, meshIndices = {14} },
    { name = "Talk 3", objID = TEN.Objects.ObjID.LARA_SPEECH_HEAD3, meshIndices = {14} },
    { name = "Talk 4", objID = TEN.Objects.ObjID.LARA_SPEECH_HEAD4, meshIndices = {14} },
}

Settings.Animations =
{
    { name = "Default",        objID = TEN.Objects.ObjID.LARA, animNumber = 0, frameNumber = 0 },
    { name = "0",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 0,  frameNumber = 0 },
    { name = "1",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 1,  frameNumber = 0 },
    { name = "2",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 2,  frameNumber = 0 },
    { name = "3",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 3,  frameNumber = 0 },
    { name = "4",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 4,  frameNumber = 0 },
    { name = "5",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 5,  frameNumber = 0 },
    { name = "6",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 6,  frameNumber = 0 },
    { name = "7",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 7,  frameNumber = 0 },
    { name = "8",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 8,  frameNumber = 0 },
    { name = "9",   objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 9,  frameNumber = 0 },
    { name = "10",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 10, frameNumber = 0 },
    { name = "11",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 11, frameNumber = 0 },
    { name = "12",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 12, frameNumber = 0 },
    { name = "13",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 13, frameNumber = 0 },
    { name = "14",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 14, frameNumber = 0 },
    { name = "15",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 15, frameNumber = 0 },
    { name = "16",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 16, frameNumber = 0 },
    { name = "17",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 17, frameNumber = 0 },
    { name = "18",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 18, frameNumber = 0 },
    { name = "19",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 19, frameNumber = 0 },
    { name = "20",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 20, frameNumber = 0 },
    { name = "21",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 21, frameNumber = 0 },
    { name = "22",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 22, frameNumber = 0 },
    { name = "23",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 23, frameNumber = 0 },
    { name = "24",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 24, frameNumber = 0 },
    { name = "25",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 25, frameNumber = 0 },
    { name = "26",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 26, frameNumber = 0 },
    { name = "27",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 27, frameNumber = 0 },
    { name = "28",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 28, frameNumber = 0 },
    { name = "29",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 29, frameNumber = 0 },
    { name = "30",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 30, frameNumber = 0 },
    { name = "31",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 31, frameNumber = 0 },
    { name = "32",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 32, frameNumber = 0 },
    { name = "33",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 33, frameNumber = 0 },
    { name = "34",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 34, frameNumber = 0 },
    { name = "35",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 35, frameNumber = 0 },
    { name = "36",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 36, frameNumber = 0 },
    { name = "37",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 37, frameNumber = 0 },
    { name = "38",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 38, frameNumber = 0 },
    { name = "39",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 39, frameNumber = 0 },
    { name = "40",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 40, frameNumber = 0 },
    { name = "41",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 41, frameNumber = 0 },
    { name = "42",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 42, frameNumber = 0 },
}

-- ============================================================================
-- Sunglasses
-- ============================================================================

Settings.Sunglasses =
{
    meshName   = "pm_Sunglasses",
    objID      = TEN.Objects.ObjID.ACTOR1_SPEECH_HEAD1,
    enabled    = true,    -- Set to false to hide the Sunglasses option entirely.
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
