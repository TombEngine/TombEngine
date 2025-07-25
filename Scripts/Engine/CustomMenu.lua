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
        title = title,
        items = items or {},
        currentItem = 1,
        visible = false,
        menuType = menuType or Menu.Type.ITEMS_AND_OPTIONS,
        exitFunction = exitFunction,
        acceptFunction = acceptFunction,
        visibleStartIndex = 1,
        maxVisibleItems = 6,
        scrollY = 0,
        targetScrollY = 0,
        wrapAround = wrapAround or false
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
            TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PREFREEZE, LevelFuncs.Engine.Menu.DrawMenu)
        elseif value == false then
            TEN.Logic.RemoveCallback(TEN.Logic.CallbackPoint.PREFREEZE, LevelFuncs.Engine.Menu.DrawMenu)
        end
    end
end

Menu.IfExists = function (menuName)
	local menu = LevelVars.Engine.Menus[menuName]
    return menu and true or false
end

function Menu:SetVisibility(visible)
    --the visible variable is a boolean
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].visible = visible == true
	end
end

function Menu:SetWrapAround(wrapAround)
    --the visible variable is a boolean
	if LevelVars.Engine.Menus[self.name] then
		LevelVars.Engine.Menus[self.name].wrapAround = wrapAround
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

LevelFuncs.Engine.Menu.DrawMenu = function()

    for _, menu in pairs(LevelVars.Engine.Menus) do
        if menu.visible then
            local itemCount = #menu.items

            if KeyIsHit(ActionID.FORWARD) then
                 if menu.wrapAround then
                    menu.currentItem = (menu.currentItem - 2) % itemCount + 1
                else
                    if menu.currentItem > 1 then
                        menu.currentItem = menu.currentItem - 1
                    end
                end
            elseif KeyIsHit(ActionID.BACK) then
                if menu.wrapAround then
                    menu.currentItem = menu.currentItem % itemCount + 1
                else
                    if menu.currentItem < itemCount then
                        menu.currentItem = menu.currentItem + 1
                    end
                end
            elseif KeyIsHit(ActionID.LEFT) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
                local currentItem = menu.items[menu.currentItem]
                if currentItem.options and #currentItem.options > 1 then
                    currentItem.currentOption = (currentItem.currentOption - 2) % #currentItem.options + 1
                end
            elseif KeyIsHit(ActionID.RIGHT) and menu.menuType ~= Menu.Type.ITEMS_ONLY then
                local currentItem = menu.items[menu.currentItem]
                if currentItem.options and #currentItem.options > 1 then
                    currentItem.currentOption = currentItem.currentOption % #currentItem.options + 1
                end
            elseif KeyIsHit(ActionID.ACTION) then
                if menu.acceptFunction then PerformFunction(menu.acceptFunction) end
            elseif KeyIsHit(ActionID.INVENTORY) then
                if menu.exitFunction then PerformFunction(menu.exitFunction) end
                return
            end

            if menu.title then
                local titleNode = LevelFuncs.Engine.Node.GenerateString(menu.title, 10, 10, 1.5, 0, 0, Color(255, 255, 255), 1)
                TEN.Strings.ShowString(titleNode, 1 / 30)
            end

            local baseY = 20
            local offset = 6
            local flagsHighlight = {Strings.DisplayStringOption.BLINK, Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
            local flagsNormal = {Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}

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
            menu.scrollY = menu.scrollY + (menu.targetScrollY - menu.scrollY) * 0.2

            for i = 1, #menu.items do
                local item = menu.items[i]
                local y = baseY + (i - 1) * offset - menu.scrollY

                -- Skip items not in visible drawing range
                if i < menu.visibleStartIndex or i > menu.visibleStartIndex + menu.maxVisibleItems - 1 then
                    goto continue
                end

                if menu.menuType == Menu.Type.ITEMS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
                    local itemNode = LevelFuncs.Engine.Node.GenerateString(item.itemName, 10, y, 1.0, 0, 0, Color(0, 255, 255), 1)
                    if menu.menuType == Menu.Type.ITEMS_ONLY and i == menu.currentItem then
                        itemNode:SetFlags(flagsHighlight)
                    else
                        itemNode:SetFlags(flagsNormal)
                    end
                    TEN.Strings.ShowString(itemNode, 1 / 30)
                end

                if menu.menuType == Menu.Type.OPTIONS_ONLY or menu.menuType == Menu.Type.ITEMS_AND_OPTIONS then
                    local selectedOption = item.options and item.options[item.currentOption] or ""
                    local optionsX = menu.menuType == Menu.Type.OPTIONS_ONLY and 10 or 50
                    local optNode = LevelFuncs.Engine.Node.GenerateString(selectedOption, optionsX, y, 1.0, 0, 0, Color(0, 255, 255), 1)
                    if i == menu.currentItem then
                        optNode:SetFlags(flagsHighlight)
                    else
                        optNode:SetFlags(flagsNormal)
                    end
                    TEN.Strings.ShowString(optNode, 1 / 30)
                end

                ::continue::
            end
        end
    end
end

return Menu
