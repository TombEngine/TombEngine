local Menu = {}
local debug = false
Menu.__index = Menu

Menu.Type = {
    ITEMS_ONLY = 1,
    ITEMS_AND_OPTIONS = 2,
    OPTIONS_ONLY = 3,
}

local SOUND_MAP =
{
    menuSelect = 109,
    menuChoose = 111,
}

LevelFuncs.Engine.Menu = {}
LevelVars.Engine.Menus = {}

local NORMAL_FONT_COLOR = Color(255,255,255,255)
local HEADER_FONT_COLOR = Color(216,117,49,255)
local HEADER_FONT_SCALE = 1.6
local NORMAL_FONT_SCALE = 1
local LINE_SPACING = 6
local TEXT_FLAGS_SELECT = {Strings.DisplayStringOption.BLINK, Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
local TEXT_FLAGS_NORMAL = {Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
local SCROLL_SPEED = 0.2

local percentPos = function(x, y)
    return TEN.Vec2(TEN.Util.PercentToScreen(x, y))
end

local colorCombine = function(color, transparency)

    return Color(color.r, color.g, color.b, transparency)

end

Menu.Create = function(menuName, title, items, acceptFunction, exitFunction, menuType)
    local self = { name = menuName }

    if debug and LevelVars.Engine.Menus[menuName] then
        print("Warning: a menu with name " .. menuName .. " already exists; overwriting it with a new one...")
    end

    if menuType ~= Menu.Type.ITEMS_ONLY then
        for _, item in ipairs(items or {}) do
            item.currentOption = item.currentOption or 1
        end
    end

    LevelVars.Engine.Menus[menuName] = {
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
        menuTransparency = 255,
        sounds = SOUND_MAP,
        inputs = true,
        visibleStartIndex = 1,
        scrollY = 0,
        targetScrollY = 0
    }

    return setmetatable(self, Menu)
end

Menu.Get = function(menuName)
    
    if LevelVars.Engine.Menus[menuName] then
        local self = {name = menuName}
        return setmetatable(self, Menu)
    end

end

Menu.Delete = function (menuName)
   
	if LevelVars.Engine.Menus[menuName] then
		LevelVars.Engine.Menus[menuName] = nil
	end

end

Menu.Status = function(value)

    if LevelVars.Engine.Menus then
        if value == true then
            TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PREFREEZE, LevelFuncs.Engine.Menu.DrawMenus)
        elseif value == false then
            TEN.Logic.RemoveCallback(TEN.Logic.CallbackPoint.PREFREEZE, LevelFuncs.Engine.Menu.DrawMenus)
        end
    end

end

Menu.IfExists = function (menuName)
	local menu = LevelVars.Engine.Menus[menuName]
    return menu and true or false
end

function Menu:Draw()
	if LevelVars.Engine.Menus[self.name] then
		LevelFuncs.Engine.Menu.DrawMenu(self.name)
	end
end

function Menu:Reset()
	local menu = LevelVars.Engine.Menus[self.name]
	if not menu then return end

	menu.currentItem = 1
	menu.visibleStartIndex = 1
	menu.scrollY = 0
	menu.targetScrollY = 0

	for _, item in ipairs(menu.items) do
		item.currentOption = 1
	end
end

function Menu:SetVisibility(visible)
    --the visible variable is a boolean
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].visible = visible == true
	end
end

function Menu:SetTransparency(transparency)
	if LevelVars.Engine.Menus[self.name] then

        transparency = math.max(0, math.min(1, transparency))

		LevelVars.Engine.Menus[self.name].menuTransparency = transparency * 255

    end
end

function Menu:SetWrapAroundItems(wrapAround)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].wrapAroundItems = wrapAround
	end
end

function Menu:SetWrapAroundOptions(wrapAround)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].wrapAroundOptions = wrapAround
	end
end

function Menu:SetAcceptFunction(functionName)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].acceptFunction = functionName
	end
end

function Menu:SetExitFunction(functionName)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].exitFunction = functionName
	end
end

function Menu:SetOnItemChangeFunction(functionName)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].itemChangeFunction = functionName
	end
end

function Menu:SetOnOptionChangeFunction(itemName, functionName)
	if LevelVars.Engine.Menus[self.name] then
        local menu = LevelVars.Engine.Menus[self.name]

        for _, item in ipairs(menu.items) do
            if item.itemName == itemName then
                item.onOptionChange = functionName
                break
            end
        end

	end
