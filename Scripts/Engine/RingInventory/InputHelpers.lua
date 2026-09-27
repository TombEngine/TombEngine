--- Internal file used by the RingInventory module.
-- @module RingInventory.InputHelpers
-- @local

local InputHelpers = {}
local PULSE_DELAY = 0.25

function InputHelpers.GuiIsPulsed(actionID, timer)
    if TEN.Input.GetActionTimeActive(actionID) >= timer then
        return false
    end

    local oppositeAction = nil
    if actionID == TEN.Input.ActionID.MENU_UP then
        oppositeAction = TEN.Input.ActionID.MENU_DOWN
    elseif actionID == TEN.Input.ActionID.MENU_DOWN then
        oppositeAction = TEN.Input.ActionID.MENU_UP
    elseif actionID == TEN.Input.ActionID.MENU_LEFT then
        oppositeAction = TEN.Input.ActionID.MENU_RIGHT
    elseif actionID == TEN.Input.ActionID.MENU_RIGHT then
        oppositeAction = TEN.Input.ActionID.MENU_LEFT
    end

    if oppositeAction ~= nil and TEN.Input.IsKeyHeld(oppositeAction) then
        return false
    end
	
	-- Hook additional input handlers for specific controls.
    local additionalAction = nil
	if actionID == TEN.Input.ActionID.DESELECT then
		additionalAction = TEN.Input.ActionID.DRAW
	end
	
	-- Return either additional input result or main input result, if additional is unavailable.
	if additionalAction ~= nil and TEN.Input.IsKeyPulsed(additionalAction, PULSE_DELAY, PULSE_DELAY) then
		return true
	else
		return TEN.Input.IsKeyPulsed(actionID, PULSE_DELAY, PULSE_DELAY)
	end
end

return InputHelpers