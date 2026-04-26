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
    if actionID == TEN.Input.ActionID.FORWARD then
        oppositeAction = TEN.Input.ActionID.BACK
    elseif actionID == TEN.Input.ActionID.BACK then
        oppositeAction = TEN.Input.ActionID.FORWARD
    elseif actionID == TEN.Input.ActionID.LEFT then
        oppositeAction = TEN.Input.ActionID.RIGHT
    elseif actionID == TEN.Input.ActionID.RIGHT then
        oppositeAction = TEN.Input.ActionID.LEFT
    end

    if oppositeAction ~= nil and TEN.Input.IsKeyHeld(oppositeAction) then
        return false
    end

    return TEN.Input.IsKeyPulsed(actionID, PULSE_DELAY, PULSE_DELAY)
end

return InputHelpers