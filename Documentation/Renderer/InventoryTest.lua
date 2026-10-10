-- Renderer test scenario: opens the inventory, for the menu screenshots of the renderer parity test.

local frame = 0

LevelFuncs.OnStart = function() end
LevelFuncs.OnLoad = function() end
LevelFuncs.OnSave = function() end
LevelFuncs.OnEnd = function() end

LevelFuncs.OnLoop = function()
	frame = frame + 1
	Lara:SetHP(1000)

	if frame == 30 then
		TEN.Inventory.GiveItem(TEN.Objects.ObjID.PISTOLS_ITEM, 1)
		TEN.Inventory.SetFocusedItem(TEN.Objects.ObjID.PISTOLS_ITEM)
	end
end
