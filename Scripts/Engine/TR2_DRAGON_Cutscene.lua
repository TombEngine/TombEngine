-- MODULE: TR2_DRAGON_Cutscene.lua
-- Provides a reusable cinematic camera for Lara's dagger removal animation.

local TR2_DRAGON_Cutscene = {}

----------------------------------------------------------------------
-- CONFIGURATION
----------------------------------------------------------------------

local DAGGER_ANIM_ID = 578

-- Camera objects
local camHelper = nil
local cam = nil

-- State flags
local camMovedOnce = false
local headAnchor = nil
local laraShifted = false
local originalPos = nil
local cinematicFOVActive = false

-- FOV settings
local DEFAULT_FOV = 80

-- Pan settings
local radius = 768
local height = -150

-- Easing
local t = 0
local duration = 240   -- frames for full orbit (~4 seconds at 60fps)

local function easeInOut(x)
    return x * x * (3 - 2 * x)
end

-- Orbit angles
local startAngle = math.rad(130)
local endAngle   = math.rad(360)
local angle = startAngle

-- Meshswap timing
local meshSwapEndingFrame = 197


----------------------------------------------------------------------
-- INITIALISE CAMERA OBJECTS
----------------------------------------------------------------------

function TR2_DRAGON_Cutscene.Init()
    camHelper = GetMoveableByName("DAGGER_CAM_HELPER")
    cam = GetCameraByName("DAGGER_CAM")
end


----------------------------------------------------------------------
-- MAIN UPDATE FUNCTION
----------------------------------------------------------------------

function TR2_DRAGON_Cutscene.Update()

    -- If animation ended, restore everything
    if Lara:GetAnim() ~= DAGGER_ANIM_ID then

        if cinematicFOVActive then
            TEN.View.SetFOV(DEFAULT_FOV)
            cinematicFOVActive = false
        end

        if laraShifted then
            Lara:SetPosition(originalPos)
            laraShifted = false
        end

        camMovedOnce = false
        headAnchor = nil
        angle = startAngle
        t = 0

        return
    end

    if not camHelper or not cam then
        return
    end


    ------------------------------------------------------------------
    -- 1) Shift Lara backwards ONCE
    ------------------------------------------------------------------
    if not laraShifted then

        originalPos = Lara:GetPosition()

        local pos = Lara:GetPosition()
        local rot = Lara:GetRotation()

        local yaw = math.rad(rot.y)
        local forward = Vec3(math.sin(yaw), 0, math.cos(yaw))

        local shiftDistance = 512

        pos.x = pos.x - forward.x * shiftDistance
        pos.z = pos.z - forward.z * shiftDistance

        Lara:SetPosition(pos)
        laraShifted = true

        PlayAudioTrack("removeDagger", Sound.SoundTrackType.ONESHOT)
        Lara:SwapMesh(10, 1108, 10)
    end


    ------------------------------------------------------------------
    -- 2) Update camera target (chest joint)
    ------------------------------------------------------------------
    headAnchor = Lara:GetJointPosition(7)
    camHelper:SetPosition(headAnchor)


    ------------------------------------------------------------------
    -- 3) Move camera to starting offset ONCE
    ------------------------------------------------------------------
    if not camMovedOnce then

        local camPos = cam:GetPosition()

        camPos.x = headAnchor.x + 1024
        camPos.y = headAnchor.y + 1024
        camPos.z = headAnchor.z - 256

        cam:SetPosition(camPos)

        TEN.View.SetFOV(55)
        cinematicFOVActive = true
        camMovedOnce = true
    end


    ------------------------------------------------------------------
    -- 4) Smooth 360° orbit
    ------------------------------------------------------------------
    if t < 1 then
        t = t + (1 / duration)
    end

    local eased = easeInOut(t)
    angle = startAngle + (endAngle - startAngle) * eased

    local cx = headAnchor.x + math.cos(angle) * radius
    local cz = headAnchor.z + math.sin(angle) * radius
    local cy = headAnchor.y + height

    local camPos = cam:GetPosition()
    camPos.x = cx
    camPos.y = cy
    camPos.z = cz
    cam:SetPosition(camPos)


    ------------------------------------------------------------------
    -- 5) Play camera + mesh unswap
    ------------------------------------------------------------------
    cam:Play(camHelper)

    if Lara:GetFrame() == meshSwapEndingFrame then
        Lara:UnswapMesh(10)
    end
end


return TR2_DRAGON_Cutscene
