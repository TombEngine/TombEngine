--- Internal file used by the RingInventory module.
-- @module RingInventory.Text
-- @local

--External Modules
local Constants = require("Engine.RingInventory.Constants")
local Settings = require("Engine.RingInventory.Settings")
local Utilities = require("Engine.RingInventory.Utilities")

--Pointer to tables
local COLOR_MAP = Settings.ColorMap
local PICKUP_DATA = require("Engine.RingInventory.PickupData")

-- ============================================================================
-- TEXT CHANNEL SYSTEM
-- ============================================================================

local TextChannels = {}
local TextChannelStates = {}

TextChannels.TRANSITION =
{
    NONE = "none",
    CROSSFADE = "crossfade",
    SWIPE_LEFT = "swipe_left",
    SWIPE_RIGHT = "swipe_right"
}

-- Configuration
local TEXT_CONFIG =
{
    FADE_SPEED = Settings.Animation.transitionSpeed,
    SWIPE_DISTANCE = 2,
    CROSSFADE_DURATION_MULTIPLIER = 2,
    CROSSFADE_BLEND_START = 0.45,
    CROSSFADE_BLEND_END = 0.65,
    CONTROL_HINT_PADDING = 3,
    MIN_ALPHA = Constants.ALPHA_MIN,
    MAX_ALPHA = Constants.ALPHA_MAX
}

local function SetTransitionType(transitionType)
    if transitionType == TextChannels.TRANSITION.NONE or 
       transitionType == TextChannels.TRANSITION.SWIPE_LEFT or 
       transitionType == TextChannels.TRANSITION.SWIPE_RIGHT then
        return transitionType
    end

    return TextChannels.TRANSITION.CROSSFADE
end

local function GetTransitionPosition(position, transitionType, progress, isNext)
    local direction = 0

    if transitionType == TextChannels.TRANSITION.SWIPE_LEFT then
        direction = -1
    elseif transitionType == TextChannels.TRANSITION.SWIPE_RIGHT then
        direction = 1
    end

    if direction == 0 then
        return position
    end

    local offsetX
    if isNext then
        offsetX = -direction * TEXT_CONFIG.SWIPE_DISTANCE * (1 - progress)
    else
        offsetX = direction * TEXT_CONFIG.SWIPE_DISTANCE * progress
    end

    return TEN.Vec2(position.x + offsetX, position.y)
end

local function GetCrossfadeAlpha(progress, isNext)
    local blendStart = TEXT_CONFIG.CROSSFADE_BLEND_START
    local blendEnd = TEXT_CONFIG.CROSSFADE_BLEND_END

    if isNext then
        if progress <= blendStart then
            return TEXT_CONFIG.MIN_ALPHA
        end

        local fadeInProgress = math.min((progress - blendStart) / (1 - blendStart), 1)
        return TEXT_CONFIG.MAX_ALPHA * fadeInProgress
    end

    if progress >= blendEnd then
        return TEXT_CONFIG.MIN_ALPHA
    end

    local fadeOutProgress = math.min(progress / blendEnd, 1)
    return TEXT_CONFIG.MAX_ALPHA * (1 - fadeOutProgress)
end

-- ============================================================================
-- TEXT CHANNEL STRUCTURE
-- ============================================================================

--[[
    Each channel has:
    {
        name = "header",                          -- Unique identifier
        text = "actions_inventory",               -- Current display text
        position = Vec2(50, 4),                   -- Screen position (percentage)
        scale = 1.5,                              -- Text scale
        color = Color(255, 255, 255, 255),        -- Base color
        visible = true,                           -- Visibility flag
        flags = {                                 -- Display flags
            Strings.DisplayStringOption.CENTER,
            Strings.DisplayStringOption.SHADOW
        },
        translate = true,                         -- Use GetString() for localization
        fadeSpeed = nil                           -- Optional: override global fade speed
        -- Callbacks (optional)
        onTransitionComplete = config.onTransitionComplete,
        onShow = config.onShow,
        onHide = config.onHide
    }
]]

-- ============================================================================
-- INITIALIZE TEXT CHANNEL
-- ============================================================================

