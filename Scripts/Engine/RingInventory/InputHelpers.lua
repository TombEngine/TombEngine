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

    return TEN.Input.IsKeyPulsed(actionID, PULSE_DELAY, PULSE_DELAY)
end

return InputHelpers