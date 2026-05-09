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
    limitDistance = true,
    maxDistance   = 4096,
}

-- ============================================================================
-- Lens Defaults
-- ============================================================================

Settings.Lens =
{
    defaultFOV  = 90,
    minFOV      = 30,
    maxFOV      = 120,
    fovStep     = 2,
    defaultRoll = 0,
    minRoll     = -180,
    maxRoll     = 180,
    rollStep    = 1,
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
    defaultRadius  = 20,
    minRadius      = 1,
    maxRadius      = 80,
    radiusStep     = 1,
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
    { name = "Pistols",  objID = TEN.Objects.ObjID.PISTOLS_ANIM, meshIndices = {10, 13}, weaponType = TEN.Objects.WeaponType.PISTOLS, type = "holsters" },
    { name = "Pistols (Left)",  objID = TEN.Objects.ObjID.PISTOLS_ANIM, meshIndices = {13}, weaponType = TEN.Objects.WeaponType.PISTOLS, type = "left" },
    { name = "Pistols (Right)",  objID = TEN.Objects.ObjID.PISTOLS_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.PISTOLS, type = "right" },
    { name = "Shotgun",  objID = TEN.Objects.ObjID.SHOTGUN_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.SHOTGUN, type = "back" },
    { name = "Uzis",  objID = TEN.Objects.ObjID.UZI_ANIM, meshIndices = {10, 13}, weaponType = TEN.Objects.WeaponType.UZIS, type = "holsters" },
    { name = "Revolver",  objID = TEN.Objects.ObjID.REVOLVER_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.REVOLVER, type = "right" },
    { name = "HK",  objID = TEN.Objects.ObjID.HK_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.HK, type = "back" },
    { name = "Crossbow",  objID = TEN.Objects.ObjID.CROSSBOW_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.CROSSBOW, type = "back" },
    { name = "Harpoon",  objID = TEN.Objects.ObjID.HARPOON_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.HARPOON_GUN, type = "back" },
    { name = "Grenade Launcher",  objID = TEN.Objects.ObjID.GRENADE_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.GRENADE_LAUNCHER, type = "back" },
    { name = "Rocket Launcher",  objID = TEN.Objects.ObjID.ROCKET_ANIM, meshIndices = {10}, weaponType = TEN.Objects.WeaponType.ROCKET_LAUNCHER, type = "back" },
    { name = "Flare",  objID = TEN.Objects.ObjID.FLARE_ANIM, meshIndices = {13}, weaponType = TEN.Objects.WeaponType.FLARE, type = "none" },
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
    { name = "Waking Up", objID = TEN.Objects.ObjID.LARA_EXTRA_ANIMS, animNumber = 1, frameNumber = 149 },
    { name = "0",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 0,  frameNumber = 0 },
    { name = "1",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 1,  frameNumber = 0 },
    { name = "2",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 2,  frameNumber = 0 },
    { name = "3",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 3,  frameNumber = 0 },
    { name = "4",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 4,  frameNumber = 0 },
    { name = "5",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 5,  frameNumber = 0 },
    { name = "6",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 6,  frameNumber = 0 },
    { name = "7",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 7,  frameNumber = 0 },
    { name = "8",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 8,  frameNumber = 0 },
    { name = "9",   objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 9,  frameNumber = 0 },
    { name = "10",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 10, frameNumber = 0 },
    { name = "11",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 11, frameNumber = 0 },
    { name = "12",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 12, frameNumber = 0 },
    { name = "13",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 13, frameNumber = 0 },
    { name = "14",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 14, frameNumber = 0 },
    { name = "15",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 15, frameNumber = 0 },
    { name = "16",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 16, frameNumber = 0 },
    { name = "17",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 17, frameNumber = 0 },
    { name = "18",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 18, frameNumber = 0 },
    { name = "19",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 19, frameNumber = 0 },
    { name = "20",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 20, frameNumber = 0 },
    { name = "21",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 21, frameNumber = 0 },
    { name = "22",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 22, frameNumber = 0 },
    { name = "23",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 23, frameNumber = 0 },
    { name = "24",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 24, frameNumber = 0 },
    { name = "25",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 25, frameNumber = 0 },
    { name = "26",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 26, frameNumber = 0 },
    { name = "27",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 27, frameNumber = 0 },
    { name = "28",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 28, frameNumber = 0 },
    { name = "29",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 29, frameNumber = 0 },
    { name = "30",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 30, frameNumber = 0 },
    { name = "31",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 31, frameNumber = 0 },
    { name = "32",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 32, frameNumber = 0 },
    { name = "33",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 33, frameNumber = 0 },
    { name = "34",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 34, frameNumber = 0 },
    { name = "35",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 35, frameNumber = 0 },
    { name = "36",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 36, frameNumber = 0 },
    { name = "37",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 37, frameNumber = 0 },
    { name = "38",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 38, frameNumber = 0 },
    { name = "39",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 39, frameNumber = 0 },
    { name = "40",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 40, frameNumber = 0 },
    { name = "41",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 41, frameNumber = 0 },
    { name = "42",  objID = TEN.Objects.ObjID.LARA_BIGGUN_ANIM, animNumber = 42, frameNumber = 0 },
}

-- ============================================================================
-- Frames (full-screen sprite overlays)
-- ============================================================================

Settings.Frames =
{
    objectID  = TEN.Objects.ObjID.DIARY_ENTRY_SPRITES,
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
-- Sunglasses
-- ============================================================================

Settings.Sunglasses =
{
    meshName   = "pm_Sunglasses",
    objID      = TEN.Objects.ObjID.ACTOR1_SPEECH_HEAD1
}

-- ============================================================================
-- Entry
-- ============================================================================

Settings.Entry =
{
    holdFrames = 15,  -- Walk + Inventory held for N frames to enter
}

return Settings
