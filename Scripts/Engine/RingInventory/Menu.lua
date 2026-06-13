--- Internal file used by the RingInventory module. Creates custom menus for ring inventory.
-- @module RingInventory.Menu
-- @local

--External Modules
local Constants = require("Engine.RingInventory.Constants")
local InputHelpers = require("Engine.RingInventory.InputHelpers")
local Settings = require("Engine.RingInventory.Settings")
local Utilities = require("Engine.RingInventory.Utilities")

--Pointer to tables
local COLOR_MAP = Settings.ColorMap

local Menu = {}
local debug = false
Menu.__index = Menu

Menu.Type =
{
    ITEMS_ONLY = 1,
    ITEMS_AND_OPTIONS = 2,
    OPTIONS_ONLY = 3,
}

local SOUND_MAP =
{
    menuSelect = 109,
    menuChoose = 111,
}

Menu.Active = {} --table to store active menus
local Menus = {} --table to store all menus
LevelFuncs.Engine.Menu = LevelFuncs.Engine.Menu or {}

local NORMAL_FONT_COLOR = COLOR_MAP.plainText
local HEADER_FONT_COLOR = COLOR_MAP.headerText
local HEADER_FONT_SCALE = 1.6
local NORMAL_FONT_SCALE = 1
local LINE_SPACING = 6
local TEXT_FLAGS_SELECT = {Strings.DisplayStringOption.BLINK, Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
local TEXT_FLAGS_NORMAL = {Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
local SCROLL_SPEED = 0.2
local FADE_SPEED = Settings.Animation.transitionSpeed -- Speed of fade animation (higher = faster)

Menu.Create = function(menuName, title, items, acceptFunction, exitFunction, menuType)
    local self = { name = menuName }

    if debug and Menus[menuName] then
        print("Warning: a menu with name " .. menuName .. " already exists; overwriting it with a new one...")
    end

    if menuType ~= Menu.Type.ITEMS_ONLY then
        for _, item in ipairs(items or {}) do
            item.currentOption = item.currentOption or 1
        end
    end

    Menus[menuName] =
    {
        name = menuName,
        titleString = title,
        items = items or {},
        currentItem = 1,
        visible = false,
        menuType = menuType or Menu.Type.ITEMS_AND_OPTIONS,
        exitFunction = exitFunction,
        acceptFunction = acceptFunction,
        itemChangeFunction = nil,
        wrapAroundItems = false,
        wrapAroundOptions = false,
        maxVisibleItems = 16,
        lineSpacing = LINE_SPACING,
        itemsPosition = Vec2(10, 20),
        itemsTextFlags = TEXT_FLAGS_NORMAL,
        itemsSelectedFlags = TEXT_FLAGS_SELECT,
        itemsTextColor = NORMAL_FONT_COLOR,
        itemsTextScale = NORMAL_FONT_SCALE,
        itemsTranslate = false,
        optionsPosition = Vec2(50, 20),
        optionsTextFlags = TEXT_FLAGS_NORMAL,
        optionsSelectedFlags = TEXT_FLAGS_SELECT,
        optionsTextColor = NORMAL_FONT_COLOR,
        optionsTextScale = NORMAL_FONT_SCALE,
        optionsTranslate = false,
        titlePosition = Vec2(50, 10),
        titleTextFlags = TEXT_FLAGS_NORMAL,
        titleTextColor = HEADER_FONT_COLOR,
        titleTextScale = HEADER_FONT_SCALE,
        titleTranslate = false,
        menuTransparency = 1,
        sounds = SOUND_MAP,
        inputs = true,
        visibleStartIndex = 1,
        scrollY = 0,
        targetScrollY = 0,
        currentAlpha = 0,
        targetAlpha = 0,
        fadeSpeed = FADE_SPEED,
        inputTimer = 0
    }

    return setmetatable(self, Menu)
end

Menu.Get = function(menuName)
    if Menus[menuName] then
        local self = {name = menuName}
        return setmetatable(self, Menu)
    end
end

Menu.Delete = function (menuName)
	if Menus[menuName] then
		Menus[menuName] = nil
	end
end

Menu.DeleteAll = function()
    Menu.Active = {}

    if Menus then
        for name, _ in pairs(Menus) do
            Menus[name] = nil
        end
    end
end

Menu.AddActive = function(menuName, instant)
    if not menuName then
        return
    end

    Menu.Active[menuName] = true
    
    -- Set visible and trigger fade in
    local menu = Menus[menuName]
    if menu then
        menu.visible = true
        menu.currentAlpha = instant and Constants.ALPHA_MAX or Constants.ALPHA_MIN
        menu.targetAlpha = Constants.ALPHA_MAX
        menu.inputTimer = 0
    end
end

Menu.RemoveActive = function(menuName)
    if not menuName then
        return
    end

    -- Trigger fade out (will be removed from Active when fade completes)
    local menu = Menus[menuName]
    if menu then
        menu.currentAlpha = Constants.ALPHA_MAX
        menu.targetAlpha = Constants.ALPHA_MIN
    end
end

Menu.Status = function(value)
    if Menus then
        if value == true then
            TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_FREEZE, LevelFuncs.Engine.Menu.DrawAllMenus)
            TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_FREEZE, LevelFuncs.Engine.Menu.UpdateAllMenus)
        elseif value == false then
            TEN.Logic.RemoveCallback(TEN.Logic.CallbackPoint.PRE_FREEZE, LevelFuncs.Engine.Menu.DrawAllMenus)
            TEN.Logic.RemoveCallback(TEN.Logic.CallbackPoint.PRE_FREEZE, LevelFuncs.Engine.Menu.UpdateAllMenus)
        end
    end