function TextChannels.Create(config)
    if not config.name then
        error("TextChannel requires a name")
    end

    -- Initialize channel state
    TextChannelStates[config.name] = {
        -- Configuration (immutable during lifetime)
        position = config.position or TEN.Vec2(50, 50),
        scale = config.scale or 1.0,
        color = config.color or TEN.Color(255, 255, 255, 255),
        flags = config.flags or {TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW},
        translate = config.translate ~= false,  -- Default true
        fadeSpeed = config.fadeSpeed or TEXT_CONFIG.FADE_SPEED,
        transitionType = SetTransitionType(config.transitionType),
        
        -- Current state (mutable)
        currentText = config.text or "",
        nextText = config.text or "",
        currentAlpha = config.visible and TEXT_CONFIG.MAX_ALPHA or TEXT_CONFIG.MIN_ALPHA,
        nextAlpha = TEXT_CONFIG.MIN_ALPHA,
        visible = config.visible or false,
        isTransitioning = false,
        activeTransition = SetTransitionType(config.transitionType),
        transitionProgress = 0,
    }
    
    return config.name
end

-- ============================================================================
-- UPDATE TEXT CHANNEL
-- ============================================================================

function TextChannels.SetText(channelName, newText, shouldShow, transitionType)
    local state = TextChannelStates[channelName]
    if not state then
        print("WARNING: Text channel '" .. channelName .. "' does not exist")
        return
    end

    transitionType = SetTransitionType(transitionType or state.transitionType)
    
     -- Normalize text (treat nil as empty string)
    newText = newText or ""
    
    -- Check if visibility is changing
    local visibilityChanged = (shouldShow ~= nil and shouldShow ~= state.visible)
    
    -- Check if text is actually changing (compare against BOTH current AND next)
    local textChanged = (newText ~= "" and newText ~= state.currentText and newText ~= state.nextText)

    if transitionType == TextChannels.TRANSITION.NONE then
        local resolvedVisible = (shouldShow ~= nil) and shouldShow or state.visible

        if resolvedVisible ~= state.visible then
            if resolvedVisible and state.onShow then
                state.onShow()
            elseif not resolvedVisible and state.onHide then
                state.onHide()
            end
        end

        state.visible = resolvedVisible
        state.currentText = newText
        state.nextText = newText
        state.isTransitioning = false
        state.activeTransition = transitionType
        state.transitionProgress = 0
        state.currentAlpha = state.visible and TEXT_CONFIG.MAX_ALPHA or TEXT_CONFIG.MIN_ALPHA
        state.nextAlpha = TEXT_CONFIG.MIN_ALPHA
        return
    end

    --check to prevent restarting mid-transition
    if state.isTransitioning and newText == state.nextText and not visibilityChanged and transitionType == state.activeTransition then
        return
    end
    
    -- Only update if something actually changed
    if not visibilityChanged and not textChanged then
        -- Nothing changed, don't restart transition
        return
    end
    
    -- Handle visibility change
    if visibilityChanged then
        state.visible = shouldShow
        
        if shouldShow and state.onShow then
            state.onShow()
        elseif not shouldShow and state.onHide then
            state.onHide()
        end

        if not shouldShow and state.currentText ~= "" then
            state.nextText = newText
            state.isTransitioning = true
            state.activeTransition = transitionType
            state.transitionProgress = 0
            state.nextAlpha = TEXT_CONFIG.MIN_ALPHA
        end
    end
    
    -- Handle text change
    if textChanged then
        -- Text is actually different, start crossfade
        state.nextText = newText
        state.isTransitioning = true
        state.activeTransition = transitionType
        state.transitionProgress = 0
        state.nextAlpha = TEXT_CONFIG.MIN_ALPHA
    end
end

function TextChannels.Show(channelName)
    local state = TextChannelStates[channelName]
    if state then
        state.visible = true
        
        if state.onShow then
            state.onShow()
        end
    end
end

function TextChannels.Hide(channelName)
    local state = TextChannelStates[channelName]
    if state then
        state.visible = false
        
        if state.onHide then
            state.onHide()
        end
        
    end
end

function TextChannels.SetPosition(channelName, position)
    local state = TextChannelStates[channelName]
    if state then
        state.position = position
    end
end

function TextChannels.SetScale(channelName, scale)
    local state = TextChannelStates[channelName]
    if state then
        state.scale = scale
    end
end

function TextChannels.SetColor(channelName, color)
    local state = TextChannelStates[channelName]
    if state then
        state.color = color
    end
end

