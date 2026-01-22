--- @entity TR2_Dragon
------
-- This module provides a complete customisation of the previously missing TR2 Dragon effects. This module also implements a customisable custscene camera when Lara pulls the dagger out of the dragon.
--	
-- Builders can change how the cutscene behaves by editing values in `TR2_DRAGON_Cutscene.Config`
-- from their own level script.
--
-- __Full example (copy and paste into your level script):__
--
--      -- Load the Dragon cutscene module
--      local Dragon = require("Engine.Entity.TR2_Dragon")
--
--      -- Change any settings you want BEFORE the cutscene starts
--      -- (Only change the lines you need. All others use default values.)
--
--      -- Camera settings
--      Dragon.Config.ORBIT_RADIUS   = 700      -- How far the camera moves around Lara
--      Dragon.Config.ORBIT_HEIGHT   = -120     -- Vertical offset of the orbit
--      Dragon.Config.ORBIT_DURATION = 300      -- How long the orbit lasts
--
--      -- Field of view
--      Dragon.Config.DEFAULT_FOV    = 80       -- Normal FOV
--      Dragon.Config.CINEMATIC_FOV  = 50       -- FOV during the cutscene
--
--      -- Particle effects
--      Dragon.Config.MARCO_PARTICLE_COUNT = 150
--      Dragon.Config.DRAGON_STUN_PARTICLE_COUNT = 80
--
--      -- Dragon health bar
--      Dragon.Config.SHOW_DRAGON_BAR = true    -- Set to false to hide the HP bar
--
--      -- The cutscene will automatically use these values when it runs.
--
-- __Important Notes__
--
-- 	• You should NOT need to edit `TR2_Dragon.lua` itself.
-- 	• Only override the values you want to change.
-- 	• All configuration values are applied when tr2_Dragon.Init() runs.
-- 	• Make sure your level contains a CAMERA named "DAGGER_CAM".


local CustomBar = require("Engine.CustomBar")
PrintLog("TR2 Dragon module detected", LogLevel.INFO)

local TR2_DRAGON_Cutscene = {}

--- @section UserConfiguration
--- __User Configuration__
---

--- @table TR2_DRAGON_Cutscene.Config
--- @tfield int DAGGER_ANIM_ID Animation ID used when Lara pulls the dagger. _Default: 578_.
--- @tfield number MESH_SWAP_ENDING_FRAME Frame at which Lara's hand mesh should be unswapped. _Default: 197_
--- @tfield number DEFAULT_FOV Default field of view. _Default: 80_
--- @tfield number CINEMATIC_FOV Cinematic field of view during the cutscene. _Default: 55
--- @tfield number ORBIT_RADIUS Radius of the camera orbit around Lara._Default: 512
--- @tfield number ORBIT_HEIGHT Vertical offset of the orbit. _Default: -150_
--- @tfield number ORBIT_DURATION Duration of the orbit animation in frames. _Default: 240_
--- @tfield number ORBIT_START_ANGLE Starting angle of the orbit (radians). _Default: 200_
--- @tfield number ORBIT_END_ANGLE Ending angle of the orbit (radians). _Default: 340_
--- @tfield number MARCO_PARTICLE_MIN_RADIUS Minimum radius for Marco transformation particles. _Default: 5_
--- @tfield number MARCO_PARTICLE_MAX_RADIUS Maximum radius for Marco transformation particles. _Default: 7_
--- @tfield number MARCO_PARTICLE_COUNT Number of Marco transformation particles emitted per update. _Default: 100_
--- @tfield number DRAGON_STUN_PARTICLE_MIN_RADIUS Minimum radius for dragon stunned particles._Default: 2_
--- @tfield number DRAGON_STUN_PARTICLE_MAX_RADIUS Maximum radius for dragon stunned particles. _Default: 4_
--- @tfield number DRAGON_STUN_PARTICLE_COUNT Number of dragon stunned particles emitted per update. _Default: 50_
--- @tfield Color MARCO_TRANSFORMATION_CORE_START_COLOR Starting colour for Marco’s transformation core glow. _Default: Color(0, 128, 0)_
--- @tfield Color MARCO_TRANSFORMATION_CORE_END_COLOR Ending colour for Marco’s transformation core glow. _Default: Color(0, 64, 0)_
--- @tfield Color MARCO_TRANSFORMATION_SPARK_MIN_COLOR Minimum spark colour for Marco’s transformation. _Default: Color(0, 64, 0)_
--- @tfield Color MARCO_TRANSFORMATION_SPARK_MAX_COLOR Maximum spark colour for Marco’s transformation. _Default: Color(0, 255, 0)_
--- @tfield Color DRAGON_STUNNED_PARTICLE_START_COLOR Starting colour for dragon stunned particles. _Default: Color(229, 128, 76)_
--- @tfield Color DRAGON_STUNNED_PARTICLE_END_COLOR Ending colour for dragon stunned particles. _Default: Color(58, 32, 19)_
--- @tfield bool SHOW_DRAGON_BAR Whether to display the custom dragon boss health bar. _Default: true_
--- @tfield string CUTSCENE_AUDIO_TRACK Audio track name for dagger removal cutscene. _Default: "removeDagger"_

