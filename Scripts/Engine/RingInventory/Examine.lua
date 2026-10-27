--- Internal file used by the RingInventory module.
-- @module RingInventory.Examine
-- @local

-- ============================================================================
-- Examine - Handles examine functions and data for ring inventory
-- ============================================================================
--External Modules
local Constants = require("Engine.RingInventory.Constants")
local Interpolate = require("Engine.RingInventory.Interpolate")
local Settings = require("Engine.RingInventory.Settings")
local Text = require("Engine.RingInventory.Text")
local Utilities = require("Engine.RingInventory.Utilities")

--Pointer to tables
local COLOR_MAP = Settings.ColorMap

--Examine functions
local Examine = {}

local EXAMINE_POSITION = Vec3(0, 50, 0)
local EXAMINE_DEFAULT_SCALE = 1
local EXAMINE_MIN_SCALE = 0.3
local EXAMINE_MAX_SCALE = 1.6
local EXAMINE_TEXT_POS = Vec2(50, 80)
local ROTATION_MULTIPLIER = 2
local ZOOM_MULTIPLIER = 0.15
local ROTATION_SMOOTHING = 0.35
local ROTATION_SNAP_THRESHOLD = 0.05
local SCALE_SNAP_THRESHOLD = 0.01

local examineRotation = Rotation(0, 0, 0)
local examineTargetRotation = Rotation(0, 0, 0)
local examineScaler = EXAMINE_DEFAULT_SCALE
local examineTargetScale = EXAMINE_DEFAULT_SCALE
local examineShowString = false
local examineStringPresent = false
local alpha = 0
local targetAlpha = 0

Examine.item = nil

local EXAMINE_TEXT = 
{
    name = "EXAMINE_TEXT",                 
    text = "",               
    position = EXAMINE_TEXT_POS,                   
    scale = 1,                             
    color = COLOR_MAP.plainText,        
    visible = false,                           
    flags = 
    {
        Strings.DisplayStringOption.VERTICAL_CENTER,
        Strings.DisplayStringOption.CENTER,
        Strings.DisplayStringOption.SHADOW
    },
    translate = false
}

local EXAMINE_CONTROLS = 
{
    name = "EXAMINE_CONTROLS",                 
    text = "",               
    position = Vec2(3, 80),              
    scale = 0.7,                          
    color = COLOR_MAP.plainText,    
    visible = false,                     
    flags = 
    {
        Strings.DisplayStringOption.SHADOW
    },
    translate = false
}

local function StepRotationAxis(current, target)
    local normalizedCurrent = Utilities.NormalizeAngle(current)
    local normalizedTarget = Utilities.NormalizeAngle(target)
    local delta = Utilities.GetShortestAngleDelta(normalizedCurrent, normalizedTarget)

    if math.abs(delta) <= ROTATION_SNAP_THRESHOLD then
        return normalizedTarget
    end

    local step = delta * Interpolate.Easing.Softstep(ROTATION_SMOOTHING)
    return Utilities.NormalizeAngle(normalizedCurrent + step)
end

local function StepScale(current, target)
    local delta = target - current

    if math.abs(delta) <= SCALE_SNAP_THRESHOLD then
        return target
    end

    return current + delta * Interpolate.Easing.Softstep(ROTATION_SMOOTHING)
end

local function ExamineLabel(showText)
    local string = ""

    if showText then
        string = string..Input.GetActionBinding(ActionID.ACTION)..": "..Flow.GetString("toggle_text")
    end

    string = string.."\n"..

    Input.GetActionBinding(ActionID.JUMP)..": "..Flow.GetString("reset").."\n"..
    Input.GetActionBinding(ActionID.SPRINT)..": "..Flow.GetString("zoom_in").."\n"..
    Input.GetActionBinding(ActionID.CROUCH)..": "..Flow.GetString("zoom_out")

    return string
end