function TextChannels.SetTranslate(channelName, translate)
    local state = TextChannelStates[channelName]
    if state then
        state.translate = translate
    end
end

-- ============================================================================
-- UPDATE ALL CHANNELS
-- ============================================================================

function TextChannels.Update()
    for channelName, state in pairs(TextChannelStates) do
        if state.isTransitioning then
            local progressStep = state.fadeSpeed / TEXT_CONFIG.MAX_ALPHA
            if state.activeTransition == TextChannels.TRANSITION.CROSSFADE then
                progressStep = progressStep / TEXT_CONFIG.CROSSFADE_DURATION_MULTIPLIER
            end
            state.transitionProgress = math.min(state.transitionProgress + progressStep, 1)

            if state.visible then
                if state.activeTransition == TextChannels.TRANSITION.CROSSFADE then
                    state.currentAlpha = GetCrossfadeAlpha(state.transitionProgress, false)
                    state.nextAlpha = GetCrossfadeAlpha(state.transitionProgress, true)
                else
                    state.currentAlpha = math.max(TEXT_CONFIG.MAX_ALPHA * (1 - state.transitionProgress), TEXT_CONFIG.MIN_ALPHA)
                    state.nextAlpha = math.min(TEXT_CONFIG.MAX_ALPHA * state.transitionProgress, TEXT_CONFIG.MAX_ALPHA)
                end
            else
                state.currentAlpha = math.max(TEXT_CONFIG.MAX_ALPHA * (1 - state.transitionProgress), TEXT_CONFIG.MIN_ALPHA)
                state.nextAlpha = TEXT_CONFIG.MIN_ALPHA
            end

            if state.currentAlpha <= TEXT_CONFIG.MIN_ALPHA and state.nextAlpha >= TEXT_CONFIG.MAX_ALPHA then
                -- Normal completion
                state.currentText = state.nextText
                state.currentAlpha = state.visible and TEXT_CONFIG.MAX_ALPHA or TEXT_CONFIG.MIN_ALPHA
                state.nextAlpha = TEXT_CONFIG.MIN_ALPHA
                state.isTransitioning = false
                state.transitionProgress = 0
                
                if state.onTransitionComplete then
                    state.onTransitionComplete(state.currentText)
                end
            elseif state.currentAlpha <= TEXT_CONFIG.MIN_ALPHA and not state.visible then
                -- Stuck fading out while invisible - force complete
                state.currentText = state.nextText
                state.currentAlpha = TEXT_CONFIG.MIN_ALPHA
                state.nextAlpha = TEXT_CONFIG.MIN_ALPHA
                state.isTransitioning = false
                state.transitionProgress = 0
            end
        else
            -- Not transitioning, just handle visibility fade
            if state.visible then
                state.currentAlpha = math.min(state.currentAlpha + state.fadeSpeed, TEXT_CONFIG.MAX_ALPHA)
            else
                state.currentAlpha = math.max(state.currentAlpha - state.fadeSpeed, TEXT_CONFIG.MIN_ALPHA)
            end
        end
    end
end

-- ============================================================================
-- RENDER CHANNEL
-- ============================================================================

function TextChannels.Draw(channelName)
    local state = TextChannelStates[channelName]
    if not state then return end
  
    -- Skip if completely invisible
    if state.currentAlpha <= 0 and state.nextAlpha <= 0 then
        return
    end
    
    local currentPosition = TEN.Util.PercentToScreen(GetTransitionPosition(state.position, state.activeTransition, state.transitionProgress, false))
    local nextPosition = TEN.Util.PercentToScreen(GetTransitionPosition(state.position, state.activeTransition, state.transitionProgress, true))
    
    -- Draw current text (fading out)
    if state.currentAlpha > 0 and state.currentText ~= "" then
        local textObj = TEN.Strings.DisplayString(
            state.currentText,
            currentPosition,
            state.scale,
            Utilities.ColorCombine(state.color, state.currentAlpha),
            state.translate,
            state.flags
        )
        TEN.Strings.ShowString(textObj, 1 / 30)
    end
    
    -- Draw next text (fading in) - only during transition
    if state.isTransitioning and state.nextAlpha > 0 and state.nextText ~= "" then
        local textObj = TEN.Strings.DisplayString(
            state.nextText,
            nextPosition,
            state.scale,
            Utilities.ColorCombine(state.color, state.nextAlpha),
            state.translate,
            state.flags)
        TEN.Strings.ShowString(textObj, 1 / 30)
    end