end

Menu.IfExists = function (menuName)
	local menu = Menus[menuName]
    return menu and true or false
end

function Menu:Draw()
	if Menus[self.name] then
		Menu.DrawMenu(self.name)
	end
end

function Menu:Update()
	if Menus[self.name] then
		Menu.UpdateMenu(self.name)
	end
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

function Menu:SetVisibility(visible)
    -- The visible variable is a boolean
	if Menus[self.name] then
		local menu = Menus[self.name]
		
		if visible then
			menu.visible = true  -- Set visible immediately for fade in
			menu.targetAlpha = Constants.ALPHA_MAX
			menu.inputTimer = 0
		else
			menu.targetAlpha = Constants.ALPHA_MIN  -- Trigger fade out
			-- Visible will be set to false when fade completes in UpdateMenu
		end
	end
end

function Menu:SetTransparency(transparency)
	if Menus[self.name] then
		transparency = Utilities.Clamp(transparency, 0, 1)
		Menus[self.name].menuTransparency = transparency
    end
end

function Menu:SetFadeSpeed(speed)
    if Menus[self.name] then
        Menus[self.name].fadeSpeed = Utilities.Clamp(speed, Constants.ALPHA_MIN, Constants.ALPHA_MAX)
    end
end

function Menu:SetWrapAroundItems(wrapAround)
	if Menus[self.name] then
		Menus[self.name].wrapAroundItems = wrapAround
	end
end

function Menu:SetWrapAroundOptions(wrapAround)
	if Menus[self.name] then
		Menus[self.name].wrapAroundOptions = wrapAround
	end
end

function Menu:SetAcceptFunction(functionName)
	if Menus[self.name] then
		Menus[self.name].acceptFunction = functionName
	end
end

function Menu:SetExitFunction(functionName)
	if Menus[self.name] then
		Menus[self.name].exitFunction = functionName
	end
end

function Menu:SetOnItemChangeFunction(functionName)
	if Menus[self.name] then
		Menus[self.name].itemChangeFunction = functionName
	end
end

function Menu:SetOnOptionChangeFunction(itemName, functionName)
	if Menus[self.name] then
        local menu = Menus[self.name]

        for _, item in ipairs(menu.items) do
            if item.itemName == itemName then
                item.onOptionChange = functionName
                break
            end
        end

	end
