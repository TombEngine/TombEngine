--- Internal constants used by the RingInventory module.
-- @module RingInventory.Constants
-- @local

local Constants = {}

Constants.NO_VALUE = -1
Constants.RING_RADIUS = 570
Constants.FOV = 35
Constants.CAMERA_START = Vec3(0, -1700,  450)
Constants.CAMERA_END   = Vec3(0,   -50, -500)
Constants.TARGET_START = Vec3(0,    60, 1000)
Constants.TARGET_END   = Vec3(0,   100,  300)
Constants.ALPHA_MAX = 255
Constants.ALPHA_MIN = 0

return Constants