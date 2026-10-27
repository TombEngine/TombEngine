--- Internal file used by the RingInventory module.
-- @module RingInventory.Settings
-- @local

local Settings = {}

Settings.SoundMap =
{
    playerNo = 2,
    menuRotate = 108,
    menuSelect = 109,
    menuChoose = 111,
    menuCombine = 114,
    inventoryOpen = 109,
    inventoryClose = 109,
}

Settings.ColorMap =
{
    plainText = Flow.GetSettings().UI.plainTextColor,
    headerText = Flow.GetSettings().UI.headerTextColor,
    optionText = Flow.GetSettings().UI.optionTextColor,
    background = Color(64, 64, 64, 128),
    inventoryAmbient = Color(255, 255, 128),
    itemHidden = Color(0, 0, 0, 0),
    itemDeselected = Color(32, 32, 32, 255),
    itemSelected = Color(128, 128, 128, 255),
    neutral = Color(255, 255, 255, 255),
    arrowColor = Flow.GetSettings().UI.headerTextColor,
}

Settings.Background =
{
    enable = true,
    objectID = TEN.Objects.ObjID.INVENTORY_SPRITES,
    spriteID = 1,
    color = TEN.Color(255, 255, 255),
    position = TEN.Vec2(50,50),
    rotation = 0,
    scale = TEN.Vec2(100,100),
    alignMode = TEN.View.AlignMode.CENTER,
    scaleMode = TEN.View.ScaleMode.STRETCH,
    blendMode = TEN.Effects.BlendID.ALPHA_BLEND,
    alpha = 64
}

Settings.Animation =
{
    inventoryAnimTime = 0.5,
    itemAnimTime = 0.2,
    skipRingClose = false,
    transitionSpeed = 50
}

Settings.Statistics =
{
    progressTime = true,
    gameStats = true
}

return Settings