end

function Menu:SetSelectedItemFlags(flags)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].itemsSelectedFlags = flags
	end
end

function Menu:SetSelectedOptionsFlags(flags)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].optionsSelectedFlags = flags
	end
end

function Menu:SetTitle(title, fontColor, titleScale, flags, translate)
	local menu = LevelVars.Engine.Menus[self.name]
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
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].titlePosition = titlePosition
	end
end

function Menu:SetItemsFont(fontColor, fontScale, flags)
    local menu = LevelVars.Engine.Menus[self.name]
    if not menu then return end

    if fontColor ~= nil then
        menu.itemsTextColor = fontColor
    end
    if titleScale ~= nil then
        menu.itemsTextScale = fontScale
    end
    if flags ~= nil then
        menu.itemsTextFlags = flags
    end

end

function Menu:SetItemsTranslate(translate)

    if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].itemsTranslate = translate
	end

end

function Menu:SetOptionsFont(fontColor, fontScale, flags)

    local menu = LevelVars.Engine.Menus[self.name]
    if not menu then return end

    if fontColor ~= nil then
        menu.optionsTextColor = fontColor
    end
    if titleScale ~= nil then
        menu.optionsTextScale = fontScale
    end
    if flags ~= nil then
        menu.optionsTextFlags = flags
    end

end

function Menu:SetOptionsTranslate(translate)

    if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].optionsTranslate = translate
	end

end

function Menu:SetItemsPosition(position)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].itemsPosition = position
	end
end

function Menu:SetOptionsPosition(position)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].optionsPosition = position
	end
end

function Menu:SetLineSpacing(lineSpacing)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].lineSpacing = lineSpacing
	end
end

function Menu:SetVisibleItems(itemCount)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].maxVisibleItems = itemCount
	end
end

function Menu:IsVisible()
	local menu = LevelVars.Engine.Menus[self.name]
    return menu and menu.visible or false
end

function Menu:SetSoundEffects(select, choose)
    if not LevelVars.Engine.Menus[self.name] then return end

    local menu = LevelVars.Engine.Menus[self.name]
    menu.sounds = {}

    if type(select) == "number" then
        menu.sounds.menuSelect = select
    end
    if type(choose) == "number" then
        menu.sounds.menuChoose = choose
    end

end

function Menu:ClearSoundEffects()

    if not LevelVars.Engine.Menus[self.name] then return end

    local menu = LevelVars.Engine.Menus[self.name]
    menu.sounds = {}

end

function Menu:EnableInputs(inputs)

    if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].inputs = inputs
	end

end

-- Getter Methods
function Menu:getCurrentItem()
    -- Returns the currently selected item
    local menu = LevelVars.Engine.Menus[self.name]
    local item = menu.items[menu.currentItem]
    return item and item.itemName or nil
end

function Menu:getCurrentOption()
    -- Returns the currently selected option for the current item
    local menu = LevelVars.Engine.Menus[self.name]
    local item = menu.items[menu.currentItem]
    return (item and item.options and item.options[item.currentOption]) or nil
end

function Menu:getOptionForItem(itemIndex)
    -- Returns the currently selected option for a specific item by index
    local menu = LevelVars.Engine.Menus[self.name]
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
function Menu:getCurrentItemIndex()
    local menu = LevelVars.Engine.Menus[self.name]
    return menu.currentItem
end

-- Returns the index of the currently selected option for the current item
function Menu:getCurrentOptionIndex()
    local menu = LevelVars.Engine.Menus[self.name]
    local item = menu.items[menu.currentItem]
    return item.currentOption or 1
end

function Menu:getOptionIndexForItem(itemIndex)
    local menu = LevelVars.Engine.Menus[self.name]
    if debug and (not menu.items or not menu.items[itemIndex]) then
        error("Invalid item index: " .. tostring(itemIndex))
    end
    local item = menu.items[itemIndex]
    if debug and not item.currentOption then
        error("currentOption is not defined for item index: " .. tostring(itemIndex))
    end
    return item.currentOption

end

