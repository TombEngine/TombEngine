--- Internal file used by the PhotoMode module.
-- @module PhotoMode.Settings
-- @local

local Settings = {}

Settings.Camera = 
{
    limitCameraDistance = true,
    distance = 4096,
    depthOfFieldEnabled = false
}

Settings.Character = 
{
    accessoriesEnabled = false,
    outfitsEnabled = false
}

Settings.ColorMap =
{
    plainText = TEN.Flow.GetSettings().UI.plainTextColor,
    headerText = TEN.Flow.GetSettings().UI.headerTextColor,
    optionText = TEN.Flow.GetSettings().UI.optionTextColor,
    neutral      = Color(255, 255, 255, 255),
    dimmed       = Color(120, 120, 120, 255),
    highlight    = Color(255, 255, 80, 255),
}

Settings.SoundMap =
{
    menuRotate = 108,
    menuSelect = 109,
    menuChoose = 111,
    menuOpen = 109,
    menuClose = 109,
}

return Settings