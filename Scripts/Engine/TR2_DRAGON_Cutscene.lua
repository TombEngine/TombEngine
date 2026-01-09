-- MODULE: TR2_DRAGON_Cutscene.lua
-- Code Version 1

local CustomBar = require("Engine.CustomBar")
PrintLog("TR2 Dragon module detected", LogLevel.INFO)

local TR2_DRAGON_Cutscene = {}

----------------------------------------------------------------------
-- USER CONFIGURATION (SAFE TO EDIT)
----------------------------------------------------------------------

local DAGGER_ANIM_ID = 578
local MESH_SWAP_ENDING_FRAME = 197

local DEFAULT_FOV   = 80
local CINEMATIC_FOV = 55

local ORBIT_RADIUS       = 512
local ORBIT_HEIGHT       = -150
local ORBIT_DURATION     = 240
local ORBIT_START_ANGLE  = math.rad(200)
local ORBIT_END_ANGLE    = math.rad(340)

local MARCO_PARTICLE_MIN_RADIUS = 5
local MARCO_PARTICLE_MAX_RADIUS = 7
local MARCO_PARTICLE_COUNT      = 400

local DRAGON_STUN_PARTICLE_MIN_RADIUS = 2
local DRAGON_STUN_PARTICLE_MAX_RADIUS = 4
local DRAGON_STUN_PARTICLE_COUNT      = 800

local COLOR_MARCO_TRANSFORMATION_CORE_START_COLOR = Color(0, 128, 0)
local COLOR_MARCO_TRANSFORMATION_CORE_END_COLOR   = Color(0, 64, 0)

local COLOR_MARCO_TRANSFORMATION_SPARK_MIN_COLOR = Color(0, 64, 0)
local COLOR_MARCO_TRANSFORMATION_SPARK_MAX_COLOR = Color(0, 255, 0)

local COLOR_DRAGON_STUNNED_PARTICLE_START_COLOR = Color(math.random(204,229), math.random(102,128),math.random(51,76))
local COLOR_DRAGON_STUNNED_PARTICLE_END_COLOR   = Color(math.random(51,58), math.random(25,32),math.random(12,19))

local SHOW_DRAGON_BAR = false
----------------------------------------------------------------------
-- INTERNAL STATE (DO NOT EDIT)
----------------------------------------------------------------------

local marco      = nil
local camHelper  = nil
local cam        = nil
local dragon     = nil
local dragonList = nil

local camMovedOnce        = false
local headAnchor          = nil
local laraShifted         = false
local originalPos         = nil
local cinematicFOVActive  = false

local time  = 0
local angle = ORBIT_START_ANGLE

----------------------------------------------------------------------
-- INTERNAL HELPERS
----------------------------------------------------------------------

local function Normalize(v)
    local len = v:Length()
    if len == 0 then return Vec3(0, 1, 0) end
    return Vec3(v.x / len, v.y / len, v.z / len)
end

local function easeInOut(x)
    return x * x * (3 - 2 * x)
end

----------------------------------------------------------------------
-- DRAGON HEALTH BAR
----------------------------------------------------------------------

local dragonHealthBar = {

    barName         = "DragonHealthBar",

    objectIdBg      = TEN.Objects.ObjID.CUSTOM_BAR_GRAPHICS,
    spriteIdBg      = 0,
    colorBg         = TEN.Color(255,255,255),
    posBg           = TEN.Vec2(50, 50),
    rotBg           = 0,
    scaleBg         = TEN.Vec2(19.05, 19.1),
    alignModeBg     = TEN.View.AlignMode.CENTER_LEFT,
    scaleModeBg     = TEN.View.ScaleMode.FIT,
    blendModeBg     = TEN.Effects.BlendID.ALPHABLEND,

    objectIdBar     = TEN.Objects.ObjID.CUSTOM_BAR_GRAPHICS,
    spriteIdBar     = 1,
    colorBar        = TEN.Color(255,0,0),
    posBar          = TEN.Vec2(50, 50),
    rot             = 0,
    scaleBar        = TEN.Vec2(20, 20),
    alignMode       = TEN.View.AlignMode.CENTER_LEFT,
    scaleMode       = TEN.View.ScaleMode.FIT,
    blendMode       = TEN.Effects.BlendID.ALPHABLEND,

    alphaBlendSpeed = 50,
    showBar         = true,
    blink           = true,
    blinkLimit      = 0.25,

    object          = nil,
    startValue      = nil,
    maxValue        = 300
}

----------------------------------------------------------------------
-- INITIALISE CAMERA OBJECTS
----------------------------------------------------------------------

