-- Internal file used by the RingInventory module.

local InputHelpers = {}
local PULSE_DELAY = 0.25
local FPS = 30
-- Acceleration thresholds for option left/right navigation (hold to repeat faster).
local ACCEL_INITIAL_DELAY = PULSE_DELAY   -- seconds after first press before repeating begins
local ACCEL_SLOW_REPEAT   = PULSE_DELAY   -- repeat interval at start of hold
local ACCEL_MED_REPEAT    = 0.08   -- repeat interval after ACCEL_MED_TIME seconds held
local ACCEL_FAST_REPEAT   = 0.04   -- repeat interval after ACCEL_FAST_TIME seconds held
local ACCEL_MED_TIME      = FPS * 1.5    -- hold time to reach medium speed
local ACCEL_FAST_TIME     = FPS * 2   -- hold time to reach fast speed

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