TR2_DRAGON_Cutscene.Config = {
    DAGGER_ANIM_ID = 578,
    MESH_SWAP_ENDING_FRAME = 197,
    DEFAULT_FOV = 80,
    CINEMATIC_FOV = 55,
    ORBIT_RADIUS = 512,
    ORBIT_HEIGHT = -150,
    ORBIT_DURATION = 240,
    ORBIT_START_ANGLE = math.rad(200),
    ORBIT_END_ANGLE = math.rad(340),

    MARCO_PARTICLE_MIN_RADIUS = 5,
    MARCO_PARTICLE_MAX_RADIUS = 7,
    MARCO_PARTICLE_COUNT      = 100,

    DRAGON_STUN_PARTICLE_MIN_RADIUS = 2,
    DRAGON_STUN_PARTICLE_MAX_RADIUS = 4,
    DRAGON_STUN_PARTICLE_COUNT      = 50,

    MARCO_TRANSFORMATION_CORE_START_COLOR = Color(0, 128, 0),
    MARCO_TRANSFORMATION_CORE_END_COLOR   = Color(0, 64, 0),

    MARCO_TRANSFORMATION_SPARK_MIN_COLOR = Color(0, 64, 0),
    MARCO_TRANSFORMATION_SPARK_MAX_COLOR = Color(0, 255, 0),

    DRAGON_STUNNED_PARTICLE_START_COLOR = Color(229,128,76),
    DRAGON_STUNNED_PARTICLE_END_COLOR   = Color(58,32,19),

	CUTSCENE_AUDIO_TRACK = "removeDagger",
    SHOW_DRAGON_BAR = true
}

-- Locals that will be synced from Config in Init()
local DAGGER_ANIM_ID
local MESH_SWAP_ENDING_FRAME
local DEFAULT_FOV
local CINEMATIC_FOV
local ORBIT_RADIUS
local ORBIT_HEIGHT
local ORBIT_DURATION
local ORBIT_START_ANGLE
local ORBIT_END_ANGLE

local MARCO_PARTICLE_MIN_RADIUS
local MARCO_PARTICLE_MAX_RADIUS
local MARCO_PARTICLE_COUNT

local DRAGON_STUN_PARTICLE_MIN_RADIUS
local DRAGON_STUN_PARTICLE_MAX_RADIUS
local DRAGON_STUN_PARTICLE_COUNT

local MARCO_TRANSFORMATION_CORE_START_COLOR
local MARCO_TRANSFORMATION_CORE_END_COLOR
local MARCO_TRANSFORMATION_SPARK_MIN_COLOR
local MARCO_TRANSFORMATION_SPARK_MAX_COLOR
local DRAGON_STUNNED_PARTICLE_START_COLOR
local DRAGON_STUNNED_PARTICLE_END_COLOR

local SHOW_DRAGON_BAR

--- @section InternalState
--- Internal State (Do Not Edit)

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
local angle = 0



--- @section InternalHelpers
--- Internal Helpers

local function Normalize(v)
    local len = v:Length()
    if len == 0 then return Vec3(0, 1, 0) end
    return Vec3(v.x / len, v.y / len, v.z / len)
end

local function easeInOut(x)
    return x * x * (3 - 2 * x)
end

--- @section DragonHealthBar
--- Dragon Health Bar Configuration

