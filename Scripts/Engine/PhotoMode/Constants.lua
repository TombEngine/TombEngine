-- ldignore
-- Shared numerical constants for the PhotoMode module.

local Constants = {}

-- Floating-point epsilon for near-zero comparisons.
Constants.EPSILON = 0.001

-- Camera limits.
Constants.PITCH_LIMIT     = 88.0
Constants.WORLD_UP        = TEN.Vec3(0, -1, 0)
Constants.WALL_TOLERANCE  = 256

-- Input dead zone.
Constants.AXIS_DEAD_ZONE = 0.15

-- Menu / UI repeat timing.
Constants.PULSE_DELAY         = 0.25
Constants.FPS                 = 30
Constants.ACCEL_INITIAL_DELAY = Constants.PULSE_DELAY
Constants.ACCEL_SLOW_REPEAT   = Constants.PULSE_DELAY
Constants.ACCEL_MED_REPEAT    = 0.08
Constants.ACCEL_FAST_REPEAT   = 0.04
Constants.ACCEL_MED_TIME      = Constants.FPS * 1.5
Constants.ACCEL_FAST_TIME     = Constants.FPS * 2

-- Menu display.
Constants.ALPHA_MAX     = 255
Constants.ALPHA_MIN     = 0
Constants.LINE_SPACING  = 6
Constants.SCROLL_SPEED  = 0.2
Constants.FADE_SPEED    = 50
Constants.DRAW_RATE     = 1 / Constants.FPS

-- Photo-mode UI layout.
Constants.HEADER_POS   = TEN.Vec2(50, 15)
Constants.HEADER_SCALE = 1.0

return Constants
