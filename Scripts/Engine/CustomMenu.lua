local Menu = {}
local debug = true
Menu.__index = Menu

Menu.Type = {
    ITEMS_ONLY = 1,
    ITEMS_AND_OPTIONS = 2,
    OPTIONS_ONLY = 3,
}

LevelFuncs.Engine.Menu = {}
LevelVars.Engine.Menus = {}

local NORMAL_FONT_COLOR = Color(255,255,255,255)
local HEADER_FONT_COLOR = Color(216,117,49,255)
local TEXT_FLAGS_SELECT = {Strings.DisplayStringOption.BLINK, Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
local TEXT_FLAGS_NORMAL = {Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
local SCROLL_SPEED = 0.2

local percentPos = function(x, y)
    return TEN.Vec2(TEN.Util.PercentToScreen(x, y))
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

    local lineSpacing  = percentPos(4, 0).x

    LevelVars.Engine.Menus[menuName] = {
        name = menuName,
        titleString = title,
        items = items or {},
        currentItem = 1,
        visible = false,
        menuType = menuType or Menu.Type.ITEMS_AND_OPTIONS,
        exitFunction = exitFunction,
        acceptFunction = acceptFunction,
        wrapAroundItems = false,
        wrapAroundOptions = false,
        lineSpacing = lineSpacing,
        selectedFlags = TEXT_FLAGS_SELECT,
        unselectedFlags = TEXT_FLAGS_NORMAL,
        fontColor = NORMAL_FONT_COLOR,
        fontScale = 1,
        itemsPosition = percentPos(10, 20),
        optionsPosition = percentPos(50, 20),
        titleColor = HEADER_FONT_COLOR,
        titlePosition = percentPos(10, 10),
        titleScale = 1.5,
        titleFlags = {Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER},
        menuTransparency = 255,
        visibleStartIndex = 1,
        maxVisibleItems = 6,
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
		LevelFuns.Engine.Menus.DrawMenu(self.name)
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

		LevelVars.Engine.Menus[self.name].transparency = transparency * 255
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

function Menu:SetSelectedFlags(flags)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].selectedFlags = flags
	end
end

function Menu:SetUnselectedFlags(flags)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].unselectedFlags = flags
	end
end

function Menu:SetTitle(title, fontColor, titleScale, flags)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].titleString = title
        LevelVars.Engine.Menus[self.name].titleColor = fontColor
        LevelVars.Engine.Menus[self.name].titleScale = titleScale
        LevelVars.Engine.Menus[self.name].titleFlags = flags
	end
end

function Menu:SetTitlePosition(titlePosition)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].titlePosition = titlePosition
	end
end

function Menu:SetItemsFont(fontColor, fontScale)
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].fontColor = fontColor
        LevelVars.Engine.Menus[self.name].fontScale = fontScale
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

        local line  = percentPos(lineSpacing, 0).x
		LevelVars.Engine.Menus[self.name].lineSpacing = line
	end
end

function Menu:IsVisible()
	local menu = LevelVars.Engine.Menus[self.name]
    return menu and menu.visible or false
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
   if debug and not menu.items or not menu.items[itemIndex] then
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
    if debug and not menu.items or not menu.items[itemIndex] then
        error("Invalid item index: " .. tostring(itemIndex))
    end
    local item = menu.items[itemIndex]
    if debug and not item.currentOption then
        error("currentOption is not defined for item index: " .. tostring(itemIndex))
    end
    return item.currentOption

end

local PerformFunction = function(functionString)
    local func = LevelFuncs[functionString]
    if func and (type(func) == "function" or type(func) == "userdata") then
        return func()
    end
end

local Input = function(menuName)

    local menu = LevelVars.Engine.Menus[menuName]

    local itemCount = #menu.items

    if KeyIsHit(ActionID.FORWARD) then
            if menu.wrapAroundItems then
            menu.currentItem = (menu.currentItem - 2) % itemCount + 1
        else
            if menu.currentItem > 1 then
                menu.currentItem = menu.currentItem - 1
            end
        end
    elseif KeyIsHit(ActionID.BACK) then
        if menu.wrapAroundItems then
            menu.currentItem = menu.currentItem % itemCount + 1
        else
            if menu.currentItem < itemCount then
                menu.currentItem = menu.currentItem + 1
            end
        end
    elseif KeyIsHit(ActionID.LEFT) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
        local currentItem = menu.items[menu.currentItem]
        if currentItem.options and #currentItem.options > 1 then
            if menu.wrapAroundOptions then
                currentItem.currentOption = (currentItem.currentOption - 2) % #currentItem.options + 1
            else
                currentItem.currentOption = math.max(1, currentItem.currentOption - 1)
            end
        end
    elseif KeyIsHit(ActionID.RIGHT) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
        local currentItem = menu.items[menu.currentItem]
        if currentItem.options and #currentItem.options > 1 then
            if menu.wrapAroundOptions then
                currentItem.currentOption = currentItem.currentOption % #currentItem.options + 1
            else
                currentItem.currentOption = math.min(#currentItem.options, currentItem.currentOption + 1)
            end
        end
    elseif KeyIsHit(ActionID.ACTION) then
        if menu.acceptFunction then PerformFunction(menu.acceptFunction) end
    elseif KeyIsHit(ActionID.INVENTORY) then
        if menu.exitFunction then PerformFunction(menu.exitFunction) end
        return
    end

end

LevelFuncs.Engine.Menu.DrawMenu = function(menuName)

    local menu = LevelVars.Engine.Menus[menuName]

    if menu.visible then
        
        Input(menuName)

        if menu.title then
            local titleNode = DisplayString(menu.title, menu.titlePosition, menu.titleScale, Color(menu.titleColor.r, menu.titleColor.g, menu.titleColor.b, menu.menuTransparency) , false, menu.titleFlags)
            TEN.Strings.ShowString(titleNode, 1 / 30)
        end

        local baseYItems = menu.itemsPosition.y
        local offset = menu.lineSpacing
        -- local minY = baseY
        -- local maxY = baseY + offset * (menu.maxVisibleItems - 1)

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

            -- if y < minY or y > maxY then goto continue end

            -- Skip items not in visible drawing range
            if i < menu.visibleStartIndex or i > menu.visibleStartIndex + menu.maxVisibleItems - 1 then
                goto continue
            end

            if menu.menuType == Menu.Type.ITEMS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
                local itemNode = DisplayString(item.itemName, Vec2(menu.itemsPosition.x, yItems), menu.fontScale, Color(menu.fontColor.r, menu.fontColor.g, menu.fontColor.b, menu.menuTransparency), false)
                if menu.menuType == Menu.Type.ITEMS_ONLY and i == menu.currentItem then
                    itemNode:SetFlags(menu.selectedFlags)
                else
                    itemNode:SetFlags(menu.unselectedFlags)
                end
                TEN.Strings.ShowString(itemNode, 1 / 30)
            end

            if menu.menuType == Menu.Type.OPTIONS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
                local baseYOptions = menu.optionsPosition.y
                local yOptions = baseYOptions + (i - 1) * offset - menu.scrollY
                local selectedOption = item.options and item.options[item.currentOption] or ""
                local optNode = DisplayString(selectedOption, Vec2(menu.optionsPosition.x, yOptions), menu.fontScale, Color(menu.fontColor.r, menu.fontColor.g, menu.fontColor.b, menu.menuTransparency), false)
                if i == menu.currentItem then
                    optNode:SetFlags(menu.selectedFlags)
                else
                    optNode:SetFlags(menu.unselectedFlags)
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