end

-- ============================================================================
-- RENDER ALL VISIBLE CHANNELS
-- ============================================================================

function TextChannels.DrawAll()
    for channelName, state in pairs(TextChannelStates) do
        if state.visible or state.currentAlpha > 0 or state.isTransitioning then
            TextChannels.Draw(channelName)
        end
    end
end

-- ============================================================================
-- QUERY FUNCTIONS
-- ============================================================================

function TextChannels.IsTransitioning(channelName)
    local state = TextChannelStates[channelName]
    return state and state.isTransitioning or false
end

function TextChannels.GetText(channelName)
    local state = TextChannelStates[channelName]
    return state and state.currentText or nil
end

function TextChannels.IsVisible(channelName)
    local state = TextChannelStates[channelName]
    return state and state.visible or false
end

function TextChannels.GetAlpha(channelName)
    local state = TextChannelStates[channelName]
    return state and state.currentAlpha or 0
end

-- ============================================================================
-- CLEANUP
-- ============================================================================

function TextChannels.Destroy(channelName)
    TextChannelStates[channelName] = nil
end

function TextChannels.DestroyAll()
    TextChannelStates = {}
end

-- ============================================================================
-- ADVANCED: CHANNEL GROUPS
-- ============================================================================

local TextChannelGroups = {}

function TextChannels.ShowGroup(groupName)
    local group = TextChannelGroups[groupName]
    if not group then return end
    
    for _, channelName in ipairs(group) do
        TextChannels.Show(channelName)
    end
end

function TextChannels.HideGroup(groupName)
    local group = TextChannelGroups[groupName]
    if not group then return end
    
    for _, channelName in ipairs(group) do
        TextChannels.Hide(channelName)
    end
end

function TextChannels.DrawGroup(groupName)
    local group = TextChannelGroups[groupName]
    if not group then return end
    
    for _, channelName in ipairs(group) do
        TextChannels.Draw(channelName)
    end
end

function TextChannels.AddToGroup(groupName, channelName)
    if not groupName or not channelName then
        return
    end

    -- Create group if it doesn't exist
    local group = TextChannelGroups[groupName]
    if not group then
        group = {}
        TextChannelGroups[groupName] = group
    end

    -- Prevent duplicates
    for _, existingChannel in ipairs(group) do
        if existingChannel == channelName then
            return
        end
    end

    table.insert(group, channelName)
end

function TextChannels.RemoveFromGroup(groupName, channelName)
    local group = TextChannelGroups[groupName]
    if not group then
        return
    end

    for i = #group, 1, -1 do
        if group[i] == channelName then
            table.remove(group, i)
            break
        end
    end

    -- Optional: clean up empty groups
    if #group == 0 then
        TextChannelGroups[groupName] = nil
    end
end

