-- Renderer sweep scenario: moves the player to every moveable of the level and turns her a full circle there, so the
-- chase camera looks at the level from many places. Each stop is logged, to match slow frames with places.

local FRAMES_PER_STOP = 45

local frame = 0
local stops = nil

local function CollectStops()
	local result = {}
	for _, id in pairs(TEN.Objects.ObjID) do
		local ok, moveables = pcall(TEN.Objects.GetMoveablesBySlot, id)
		if ok and moveables ~= nil then
			for _, moveable in ipairs(moveables) do
				if moveable:GetName() ~= Lara:GetName() then
					table.insert(result, { Name = moveable:GetName(), Position = moveable:GetPosition() })
				end
			end
		end
	end

	table.sort(result, function(a, b) return a.Name < b.Name end)
	return result
end

LevelFuncs.OnStart = function() end
LevelFuncs.OnLoad = function() end
LevelFuncs.OnSave = function() end
LevelFuncs.OnEnd = function() end

LevelFuncs.OnLoop = function()
	if stops == nil then
		stops = CollectStops()
		TEN.Util.PrintLog("Sweep: " .. #stops .. " stops", TEN.Util.LogLevel.INFO)
	end

	if #stops == 0 then
		return
	end

	local index = math.floor(frame / FRAMES_PER_STOP) % #stops + 1
	local stop = stops[index]
	if frame % FRAMES_PER_STOP == 0 then
		TEN.Util.PrintLog("Sweep stop " .. index .. ": " .. stop.Name, TEN.Util.LogLevel.INFO)
	end

	Lara:SetHP(1000)
	Lara:SetPosition(stop.Position, true)
	Lara:SetRotation(Rotation.new(0, (frame % FRAMES_PER_STOP) * 360 / FRAMES_PER_STOP, 0))
	frame = frame + 1
end