function TR2_DRAGON_Cutscene.Init()

    PrintLog("TR2 Dragon module loaded successfully", LogLevel.INFO)

    -- Disable generic enemy HP bars so they don't override our custom dragon bar
    CustomBar.ShowEnemiesHpGenericBar(false)

    local pos = Lara:GetPosition()
    local room = Lara:GetRoom()

    camHelper = Moveable(TEN.Objects.ObjID.CAMERA_TARGET, "DAGGER_CAM_HELPER", pos, Rotation(0,0,0), room)
    cam       = GetCameraByName("DAGGER_CAM")
    marco     = GetMoveablesBySlot(TEN.Objects.ObjID.MARCO_BARTOLI)

    dragonList = GetMoveablesBySlot(TEN.Objects.ObjID.DRAGON_FRONT)
    dragon     = dragonList[1]

    dragonHealthBar.object     = dragon:GetName()
    dragonHealthBar.startValue = dragon:GetHP()

    if SHOW_DRAGON_BAR then
    CustomBar.CreateEnemyHpBar(dragonHealthBar)
    end
end

----------------------------------------------------------------------
-- MAIN UPDATE FUNCTION
----------------------------------------------------------------------

function TR2_DRAGON_Cutscene.Update()

    ------------------------------------------------------------------
    -- MINIMAL, CLEAN BOSS BAR LOGIC
    ------------------------------------------------------------------
    if SHOW_DRAGON_BAR then

        local bar = CustomBar.Get("DragonHealthBar")

        -- If the bar was deleted (HP hit 0), recreate it
        if not bar then
            CustomBar.CreateEnemyHpBar(dragonHealthBar)
            bar = CustomBar.Get("DragonHealthBar")

            -- Restore correct max HP
            local dataName = "DragonHealthBar_bar_data"
            LevelVars.Engine.CustomBars.bars[dataName].maxValue = 300

            -- Set correct current HP (e.g., 150 when dragon wakes)
            if dragon then
                bar:SetBarValue(dragon:GetHP(), 0)
            end
        end

        -- Normal update
        if dragon and bar then
            bar:SetBarValue(dragon:GetHP(), 0)
            bar:SetVisibility(true)
        end
    end

    ------------------------------------------------------------------
    -- PARTICLE HELPERS
    ------------------------------------------------------------------

    local function randomPointOnSphere(radius)
        local u = math.random()
        local v = math.random()
        local theta = 2 * math.pi * u
        local phi   = math.acos(2 * v - 1)
        return Vec3(
            radius * math.sin(phi) * math.cos(theta),
            radius * math.sin(phi) * math.sin(theta),
            radius * math.cos(phi)
        )
    end

    local function ComputeLife(radius, speed)
        return math.max((radius / speed) * 0.85, 0.2)
    end

    local function emitInwardSphereParticles(targetPos, radius, count, startColor, endColor)
        for i = 1, count do
            local startOffset = randomPointOnSphere(radius)
            local startPos    = targetPos + startOffset
            local dir         = Normalize(targetPos - startPos)
            local speed       = 2500 + math.random() * 500
            local particleLife = ComputeLife(radius, speed)

            EmitAdvancedParticle({
                pos        = startPos,
                vel        = dir * speed,
                life       = particleLife,
                friction   = 0,
                gravity    = 0,
                startSize  = math.random(8, 16),
                endSize    = 0,
                startColor = startColor,
                endColor   = endColor,
                blendMode  = TEN.Effects.BlendID.ADDITIVE,
                spriteSeqID = Objects.ObjID.SPARK_SPRITE,
                rotVel     = 0,
                animated   = false,
                light      = true,
                lightRadius = 8
            })
        end
    end

    local function emitCoreGlow(pos, startColor, endColor)
        EmitAdvancedParticle({
            pos        = pos,
            vel        = Vec3(0,0,0),
            life       = 0.1,
            startSize  = 56 + math.random(0, 16),
            endSize    = 64,
            startColor = startColor,
            endColor   = endColor,
            blendMode  = TEN.Effects.BlendID.ADDITIVE,
            spriteSeqID = Objects.ObjID.DEFAULT_SPRITES,
            spriteID   = 14,
            animated   = false,
            light      = true,
            lightRadius = 8,
        })
    end

    ------------------------------------------------------------------
    -- MARCO TRANSFORMATION EFFECT
    ------------------------------------------------------------------

    if marco[1] and marco[1]:GetVisible() and marco[1]:GetActive() then
        local target = marco[1]:GetJointPosition(18)
        emitInwardSphereParticles(
            target,
            math.random((MARCO_PARTICLE_MIN_RADIUS*1024), (MARCO_PARTICLE_MAX_RADIUS*1024)),
            MARCO_PARTICLE_COUNT,
            COLOR_MARCO_TRANSFORMATION_SPARK_MIN_COLOR,
            COLOR_MARCO_TRANSFORMATION_SPARK_MAX_COLOR
        )
        emitCoreGlow(
            target,
            COLOR_MARCO_TRANSFORMATION_CORE_START_COLOR,
            COLOR_MARCO_TRANSFORMATION_CORE_END_COLOR
        )
    end

    ------------------------------------------------------------------
    -- DRAGON STUNNED EFFECT
    ------------------------------------------------------------------

    if dragon and dragon:GetActive() and dragon:GetAnim() == 22 then

        local stunTimer = dragon:GetItemFlags(1)

        if stunTimer and stunTimer > 0 then
            if Lara:GetAnim() ~= DAGGER_ANIM_ID then

                local dragonTarget = dragon:GetJointPosition(0) + Vec3(750, 256, 128)

                emitInwardSphereParticles(
                    dragonTarget,
                    math.random((DRAGON_STUN_PARTICLE_MIN_RADIUS * 1024),
                                (DRAGON_STUN_PARTICLE_MAX_RADIUS * 1024)),
                    DRAGON_STUN_PARTICLE_COUNT,
                    COLOR_DRAGON_STUNNED_PARTICLE_START_COLOR,
                    COLOR_DRAGON_STUNNED_PARTICLE_END_COLOR
                )

                emitCoreGlow(
                    dragonTarget,
                    COLOR_DRAGON_STUNNED_PARTICLE_START_COLOR,
                    COLOR_DRAGON_STUNNED_PARTICLE_END_COLOR
                )
            end
        end
    end

    ------------------------------------------------------------------
    -- DAGGER PULL CUTSCENE
    ------------------------------------------------------------------

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
        headAnchor   = nil
        angle        = ORBIT_START_ANGLE
        time         = 0

        return
    end

    if not camHelper or not cam then return end

    TEN.Input.ClearAllKeys()

    ------------------------------------------------------------------
    -- 1) Shift Lara backwards ONCE
    ------------------------------------------------------------------

    if not laraShifted then
        originalPos = Lara:GetPosition()

        local pos = Lara:GetPosition()
        local rot = Lara:GetRotation()
        local yaw = math.rad(rot.y)
        local forward = Vec3(math.sin(yaw), 0, math.cos(yaw))

        pos.x = pos.x - forward.x * 512
        pos.z = pos.z - forward.z * 512

        Lara:SetPosition(pos)
        laraShifted = true

        PlayAudioTrack("removeDagger", Sound.SoundTrackType.ONESHOT)
        Lara:SwapMesh(10, 1108, 10)
        EmitBlood(Lara:GetJointPosition(10), 5000)
    end

    ------------------------------------------------------------------
    -- 2) Update camera target
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

        TEN.View.SetFOV(CINEMATIC_FOV)
        cinematicFOVActive = true
        camMovedOnce = true
    end

    ------------------------------------------------------------------
    -- 4) Smooth 360 orbit (LOCAL SPACE AROUND LARA)
    ------------------------------------------------------------------

    if time < 1 then
        time = time + (1 / ORBIT_DURATION)
    end

    local eased      = easeInOut(time)
    local localAngle = ORBIT_START_ANGLE + (ORBIT_END_ANGLE - ORBIT_START_ANGLE) * eased

    local rot = Lara:GetRotation()
    local yaw = math.rad(rot.y)

    local right   = Vec3(math.cos(yaw), 0, -math.sin(yaw))
    local forward = Vec3(math.sin(yaw), 0,  math.cos(yaw))

    local offset =
        right   * (ORBIT_RADIUS * math.cos(localAngle)) +
        forward * (ORBIT_RADIUS * math.sin(localAngle)) +
        Vec3(0, ORBIT_HEIGHT, 0)

    local camPos = headAnchor + offset
    cam:SetPosition(camPos)

    ------------------------------------------------------------------
    -- 5) Play camera + mesh unswap
    ------------------------------------------------------------------

    cam:Play(camHelper)

    if Lara:GetFrame() == MESH_SWAP_ENDING_FRAME then
        Lara:UnswapMesh(10)
    end
end

----------------------------------------------------------------------
-- AUTO‑REGISTRATION USING TEN CALLBACKS
----------------------------------------------------------------------

-- Wrap your Init() inside LevelFuncs
LevelFuncs.DragonCutscene_Init = function()
    TR2_DRAGON_Cutscene.Init()
end

-- Wrap your Update() inside LevelFuncs
LevelFuncs.DragonCutscene_Update = function(dt)
    TR2_DRAGON_Cutscene.Update()
end

-- Register callbacks
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_START, LevelFuncs.DragonCutscene_Init)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP,  LevelFuncs.DragonCutscene_Update)

return TR2_DRAGON_Cutscene