-- ============================================================================
-- TEXT SETUP
-- ============================================================================
TextChannels.CONFIGS =
{
    HEADER = 
    {
        name = "HEADER",              -- Unique identifier
        text = "",                    -- Current display text
        position = TEN.Vec2(50, 4),   -- Screen position (percentage)
        scale = 1.5,                  -- Text scale
        color = COLOR_MAP.headerText, -- Base color
        visible = false,              -- Visibility flag
        translate = true,             -- Use GetString() for localization
        flags =                       -- Display flags
        {
            TEN.Strings.DisplayStringOption.CENTER,
            TEN.Strings.DisplayStringOption.SHADOW
        }
    },
    SUB_HEADER = 
    {
        name = "SUB_HEADER",
        text = "",
        position = TEN.Vec2(50, 45),
        scale = 0.9,
        color = COLOR_MAP.headerText,
        visible = false,
        translate = true,
        flags = 
        {                                
            TEN.Strings.DisplayStringOption.CENTER,
            TEN.Strings.DisplayStringOption.SHADOW
        }
    },
    ITEM_LABEL_PRIMARY = 
    {
        name = "ITEM_LABEL_PRIMARY",
        text = "",
        position = TEN.Vec2(50, 77),
        scale = 1.4,
        color = COLOR_MAP.plainText,
        visible = false,
        translate = false,
        flags = 
        {                                
            TEN.Strings.DisplayStringOption.CENTER,
            TEN.Strings.DisplayStringOption.SHADOW
        }
    },
    ITEM_LABEL_SECONDARY = 
    {
        name = "ITEM_LABEL_SECONDARY",
        text = "",
        position = TEN.Vec2(50, 85),
        scale = 1,
        color = COLOR_MAP.plainText,
        visible = false,
        translate = false,
        flags = 
        {
            TEN.Strings.DisplayStringOption.CENTER,
            TEN.Strings.DisplayStringOption.SHADOW
        }
    },
    CONTROLS_SELECT = 
    {
        name = "CONTROLS_SELECT",
        text = "",
        position = TEN.Vec2(TEXT_CONFIG.CONTROL_HINT_PADDING, 100 - TEXT_CONFIG.CONTROL_HINT_PADDING),
        scale = 0.7,
        color = COLOR_MAP.plainText,
        visible = false,
        translate = false,
        flags = 
        {
            TEN.Strings.DisplayStringOption.SHADOW,
            TEN.Strings.DisplayStringOption.VERTICAL_BOTTOM
        }
    },
    CONTROLS_BACK = 
    {
        name = "CONTROLS_BACK",
        text =  "",
        position = TEN.Vec2(100 - TEXT_CONFIG.CONTROL_HINT_PADDING, 100 - TEXT_CONFIG.CONTROL_HINT_PADDING),
        scale = 0.7,
        color = COLOR_MAP.plainText,
        visible = false,
        translate = false,
        flags = 
        {
            TEN.Strings.DisplayStringOption.RIGHT,
            TEN.Strings.DisplayStringOption.SHADOW,
            TEN.Strings.DisplayStringOption.VERTICAL_BOTTOM
        }
    }
}

function TextChannels.Setup()
    TextChannels.Create(TextChannels.CONFIGS.HEADER)
    TextChannels.Create(TextChannels.CONFIGS.SUB_HEADER)
    TextChannels.Create(TextChannels.CONFIGS.ITEM_LABEL_PRIMARY)
    TextChannels.Create(TextChannels.CONFIGS.ITEM_LABEL_SECONDARY)
    TextChannels.Create(TextChannels.CONFIGS.CONTROLS_SELECT)
    TextChannels.Create(TextChannels.CONFIGS.CONTROLS_BACK)
    TextChannels.AddToGroup("INVENTORY_UI", "HEADER")
    TextChannels.AddToGroup("INVENTORY_UI", "SUB_HEADER")
    TextChannels.AddToGroup("INVENTORY_UI", "ITEM_LABEL_PRIMARY")
    TextChannels.AddToGroup("INVENTORY_UI", "ITEM_LABEL_SECONDARY")
    TextChannels.AddToGroup("INVENTORY_UI", "CONTROLS_SELECT")
    TextChannels.AddToGroup("INVENTORY_UI", "CONTROLS_BACK")
end


function TextChannels.CreateItemLabel(item)
    if item then
        local label = TEN.Flow.GetString(item.name)
        local count = item.count
        local result = ""
        local type = ""
    
        if count == -1 then
            result = TEN.Flow.GetString("unlimited"):gsub(" ", ""):gsub("%%s", "").." "
        elseif count > 1 or item.type == PICKUP_DATA.TYPE.AMMO or item.type == PICKUP_DATA.TYPE.MEDIPACK then
            result = tostring(count).." x "
        end
        
        if item:GetObjectID() == TEN.Objects.ObjID.HK_ITEM then
            local modeIndex = Lara:GetWeaponMode(TEN.Objects.WeaponType.HK)
            local modeString = PICKUP_DATA.HK_MODES[modeIndex]
            type = " ("..TEN.Flow.GetString(modeString)..")"
        end

        local string = result..label..type

        return string
    end

    return ""
end

function TextChannels.SetItemLabel(item, transitionType)
    local text = TextChannels.CreateItemLabel(item)
    TextChannels.SetText("ITEM_LABEL_PRIMARY", text, true, transitionType)
end

function TextChannels.SetItemSubLabel(item)
    local text = TextChannels.CreateItemLabel(item)
    TextChannels.SetText("ITEM_LABEL_SECONDARY", text, true, TextChannels.TRANSITION.CROSSFADE)
end

function TextChannels.HideItemSubLabel()
    TextChannels.SetText("ITEM_LABEL_SECONDARY", "", false, TextChannels.TRANSITION.CROSSFADE)
end

return TextChannels