local dragonHealthBar = {
    barName         = "DragonHealthBar",
    objectIdBg      = TEN.Objects.ObjID.CUSTOM_BAR_GRAPHICS,
    spriteIdBg      = 0,
    colorBg         = TEN.Color(255,255,255),
    posBg           = TEN.Vec2(50, 50),
    rotBg           = 0,
    scaleBg         = TEN.Vec2(19.05, 19.1),
    alignMode       = TEN.View.AlignMode.CENTER,
    alignModeBg     = TEN.View.AlignMode.CENTER,
    scaleModeBg     = TEN.View.ScaleMode.FIT,
    blendModeBg     = TEN.Effects.BlendID.ALPHABLEND,

    objectIdBar     = TEN.Objects.ObjID.CUSTOM_BAR_GRAPHICS,
    spriteIdBar     = 1,
    colorBar        = TEN.Color(255,0,0),
    posBar          = TEN.Vec2(50, 80),
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

function TR2_DRAGON_Cutscene.Init()

    -- Sync config into locals
    local C = TR2_DRAGON_Cutscene.Config

    DAGGER_ANIM_ID = C.DAGGER_ANIM_ID
    MESH_SWAP_ENDING_FRAME = C.MESH_SWAP_ENDING_FRAME
    DEFAULT_FOV = C.DEFAULT_FOV
    CINEMATIC_FOV = C.CINEMATIC_FOV
    ORBIT_RADIUS = C.ORBIT_RADIUS
    ORBIT_HEIGHT = C.ORBIT_HEIGHT
    ORBIT_DURATION = C.ORBIT_DURATION
    ORBIT_START_ANGLE = C.ORBIT_START_ANGLE
    ORBIT_END_ANGLE = C.ORBIT_END_ANGLE

    MARCO_PARTICLE_MIN_RADIUS = C.MARCO_PARTICLE_MIN_RADIUS
    MARCO_PARTICLE_MAX_RADIUS = C.MARCO_PARTICLE_MAX_RADIUS
    MARCO_PARTICLE_COUNT      = C.MARCO_PARTICLE_COUNT

    DRAGON_STUN_PARTICLE_MIN_RADIUS = C.DRAGON_STUN_PARTICLE_MIN_RADIUS
    DRAGON_STUN_PARTICLE_MAX_RADIUS = C.DRAGON_STUN_PARTICLE_MAX_RADIUS
    DRAGON_STUN_PARTICLE_COUNT      = C.DRAGON_STUN_PARTICLE_COUNT

    MARCO_TRANSFORMATION_CORE_START_COLOR = C.MARCO_TRANSFORMATION_CORE_START_COLOR
    MARCO_TRANSFORMATION_CORE_END_COLOR   = C.MARCO_TRANSFORMATION_CORE_END_COLOR
    MARCO_TRANSFORMATION_SPARK_MIN_COLOR  = C.MARCO_TRANSFORMATION_SPARK_MIN_COLOR
    MARCO_TRANSFORMATION_SPARK_MAX_COLOR  = C.MARCO_TRANSFORMATION_SPARK_MAX_COLOR
    DRAGON_STUNNED_PARTICLE_START_COLOR   = C.DRAGON_STUNNED_PARTICLE_START_COLOR
    DRAGON_STUNNED_PARTICLE_END_COLOR     = C.DRAGON_STUNNED_PARTICLE_END_COLOR

	CUTSCENE_AUDIO_TRACK = C.CUTSCENE_AUDIO_TRACK
    SHOW_DRAGON_BAR = C.SHOW_DRAGON_BAR
	
    angle = ORBIT_START_ANGLE
    time  = 0

    PrintLog("TR2 Dragon module loaded successfully", LogLevel.INFO)

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

function TR2_DRAGON_Cutscene.Update()

    if SHOW_DRAGON_BAR then

        local bar = CustomBar.Get("DragonHealthBar")

        if not bar then
            CustomBar.CreateEnemyHpBar(dragonHealthBar)
            bar = CustomBar.Get("DragonHealthBar")

            local dataName = "DragonHealthBar_bar_data"
            LevelVars.Engine.CustomBars.bars[dataName].maxValue = 300

            if dragon then
                bar:SetBarValue(dragon:GetHP(), 0)
            end
        end

        if dragon and bar then
            bar:SetBarValue(dragon:GetHP(), 0)
            bar:SetVisibility(true)
        end
    end

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

    if marco[1] and marco[1]:GetVisible() and marco[1]:GetActive() then
        local target = marco[1]:GetJointPosition(18)
        emitInwardSphereParticles(
            target,
            math.random((MARCO_PARTICLE_MIN_RADIUS*1024), (MARCO_PARTICLE_MAX_RADIUS*1024)),
            MARCO_PARTICLE_COUNT,
            MARCO_TRANSFORMATION_SPARK_MIN_COLOR,
            MARCO_TRANSFORMATION_SPARK_MAX_COLOR
        )
        emitCoreGlow(
            target,
            MARCO_TRANSFORMATION_CORE_START_COLOR,
            MARCO_TRANSFORMATION_CORE_END_COLOR
        )
    end

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
                    DRAGON_STUNNED_PARTICLE_START_COLOR,
                    DRAGON_STUNNED_PARTICLE_END_COLOR
                )

                emitCoreGlow(
                    dragonTarget,
                    DRAGON_STUNNED_PARTICLE_START_COLOR,
                    DRAGON_STUNNED_PARTICLE_END_COLOR
                )
            end
        end
    end

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

		PlayAudioTrack(CUTSCENE_AUDIO_TRACK, Sound.SoundTrackType.ONESHOT)

        Lara:SwapMesh(10, 1108, 10)
        EmitBlood(Lara:GetJointPosition(10), 5000)
    end

    headAnchor = Lara:GetJointPosition(7)
    camHelper:SetPosition(headAnchor)

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

    cam:Play(camHelper)

    if Lara:GetFrame() == MESH_SWAP_ENDING_FRAME then
        Lara:UnswapMesh(10)
    end
end

--- @section CallbackRegistration
--- Callback Registration

LevelFuncs.DragonCutscene_Init = function()
    TR2_DRAGON_Cutscene.Init()
end

LevelFuncs.DragonCutscene_Update = function(dt)
    TR2_DRAGON_Cutscene.Update(dt)
end

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_START, LevelFuncs.DragonCutscene_Init)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_LOOP,  LevelFuncs.DragonCutscene_Update)

return TR2_DRAGON_Cutscene
