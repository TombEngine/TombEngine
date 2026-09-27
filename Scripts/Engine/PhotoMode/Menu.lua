-- ldignore
-- Menu handling for the PhotoMode module.

local InputHelpers  = require("Engine.PhotoMode.InputHelpers")
local Configuration = require("Engine.PhotoMode.Configuration")
local Constants     = require("Engine.PhotoMode.Constants")

local COLOR_MAP  = Configuration.ColorMap
local SOUND_MAP  = Configuration.SoundMap
local ALPHA_MAX  = Constants.ALPHA_MAX
local ALPHA_MIN  = Constants.ALPHA_MIN
local ActionID   = TEN.Input.ActionID

local Menu = {}
Menu.__index = Menu

-- ============================================================================
-- Menu types (same as RingInventory)
-- ============================================================================

Menu.Type =
{
    ITEMS_ONLY         = 1,
    ITEMS_AND_OPTIONS  = 2,
    OPTIONS_ONLY       = 3,
}

-- ============================================================================
-- Internals
-- ============================================================================

local Menus   = {}            -- All created menus by name
local Headers = {}            -- Ordered list of header definitions
local ActiveHeader = 1        -- Currently selected header index
local ActiveMenus  = {}       -- Set of currently active menus  { [menuName] = true }

local LINE_SPACING = Constants.LINE_SPACING
local SCROLL_SPEED = Constants.SCROLL_SPEED
local FADE_SPEED   = Constants.FADE_SPEED

local DRAW_RATE = Constants.DRAW_RATE

local ITEM_FLAGS_NORMAL = { Strings.DisplayStringOption.SHADOW }
local TEXT_FLAGS_NORMAL = { Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER }
local TEXT_FLAGS_SELECT = { Strings.DisplayStringOption.BLINK, Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER }

Menu.Active = ActiveMenus

LevelFuncs.Engine.PhotoMode = LevelFuncs.Engine.PhotoMode or {}
LevelFuncs.Engine.PhotoMode.Menu = LevelFuncs.Engine.PhotoMode.Menu or {}

-- ============================================================================
-- Utility
-- ============================================================================

local function Clamp(v, lo, hi)
    return math.max(lo, math.min(hi, v))
end

local function ColorCombine(color, alpha)
    return TEN.Color(color.r, color.g, color.b, alpha)
end

local function PerformFunction(functionString)
    local parts = {}
    for part in string.gmatch(functionString, "[^%.]+") do
        table.insert(parts, part)
    end
    local func = LevelFuncs
    for _, key in ipairs(parts) do
        func = func[key]
        if not func then return end
    end
    if type(func) == "function" or type(func) == "userdata" then
        return func()
    end
end

local function PlaySound(soundIndex)
    if type(soundIndex) == "number" then
        TEN.Sound.PlaySound(soundIndex)
    end
end

-- ============================================================================
-- Header System
-- ============================================================================

-- Define all headers. Call once during setup.
-- @param headerList  Array of { name = "Camera", menuName = "pm_camera", hideText = false }
function Menu.SetHeaders(headerList)
    Headers = {}
    for i, h in ipairs(headerList) do
        Headers[i] = {
            name     = h.name,
            menuName = h.menuName,
            hideText = h.hideText or false,
        }
    end
    ActiveHeader = 1
end

function Menu.SetHeaderSpacing(spacing)

    Headers.spacing = spacing

end

-- Get the currently selected header index.
function Menu.GetActiveHeaderIndex()
    return ActiveHeader
end

-- Get the currently selected header name.
function Menu.GetActiveHeaderName()
    local h = Headers[ActiveHeader]
    return h and h.name or ""
end

-- Get the menu name associated with the active header.
function Menu.GetActiveHeaderMenu()
    local h = Headers[ActiveHeader]
    return h and h.menuName or nil
end

-- Get the total number of headers.
function Menu.GetHeaderCount()
    return #Headers
end

-- Get header at index.
function Menu.GetHeader(index)
    return Headers[index]
end

