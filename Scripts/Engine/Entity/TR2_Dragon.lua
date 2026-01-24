--- @entity TR2_Dragon
------
-- This module provides a complete customisation of the previously missing TR2 Dragon effects.
-- It also implements a fully customisable cutscene camera for the dagger‑pull sequence.
--
-- -  Builders can override any values in `TR2_DRAGON_Cutscene.Config` from their own level script.
-- -  Only override what you need — all other values fall back to defaults.
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
-- __Important Notes__
--
-- -  __Make sure your level contains a CAMERA named "daggerCam".__
-- -  This can be overriden using `Dragon.Config.CAMERA_NAME`
-- -  __Make sure your level contains a CAMERA_TARGET named "daggerCamHelper".__
-- -  This can be overriden using `Dragon.Config.CAMERA_TARGET_NAME`

local CustomBar = require("Engine.CustomBar")
PrintLog("TR2 Dragon module detected", LogLevel.INFO)

-- Main module table. All public configuration lives inside this.
local TR2_DRAGON_Cutscene = {}

--- @section UserConfiguration
--- __User Configuration__
---
--- These values can be overridden by builders from their level script.
--- Only change what you need — everything else uses defaults.

--- @table TR2_DRAGON_Cutscene.Config
--- @tfield int DAGGER_ANIM_ID Animation ID used when Lara pulls the dagger. _Default: 578_.
--- @tfield number MESH_SWAP_ENDING_FRAME Frame at which Lara's hand mesh should be unswapped. _Default: 197_
--- @tfield number DEFAULT_FOV Default field of view. _Default: 80_
--- @tfield number CINEMATIC_FOV Cinematic field of view during the cutscene. _Default: 55_
--- @tfield number ORBIT_RADIUS Radius of the camera orbit around Lara. _Default: 1024_
--- @tfield number ORBIT_HEIGHT Vertical offset of the orbit. _Default: -150_
--- @tfield number ORBIT_DURATION Duration of the orbit animation in frames. _Default: 240_
--- @tfield number ORBIT_START_ANGLE Starting angle of the orbit (radians). _Default: 200°_
--- @tfield number ORBIT_END_ANGLE Ending angle of the orbit (radians). _Default: 340°_
--- @tfield number MARCO_PARTICLE_MIN_RADIUS Minimum radius for Marco transformation particles. _Default: 5_
--- @tfield number MARCO_PARTICLE_MAX_RADIUS Maximum radius for Marco transformation particles. _Default: 7_
--- @tfield number MARCO_PARTICLE_COUNT Number of Marco transformation particles emitted per update. _Default: 100_
--- @tfield number DRAGON_STUN_PARTICLE_MIN_RADIUS Minimum radius for dragon stunned particles. _Default: 2_
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
--- @tfield string DRAGON_NAME Name for Dragon Boss (set in Tomb Editor). _Default: "dragon"_
--- @tfield string CAMERA_NAME Name of the cutscene camera. _Default: "daggerCam"_
--- @tfield string CAMERA_TARGET_NAME Name of the CAMERA_TARGET used as the camera target. _Default: "daggerCamHelper"_

-- Default configuration values.
-- These are copied into local variables during Init() for performance.
TR2_DRAGON_Cutscene.Config = {
    DAGGER_ANIM_ID = 578,
    MESH_SWAP_ENDING_FRAME = 197,

    DEFAULT_FOV = 80,
    CINEMATIC_FOV = 55,

    ORBIT_RADIUS = 1024,
    ORBIT_HEIGHT = -150,
    ORBIT_DURATION = 240,
    ORBIT_START_ANGLE = math.rad(200),
    ORBIT_END_ANGLE   = math.rad(340),

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
    SHOW_DRAGON_BAR      = true,
    DRAGON_NAME          = "dragon",
	CAMERA_NAME         = "daggerCam",
	CAMERA_TARGET_NAME  = "daggerCamHelper",

}

--- @section InternalState
--- Internal State (Do Not Edit)