function Menu:setOptionIndexForItem(itemIndex, optionIndex)
    local menu = LevelVars.Engine.Menus[self.name]
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

    optionIndex = math.max(1, math.min(optionIndex, maxOptions))
    item.currentOption = optionIndex

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
    local menu = LevelVars.Engine.Menus[menuName]
    if menu and menu.sounds and type(soundIndex) == "number" then
        TEN.Sound.PlaySound(soundIndex)
    end
end

local Input = function(menuName)

    local menu = LevelVars.Engine.Menus[menuName]

    local itemCount = #menu.items

    local previousItem = menu.currentItem

    if itemCount == 0 then return end

    if KeyIsHit(ActionID.FORWARD) then
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
        
    elseif KeyIsHit(ActionID.BACK) then
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
    elseif KeyIsHit(ActionID.LEFT) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
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
    elseif KeyIsHit(ActionID.RIGHT) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
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
    elseif KeyIsHit(ActionID.ACTION) or KeyIsHit(ActionID.SELECT) then
        if menu.acceptFunction then 
            PlaySoundEffect(menu.name, menu.sounds.menuChoose)
            PerformFunction(menu.acceptFunction)
        end
    elseif KeyIsHit(ActionID.INVENTORY) or KeyIsHit(ActionID.DESELECT) then
        if menu.exitFunction then 
            PlaySoundEffect(menu.name, menu.sounds.menuSelect)
            PerformFunction(menu.exitFunction) end
        return
    end

end

LevelFuncs.Engine.Menu.DrawMenu = function(menuName)

    local menu = LevelVars.Engine.Menus[menuName]

    if menu.visible then
        
        if menu.inputs then Input(menuName) end

        if menu.titleString then

            --Hack to hardcode the translate to false for strings which are blank
                local translate = menu.titleTranslate

                if menu.titleString == "" then translate = false end

            local titleNode = DisplayString(menu.titleString, percentPos(menu.titlePosition.x, menu.titlePosition.y), menu.titleTextScale, colorCombine(menu.titleTextColor, menu.menuTransparency) , translate, menu.titleTextFlags)
            TEN.Strings.ShowString(titleNode, 1 / 30)
        end

        local baseYItems = menu.itemsPosition.y
        local offset = menu.lineSpacing

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
            menu.targetScrollY = (menu.visibleStartIndex - 1) * offset
            menu.prevVisibleStartIndex = menu.visibleStartIndex
        end

        -- Smooth scroll animation
        menu.scrollY = menu.scrollY + (menu.targetScrollY - menu.scrollY) * SCROLL_SPEED

        for i = 1, #menu.items do
            local item = menu.items[i]
            local yItems = baseYItems + (i - 1) * offset - menu.scrollY

            -- Skip items not in visible drawing range
            if i < menu.visibleStartIndex or i > menu.visibleStartIndex + menu.maxVisibleItems - 1 then
                goto continue
            end

            if menu.menuType == Menu.Type.ITEMS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
                
                --Hack to hardcode the translate to false for strings which are blank
                local translate = menu.itemsTranslate

                if item.itemName == "" then translate = false end

                local itemNode = DisplayString(item.itemName, percentPos(menu.itemsPosition.x, yItems), menu.itemsTextScale, colorCombine(menu.itemsTextColor, menu.menuTransparency), translate)
                if menu.menuType == Menu.Type.ITEMS_ONLY and i == menu.currentItem then
                    itemNode:SetFlags(menu.itemsSelectedFlags)
                else
                    itemNode:SetFlags(menu.itemsTextFlags)
                end
                TEN.Strings.ShowString(itemNode, 1 / 30)
            end

            if menu.menuType == Menu.Type.OPTIONS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
                local baseYOptions = menu.optionsPosition.y
                local yOptions = baseYOptions + (i - 1) * offset - menu.scrollY
                local selectedOption = item.options and item.options[item.currentOption] or ""
                local optNode = DisplayString(selectedOption, percentPos(menu.optionsPosition.x, yOptions), menu.optionsTextScale, colorCombine(menu.optionsTextColor, menu.menuTransparency), menu.optionsTranslate)
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
end


LevelFuncs.Engine.Menu.DrawMenus = function()

    for menuName in pairs(LevelVars.Engine.Menus) do
        LevelFuncs.Engine.Menu.DrawMenu(menuName)
    end
end

return Menu