-- Switch to a specific header by index. Deactivates the old header's menu
-- and activates the new one.
function Menu.SetActiveHeader(index)
    if #Headers == 0 then return end

    index = Clamp(index, 1, #Headers)

    -- Deactivate old header's menu
    local oldHeader = Headers[ActiveHeader]
    if oldHeader and oldHeader.menuName then
        Menu.RemoveActive(oldHeader.menuName)
    end

    ActiveHeader = index

    -- Activate new header's menu
    local newHeader = Headers[ActiveHeader]
    if newHeader and newHeader.menuName then
        Menu.AddActive(newHeader.menuName)
    end
end

-- Navigate headers left/right. Called from input handling.
function Menu.NavigateHeader(direction)
    if #Headers == 0 then return end

    local newIndex = ActiveHeader + direction
    if newIndex < 1 then
        newIndex = #Headers
    elseif newIndex > #Headers then
        newIndex = 1
    end

    PlaySound(SOUND_MAP.menuRotate)
    Menu.SetActiveHeader(newIndex)
end

-- ============================================================================
-- Header Drawing
-- ============================================================================

-- Draw the header bar at the specified position.
function Menu.DrawHeaders(position, scale, alpha)
    if #Headers == 0 or alpha < 1 then return end

    local spacing = Headers.spacing or 18  -- percent spacing between header labels
    local totalWidth = (#Headers - 1) * spacing
    local startX = position.x - totalWidth / 2

    for i, header in ipairs(Headers) do
        -- Skip drawing text for headers flagged as text-hidden
        if not header.hideText then
            local x = startX + (i - 1) * spacing
            local isActive = (i == ActiveHeader)

            local color = isActive and COLOR_MAP.headerText or COLOR_MAP.dimmed
            local flags = isActive
                and { Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER, Strings.DisplayStringOption.BLINK }
                or  { Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER }

            local pos = TEN.Util.PercentToScreen(TEN.Vec2(x, position.y))
            local string = TEN.Flow.GetString(header.name)
            local displayText = isActive and ("[ " .. string .. " ]") or string

            local textObj = TEN.Strings.DisplayString(displayText, pos, scale or 1.0, ColorCombine(color, alpha), false, flags)
            TEN.Strings.ShowString(textObj, DRAW_RATE)
        end
    end
end

-- ============================================================================
-- Menu Create / Get / Delete  (same API as RingInventory.Menu)
-- ============================================================================

function Menu.Create(menuName, title, items, acceptFunction, exitFunction, menuType)
    local self = { name = menuName }

    if menuType ~= Menu.Type.ITEMS_ONLY then
        for _, item in ipairs(items or {}) do
            item.currentOption = item.currentOption or 1
        end
    end

    Menus[menuName] =
    {
        name                 = menuName,
        titleString          = title,
        items                = items or {},
        currentItem          = 1,
        visible              = false,
        menuType             = menuType or Menu.Type.ITEMS_AND_OPTIONS,
        exitFunction         = exitFunction,
        acceptFunction       = acceptFunction,
        itemChangeFunction   = nil,
        wrapAroundItems      = true,
        maxVisibleItems      = 16,
        lineSpacing          = LINE_SPACING,
        itemsPosition        = Vec2(10, 20),
        itemsTextFlags       = ITEM_FLAGS_NORMAL,
        itemsSelectedFlags   = ITEM_FLAGS_NORMAL,
        itemsTextColor       = COLOR_MAP.plainText,
        itemsTextScale       = 1,
        itemsTranslate       = false,
        optionsPosition      = Vec2(50, 20),
        optionsTextFlags     = TEXT_FLAGS_NORMAL,
        optionsSelectedFlags = TEXT_FLAGS_SELECT,
        optionsTextColor     = COLOR_MAP.plainText,
        optionsTextScale     = 1,
        optionsTranslate     = false,
        titlePosition        = Vec2(50, 10),
        titleTextFlags       = TEXT_FLAGS_NORMAL,
        titleTextColor       = COLOR_MAP.headerText,
        titleTextScale       = 1.6,
        titleTranslate       = false,
        menuTransparency     = 1,
        sounds               = SOUND_MAP,
        inputs               = true,
        visibleStartIndex    = 1,
        scrollY              = 0,
        targetScrollY        = 0,
        currentAlpha         = 0,
        targetAlpha          = 0,
        fadeSpeed            = FADE_SPEED,
        inputTimer           = 0,
    }

    return setmetatable(self, Menu)
end

function Menu.Get(menuName)
    if Menus[menuName] then
        return setmetatable({ name = menuName }, Menu)
    end
end

function Menu.Delete(menuName)
    Menus[menuName] = nil
end

function Menu.DeleteAll()
    ActiveMenus = {}
    Menu.Active = ActiveMenus
    for name in pairs(Menus) do
        Menus[name] = nil
    end
end

-- ============================================================================
-- Active Menu Management
-- ============================================================================

function Menu.AddActive(menuName, instant)
    if not menuName then return end
    ActiveMenus[menuName] = true
    Menu.Active = ActiveMenus

    local menu = Menus[menuName]
    if menu then
        menu.visible = true
        menu.currentAlpha = instant and ALPHA_MAX or ALPHA_MIN
        menu.targetAlpha = ALPHA_MAX
        menu.inputTimer = 0
    end
end

function Menu.RemoveActive(menuName)
    if not menuName then return end
    local menu = Menus[menuName]
    if menu then
        menu.currentAlpha = ALPHA_MAX
        menu.targetAlpha = ALPHA_MIN
    end
end

function Menu.IsAnyActive()
    for _ in pairs(ActiveMenus) do
        return true
    end
    return false
end

-- ============================================================================
-- Setters (method-style, same API as RingInventory.Menu)
-- ============================================================================

function Menu:SetVisibility(visible)
    local menu = Menus[self.name]
    if not menu then return end
    if visible then
        menu.visible = true
        menu.targetAlpha = ALPHA_MAX
        menu.inputTimer = 0
    else
        menu.targetAlpha = ALPHA_MIN
    end
end

function Menu:SetTransparency(t)
    local menu = Menus[self.name]
    if menu then menu.menuTransparency = Clamp(t, 0, 1) end
end

function Menu:SetFadeSpeed(speed)
    local menu = Menus[self.name]
    if menu then menu.fadeSpeed = Clamp(speed, 1, ALPHA_MAX) end
end

function Menu:SetWrapAroundItems(v)
    local menu = Menus[self.name]
    if menu then menu.wrapAroundItems = v end
end

function Menu:SetAcceptFunction(f)
    local menu = Menus[self.name]
    if menu then menu.acceptFunction = f end
end

function Menu:SetExitFunction(f)
    local menu = Menus[self.name]
    if menu then menu.exitFunction = f end
end

function Menu:SetOnItemChangeFunction(f)
    local menu = Menus[self.name]
    if menu then menu.itemChangeFunction = f end
end

function Menu:SetOnOptionChangeFunction(itemName, f)
    local menu = Menus[self.name]
    if not menu then return end
    for _, item in ipairs(menu.items) do
        if item.itemName == itemName then
            item.onOptionChange = f
            break
        end
    end
end

function Menu:SetTitle(title, fontColor, titleScale, flags, translate)
    local menu = Menus[self.name]
    if not menu then return end
    if title      ~= nil then menu.titleString    = title end
    if fontColor  ~= nil then menu.titleTextColor  = fontColor end
    if titleScale ~= nil then menu.titleTextScale  = titleScale end
    if flags      ~= nil then menu.titleTextFlags  = flags end
    if translate  ~= nil then menu.titleTranslate  = translate end
end

function Menu:SetTitlePosition(p)
    local menu = Menus[self.name]
    if menu then menu.titlePosition = p end
end

function Menu:SetItemsFont(fontColor, fontScale, flags)
    local menu = Menus[self.name]
    if not menu then return end
    if fontColor ~= nil then menu.itemsTextColor = fontColor end
    if fontScale ~= nil then menu.itemsTextScale = fontScale end
    if flags     ~= nil then menu.itemsTextFlags = flags end
end

function Menu:SetItemsTranslate(v)
    local menu = Menus[self.name]
    if menu then menu.itemsTranslate = v end
end

function Menu:SetOptionsFont(fontColor, fontScale, flags)
    local menu = Menus[self.name]
    if not menu then return end
    if fontColor ~= nil then menu.optionsTextColor = fontColor end
    if fontScale ~= nil then menu.optionsTextScale = fontScale end
    if flags     ~= nil then menu.optionsTextFlags = flags end
end

function Menu:SetOptionsTranslate(v)
    local menu = Menus[self.name]
    if menu then menu.optionsTranslate = v end
end

function Menu:SetItemsPosition(p)
    local menu = Menus[self.name]
    if menu then menu.itemsPosition = p end
end

function Menu:SetOptionsPosition(p)
    local menu = Menus[self.name]
    if menu then menu.optionsPosition = p end
end

function Menu:SetLineSpacing(s)
    local menu = Menus[self.name]
    if menu then menu.lineSpacing = s end
end

function Menu:SetVisibleItems(count)
    local menu = Menus[self.name]
    if menu then menu.maxVisibleItems = count end
end

function Menu:SetSelectedItemFlags(flags)
    local menu = Menus[self.name]
    if menu then menu.itemsSelectedFlags = flags end
end

function Menu:SetSelectedOptionsFlags(flags)
    local menu = Menus[self.name]
    if menu then menu.optionsSelectedFlags = flags end
end

function Menu:IsVisible()
    local menu = Menus[self.name]
    return menu and menu.visible or false
end

function Menu:GetAlpha()
    local menu = Menus[self.name]
    return menu and menu.currentAlpha or 0
end

function Menu:SetSoundEffects(select, choose)
    local menu = Menus[self.name]
    if not menu then return end
    menu.sounds = {}
    if type(select) == "number" then menu.sounds.menuSelect = select end
    if type(choose) == "number" then menu.sounds.menuChoose = choose end
end

function Menu:ClearSoundEffects()
    local menu = Menus[self.name]
    if menu then menu.sounds = {} end
end

function Menu:EnableInputs(v)
    local menu = Menus[self.name]
    if menu then menu.inputs = v end
end

function Menu:Reset()
    local menu = Menus[self.name]
    if not menu then return end
    menu.currentItem = 1
    menu.visibleStartIndex = 1
    menu.scrollY = 0
    menu.targetScrollY = 0
    menu.inputTimer = 0
    for _, item in ipairs(menu.items) do
        item.currentOption = 1
    end
end

-- ============================================================================
-- Getters
-- ============================================================================

function Menu:GetCurrentItem()
    local menu = Menus[self.name]
    return menu and menu.items[menu.currentItem] or nil
end

function Menu:GetCurrentItemName()
    local menu = Menus[self.name]
    local item = menu and menu.items[menu.currentItem]
    return item and item.itemName or nil
end

function Menu:GetCurrentOption()
    local menu = Menus[self.name]
    local item = menu and menu.items[menu.currentItem]
    return (item and item.options and item.options[item.currentOption]) or nil
end

function Menu:GetOptionForItem(itemIndex)
    local menu = Menus[self.name]
    local item = menu.items[itemIndex]
    return item.options[item.currentOption]
end

function Menu:GetCurrentItemIndex()
    local menu = Menus[self.name]
    return menu.currentItem
end

function Menu:GetCurrentOptionIndex()
    local menu = Menus[self.name]
    local item = menu.items[menu.currentItem]
    return item.currentOption or 1
end

function Menu:GetOptionIndexForItem(itemIndex)
    local menu = Menus[self.name]
    return menu.items[itemIndex].currentOption
end

function Menu:SetOptionIndexForItem(itemIndex, optionIndex)
    local menu = Menus[self.name]
    local item = menu.items[itemIndex]
    local maxOpt = item.options and #item.options or 1
    item.currentOption = Clamp(optionIndex, 1, maxOpt)
end

function Menu:SetOptionIndexForItemName(itemName, optionIndex)
    local menu = Menus[self.name]
    if not menu then return end
    for _, item in ipairs(menu.items) do
        if item.itemName == itemName then
            local maxOpt = item.options and #item.options or 1
            item.currentOption = Clamp(optionIndex, 1, maxOpt)
            return
        end
    end
end

function Menu:GetOptionIndexForItemName(itemName)
    local menu = Menus[self.name]
    if not menu then return nil end
    for _, item in ipairs(menu.items) do
        if item.itemName == itemName then
            return item.currentOption or 1
        end
    end
    return nil
end

function Menu:SetCurrentItem(itemIndex)
    local menu = Menus[self.name]
    menu.currentItem = Clamp(itemIndex, 1, #menu.items)
end

-- ============================================================================
-- Input Handling
-- ============================================================================

local function GetRawKey(actionID)

    local device = TEN.Input.GetLastInputDevice()
   
   if device == TEN.Input.InputDevice.GAMEPAD then

        if actionID == ActionID.MENU_UP then
            return ActionID.GAMEPAD_DPAD_UP
        elseif actionID == ActionID.MENU_DOWN then
            return ActionID.GAMEPAD_DPAD_DOWN
        elseif actionID == ActionID.MENU_LEFT then
            return ActionID.GAMEPAD_DPAD_LEFT
        elseif actionID == ActionID.MENU_RIGHT then
            return ActionID.GAMEPAD_DPAD_RIGHT
        end
    else 
        return actionID
    end
    
end

local function HandleInput(menuName)
    local menu = Menus[menuName]
    local itemCount = #menu.items
    if itemCount == 0 then return end

    local previousItem = menu.currentItem

    -- Header navigation: STEP_LEFT / STEP_RIGHT
    if InputHelpers.GuiIsPulsed(ActionID.GAMEPAD_LEFT_SHOULDER, menu.inputTimer) or InputHelpers.GuiIsPulsed(ActionID.Q, menu.inputTimer) then
        Menu.NavigateHeader(-1)
        return
    elseif InputHelpers.GuiIsPulsed(ActionID.GAMEPAD_RIGHT_SHOULDER, menu.inputTimer) or InputHelpers.GuiIsPulsed(ActionID.E, menu.inputTimer) then
        Menu.NavigateHeader(1)
        return
    end

    -- Navigate items: MENU_UP / MENU_DOWN
    if InputHelpers.GuiIsPulsed(GetRawKey(ActionID.MENU_UP), menu.inputTimer) then
        PlaySound(menu.sounds and menu.sounds.menuSelect)
        if menu.wrapAroundItems then
            menu.currentItem = (menu.currentItem - 2) % itemCount + 1
        else
            menu.currentItem = math.max(1, menu.currentItem - 1)
        end
        if previousItem ~= menu.currentItem and menu.itemChangeFunction then
            PerformFunction(menu.itemChangeFunction)
        end

    elseif InputHelpers.GuiIsPulsed(GetRawKey(ActionID.MENU_DOWN), menu.inputTimer) then
        PlaySound(menu.sounds and menu.sounds.menuSelect)
        if menu.wrapAroundItems then
            menu.currentItem = menu.currentItem % itemCount + 1
        else
            menu.currentItem = math.min(itemCount, menu.currentItem + 1)
        end
        if previousItem ~= menu.currentItem and menu.itemChangeFunction then
            PerformFunction(menu.itemChangeFunction)
        end

    elseif (InputHelpers.GuiIsPulsed(GetRawKey(ActionID.MENU_LEFT), menu.inputTimer, true) or InputHelpers.GuiIsPulsed(ActionID.MOUSE_SCROLL_DOWN, menu.inputTimer)) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
        local currentItem = menu.items[menu.currentItem]
        if currentItem.options and #currentItem.options > 1 then
            PlaySound(menu.sounds and menu.sounds.menuSelect)
            if currentItem.wrapOptions then
                currentItem.currentOption = (currentItem.currentOption - 2) % #currentItem.options + 1
            else
                currentItem.currentOption = math.max(1, currentItem.currentOption - 1)
            end
            if currentItem.onOptionChange then
                PerformFunction(currentItem.onOptionChange)
            end
        end

    elseif (InputHelpers.GuiIsPulsed(GetRawKey(ActionID.MENU_RIGHT), menu.inputTimer, true) or InputHelpers.GuiIsPulsed(ActionID.MOUSE_SCROLL_UP, menu.inputTimer)) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
        local currentItem = menu.items[menu.currentItem]
        if currentItem.options and #currentItem.options > 1 then
            PlaySound(menu.sounds and menu.sounds.menuSelect)
            if currentItem.wrapOptions then
                currentItem.currentOption = currentItem.currentOption % #currentItem.options + 1
            else
                currentItem.currentOption = math.min(#currentItem.options, currentItem.currentOption + 1)
            end
            if currentItem.onOptionChange then
                PerformFunction(currentItem.onOptionChange)
            end
        end

    -- Accept / Exit
    elseif TEN.Input.IsKeyHit(ActionID.ACTION) or TEN.Input.IsKeyHit(ActionID.SELECT) then
        if menu.acceptFunction then
            PlaySound(menu.sounds and menu.sounds.menuChoose)
            PerformFunction(menu.acceptFunction)
        end

    elseif TEN.Input.IsKeyHit(ActionID.INVENTORY) or TEN.Input.IsKeyHit(ActionID.DESELECT) then
        if menu.exitFunction then
            PlaySound(menu.sounds and menu.sounds.menuSelect)
            PerformFunction(menu.exitFunction)
        end
    end
end

-- ============================================================================
-- Update
-- ============================================================================

function Menu.UpdateMenu(menuName)
    local menu = Menus[menuName]
    if not menu or not menu.visible then return end

    menu.inputTimer = (menu.inputTimer or 0) + 1

    -- Fade animation
    if menu.currentAlpha ~= menu.targetAlpha then
        if menu.targetAlpha > menu.currentAlpha then
            menu.currentAlpha = math.min(menu.currentAlpha + menu.fadeSpeed, menu.targetAlpha)
        else
            menu.currentAlpha = math.max(menu.currentAlpha - menu.fadeSpeed, menu.targetAlpha)
        end
        if menu.currentAlpha <= ALPHA_MIN then
            ActiveMenus[menuName] = nil
            Menu.Active = ActiveMenus
            menu.visible = false
        end
    end

    -- Input only when fully visible
    if menu.inputs and menu.currentAlpha >= ALPHA_MAX then
        HandleInput(menuName)
    end

    -- Scroll management
    menu.prevVisibleStartIndex = menu.prevVisibleStartIndex or menu.visibleStartIndex
    if menu.currentItem < menu.visibleStartIndex then
        menu.visibleStartIndex = menu.currentItem
    elseif menu.currentItem >= menu.visibleStartIndex + menu.maxVisibleItems then
        menu.visibleStartIndex = menu.currentItem - menu.maxVisibleItems + 1
    end
    if menu.visibleStartIndex ~= menu.prevVisibleStartIndex then
        menu.targetScrollY = (menu.visibleStartIndex - 1) * menu.lineSpacing
        menu.prevVisibleStartIndex = menu.visibleStartIndex
    end
    menu.scrollY = menu.scrollY + (menu.targetScrollY - menu.scrollY) * SCROLL_SPEED
end

-- ============================================================================
-- Draw
-- ============================================================================

function Menu.DrawMenu(menuName)
    local menu = Menus[menuName]
    if not menu or not menu.visible or menu.currentAlpha < 0.5 then return end

    local actualTransparency = menu.menuTransparency * menu.currentAlpha

    -- Title
    if menu.titleString and menu.titleString ~= "" then
        local pos = TEN.Util.PercentToScreen(TEN.Vec2(menu.titlePosition.x, menu.titlePosition.y))
        local titleNode = TEN.Strings.DisplayString(
            menu.titleString, pos, menu.titleTextScale,
            ColorCombine(menu.titleTextColor, actualTransparency),
            menu.titleTranslate, menu.titleTextFlags)
        TEN.Strings.ShowString(titleNode, DRAW_RATE)
    end

    local baseYItems = menu.itemsPosition.y
    local offset = menu.lineSpacing

    for i = 1, #menu.items do
        local item = menu.items[i]
        local yItems = baseYItems + (i - 1) * offset - menu.scrollY

        if i < menu.visibleStartIndex or i > menu.visibleStartIndex + menu.maxVisibleItems - 1 then
            goto continue
        end

        -- Item names
        if menu.menuType == Menu.Type.ITEMS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
            local position = TEN.Vec2(menu.itemsPosition.x, yItems)
            local itemNode = TEN.Strings.DisplayString(
                item.itemName,
                TEN.Util.PercentToScreen(position),
                menu.itemsTextScale,
                ColorCombine(menu.itemsTextColor, actualTransparency),
                menu.itemsTranslate
            )
            if menu.menuType == Menu.Type.ITEMS_ONLY and i == menu.currentItem then
                itemNode:SetFlags(menu.itemsSelectedFlags)
            else
                itemNode:SetFlags(menu.itemsTextFlags)
            end
            TEN.Strings.ShowString(itemNode, DRAW_RATE)
        end

        -- Options
        if menu.menuType == Menu.Type.OPTIONS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
            local baseYOptions = menu.optionsPosition.y
            local yOptions = baseYOptions + (i - 1) * offset - menu.scrollY
            local selectedOption = item.options and item.options[item.currentOption] or ""

            local position = TEN.Vec2(menu.optionsPosition.x, yOptions)
            local optNode = TEN.Strings.DisplayString(
                selectedOption,
                TEN.Util.PercentToScreen(position),
                menu.optionsTextScale,
                ColorCombine(menu.optionsTextColor, actualTransparency),
                menu.optionsTranslate)
            if i == menu.currentItem then
                optNode:SetFlags(menu.optionsSelectedFlags)
            else
                optNode:SetFlags(menu.optionsTextFlags)
            end
            TEN.Strings.ShowString(optNode, DRAW_RATE)
        end

        ::continue::
    end
end

-- ============================================================================
-- Batch Update / Draw for active menus
-- ============================================================================

function Menu.UpdateActiveMenus()
    for menuName in pairs(ActiveMenus) do
        Menu.UpdateMenu(menuName)
    end
end

function Menu.DrawActiveMenus()
    for menuName in pairs(ActiveMenus) do
        Menu.DrawMenu(menuName)
    end
end

-- Register as callback targets
LevelFuncs.Engine.PhotoMode.Menu.UpdateActiveMenus = Menu.UpdateActiveMenus
LevelFuncs.Engine.PhotoMode.Menu.DrawActiveMenus   = Menu.DrawActiveMenus

return Menu