-- These locals mirror the user‑configurable values.
-- They are copied from TR2_DRAGON_Cutscene.Config during Init()
-- so the module never reads directly from the config table at runtime.
local DAGGER_ANIM_ID
local MESH_SWAP_ENDING_FRAME
local DEFAULT_FOV
local CINEMATIC_FOV
local ORBIT_RADIUS
local ORBIT_HEIGHT
local ORBIT_DURATION
local ORBIT_START_ANGLE
local ORBIT_END_ANGLE

-- Particle configuration copied from Config during Init().
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

-- Misc configuration values
local SHOW_DRAGON_BAR
local CUTSCENE_AUDIO_TRACK
local DRAGON_NAME

-- References to in‑game objects.
local marco      = nil
local camHelper  = nil
local cam        = nil
local dragon     = nil

-- Runtime state flags used during the cutscene.
local camMovedOnce       = false
local headAnchor         = nil
local laraShifted        = false
local originalPos        = nil
local cinematicFOVActive = false
local cutsceneEnabled    = false

-- Time/angle values used for the orbit camera interpolation.
local time  = 0

-- Persistent stunned state.
LevelVars.TR2_Dragon = LevelVars.TR2_Dragon or {
    stunned   = false,
    stunTimer = 0
}

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

local function FindMoveableByName(name)
    for slot = 0, 1023 do
        local list = GetMoveablesBySlot(slot)
        if list then
            for _, mov in ipairs(list) do
                if mov:GetName() == name then
                    return mov
                end
            end
        end
    end
    return nil
end

--- @section DragonHealthBar
--- Dragon Health Bar Configuration

