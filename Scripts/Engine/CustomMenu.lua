local Menu = {}
local debug = true
Menu.__index = Menu

LevelFuncs.Engine.Menu = {}
LevelVars.Engine.Menus = {}

Menu.Create = function (menuName, title, items, acceptFunction, exitFunction, hideItemNames)

    local self = {name = menuName}

    if debug and LevelVars.Engine.Menus[menuName] then
    print("Warning: a menu with name " .. menuName .. " already exists; overwriting it with a new one...")
    end

    -- Initialize each item's currentOption if not provided
	for _, item in ipairs(items or {}) do
		item.currentOption = item.currentOption or 1
	end

    LevelVars.Engine.Menus[menuName]			        = {}
    LevelVars.Engine.Menus[menuName].name				= menuName
    LevelVars.Engine.Menus[menuName].title				= title
    LevelVars.Engine.Menus[menuName].items  			= items or {}
    LevelVars.Engine.Menus[menuName].currentItem        = 1
    LevelVars.Engine.Menus[menuName].visible			= false
    LevelVars.Engine.Menus[menuName].hideItemNames      = hideItemNames or false
    LevelVars.Engine.Menus[menuName].exitFunction       = exitFunction 
    LevelVars.Engine.Menus[menuName].acceptFunction     = acceptFunction 
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

LevelFuncs.Engine.Menu.DrawMenu = function()


    for _, menu in pairs (LevelVars.Engine.Menus) do

       if menu.visible==true then
    
		
            if KeyIsHit(ActionID.FORWARD) then
                menu.currentItem = (menu.currentItem - 2) % #menu.items + 1 -- Move up
            elseif KeyIsHit(ActionID.BACK) then
                menu.currentItem = menu.currentItem % #menu.items + 1 -- Move down
            elseif KeyIsHit(ActionID.LEFT) then
                local currentItem = menu.items[menu.currentItem]
                if #currentItem.options > 1 then 
					currentItem.currentOption = (currentItem.currentOption - 2) % #currentItem.options + 1
				end
            elseif KeyIsHit(ActionID.RIGHT) then
                local currentItem = menu.items[menu.currentItem]
                if #currentItem.options > 1 then
					currentItem.currentOption = currentItem.currentOption % #currentItem.options + 1
				end
            elseif KeyIsHit(ActionID.ACTION) then
                menu.acceptFunction()
                return
            elseif KeyIsHit(ActionID.INVENTORY) then
                menu.exitFunction()
                return
            end

            if menu.title then
                local titleNode = LevelFuncs.Engine.Node.GenerateString(menu.title, 10, 10, 1.5, 0, 0, Color(255, 255, 255), 1)
                TEN.Strings.ShowString(titleNode, 1/30)
            end

            local optionsY = 20
            local offset = 6

            if not menu.hideItemNames then
                for i, item in ipairs(menu.items) do


                local optionsY2 = optionsY + offset * (i-1)
                -- Draw the items


                local itemsNode = LevelFuncs.Engine.Node.GenerateString(item.itemName, 10, optionsY2, 1.0, 0, 0, Color(0, 255, 255), 1)
                TEN.Strings.ShowString(itemsNode, 1/30)
                end
            end

            

            -- Get the currently selected options

            local flagsHighlight = {Strings.DisplayStringOption.BLINK, Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}
            local flagsNormal = {Strings.DisplayStringOption.SHADOW, Strings.DisplayStringOption.CENTER}

            local optionsX = menu.hideItemNames and 10 or 50
            
            for i, item in ipairs(menu.items) do
                local selectedOption = item.options[item.currentOption] -- Get the selected option for each item
                    
                
                local optionsY2 = optionsY + offset * (i-1)
                -- Draw the current option
                local optionsNode = LevelFuncs.Engine.Node.GenerateString(selectedOption, optionsX, optionsY2, 1.0, 0, 0, Color(0, 255, 255), 1)

                if i == menu.currentItem then
                    optionsNode:SetFlags(flagsHighlight)
                else
                    optionsNode:SetFlags(flagsNormal)
                end

                TEN.Strings.ShowString(optionsNode, 1/30)

            end


       end
    end
end

return Menu