end

function Menu:SetSelectedItemFlags(flags)
	if Menus[self.name] then
		Menus[self.name].itemsSelectedFlags = flags
	end
end

function Menu:SetSelectedOptionsFlags(flags)
	if Menus[self.name] then
		Menus[self.name].optionsSelectedFlags = flags
	end
end

function Menu:SetTitle(title, fontColor, titleScale, flags, translate)
	local menu = Menus[self.name]
    if not menu then return end
    if title ~= nil then
        menu.titleString = title
    end
    if fontColor ~= nil then
        menu.titleTextColor = fontColor
    end
    if titleScale ~= nil then
        menu.titleTextScale = titleScale
    end
    if flags ~= nil then
        menu.titleTextFlags = flags
    end
    if translate ~= nil then
        menu.titleTranslate = translate
    end
end

function Menu:SetTitlePosition(titlePosition)
	if Menus[self.name] then
		Menus[self.name].titlePosition = titlePosition
	end
end

function Menu:SetItemsFont(fontColor, fontScale, flags)
    local menu = Menus[self.name]
    if not menu then return end

    if fontColor ~= nil then
        menu.itemsTextColor = fontColor
    end
    if fontScale ~= nil then
        menu.itemsTextScale = fontScale
    end
    if flags ~= nil then
        menu.itemsTextFlags = flags
    end

end

function Menu:SetItemsTranslate(translate)
    if Menus[self.name] then
		Menus[self.name].itemsTranslate = translate
	end
end

function Menu:SetOptionsFont(fontColor, fontScale, flags)
    local menu = Menus[self.name]
    if not menu then return end

    if fontColor ~= nil then
        menu.optionsTextColor = fontColor
    end
    if fontScale ~= nil then
        menu.optionsTextScale = fontScale
    end
    if flags ~= nil then
        menu.optionsTextFlags = flags
    end
end

function Menu:SetOptionsTranslate(translate)
    if Menus[self.name] then
		Menus[self.name].optionsTranslate = translate
	end
end

function Menu:SetItemsPosition(position)
	if Menus[self.name] then
		Menus[self.name].itemsPosition = position
	end
end

function Menu:SetOptionsPosition(position)
	if Menus[self.name] then
		Menus[self.name].optionsPosition = position
	end
end

function Menu:SetLineSpacing(lineSpacing)
	if Menus[self.name] then
		Menus[self.name].lineSpacing = lineSpacing
	end
end

function Menu:SetVisibleItems(itemCount)
	if Menus[self.name] then
		Menus[self.name].maxVisibleItems = itemCount
	end
end

function Menu:IsVisible()
	local menu = Menus[self.name]
    return menu and menu.visible or false
end

function Menu:SetSoundEffects(select, choose)
    if not Menus[self.name] then return end

    local menu = Menus[self.name]
    menu.sounds = {}

    if type(select) == "number" then
        menu.sounds.menuSelect = select
    end
    if type(choose) == "number" then
        menu.sounds.menuChoose = choose
    end
end

function Menu:ClearSoundEffects()
    if not Menus[self.name] then return end

    local menu = Menus[self.name]
    menu.sounds = {}
end

function Menu:EnableInputs(inputs)
    if Menus[self.name] then
		Menus[self.name].inputs = inputs
	end
end

-- Getter Methods
function Menu:GetCurrentItem()
    -- Returns the currently selected item
    local menu = Menus[self.name]
    local item = menu.items[menu.currentItem]
    return item and item or nil
end

-- Returns the currently selected item
function Menu:GetCurrentItemName()
    local menu = Menus[self.name]
    local item = menu.items[menu.currentItem]
    return item and item.itemName or nil
end

-- Returns the currently selected option for the current item
function Menu:GetCurrentOption()
    
    local menu = Menus[self.name]
    local item = menu.items[menu.currentItem]
    return (item and item.options and item.options[item.currentOption]) or nil
