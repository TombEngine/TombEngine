-- ldignore
-- Internal file used by the RingInventory module.

local Constants = require("Engine.PhotoMode.Constants")

local InputHelpers = {}
local PULSE_DELAY = Constants.PULSE_DELAY
local FPS = Constants.FPS
-- Acceleration thresholds for option left/right navigation (hold to repeat faster).
local ACCEL_INITIAL_DELAY = Constants.ACCEL_INITIAL_DELAY
local ACCEL_SLOW_REPEAT   = Constants.ACCEL_SLOW_REPEAT
local ACCEL_MED_REPEAT    = Constants.ACCEL_MED_REPEAT
local ACCEL_FAST_REPEAT   = Constants.ACCEL_FAST_REPEAT
local ACCEL_MED_TIME      = Constants.ACCEL_MED_TIME
local ACCEL_FAST_TIME     = Constants.ACCEL_FAST_TIME

local function GetAccelerationRate(timeactive)
    
    local t = timeactive

    -- Suppress repeats during initial delay.
    if t < ACCEL_INITIAL_DELAY then
        return false
    end

    if t > ACCEL_FAST_TIME then
        return ACCEL_FAST_REPEAT
    elseif t > ACCEL_MED_TIME then
        return ACCEL_MED_REPEAT
    else
        return ACCEL_SLOW_REPEAT
    end

end
function InputHelpers.GuiIsPulsed(actionID, timer, accelerate)

    local timeActive = TEN.Input.GetActionTimeActive(actionID)
    if timeActive >= timer then
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

    if accelerate then
        local accelRate = GetAccelerationRate(timeActive)
        if accelRate then
            return TEN.Input.IsKeyPulsed(actionID, accelRate, accelRate)
        end
    end

    return TEN.Input.IsKeyPulsed(actionID, PULSE_DELAY, PULSE_DELAY)
end

return InputHelpers