function Examine.SetupText(itemData)
    local item = itemData:GetObjectID()

    local objectName = Objects.GetSlotName(item)
    local stringKey = objectName:lower().."_text"
    local localizedString = Flow.IsStringPresent(stringKey) and Flow.GetString(stringKey) or nil
    examineStringPresent = false

    if localizedString then
        examineShowString = true
        Text.Create(EXAMINE_TEXT)
        Text.SetText("EXAMINE_TEXT", localizedString, true)
        examineStringPresent = true
    end

    Text.Create(EXAMINE_CONTROLS)
    Text.SetText("EXAMINE_CONTROLS", ExamineLabel(examineShowString), true)
end

function Examine.ToggleText()
    
    if not examineStringPresent then
        return
    end
    
    examineShowString = not examineShowString
    TEN.Sound.PlaySound(Settings.SoundMap.menuChoose)
    if examineShowString then
        Text.Show("EXAMINE_TEXT")
    else
        Text.Hide("EXAMINE_TEXT")
    end
end

function Examine.ModifyRotation(dirX, dirY, dirZ)
    examineTargetRotation.x = Utilities.NormalizeAngle(examineTargetRotation.x + dirX * ROTATION_MULTIPLIER)
    examineTargetRotation.y = Utilities.NormalizeAngle(examineTargetRotation.y + dirY * ROTATION_MULTIPLIER)
    examineTargetRotation.z = Utilities.NormalizeAngle(examineTargetRotation.z + dirZ * ROTATION_MULTIPLIER)

end

function Examine.ModifyScale(dir)
    examineTargetScale = examineTargetScale + dir * ZOOM_MULTIPLIER
end

function Examine.SetRotation(rotation)
    examineRotation = Utilities.NormalizeRotation(rotation)
    examineTargetRotation = Utilities.NormalizeRotation(rotation)
end

function Examine.SetScale(scaleValue)
    local clampedScale = Utilities.Clamp(scaleValue, EXAMINE_MIN_SCALE, EXAMINE_MAX_SCALE)
    examineScaler = clampedScale
    examineTargetScale = clampedScale
end

function Examine.ResetExamine(item)
    Examine.SetRotation(item:GetRotation())
    Examine.SetScale(item:GetScale())
end

function Examine.Show(item)
    if not item then return end

    Examine.ResetExamine(item)
    Examine.SetupText(item)
    targetAlpha = 255
    Examine.item = TEN.View.DisplayItem(item:GetObjectID(), Utilities.OffsetY(EXAMINE_POSITION, item:GetYOffset()), examineRotation, Vec3(examineScaler), item:GetMeshBits())
end

function Examine.Draw()
    if not Examine.item then return end

    examineScaler = Utilities.Clamp(examineScaler, EXAMINE_MIN_SCALE, EXAMINE_MAX_SCALE)
    local color = Examine.item:GetColor()
    Examine.item:SetRotation(examineRotation)
    Examine.item:SetScale(Vec3(examineScaler))
    Examine.item:SetColor(Utilities.ColorCombine(color, alpha))
    Examine.item:Draw()
end

function Examine.Hide()
    Text.Hide("EXAMINE_CONTROLS")
    Text.Hide("EXAMINE_TEXT")
    examineShowString = false
    targetAlpha = 0
end

function Examine.Clear()
    Examine.item = nil
    Text.Destroy("EXAMINE_TEXT")
    Text.Destroy("EXAMINE_CONTROLS")
end

function Examine.Update()
    if not Examine.item then return end

    examineRotation = Rotation(
        StepRotationAxis(examineRotation.x, examineTargetRotation.x),
        StepRotationAxis(examineRotation.y, examineTargetRotation.y),
        StepRotationAxis(examineRotation.z, examineTargetRotation.z))

    examineTargetScale = Utilities.Clamp(examineTargetScale, EXAMINE_MIN_SCALE, EXAMINE_MAX_SCALE)
    examineScaler = StepScale(examineScaler, examineTargetScale)

    local color = Examine.item:GetColor()
    alpha = Interpolate.StepAlpha(alpha, targetAlpha, Settings.Animation.transitionSpeed)
    local targetColor = Utilities.ColorCombine(color, alpha)
    Examine.item:SetColor(targetColor)

    if alpha == 0 then
        Examine.item = nil
        Text.Destroy("EXAMINE_TEXT")
        Text.Destroy("EXAMINE_CONTROLS")
    end
end

return Examine