end

-- Returns the currently selected option for a specific item by index
function Menu:GetOptionForItem(itemIndex)
    local menu = Menus[self.name]
   if debug and (not menu.items or not menu.items[itemIndex]) then
        error("Invalid item index: " .. tostring(itemIndex))
    end
    local item = menu.items[itemIndex]
    if debug and not item.options or not item.currentOption then
        error("Options or currentOption is not defined for item index: " .. tostring(itemIndex))
    end
    return item.options[item.currentOption]
end

-- Returns the index of the currently selected item
function Menu:GetCurrentItemIndex()
    local menu = Menus[self.name]
    return menu.currentItem
end

-- Returns the index of the currently selected option for the current item
function Menu:GetCurrentOptionIndex()
    local menu = Menus[self.name]
    local item = menu.items[menu.currentItem]
    return item.currentOption or 1
end

function Menu:GetOptionIndexForItem(itemIndex)
    local menu = Menus[self.name]
    if debug and (not menu.items or not menu.items[itemIndex]) then
        error("Invalid item index: " .. tostring(itemIndex))
    end
    local item = menu.items[itemIndex]
    if debug and not item.currentOption then
        error("currentOption is not defined for item index: " .. tostring(itemIndex))
    end
    return item.currentOption
end

function Menu:SetOptionIndexForItem(itemIndex, optionIndex)
    local menu = Menus[self.name]
    if debug and (not menu.items or not menu.items[itemIndex]) then
        error("Invalid item index: " .. tostring(itemIndex))
    end
    local item = menu.items[itemIndex]
    if debug and not item.currentOption then
        error("currentOption is not defined for item index: " .. tostring(itemIndex))
    end
    
    local maxOptions = item.options and #item.options or 1
    if maxOptions < 1 then
        error("Item at index " .. tostring(itemIndex) .. " has no options.")
    end

    optionIndex = Utilities.Clamp(optionIndex, 1, maxOptions)
    item.currentOption = optionIndex
end

function Menu:SetCurrentItem(itemIndex)
    local menu = Menus[self.name]

    if debug and not menu.items then
        error("Menu '" .. tostring(self.name) .. "' has no items table.")
    end

    local itemCount = #menu.items
    if itemCount < 1 then
        error("Menu '" .. tostring(self.name) .. "' contains no items.")
    end

    -- Clamp to valid range
    itemIndex = Utilities.Clamp(itemIndex, 1, itemCount)

    -- Set
    menu.currentItem = itemIndex
end

local PerformFunction = function(functionString)
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

local PlaySoundEffect = function(menuName, soundIndex)
    local menu = Menus[menuName]
    if menu and menu.sounds and type(soundIndex) == "number" then
        TEN.Sound.PlaySound(soundIndex)
    end
end