local dragonHealthBar = {
    barName         = "DragonHealthBar",
    objectIdBg      = TEN.Objects.ObjID.CUSTOM_BAR_GRAPHICS,
    spriteIdBg      = 0,
    colorBg         = TEN.Color(255,255,255),
    posBg           = TEN.Vec2(50, 80),
    rotBg           = 0,
    scaleBg         = TEN.Vec2(20.5, 20),
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

--- @section Init
--- Initialisation Logic

function TR2_DRAGON_Cutscene.Init()
    local C = TR2_DRAGON_Cutscene.Config

    DAGGER_ANIM_ID        					= C.DAGGER_ANIM_ID
    MESH_SWAP_ENDING_FRAME 					= C.MESH_SWAP_ENDING_FRAME
    DEFAULT_FOV          				 	= C.DEFAULT_FOV
    CINEMATIC_FOV        				 	= C.CINEMATIC_FOV
    ORBIT_RADIUS         				 	= C.ORBIT_RADIUS
    ORBIT_HEIGHT         				 	= C.ORBIT_HEIGHT
    ORBIT_DURATION       				 	= C.ORBIT_DURATION
    ORBIT_START_ANGLE    				 	= C.ORBIT_START_ANGLE
    ORBIT_END_ANGLE       					= C.ORBIT_END_ANGLE

    MARCO_PARTICLE_MIN_RADIUS				= C.MARCO_PARTICLE_MIN_RADIUS
    MARCO_PARTICLE_MAX_RADIUS 				= C.MARCO_PARTICLE_MAX_RADIUS
    MARCO_PARTICLE_COUNT      				= C.MARCO_PARTICLE_COUNT

    DRAGON_STUN_PARTICLE_MIN_RADIUS 		= C.DRAGON_STUN_PARTICLE_MIN_RADIUS
    DRAGON_STUN_PARTICLE_MAX_RADIUS 		= C.DRAGON_STUN_PARTICLE_MAX_RADIUS
    DRAGON_STUN_PARTICLE_COUNT      		= C.DRAGON_STUN_PARTICLE_COUNT

    MARCO_TRANSFORMATION_CORE_START_COLOR 	= C.MARCO_TRANSFORMATION_CORE_START_COLOR
    MARCO_TRANSFORMATION_CORE_END_COLOR   	= C.MARCO_TRANSFORMATION_CORE_END_COLOR
    MARCO_TRANSFORMATION_SPARK_MIN_COLOR  	= C.MARCO_TRANSFORMATION_SPARK_MIN_COLOR
    MARCO_TRANSFORMATION_SPARK_MAX_COLOR  	= C.MARCO_TRANSFORMATION_SPARK_MAX_COLOR
    DRAGON_STUNNED_PARTICLE_START_COLOR   	= C.DRAGON_STUNNED_PARTICLE_START_COLOR
    DRAGON_STUNNED_PARTICLE_END_COLOR     	= C.DRAGON_STUNNED_PARTICLE_END_COLOR

    CUTSCENE_AUDIO_TRACK 					= C.CUTSCENE_AUDIO_TRACK
    SHOW_DRAGON_BAR      					= C.SHOW_DRAGON_BAR
	DRAGON_NAME 							= C.DRAGON_NAME
	CAMERA_NAME    							= C.CAMERA_NAME
	CAMERA_TARGET_NAME 						= C.CAMERA_TARGET_NAME

    time  									= 0

    PrintLog("TR2 Dragon module initialised", LogLevel.INFO)
    CustomBar.ShowEnemiesHpGenericBar(false)

    -- Clean up any leftover bar safely
    local oldBar = CustomBar.Get("DragonHealthBar")
    if oldBar then
        CustomBar.Delete("DragonHealthBar")
    end

	camHelper = GetMoveableByName(CAMERA_TARGET_NAME)
	if not camHelper then
		PrintLog("ERROR: " .. CAMERA_TARGET_NAME .. " not found in level!", LogLevel.ERROR)
		return
	end
	cam = GetCameraByName(CAMERA_NAME)

    marco = GetMoveablesBySlot(TEN.Objects.ObjID.MARCO_BARTOLI)

    -- Single, named dragon
    dragon = DRAGON_NAME and GetMoveableByName(DRAGON_NAME) or nil

    -- Determine dragon mode based on OCB:
    -- OCB 0 = TR2‑accurate auto‑death at 0 HP, no dagger cutscene.
    -- OCB 1 = dagger‑pull cutscene enabled.
    local ocb = dragon and dragon:GetOCB() or 0
    cutsceneEnabled = (ocb == 1)

    if dragon and dragon:GetName() ~= "" then
        dragonHealthBar.object     = dragon:GetName()
        dragonHealthBar.startValue = dragon:GetHP()
    end

    -- Only restore stun state if cutscene is enabled
    if cutsceneEnabled and dragon and LevelVars.TR2_Dragon.stunned then
        dragon:SetAnim(22)
        dragon:SetItemFlags(1, LevelVars.TR2_Dragon.stunTimer or 30)
    end

    camMovedOnce       = false
    headAnchor         = nil
    laraShifted        = false
    originalPos        = nil
    cinematicFOVActive = false
end

--- @section ParticleHelpers
--- Particle Helper Functions

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

        local dir = Normalize(targetPos - startPos)
        local speed = 2500 + math.random() * 500
        local particleLife = ComputeLife(radius, speed)

        EmitAdvancedParticle({
            pos         = startPos,
            vel         = dir * speed,
            life        = particleLife,
            friction    = 0,
            gravity     = 0,
            startSize   = math.random(8, 16),
            endSize     = 0,
            startColor  = startColor,
            endColor    = endColor,
            blendMode   = TEN.Effects.BlendID.ADDITIVE,
            spriteSeqID = Objects.ObjID.SPARK_SPRITE,
            rotVel      = 0,
            animated    = false,
            light       = true,
            lightRadius = 8
        })
    end
end

local function emitCoreGlow(pos, startColor, endColor)
    EmitAdvancedParticle({
        pos         = pos,
        vel         = Vec3(0,0,0),
        life        = 0.1,
        startSize   = 56 + math.random(0, 16),
        endSize     = 64,
        startColor  = startColor,
        endColor    = endColor,
        blendMode   = TEN.Effects.BlendID.ADDITIVE,
        spriteSeqID = Objects.ObjID.DEFAULT_SPRITES,
        spriteID    = 14,
        animated    = false,
        light       = true,
        lightRadius = 8,
    })
end

--- @section Update
--- Per‑frame Update Logic

function TR2_DRAGON_Cutscene.Update()

    -- Ensure helper and camera exist
	if not camHelper then
		camHelper = FindMoveableByName(CAMERA_TARGET_NAME)
		if not camHelper then return end
	end

	if not cam then
		cam = GetCameraByName(CAMERA_NAME)
		if not cam then return end
	end

    -- Ensure dragon reference is still valid (e.g. after reload)
    if not dragon then
        dragon = DRAGON_NAME and GetMoveableByName(DRAGON_NAME) or nil
    end
    
    -- Dragon HP bar (only visible when targeted)
    
    if SHOW_DRAGON_BAR and dragon and dragon:GetName() ~= "" then
        local target     = Lara:GetTarget()
        local isTargeted = (target == dragon)
        local bar        = CustomBar.Get("DragonHealthBar")

        if not isTargeted then
            if bar then
                bar:SetVisibility(false)
            end
        else
            if not bar then
                CustomBar.CreateEnemyHpBar(dragonHealthBar)
                bar = CustomBar.Get("DragonHealthBar")
            end

            if bar then
                bar:SetBarValue(dragon:GetHP(), 0)
                bar:SetVisibility(true)
            end
        end
    end
    
    -- Marco transformation effect
    
    if marco and marco[1]
       and marco[1]:GetVisible()
       and marco[1]:GetStatus() == MoveableStatus.ACTIVE then

        local target = marco[1]:GetJointPosition(18)

        emitInwardSphereParticles(
            target,
            math.random(MARCO_PARTICLE_MIN_RADIUS * 1024, MARCO_PARTICLE_MAX_RADIUS * 1024),
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

    -- Dragon stunned effect (only for cutscene‑enabled dragon)
    
    if cutsceneEnabled
       and dragon
       and dragon:GetStatus() == MoveableStatus.ACTIVE
       and dragon:GetAnim() == 22 then

        local stunTimer = dragon:GetItemFlags(1)

        if stunTimer and stunTimer > 0 then
            LevelVars.TR2_Dragon.stunned   = true
            LevelVars.TR2_Dragon.stunTimer = stunTimer

            if Lara:GetAnim() ~= DAGGER_ANIM_ID then
                local dragonTarget =
                    dragon:GetJointPosition(0) +
                    Vec3(750, 256, 128)

                emitInwardSphereParticles(
                    dragonTarget,
                    math.random(DRAGON_STUN_PARTICLE_MIN_RADIUS * 1024,
                                DRAGON_STUN_PARTICLE_MAX_RADIUS * 1024),
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

        if stunTimer == 0 then
            LevelVars.TR2_Dragon.stunned   = false
            LevelVars.TR2_Dragon.stunTimer = 0
        end
    end
    
    -- If this dragon does not use the cutscene (OCB 0), stop here.
    -- Dragon behaves like a normal enemy; engine handles death at 0 HP.
    
    if not cutsceneEnabled then
        return
    end
    
    -- Cutscene gating
    
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
        time         = 0

        return
    end
    
    -- Dagger‑pull cutscene
    
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

--- @section Callbacks
--- Callback Registration

LevelFuncs.DragonCutscene_Init = function()
    TR2_DRAGON_Cutscene.Init()
end

LevelFuncs.DragonCutscene_Update = function()
    TR2_DRAGON_Cutscene.Update()
end

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRE_START,  LevelFuncs.DragonCutscene_Init)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.POST_LOOP,  LevelFuncs.DragonCutscene_Update)
TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.POST_LOAD,  LevelFuncs.DragonCutscene_Init)

--- @section Return
--- Return Module Table

return TR2_DRAGON_Cutscene