local HandleInput = function(menuName)
    local menu = Menus[menuName]
    local itemCount = #menu.items
    local previousItem = menu.currentItem

    if itemCount == 0 then return end

    if InputHelpers.GuiIsPulsed(ActionID.FORWARD, menu.inputTimer) then
        if menu.sounds then PlaySoundEffect(menu.name, menu.sounds.menuSelect) end
        if menu.wrapAroundItems then
            menu.currentItem = (menu.currentItem - 2) % itemCount + 1
        else
            if menu.currentItem > 1 then
                menu.currentItem = menu.currentItem - 1
            end
        end

        if previousItem ~= menu.currentItem and menu.itemChangeFunction then
            PerformFunction(menu.itemChangeFunction)
        end
        
    elseif InputHelpers.GuiIsPulsed(ActionID.BACK, menu.inputTimer) then
        PlaySoundEffect(menu.name, menu.sounds.menuSelect)
        if menu.wrapAroundItems then
            menu.currentItem = menu.currentItem % itemCount + 1
        else
            if menu.currentItem < itemCount then
                menu.currentItem = menu.currentItem + 1
            end
        end

        if previousItem ~= menu.currentItem and menu.itemChangeFunction then
            PerformFunction(menu.itemChangeFunction)
        end
    elseif InputHelpers.GuiIsPulsed(ActionID.LEFT, menu.inputTimer) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
        PlaySoundEffect(menu.name, menu.sounds.menuSelect)
        local currentItem = menu.items[menu.currentItem]
        if currentItem.options and #currentItem.options > 1 then
            if menu.wrapAroundOptions then
                currentItem.currentOption = (currentItem.currentOption - 2) % #currentItem.options + 1
            else
                currentItem.currentOption = math.max(1, currentItem.currentOption - 1)
            end

            if currentItem.onOptionChange then
                PerformFunction(currentItem.onOptionChange)
		    end
        end
    elseif InputHelpers.GuiIsPulsed(ActionID.RIGHT, menu.inputTimer) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
        PlaySoundEffect(menu.name, menu.sounds.menuSelect)
        local currentItem = menu.items[menu.currentItem]
        if currentItem.options and #currentItem.options > 1 then
            if menu.wrapAroundOptions then
                currentItem.currentOption = currentItem.currentOption % #currentItem.options + 1
            else
                currentItem.currentOption = math.min(#currentItem.options, currentItem.currentOption + 1)
            end

            if currentItem.onOptionChange then
                PerformFunction(currentItem.onOptionChange)
		    end
        end
    elseif TEN.Input.IsKeyHit(ActionID.ACTION) or TEN.Input.IsKeyHit(ActionID.SELECT) then
        if menu.acceptFunction then 
            PlaySoundEffect(menu.name, menu.sounds.menuChoose)
            PerformFunction(menu.acceptFunction)
        end
    elseif TEN.Input.IsKeyHit(ActionID.INVENTORY) or TEN.Input.IsKeyHit(ActionID.DESELECT) then
        if menu.exitFunction then 
            PlaySoundEffect(menu.name, menu.sounds.menuSelect)
            PerformFunction(menu.exitFunction) end
        return
    end

end

-- Update function - handles logic, input, and animations
function Menu.UpdateMenu(menuName)
    local menu = Menus[menuName]

    if not menu.visible then
        return
    end

    menu.inputTimer = (menu.inputTimer or 0) + 1
    
    -- Update fade animation
    if menu.currentAlpha ~= menu.targetAlpha then

        if menu.targetAlpha > menu.currentAlpha then
            -- Fading in
            menu.currentAlpha = math.min(menu.currentAlpha + menu.fadeSpeed, menu.targetAlpha)
        else
            -- Fading out
            menu.currentAlpha = math.max(menu.currentAlpha - menu.fadeSpeed, menu.targetAlpha)
        end
            
        -- If fade out completed, remove from active and set invisible
        if menu.currentAlpha <= Constants.ALPHA_MIN then
            Menu.Active[menuName] = nil
            menu.visible = false
        end
    end
    
    -- Handle input only when fully faded in
    if menu.inputs and menu.currentAlpha >= Constants.ALPHA_MAX then
        HandleInput(menuName)
    end
    
    -- Store previous visibleStartIndex to detect change
    menu.prevVisibleStartIndex = menu.prevVisibleStartIndex or menu.visibleStartIndex

    -- Adjust visibleStartIndex based on current selection
    if menu.currentItem < menu.visibleStartIndex then
        menu.visibleStartIndex = menu.currentItem
    elseif menu.currentItem >= menu.visibleStartIndex + menu.maxVisibleItems then
        menu.visibleStartIndex = menu.currentItem - menu.maxVisibleItems + 1
    end

    -- If visibleStartIndex changed, update scroll target
    if menu.visibleStartIndex ~= menu.prevVisibleStartIndex then
        menu.targetScrollY = (menu.visibleStartIndex - 1) * menu.lineSpacing
        menu.prevVisibleStartIndex = menu.visibleStartIndex
    end

    -- Smooth scroll animation
    menu.scrollY = menu.scrollY + (menu.targetScrollY - menu.scrollY) * SCROLL_SPEED
end

-- Draw function - handles rendering only
function Menu.DrawMenu(menuName)
    local menu = Menus[menuName]

    if not menu.visible then
        return
    end
    
    -- Skip rendering if fully transparent
    if menu.currentAlpha < 0.5 then
        return
    end
    
    -- Calculate actual transparency based on fade
    local actualTransparency = menu.menuTransparency * menu.currentAlpha

    -- Draw title
    if menu.titleString then
        local translate = menu.titleTranslate
        if menu.titleString == "" then translate = false end

        local position = TEN.Vec2(menu.titlePosition.x, menu.titlePosition.y)

        local titleNode = DisplayString(menu.titleString, TEN.Util.PercentToScreen(position), menu.titleTextScale, Utilities.ColorCombine(menu.titleTextColor, actualTransparency) , translate, menu.titleTextFlags)
        TEN.Strings.ShowString(titleNode, 1 / 30)
    end

    local baseYItems = menu.itemsPosition.y
    local offset = menu.lineSpacing

    -- Draw menu items
    for i = 1, #menu.items do
        local item = menu.items[i]
        local yItems = baseYItems + (i - 1) * offset - menu.scrollY

        -- Skip items not in visible drawing range
        if i < menu.visibleStartIndex or i > menu.visibleStartIndex + menu.maxVisibleItems - 1 then
            goto continue
        end

        -- Draw item names
        if menu.menuType == Menu.Type.ITEMS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
            local translate = menu.itemsTranslate
            if item.itemName == "" then translate = false end

            local position = TEN.Vec2(menu.itemsPosition.x, yItems)

            local itemNode = DisplayString(item.itemName, TEN.Util.PercentToScreen(position), menu.itemsTextScale, Utilities.ColorCombine(menu.itemsTextColor, actualTransparency), translate)
            if menu.menuType == Menu.Type.ITEMS_ONLY and i == menu.currentItem then
                itemNode:SetFlags(menu.itemsSelectedFlags)
            else
                itemNode:SetFlags(menu.itemsTextFlags)
            end
            TEN.Strings.ShowString(itemNode, 1 / 30)
        end

        -- Draw options
        if menu.menuType == Menu.Type.OPTIONS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
            local baseYOptions = menu.optionsPosition.y
            local yOptions = baseYOptions + (i - 1) * offset - menu.scrollY
            local selectedOption = item.options and item.options[item.currentOption] or ""

            local position = TEN.Vec2(menu.optionsPosition.x, yOptions)

            local optNode = DisplayString(selectedOption, TEN.Util.PercentToScreen(position), menu.optionsTextScale, Utilities.ColorCombine(menu.optionsTextColor, actualTransparency), menu.optionsTranslate)
            if i == menu.currentItem then
                optNode:SetFlags(menu.optionsSelectedFlags)
            else
                optNode:SetFlags(menu.optionsTextFlags)
            end
            TEN.Strings.ShowString(optNode, 1 / 30)
        end

        ::continue::
    end
end


LevelFuncs.Engine.Menu.UpdateAllMenus = function()
    for menuName in pairs(Menus) do
        Menu.UpdateMenu(menuName)
    end
end

LevelFuncs.Engine.Menu.DrawAllMenus = function()
    for menuName in pairs(Menus) do
        Menu.DrawMenu(menuName)
    end
end

Menu.UpdateAllMenus = LevelFuncs.Engine.Menu.UpdateAllMenus
Menu.DrawAllMenus = LevelFuncs.Engine.Menu.DrawAllMenus

function Menu.UpdateActiveMenus()
    for menuName in pairs(Menu.Active) do
        Menu.UpdateMenu(menuName)
    end
end

function Menu.DrawActiveMenus()
    for menuName in pairs(Menu.Active) do
        Menu.DrawMenu(menuName)
    end
end

return Menu