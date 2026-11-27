-- NOTE: This module will be rewritten at some point for clean code purposes. It is completely functional despite its current flaws.


local MPUI = {}

LevelFuncs.Engine = LevelFuncs.Engine or {}
LevelFuncs.Engine.MPUI = {}


local activeNotifs = {}


local joinLeaveQueue  = {}
local joinLeaveDS     = nil
local joinLeaveFrames = 0


local JL_MODE_TOP  = 1
local JL_MODE_CHAT = 2
local JL_MODE_OFF  = 3
local joinLeaveMode = JL_MODE_TOP

MPUI.JL_MODE_TOP  = JL_MODE_TOP
MPUI.JL_MODE_CHAT = JL_MODE_CHAT
MPUI.JL_MODE_OFF  = JL_MODE_OFF

local DEFAULT_SCALE = 0.55
local DEFAULT_POS_X = 50
local DEFAULT_POS_Y = 8
local DEFAULT_COLOR = Color(255, 255, 255)


local CHAT_SCALE        = 0.432
local CHAT_INPUT_SCALE  = 0.62
local CHAT_X            = 3
local CHAT_INPUT_X      = 50
local CHAT_INPUT_Y      = 93
local CHAT_TAB_HINT_Y   = CHAT_INPUT_Y - 4.0
local CHAT_MSG_BASE_Y   = 85
local CHAT_MSG_SPACING  = 3.5

local CHAT_MAX_MESSAGES  = 7
local CHAT_MSG_TTL       = 480
local CHAT_MAX_LENGTH    = 100

local CHAT_HEADER_Y = CHAT_MSG_BASE_Y - CHAT_MAX_MESSAGES * CHAT_MSG_SPACING

local CHAT_INPUT_COLOR        = Color(255, 220, 80)
local CHAT_HEADER_COLOR       = Color(255, 220, 80)
local CHAT_INPUT_GLOBAL_COLOR = Color(255, 140, 0)
local CHAT_OWN_COLOR          = Color(150, 255, 150)
local CHAT_OWN_GLOBAL_COLOR   = Color(255, 180, 80)
local CHAT_REMOTE_COLOR       = Color(255, 255, 255)
local CHAT_REMOTE_GLOBAL_COLOR= Color(255, 180, 80)


local MENU_BTN_X        = 97
local LOBBY_BTN_Y       = 3
local MENU_BTN_SCALE    = 0.42
local MENU_BTN_COLOR    = Color(200, 200, 200)
local MENU_BTN_HINT_Y   = 0.5
local MENU_BTN_HINT_SCALE = 0.35
local MENU_BTN_HINT_COLOR = Color(140, 140, 140)

local MENU_AUTO_CLOSE_FRAMES  = 600
local MENU_HIDE_START_FRAME   = 20
local MENU_HIDE_WARN_FRAMES   = 110
local MENU_ITEM_COLOR = Color(220, 220, 220)

local MENU_HIGHLIGHT_COLOR = Color(255, 220, 80)
local MENU_SCALE           = 0.55
local MENU_CHAT_HINT_SCALE = MENU_SCALE * 0.70
local CHAT_TAB_HINT_SCALE = MENU_SCALE * 0.7

local NAME_INPUT_X      = 50
local NAME_INPUT_Y      = 88
local NAME_INPUT_SCALE  = 0.62
local NAME_INPUT_COLOR  = Color(100, 200, 255)
local NAME_MAX_LENGTH   = 20


local LOBBY_CONFIRM_Y1  = 47
local LOBBY_CONFIRM_Y2  = 53
local LOBBY_CONFIRM_SCALE = 0.55





local uiFrame = 0


local LINE_HEIGHT_PCT = 35 / 600 * 100

local function clamp01(t)
    if t < 0 then return 0 end
    if t > 1 then return 1 end
    return t
end

local function lerp(a, b, t)
    return a + (b - a) * t
end


local function easeOutQuad(t)
    t = clamp01(t)
    return 1 - (1 - t) * (1 - t)
end


local function approach(current, target, fraction, epsilon)
    local nxt = current + (target - current) * fraction
    if math.abs(target - nxt) <= (epsilon or 0.05) then
        return target
    end
    return nxt
end


local function tweenProgress(startFrame, durationFrames)
    if durationFrames <= 0 then return 1 end
    return clamp01((uiFrame - startFrame) / durationFrames)
end


local function lineHitBandY(topY, scale)
    return topY, topY + LINE_HEIGHT_PCT * scale
end


local HS = {
    FLASH_FRAMES   = 8,
    HALF_WIDTH_PCT = 14,
    spots          = {},
    flash          = {},
    clicks         = {},
    clickConsumed  = false,
    enterConsumed  = false,
    leftConsumed   = false,
}


function HS.register(id, spot)
    spot.heldFrames = 0
    HS.spots[id] = spot
end

function HS.unregister(id)
    HS.spots[id] = nil
    HS.flash[id] = nil
end

function HS.hitTest(spot, mx, my)
    local bottomY = spot.y + LINE_HEIGHT_PCT * (spot.scale or DEFAULT_SCALE)
    if my < spot.y or my > bottomY then return false end
    local half = spot.halfWidth or HS.HALF_WIDTH_PCT
    if spot.align == "right" then
        return mx <= spot.x and mx >= spot.x - half * 2
    end
    return math.abs(mx - spot.x) <= half
end


function HS.pass(onlyIds)
    local mouse     = TEN.Input.GetMouseDisplayPosition()
    local clickHit  = KeyIsHit(ActionID.MOUSE_CLICK_LEFT)
    local clickHeld = KeyIsHeld(ActionID.MOUSE_CLICK_LEFT)
    for id, spot in pairs(HS.spots) do
        if onlyIds == nil or onlyIds[id] then
        if spot.flashCheck and spot.flashCheck() then
            HS.flash[id] = HS.FLASH_FRAMES
        end
        local hovered = HS.hitTest(spot, mouse.x, mouse.y)
        spot.isHovered = hovered
        if hovered and clickHit then
            HS.clicks[id]    = true
            HS.clickConsumed = true
            if spot.onClick then spot.onClick() end
        end
        if spot.holdable then
            spot.heldFrames = (hovered and clickHeld) and (spot.heldFrames + 1) or 0
        end
        local flash = HS.flash[id] or 0
        if spot.ds then
            local color = (hovered or flash > 0) and MENU_HIGHLIGHT_COLOR or (spot.baseColor or MENU_ITEM_COLOR)
            pcall(function() spot.ds:SetColor(color) end)
        end
        if flash > 0 then HS.flash[id] = flash - 1 end
        end
    end
end


function MPUI.UIClicked(id)          return HS.clicks[id] == true end
function MPUI.WasClickConsumedByUI() return HS.clickConsumed end
function MPUI.WasEnterConsumedByUI() return HS.enterConsumed end


local function newItemState()
    return {
        currentX   = 0, currentY = 0,
        targetX    = 0, targetY  = 0,
        offsetX    = 0, offsetY  = 0,
        spawnFrame = 0,
        whipStartX = 0,
        isHovered  = false,
        isSelected = false,
        ds         = nil,
        subDs      = nil,
        valDs      = nil,
    }
end






local MENU_ROW_START_Y = 29.0
local MENU_ROW_STEP_Y  = 6.5
local MENU_SUBHINT_DY  = 3.5

local WHIP_DURATION_FRAMES = 12
local WHIP_STAGGER_FRAMES  = 3
local WHIP_START_LEFT_X    = -25.0
local WHIP_START_RIGHT_X   = 125.0





local MENU_BOARD = {
    SPRITE_ID    = 48,
    REST_X       = 18.0,
    Y            = 50.0,
    OFFSCREEN_X  = -40.0,
    SCALE        = Vec2(56.0, 56.0),
    SLIDE_FRAMES = WHIP_DURATION_FRAMES,
}


local CYCLE_BOARD = {
    SPRITE_ID    = 48,

REST_X       = 83.0,
    Y            = 50.0,
    OFFSCREEN_X  = 140.0,

SCALE        = Vec2(66.0, 56.0),
    SLIDE_FRAMES = WHIP_DURATION_FRAMES,

LABEL_DX     = 0.0,  LABEL_DY    = -3.0,
    PROGRESS_DX  = 0.0,  PROGRESS_DY = 0.5,

ROW_SCALE    = 0.50,

ROW_LABEL_DX = -11.0,
    ROW_VALUE_DX =   7.0,

BACK_Y       = 62.0,
}





local MENU_CURSOR = {
    SPRITE_ID = 50,
    SCALE     = Vec2(4.0, 4.0),

HOTSPOT   = Vec2(0.0, 2.0),
}

local DRIFT_AMP_PCT    = 0.05
local DRIFT_SPEED      = 0.02
local DRIFT_PHASE_STEP = 1.7


local FLOAT_AMP_PCT = 0.3
local FLOAT_SPEED_X = 0.031
local FLOAT_SPEED_Y = 0.047

local MOUSE_DELTA_MIN_PCT = 0.05


local MENUS = {}


MENUS._historyHintUseClick = false

MENUS.main = {
    id = "main",

columns = {
        board = MENU_BOARD.REST_X,
    },
    items = {

{ id = "social_settings", label = "my settings", hotkey = "X", column = "board",

action = { type = "submenu", menu = "social" } },

        { id = "teleport",        label = "teleport",        hotkey = "T", column = "board",
          action = { type = "command", command = "teleport" } },
        { id = "toggle_pvp",      label = "toggle pvp",      hotkey = "V", column = "board",
          action = { type = "command", command = "toggle_pvp" } },
        { id = "chat",            label = "chat",            hotkey = "C", column = "board",

          getSubhint = function()
              return MENUS._historyHintUseClick and "Hold [click] to see history"
                  or "Hold C to see history"
          end,
          action = { type = "command", command = "chat" } },
        { id = "interact",        label = "interact",        hotkey = "N", column = "board",
          action = { type = "submenu", menu = "interact" } },
    },
}


MENUS.interact = {
    id = "interact",
    columns = { board = MENU_BOARD.REST_X },
    items = {

{ id = "see_players", label = "see players", hotkey = nil, column = "board",
          action = { type = "submenu", menu = "players" } },
        { id = "back",    label = "back",    hotkey = nil, column = "board",
          action = { type = "back" } },
    },
}


MENUS.social = {
    id = "social",
    keepParentVisible = true,
    columns = { board = CYCLE_BOARD.REST_X },
    items = {},
    descriptors = {},
}


function MPUI.SetSocialSettings(list)
    MENUS.social.descriptors = list or {}
    local items = {}
    for i, desc in ipairs(MENUS.social.descriptors) do
        items[#items + 1] = {
            id            = "social_" .. i,
            label         = desc.name,
            getValue      = desc.getValue,
            getValueColor = desc.getValueColor,
            twoColumn     = true,
            column        = "board",
            action        = { type = "social_apply", index = i },
        }
    end

items[#items + 1] = { id = "change_name", label = "change name", column = "board",
                          twoColumn = true, getValue = function() return "" end,
                          action = { type = "command", command = "change_name" } }

items[#items + 1] = { id = "back", label = "back", column = "board",
                          bottomAnchor = true,
                          action = { type = "back" } }
    MENUS.social.items = items


if MENUS._evAlerts ~= nil then MPUI.SetEventAlertsToggle(MENUS._evAlerts) end
end


function MPUI.SetEventAlertsToggle(desc)
    MENUS._evAlerts = desc

for i = #MENUS.interact.items, 1, -1 do
        if MENUS.interact.items[i].id == "event_alerts" then table.remove(MENUS.interact.items, i) end
    end
    if desc == nil then return end
    for _, it in ipairs(MENUS.social.items) do
        if it.id == "event_alerts" then return end
    end

local on = function()
        if desc.getValue == nil then return true end
        local ok, v = pcall(desc.getValue)
        return (not ok) or (v and true or false)
    end
    local row = {
        id = "event_alerts", hotkey = nil, column = "board",
        label         = "event alerts",
        getValue      = function() return on() and "on" or "off" end,
        getValueColor = function()
            return on() and MPUI.CONFIRM_ON_COLOR or MPUI.CONFIRM_OFF_COLOR
        end,
        twoColumn     = true,
        action        = { type = "ev_toggle" },
    }

local at = #MENUS.social.items + 1
    for i, it in ipairs(MENUS.social.items) do
        if it.id == "change_name" or (it.action and it.action.type == "back") then at = i break end
    end
    table.insert(MENUS.social.items, at, row)
end


MENUS.players = {
    id = "players",
    keepParentVisible = true,
    columns = { board = CYCLE_BOARD.REST_X },
    items = {},
    rosterFn  = nil,
    refreshFn = nil,
    build = function()
        local self = MENUS.players

        if self.refreshFn then pcall(self.refreshFn) end
        local roster = (self.rosterFn and self.rosterFn()) or {}
        local items = {}
        for _, p in ipairs(roster) do
            local nm  = p.name or ("Player " .. tostring(p.id))
            if p.isSelf then nm = nm .. " (you)" end
            local lvl = p.levelName or p.levelLabel or "?"
            items[#items + 1] = {
                id        = "player_" .. tostring(p.id),
                label     = nm,
                getValue  = function() return lvl end,
                twoColumn = true,
                column    = "board",
                action    = nil,
            }
        end
        items[#items + 1] = { id = "refresh", label = "refresh", column = "board",
                              action = { type = "players_refresh" } }
        items[#items + 1] = { id = "back", label = "back", column = "board",
                              bottomAnchor = true,
                              action = { type = "back" } }
        self.items = items
    end,
}


function MPUI.SetPlayerRosterProvider(listFn, refreshFn)
    MENUS.players.rosterFn  = listFn
    MENUS.players.refreshFn = refreshFn
end


local RECPANEL = {
    TITLE_Y = 14, DY = 3.6,

    TITLE_SCALE = 0.6, SCALE = 0.45,
    TITLE_COLOR = Color(255, 220, 120),
    HEAD_COLOR  = Color(150, 200, 255),
    ROW_COLOR   = Color(220, 220, 220),
    REFRESH = 15,
    TITLE = "records",
    ALIGN_KEY = "name",
    EMPTY_TEXT = "none yet",
    provider = nil, title = nil,
    ds = {}, labels = {}, count = 0, frame = 0, open = false,
}


function MPUI.SetRecordsProvider(providerFn, title)
    RECPANEL.provider = providerFn
    RECPANEL.title    = title

RECPANEL.dirty    = true
end


local menuStack = {}


menuStack.INPUT_COOLDOWN = 3
menuStack.changedFrame   = -100

local function activeMenuEntry()
    return menuStack[#menuStack]
end


local function initMenuEntryLayout(entry)
    local rowsUsed = {}
    for i, item in ipairs(entry.def.items) do
        local st   = entry.itemStates[i]
        local colX = entry.def.columns[item.column] or MENU_BOARD.REST_X
        local row  = rowsUsed[item.column] or 0
        rowsUsed[item.column] = row + 1
        st.targetX = colX
        st.targetY = MENU_ROW_START_Y + row * MENU_ROW_STEP_Y

if item.bottomAnchor then st.targetY = CYCLE_BOARD.BACK_Y end

st.whipStartX = (colX > 50) and WHIP_START_RIGHT_X or WHIP_START_LEFT_X
        st.currentX   = st.whipStartX
        st.currentY   = st.targetY
        st.spawnFrame = entry.openedFrame + (i - 1) * WHIP_STAGGER_FRAMES
    end
end

local function pushMenu(menuId)
    local def = MENUS[menuId]
    if not def then
        print("MPUI: pushMenu ignored unknown menu id '" .. tostring(menuId) .. "'")
        return nil
    end

if def.build then pcall(def.build) end
    local states = {}
    for i = 1, #def.items do
        states[i] = newItemState()
    end
    local entry = {
        def           = def,
        itemStates    = states,
        selectedIndex = 1,
        hoverIndex    = nil,
        openedFrame   = uiFrame,
    }
    initMenuEntryLayout(entry)
    menuStack[#menuStack + 1] = entry
    menuStack.changedFrame = uiFrame
    return entry
end


local function popMenu()
    local entry = menuStack[#menuStack]
    if not entry then return nil end
    local poppedKeepParent = entry.def.keepParentVisible
    for _, st in ipairs(entry.itemStates) do
        if st.ds then
            pcall(function() TEN.Strings.HideString(st.ds) end)
            st.ds = nil
        end
        if st.subDs then
            pcall(function() TEN.Strings.HideString(st.subDs) end)
            st.subDs = nil
        end
        if st.valDs then
            pcall(function() TEN.Strings.HideString(st.valDs) end)
            st.valDs = nil
        end
    end
    menuStack[#menuStack] = nil
    menuStack.changedFrame = uiFrame

local parent = menuStack[#menuStack]
    if parent and not poppedKeepParent then
        for _, st in ipairs(parent.itemStates) do
            if st.ds then
                pcall(function() TEN.Strings.HideString(st.ds) end)
                st.ds = nil
            end
            if st.subDs then
                pcall(function() TEN.Strings.HideString(st.subDs) end)
                st.subDs = nil
            end
            if st.valDs then
                pcall(function() TEN.Strings.HideString(st.valDs) end)
                st.valDs = nil
            end
        end
        parent.openedFrame = uiFrame
        initMenuEntryLayout(parent)
    end
    return entry
end

local function clearMenuStack()
    while #menuStack > 0 do popMenu() end
end


local stagedMenuCommands = {}
local readyMenuCommands  = {}

local function queueMenuCommand(commandId)
    stagedMenuCommands[commandId] = true
end


function MPUI.ConsumeMenuCommand(commandId)
    if readyMenuCommands[commandId] then
        readyMenuCommands[commandId] = nil
        return true
    end
    return false
end

local chatOpen = false
local chatBuffer = ""
local chatInputDS = nil
local chatTabHintDS = nil
local chatJustOpened = false
local chatMode = "local"

local chatLog = {}

local chatOverlayActive = false
local chatOverlayDS     = {}
local chatCache         = {}
local chatCKeyFrames    = 0
local CHAT_C_HOLD_THRESHOLD = 12

local chatItemHeldFrames     = 0
local chatItemOverlayByMouse  = false

local pendingChatMsg = nil
local pendingChatGlobal = false

local getLocalNameFn = nil

local getConnectedFn = nil

local menuOpen        = false
local menuBoardProgress = 0
local cycleBoardProgress = 0

local xCycleUIActive  = false

local uiMouseMode     = false
local lastMouseX      = nil
local lastMouseY      = nil
local menuHintDS      = nil
local menuHoldHintDS  = nil
local menuIdleFrames  = 0
local menuHintVisible  = true
local menuHoldFrames   = 0
local menuHoldConsumed = false

local nameInputOpen       = false
local nameInputBuffer     = ""
local nameInputDS         = nil
local nameInputJustOpened = false
local pendingNameChange   = nil

local chatHeaderText = nil
local chatHeaderDS   = nil


local KEY_MAP = {}

for i = 0, 25 do
    local letter = string.char(65 + i)
    KEY_MAP[#KEY_MAP + 1] = { action = ActionID[letter], lower = string.char(97 + i), upper = letter }
end

local NUM_CHARS = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" }
local NUM_ACTIONS = { "NUM0", "NUM1", "NUM2", "NUM3", "NUM4", "NUM5", "NUM6", "NUM7", "NUM8", "NUM9" }
for i = 1, 10 do
    KEY_MAP[#KEY_MAP + 1] = { action = ActionID[NUM_ACTIONS[i]], lower = NUM_CHARS[i], upper = NUM_CHARS[i] }
end

local PUNCT_MAP = {
    { action = ActionID.SPACE,      lower = " ",  upper = " " },
    { action = ActionID.PERIOD,     lower = ".",  upper = "." },
    { action = ActionID.COMMA,      lower = ",",  upper = "," },
    { action = ActionID.MINUS,      lower = "-",  upper = "-" },
    { action = ActionID.SLASH,      lower = "?",  upper = "?" },
    { action = ActionID.APOSTROPHE, lower = "'",  upper = "'" },
    { action = ActionID.SEMICOLON,  lower = ";",  upper = ";" },
}
for _, p in ipairs(PUNCT_MAP) do
    KEY_MAP[#KEY_MAP + 1] = p
end


local azertyMode = false

local AZERTY_OVERRIDES = {

    { action = ActionID.Q,         lower = "a", upper = "A" },
    { action = ActionID.A,         lower = "q", upper = "Q" },
    { action = ActionID.W,         lower = "z", upper = "Z" },
    { action = ActionID.Z,         lower = "w", upper = "W" },

    { action = ActionID.SEMICOLON, lower = "m", upper = "M" },
    { action = ActionID.M,         lower = ",", upper = "?" },

    { action = ActionID.COMMA,     lower = ";", upper = "." },

    { action = ActionID.NUM4,      lower = "'", upper = "4" },
}
local AZERTY_REPLACED = {}
for _, e in ipairs(AZERTY_OVERRIDES) do AZERTY_REPLACED[e.action] = true end

local activeKeyMap = KEY_MAP

local function rebuildKeyMap()
    if not azertyMode then activeKeyMap = KEY_MAP; return end
    local t = {}
    for _, e in ipairs(AZERTY_OVERRIDES) do t[#t+1] = e end
    for _, e in ipairs(KEY_MAP) do
        if not AZERTY_REPLACED[e.action] then t[#t+1] = e end
    end
    activeKeyMap = t
end

function MPUI.SetAzertyMode(val)
    azertyMode = val
    rebuildKeyMap()
end

function MPUI.GetAzertyMode()
    return azertyMode
end


local GAMEPLAY_ACTIONS = {
    "WALK", "SPRINT", "CROUCH", "JUMP", "ROLL",
    "ACTION", "DRAW", "LOOK",
    "ACCELERATE", "REVERSE", "FASTER", "SLOWER", "BRAKE",
    "FIRE", "FLARE",
    "SMALL_MEDIPACK", "LARGE_MEDIPACK",
    "PREVIOUS_WEAPON", "NEXT_WEAPON",
    "WEAPON_1", "WEAPON_2", "WEAPON_3", "WEAPON_4", "WEAPON_5",
    "WEAPON_6", "WEAPON_7", "WEAPON_8", "WEAPON_9", "WEAPON_10",
    "SELECT", "DESELECT", "PAUSE", "INVENTORY", "SAVE", "LOAD",
}

local function clearGameplayKeys()
    for _, name in ipairs(GAMEPLAY_ACTIONS) do
        KeyClear(ActionID[name])
    end
end


local function clearMenuMovementKeys()

    for _, name in ipairs({ "FORWARD", "BACK", "LEFT", "RIGHT" }) do
        KeyClear(ActionID[name])
    end
end


local function openMenu()
    menuOpen = true
    menuIdleFrames = 0
    uiMouseMode = false
    MENUS._historyHintUseClick = not MENUS._historyHintUseClick

local mouse = TEN.Input.GetMouseDisplayPosition()
    lastMouseX, lastMouseY = mouse.x, mouse.y
    clearMenuStack()
    pushMenu("main")
end

local function closeMenu()
    menuOpen = false
    menuIdleFrames = 0
    clearMenuStack()
end


function MPUI.OpenSocialSettings()
    local top = activeMenuEntry()
    if top and top.def.id == "social" then return end
    if not menuOpen then
        menuOpen = true
        menuIdleFrames = 0
        uiMouseMode = false
        local mouse = TEN.Input.GetMouseDisplayPosition()
        lastMouseX, lastMouseY = mouse.x, mouse.y
        clearMenuStack()
        pushMenu("social")
    else
        pushMenu("social")
    end
end


local function activateMenuItem(entry, index)
    local item = entry.def.items[index]
    if not item or not item.action then return end
    local action = item.action
    if action.type == "submenu" then
        pushMenu(action.menu)
    elseif action.type == "back" then
        popMenu()
        if #menuStack == 0 then closeMenu() end
    elseif action.type == "command" then

queueMenuCommand(action.command)

if not action.keepMenuOpen then closeMenu() end
    elseif action.type == "social_apply" then

local desc = entry.def.descriptors[action.index]
        if desc and desc.cycle then
            pcall(function() desc.cycle() end)
        end
    elseif action.type == "ev_toggle" then

local desc = MENUS._evAlerts
        if desc and desc.cycle then
            pcall(function() desc.cycle() end)
        end
    elseif action.type == "players_refresh" then

popMenu()
        pushMenu("players")
    end
end


local function menuInputPass()
    local entry = activeMenuEntry()
    if not entry then return end

    local mouse = TEN.Input.GetMouseDisplayPosition()
    local moved = false
    if lastMouseX then
        local delta = math.abs(mouse.x - lastMouseX) + math.abs(mouse.y - lastMouseY)
        if delta >= MOUSE_DELTA_MIN_PCT then
            uiMouseMode = true
            moved = true
        end
    end
    lastMouseX, lastMouseY = mouse.x, mouse.y

    local navDir = 0
    if KeyIsHit(ActionID.ARROW_UP)   then navDir = -1 end
    if KeyIsHit(ActionID.ARROW_DOWN) then navDir = 1 end

if KeyIsHit(ActionID.MOUSE_SCROLL_DOWN) then navDir = 1  end
    if KeyIsHit(ActionID.MOUSE_SCROLL_UP)   then navDir = -1 end
    local enterHit = KeyIsHit(ActionID.RETURN)
    local clickHit = KeyIsHit(ActionID.MOUSE_CLICK_LEFT)
    if navDir ~= 0 or enterHit then uiMouseMode = false end


local colStep = 0
    if not KeyIsHeld(ActionID.WALK) then
        if KeyIsHit(ActionID.ARROW_RIGHT) then colStep = 1  end
        if KeyIsHit(ActionID.ARROW_LEFT)  then colStep = -1 end
    end
    if colStep ~= 0 then
        local selItem = entry.def.items[entry.selectedIndex]
        local curX    = (selItem and entry.def.columns[selItem.column]) or 0
        local bestIdx, bestX = nil, nil
        for i, item in ipairs(entry.def.items) do
            local x = entry.def.columns[item.column]
            if x ~= nil and ((colStep > 0 and x > curX) or (colStep < 0 and x < curX))
                and (bestX == nil or (colStep > 0 and x < bestX) or (colStep < 0 and x > bestX)) then
                bestIdx, bestX = i, x
            end
        end
        if bestIdx ~= nil then
            entry.selectedIndex = bestIdx
            uiMouseMode = false

if colStep < 0 then HS.leftConsumed = true end
        end
    end


if not uiMouseMode then
        clearMenuMovementKeys()
    end


if moved or navDir ~= 0 or enterHit or clickHit then
        menuIdleFrames = 0
    end

    entry.hoverIndex = nil
    if uiMouseMode then
        local nearestCol, nearestDist = nil, math.huge
        for name, colX in pairs(entry.def.columns) do
            local dist = math.abs(mouse.x - colX)
            if dist < nearestDist then nearestCol, nearestDist = name, dist end
        end
        for i, item in ipairs(entry.def.items) do
            if item.column == nearestCol then
                local topY, bottomY = lineHitBandY(entry.itemStates[i].targetY, MENU_SCALE)
                if mouse.y >= topY and mouse.y <= bottomY then
                    entry.hoverIndex = i
                    entry.selectedIndex = i
                    break
                end
            end
        end
    end

    local n = #entry.def.items
    if navDir ~= 0 and n > 0 then
        entry.selectedIndex = (entry.selectedIndex + navDir - 1) % n + 1
    end


if (uiFrame - menuStack.changedFrame) < menuStack.INPUT_COOLDOWN then return end

    if enterHit then
        HS.enterConsumed = true
        activateMenuItem(entry, entry.selectedIndex)
    elseif uiMouseMode and entry.hoverIndex and clickHit then
        HS.clickConsumed = true
        local hItem = entry.def.items[entry.hoverIndex]

if not (hItem and hItem.id == "chat") then
            activateMenuItem(entry, entry.hoverIndex)
        end
    end
end


local function renderMenuEntry(entry, locked)
    local focusedIndex
    if locked then
        focusedIndex = nil
    elseif uiMouseMode then

focusedIndex = entry.hoverIndex
    else
        focusedIndex = entry.selectedIndex
    end

    for i, item in ipairs(entry.def.items) do
        local st = entry.itemStates[i]
        st.isHovered  = (not locked) and (entry.hoverIndex == i)
        st.isSelected = (not locked) and (entry.selectedIndex == i)


local t = easeOutQuad(tweenProgress(st.spawnFrame, WHIP_DURATION_FRAMES))
        st.currentX = lerp(st.whipStartX, st.targetX, t)
        st.currentY = st.targetY


        local phase = uiFrame * DRIFT_SPEED + i * DRIFT_PHASE_STEP
        st.offsetX = DRIFT_AMP_PCT * math.sin(phase)
        st.offsetY = DRIFT_AMP_PCT * math.cos(phase)
        local focused = (focusedIndex == i)
        if focused then

            st.offsetX = st.offsetX + FLOAT_AMP_PCT * math.sin(uiFrame * FLOAT_SPEED_X + i * DRIFT_PHASE_STEP)
            st.offsetY = st.offsetY + FLOAT_AMP_PCT * math.sin(uiFrame * FLOAT_SPEED_Y + i * DRIFT_PHASE_STEP)
        end

        local ox, oy = st.currentX + st.offsetX, st.currentY + st.offsetY

        if item.twoColumn then

local lx, ly = TEN.Util.PercentToScreen(ox + CYCLE_BOARD.ROW_LABEL_DX, oy)
            if not st.ds then
                st.ds = TEN.Strings.DisplayString(item.label or "", Vec2(lx, ly),
                    CYCLE_BOARD.ROW_SCALE, MENU_ITEM_COLOR, false,
                    { TEN.Strings.DisplayStringOption.SHADOW })
            else
                st.ds:SetPosition(Vec2(lx, ly))
            end
            st.ds:SetColor(focused and MENU_HIGHLIGHT_COLOR or MENU_ITEM_COLOR)
            TEN.Strings.ShowString(st.ds, 1/30)

            local valText = (item.getValue and item.getValue()) or ""
            local vx, vy = TEN.Util.PercentToScreen(ox + CYCLE_BOARD.ROW_VALUE_DX, oy)
            if not st.valDs then
                st.valDs = TEN.Strings.DisplayString(valText, Vec2(vx, vy),
                    CYCLE_BOARD.ROW_SCALE, MENU_ITEM_COLOR, false,
                    { TEN.Strings.DisplayStringOption.SHADOW })
            else
                st.valDs:SetPosition(Vec2(vx, vy))
                st.valDs:SetKey(valText)
            end
            local vcolor = (item.getValueColor and item.getValueColor()) or MENU_ITEM_COLOR
            st.valDs:SetColor(focused and MENU_HIGHLIGHT_COLOR or vcolor)
            TEN.Strings.ShowString(st.valDs, 1/30)
        else

            local labelText = (item.getLabel and item.getLabel()) or item.label or ""
            local px, py = TEN.Util.PercentToScreen(ox, oy)
            if not st.ds then
                st.ds = TEN.Strings.DisplayString(labelText, Vec2(px, py), MENU_SCALE, MENU_ITEM_COLOR, false,
                    { TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW })
            else
                st.ds:SetPosition(Vec2(px, py))
                if item.getLabel then st.ds:SetKey(labelText) end
            end
            local baseColor = (item.getLabelColor and item.getLabelColor()) or MENU_ITEM_COLOR
            st.ds:SetColor(focused and MENU_HIGHLIGHT_COLOR or baseColor)
            TEN.Strings.ShowString(st.ds, 1/30)


local subhintText = (item.getSubhint and item.getSubhint()) or item.subhint
            if subhintText then
                local spx, spy = TEN.Util.PercentToScreen(ox, oy + MENU_SUBHINT_DY)
                if not st.subDs then
                    st.subDs = TEN.Strings.DisplayString(subhintText, Vec2(spx, spy),
                        MENU_CHAT_HINT_SCALE, MENU_BTN_HINT_COLOR, false,
                        { TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW })
                else
                    st.subDs:SetPosition(Vec2(spx, spy))
                    if item.getSubhint then st.subDs:SetKey(subhintText) end
                end
                TEN.Strings.ShowString(st.subDs, 1/30)
            end
        end
    end
end

local function renderMenuStack()
    local entry = activeMenuEntry()
    if not entry then return end

if entry.def.keepParentVisible and #menuStack >= 2 then
        renderMenuEntry(menuStack[#menuStack - 1], true)
    end
    renderMenuEntry(entry, false)
end


local function updateMenuBoard()
    local step = 1 / MENU_BOARD.SLIDE_FRAMES
    if menuOpen then
        menuBoardProgress = math.min(1, menuBoardProgress + step)
    else
        menuBoardProgress = math.max(0, menuBoardProgress - step)
    end
    if menuBoardProgress <= 0 then return end

if not (menuStack[1] and menuStack[1].def.id == "main") then return end

    local t  = easeOutQuad(menuBoardProgress)
    local cx = lerp(MENU_BOARD.OFFSCREEN_X, MENU_BOARD.REST_X, t)
    TEN.View.DisplaySprite(
        TEN.Objects.ObjID.DEFAULT_SPRITES, MENU_BOARD.SPRITE_ID,
        Vec2(cx, MENU_BOARD.Y), 0, MENU_BOARD.SCALE
    ):Draw()
end





local function updateCycleBoard()
    local step = 1 / CYCLE_BOARD.SLIDE_FRAMES
    local top  = menuStack[#menuStack]
    local socialActive = top ~= nil and top.def.id == "social"
    if socialActive then
        cycleBoardProgress = math.min(1, cycleBoardProgress + step)
    else
        cycleBoardProgress = math.max(0, cycleBoardProgress - step)
    end
    if cycleBoardProgress <= 0 then return end

    local t  = easeOutQuad(cycleBoardProgress)
    local cx = lerp(CYCLE_BOARD.OFFSCREEN_X, CYCLE_BOARD.REST_X, t)
    TEN.View.DisplaySprite(
        TEN.Objects.ObjID.DEFAULT_SPRITES, CYCLE_BOARD.SPRITE_ID,
        Vec2(cx, CYCLE_BOARD.Y), 0, CYCLE_BOARD.SCALE
    ):Draw()
end

local function updateMenuBtn()
    if not menuHintVisible then return end
    local label = menuOpen and "[M] Close Menu" or "[M]Menu"
    if not menuHintDS then
        local px, py = TEN.Util.PercentToScreen(MENU_BTN_X, LOBBY_BTN_Y)
        menuHintDS = TEN.Strings.DisplayString(label, Vec2(px, py), MENU_BTN_SCALE, MENU_BTN_COLOR, false,
            { TEN.Strings.DisplayStringOption.RIGHT, TEN.Strings.DisplayStringOption.SHADOW })

HS.register("menu_btn", {
            x = MENU_BTN_X, y = LOBBY_BTN_Y, scale = MENU_BTN_SCALE,
            align = "right", halfWidth = 6,
            ds = menuHintDS, baseColor = MENU_BTN_COLOR,
            onClick = function() HS.menuBtnClicked = true end,
            flashCheck = function()
                if xCycleUIActive then return false end
                return KeyIsHit(ActionID.M) or (azertyMode and KeyIsHit(ActionID.SEMICOLON))
            end,
        })
    else
        menuHintDS:SetKey(label)
    end
    TEN.Strings.ShowString(menuHintDS, 1/30)

    if menuOpen then
        if not menuHoldHintDS then
            local px2, py2 = TEN.Util.PercentToScreen(MENU_BTN_X, MENU_BTN_HINT_Y)
            menuHoldHintDS = TEN.Strings.DisplayString("(Hold) Hide", Vec2(px2, py2), MENU_BTN_HINT_SCALE, MENU_BTN_HINT_COLOR, false,
                { TEN.Strings.DisplayStringOption.RIGHT, TEN.Strings.DisplayStringOption.SHADOW })

HS.register("menu_hold_hide", {
                x = MENU_BTN_X, y = MENU_BTN_HINT_Y, scale = MENU_BTN_HINT_SCALE,
                align = "right", halfWidth = 5,
                ds = menuHoldHintDS, baseColor = MENU_BTN_HINT_COLOR,
                holdable = true,
            })
        end
        TEN.Strings.ShowString(menuHoldHintDS, 1/30)
    elseif menuHoldHintDS then
        pcall(function() TEN.Strings.HideString(menuHoldHintDS) end)
        menuHoldHintDS = nil
        HS.unregister("menu_hold_hide")
    end
end



local RESTART_HINT = {
    X = 97,
    Y = 94,
    SCALE = 0.5,
    COLOR = Color(200, 200, 200),
    visible = false, cb = nil, ds = nil,
}


function MPUI.SetRestartHint(visible, onRestart)
    RESTART_HINT.visible = visible and true or false
    RESTART_HINT.cb = onRestart
    if not RESTART_HINT.visible and RESTART_HINT.ds then
        pcall(function() TEN.Strings.HideString(RESTART_HINT.ds) end)
        RESTART_HINT.ds = nil
        HS.unregister("restart_hint")
    end
end

local function updateRestartHint()
    if not RESTART_HINT.visible then return end
    if not RESTART_HINT.ds then
        local px, py = TEN.Util.PercentToScreen(RESTART_HINT.X, RESTART_HINT.Y)
        RESTART_HINT.ds = TEN.Strings.DisplayString("[R] Restart", Vec2(px, py),
            RESTART_HINT.SCALE, RESTART_HINT.COLOR, false,
            { TEN.Strings.DisplayStringOption.RIGHT, TEN.Strings.DisplayStringOption.SHADOW })
        HS.register("restart_hint", {
            x = RESTART_HINT.X, y = RESTART_HINT.Y, scale = RESTART_HINT.SCALE,
            align = "right", halfWidth = 7,
            ds = RESTART_HINT.ds, baseColor = RESTART_HINT.COLOR,
            onClick = function() if RESTART_HINT.cb then RESTART_HINT.cb() end end,
            flashCheck = function() return not chatOpen and not nameInputOpen and KeyIsHit(ActionID.R) end,
        })
    end
    TEN.Strings.ShowString(RESTART_HINT.ds, 1/30)

if not chatOpen and not nameInputOpen and KeyIsHit(ActionID.R) and RESTART_HINT.cb then
        RESTART_HINT.cb()
    end
end


local function releaseRecordsPanel()
    for i = 1, RECPANEL.count do
        if RECPANEL.ds[i] ~= nil then pcall(function() TEN.Strings.HideString(RECPANEL.ds[i]) end) end
        RECPANEL.ds[i], RECPANEL.labels[i] = nil, nil
    end
    RECPANEL.count = 0
    RECPANEL.open  = false
end

local function updateRecordsPanel()

    if RECPANEL.provider == nil then
        if RECPANEL.open then releaseRecordsPanel() end
        return
    end
    if #menuStack == 0 then
        if RECPANEL.open then releaseRecordsPanel() end
        return
    end

if RECPANEL.dirty then
        releaseRecordsPanel()
        RECPANEL.dirty = false
    end
    RECPANEL.open  = true
    RECPANEL.frame = RECPANEL.frame + 1
    if RECPANEL.count == 0 or RECPANEL.frame % RECPANEL.REFRESH == 0 then
        local data = RECPANEL.provider() or {}
        local cols = data.columns or {}
        local rows = data.rows or {}
        local title = data.title or RECPANEL.title or RECPANEL.TITLE

local nameOff = 0
        for _, col in ipairs(cols) do
            if col.key == RECPANEL.ALIGN_KEY then nameOff = col.off break end
        end
        local labelX = CYCLE_BOARD.REST_X + CYCLE_BOARD.ROW_LABEL_DX
        local baseX  = labelX - nameOff
        local units = {}
        units[#units + 1] = { t = title, x = labelX, line = 0, center = false,
                              s = RECPANEL.TITLE_SCALE, c = RECPANEL.TITLE_COLOR }
        for _, col in ipairs(cols) do
            if col.head ~= nil and col.head ~= "" then
                units[#units + 1] = { t = col.head, x = baseX + col.off, line = 1, center = col.center,
                                      s = RECPANEL.SCALE, c = RECPANEL.HEAD_COLOR }
            end
        end
        if #rows == 0 then
            units[#units + 1] = { t = RECPANEL.EMPTY_TEXT, x = labelX, line = 2, center = false,
                                  s = RECPANEL.SCALE, c = RECPANEL.ROW_COLOR }
        else
            for i = 1, #rows do
                for _, col in ipairs(cols) do
                    local cell = rows[i][col.key]
                    if cell ~= nil and cell ~= "" then
                        units[#units + 1] = { t = cell, x = baseX + col.off, line = 1 + i, center = col.center,
                                              s = RECPANEL.SCALE, c = RECPANEL.ROW_COLOR }
                    end
                end
            end
        end
        for i = 1, #units do
            local u = units[i]
            local sig = string.format("%s|%.1f|%d", u.t, u.x, u.line)
            if RECPANEL.labels[i] ~= sig then
                if RECPANEL.ds[i] ~= nil then pcall(function() TEN.Strings.HideString(RECPANEL.ds[i]) end) end
                local px, py = TEN.Util.PercentToScreen(u.x, RECPANEL.TITLE_Y + u.line * RECPANEL.DY)
                local opts = u.center
                    and { TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW }
                    or  { TEN.Strings.DisplayStringOption.SHADOW }
                RECPANEL.ds[i] = TEN.Strings.DisplayString(u.t, Vec2(px, py), u.s, u.c, false, opts)
                RECPANEL.labels[i] = sig
            end
        end
        for i = #units + 1, RECPANEL.count do
            if RECPANEL.ds[i] ~= nil then pcall(function() TEN.Strings.HideString(RECPANEL.ds[i]) end) end
            RECPANEL.ds[i], RECPANEL.labels[i] = nil, nil
        end
        RECPANEL.count = #units
    end
    for i = 1, RECPANEL.count do
        if RECPANEL.ds[i] ~= nil then TEN.Strings.ShowString(RECPANEL.ds[i], 1 / 30, false) end
    end
end

local function createNameInputDS()
    if nameInputDS then
        pcall(function() TEN.Strings.HideString(nameInputDS) end)
    end
    local px, py = TEN.Util.PercentToScreen(NAME_INPUT_X, NAME_INPUT_Y)
    nameInputDS = TEN.Strings.DisplayString("Name > _", Vec2(px, py), NAME_INPUT_SCALE, NAME_INPUT_COLOR, false,
        { TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW })
    TEN.Strings.ShowString(nameInputDS)
end

local function refreshNameInputDS()
    if not nameInputDS then return end
    nameInputDS:SetKey("Name > " .. nameInputBuffer .. "_")
    TEN.Strings.ShowString(nameInputDS, 1/30)
end

local function openNameInput()
    if menuOpen then closeMenu() end
    nameInputOpen       = true
    nameInputBuffer     = ""
    nameInputJustOpened = true
    createNameInputDS()
    refreshNameInputDS()
end

local function closeNameInput()
    nameInputOpen       = false
    nameInputBuffer     = ""
    nameInputJustOpened = false
    if nameInputDS then
        pcall(function() TEN.Strings.HideString(nameInputDS) end)
        nameInputDS = nil
    end
end

local function pollNameInput()
    if not nameInputOpen then return end

    if nameInputJustOpened then
        nameInputJustOpened = false
        clearGameplayKeys()
        return
    end

    if KeyIsHit(ActionID.RETURN) then
        if #nameInputBuffer > 0 then
            pendingNameChange = nameInputBuffer
        end
        closeNameInput()
        clearGameplayKeys()
        return
    end


if KeyIsHit(ActionID.TAB) then
        closeNameInput()
        clearGameplayKeys()
        return
    end

    if KeyIsHit(ActionID.BACKSPACE) then
        if #nameInputBuffer > 0 then
            nameInputBuffer = nameInputBuffer:sub(1, #nameInputBuffer - 1)
        end
    end

    if #nameInputBuffer < NAME_MAX_LENGTH then
        local shift = KeyIsHeld(ActionID.SHIFT)
        for _, mapping in ipairs(activeKeyMap) do
            if KeyIsHit(mapping.action) then
                local ch = shift and mapping.upper or mapping.lower
                nameInputBuffer = nameInputBuffer .. ch
            end
        end
    end

    refreshNameInputDS()
    clearGameplayKeys()
end



MENUS._confirm = {
    open = false, ds = nil, yesDS = nil, noDS = nil, onConfirm = nil,
    X_YES = 35, X_NO = 65,
    HALF_W = 11,
    YES_TEXT = "[Action] Confirm",
    NO_TEXT  = "[Draw] Cancel",
    IDS = { confirm_yes = true, confirm_no = true },
}


MENUS._confirm.makeOption = function(id, text, x, actionID)
    local C = MENUS._confirm
    local px, py = TEN.Util.PercentToScreen(x, LOBBY_CONFIRM_Y2)
    local ds = TEN.Strings.DisplayString(text, Vec2(px, py),
        LOBBY_CONFIRM_SCALE, MENU_ITEM_COLOR, false,
        { TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW })
    TEN.Strings.ShowString(ds, 1/30, false)
    HS.register(id, {
        x = x, y = LOBBY_CONFIRM_Y2, scale = LOBBY_CONFIRM_SCALE,
        halfWidth = C.HALF_W, ds = ds, baseColor = MENU_ITEM_COLOR,
        flashCheck = function() return KeyIsHit(actionID) end,
    })
    return ds
end


function MPUI.OpenConfirm(message, onConfirm)
    local C = MENUS._confirm
    if C.open then return end
    C.open = true
    C.onConfirm = onConfirm
    local px1, py1 = TEN.Util.PercentToScreen(50, LOBBY_CONFIRM_Y1)
    C.ds = TEN.Strings.DisplayString(message, Vec2(px1, py1),
        LOBBY_CONFIRM_SCALE, Color(255, 220, 80), false,
        { TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW })
    TEN.Strings.ShowString(C.ds, 1/30, false)
    C.yesDS = C.makeOption("confirm_yes", C.YES_TEXT, C.X_YES, ActionID.ACTION)
    C.noDS  = C.makeOption("confirm_no",  C.NO_TEXT,  C.X_NO,  ActionID.DRAW)
end

MENUS._confirm.close = function()
    local C = MENUS._confirm
    C.open, C.onConfirm = false, nil
    HS.unregister("confirm_yes")
    HS.unregister("confirm_no")
    if C.ds    then pcall(function() TEN.Strings.HideString(C.ds) end)    C.ds = nil end
    if C.yesDS then pcall(function() TEN.Strings.HideString(C.yesDS) end) C.yesDS = nil end
    if C.noDS  then pcall(function() TEN.Strings.HideString(C.noDS) end)  C.noDS = nil end
end

function MPUI.IsConfirmOpen() return MENUS._confirm.open end


function MPUI.ShowNotification(key, text, durationFrames, opts)
    opts = opts or {}
    local isOverlay = opts.overlay or false

    if not isOverlay then
        for k, notif in pairs(activeNotifs) do
            if not notif.overlay and notif.visible then
                pcall(function() TEN.Strings.HideString(notif.ds) end)
                activeNotifs[k] = nil
                HS.unregister("notif_" .. k)
            end
        end
    end

    local posX = opts.x or DEFAULT_POS_X
    local posY = opts.y or DEFAULT_POS_Y
    local scale = opts.scale or DEFAULT_SCALE
    local color = opts.color or DEFAULT_COLOR

    local alignOpt = (opts.align == "right")
        and TEN.Strings.DisplayStringOption.RIGHT
        or  TEN.Strings.DisplayStringOption.CENTER

    local px, py = TEN.Util.PercentToScreen(posX, posY)
    local ds = TEN.Strings.DisplayString(text, Vec2(px, py), scale, color, false,
        { alignOpt, TEN.Strings.DisplayStringOption.SHADOW })

    TEN.Strings.ShowString(ds)
    activeNotifs[key] = { ds = ds, framesTTL = durationFrames, visible = true, overlay = isOverlay }


if opts.interactive then
        HS.register("notif_" .. key, {
            x = posX, y = posY, scale = scale,
            align = (opts.align == "right") and "right" or "center",
            ds = ds, baseColor = color,
            flashCheck = opts.flashCheck,
        })
    else
        HS.unregister("notif_" .. key)
    end
end

function MPUI.HideNotification(key)
    if activeNotifs[key] and activeNotifs[key].visible then
        pcall(function() TEN.Strings.HideString(activeNotifs[key].ds) end)
        activeNotifs[key].visible = false
        HS.unregister("notif_" .. key)
    end
end


function MPUI.ShowXCycleHints() end
function MPUI.HideXCycleHints() end



local JOIN_VARIANTS = {
    "%s steps in - let's see what you've got",
    "%s joins the expedition. Try to keep up",
    "%s arrives, right on cue",
    "%s makes an entrance",
    "%s is in. This just got interesting",
    "%s shows up - about time",
    "%s answers the call. Bold move",
    "%s drops in like they own the place",
    "%s is here. Stay sharp",
    "%s joins the adventure - no turning back now",

    "%s strolls in like this is a guided tour",
    "%s joins. Try not to slow things down",
    "%s arrives - hope you read the briefing",
    "%s steps in. Do not touch anything... yet",
    "%s joins the expedition. Brave, in a way",
    "%s shows up - fashionably late",
    "%s drops in. Subtle as ever",
    "%s enters. Let's pretend that was planned",
    "%s walks in like danger is optional",

    "%s appears. Convenient timing",
    "%s joins - let's see how long that lasts",
    "%s steps into the tomb. Good luck",
    "%s arrives. Try not to trigger anything",
    "%s joins the fun.",
    "%s shows up - right when it gets complicated",
    "%s enters like they've done this before",
    "%s joins. Confidence noted",
    "%s drops in - hopefully with a plan",
    "%s is here. This should be interesting",

    "%s pops up. This should be entertaining",
    "%s arrives. Try not to embarrass yourself",
    "%s shows up - I'll keep expectations low",
    "%s joins. Let's see if you're as good as you think",
    "%s steps in. Keep it together",
    "%s arrives. I assume you have a plan",
    "%s joins - confidence is not a strategy",
    "%s shows up. I'll be watching",
    "%s enters. Try to make it count",

    "%s drops in. I do hope that was intentional",
    "%s arrives - let's pretend this improves our odds",
    "%s joins. I'll expect accordingly",
    "%s steps in. Try not to slow me down",
    "%s appears. This could go either way",

    "%s arrives - I assume they know what they're doing",
    "%s shows up. Confidence noted... we'll see",
    "%s joins. Let's hope you're useful",
    "%s steps in. Try not to improvise too much",
    "%s appears. I'll reserve judgment",
}

local LEAVE_VARIANTS = {
    "%s exits the scene",
    "%s slips away - unfinished business",
    "%s pops out... for now",
    "%s disappears without a trace",
    "%s makes a quiet exit",
    "%s leaves the adventure behind",
    "%s falls back - probably eating a snack",
    "%s vanishes. I wonder why.",
    "%s calls it. Not everyone makes it",
    "%s walks away - their loss",

    "%s leaves. Probably for the best",
    "%s slips out. Can't blame them",
    "%s exits -- I'll try not to take it personally",
    "%s disappears. How mysterious",
    "%s gave up. Sensible, really",
    "%s leaves the mess to the rest of us",
    "%s walks away. Timing is everything",
    "%s vanishes - just like that",
    "%s calls it quits.",
    "%s exits. I was just getting used to them",

    "%s leaves before things get worse. Smart",
    "%s disappears mid-adventure. Bold choice",
    "%s walks out. I suppose that's one strategy",
    "%s slips away. No dramatic exit, then",
    "%s leaves. I'll mark that as 'temporary'",
    "%s exits the field. Can't win them all",
    "%s vanishes. I'll try to carry on",
    "%s calls it early. Curious decision",
    "%s walks away -- I'll manage",

    "%s leaves. I'll lower expectations accordingly",
    "%s exits. That simplifies things",
    "%s walks away. Not unexpected",
    "%s leaves - I'll adapt",
    "%s slips out. I'll carry on",
    "%s exits. I suppose that's one less Lara",
    "%s vanishes. How convenient",
    "%s leaves. I'll handle it",
    "%s calls it. I had a feeling",

    "%s walks away. I won't stop them",
    "%s leaves - probably for the best",
    "%s slips away. I'll remember that",

    "%s leaves. I'll consider that a tactical choice",
    "%s exits. I'll adjust expectations",
    "%s disappears. I suppose that answers my question",
    "%s walks away. Timing could've been better",
    "%s slips out. I'll keep that in mind",
    "%s leaves - I'll try not to be personal",
}

local JOIN_COLOR          = Color(100, 255, 130)
local LEAVE_COLOR         = Color(255, 140, 100)
MPUI.CONFIRM_ON_COLOR  = JOIN_COLOR
MPUI.CONFIRM_OFF_COLOR = LEAVE_COLOR

MPUI.CYCLE_FEEDBACK_X = CYCLE_BOARD.REST_X
MPUI.CYCLE_FEEDBACK_Y = CYCLE_BOARD.Y + (CYCLE_BOARD.SCALE.y / 2) + 3.0
local JOIN_LEAVE_DURATION = 300

local JOIN_LEAVE_POS_Y    = 3

local JOIN_LEAVE_MAX_INDIVIDUAL  = 5
local JOIN_LEAVE_OVERFLOW_MSG    = "...and a crowd of others arrived today"
local JOIN_LEAVE_OVERFLOW_COLOR  = Color(180, 180, 180)

local function pushJoinLeave(text, color)
    if #joinLeaveQueue < JOIN_LEAVE_MAX_INDIVIDUAL then
        table.insert(joinLeaveQueue, { text = text, color = color })
    elseif #joinLeaveQueue == JOIN_LEAVE_MAX_INDIVIDUAL then

        table.insert(joinLeaveQueue, { text = JOIN_LEAVE_OVERFLOW_MSG, color = JOIN_LEAVE_OVERFLOW_COLOR })
    else
    end

end

function MPUI.ShowJoinNotification(name)
    if joinLeaveMode == JL_MODE_OFF then return end
    local variant = JOIN_VARIANTS[math.random(#JOIN_VARIANTS)]
    local text = string.format(variant, name)
    if joinLeaveMode == JL_MODE_CHAT then
        MPUI.AddSystemChatMessage(text, JOIN_COLOR)
    else
        pushJoinLeave(text, JOIN_COLOR)
    end
end

function MPUI.ShowLeaveNotification(name)
    if joinLeaveMode == JL_MODE_OFF then return end
    local variant = LEAVE_VARIANTS[math.random(#LEAVE_VARIANTS)]
    local text = string.format(variant, name)
    if joinLeaveMode == JL_MODE_CHAT then
        MPUI.AddSystemChatMessage(text, LEAVE_COLOR)
    else
        pushJoinLeave(text, LEAVE_COLOR)
    end
end

function MPUI.SetJoinLeaveMode(mode)
    joinLeaveMode = mode
end

function MPUI.GetJoinLeaveMode()
    return joinLeaveMode
end


local function createInputDS()
    if chatInputDS then
        pcall(function() TEN.Strings.HideString(chatInputDS) end)
    end
    local px, py = TEN.Util.PercentToScreen(CHAT_INPUT_X, CHAT_INPUT_Y)
    chatInputDS = TEN.Strings.DisplayString("> _", Vec2(px, py), CHAT_INPUT_SCALE, CHAT_INPUT_COLOR, false,
        {TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW})
    TEN.Strings.ShowString(chatInputDS)
end

local function refreshInputDS()
    if not chatInputDS then return end
    local prefix = chatMode == "global" and "[ALL] > " or "> "
    local color  = chatMode == "global" and CHAT_INPUT_GLOBAL_COLOR or CHAT_INPUT_COLOR
    chatInputDS:SetKey(prefix .. chatBuffer .. "_")
    chatInputDS:SetColor(color)
    TEN.Strings.ShowString(chatInputDS, 1/30)
end

local function openChatOverlay()
    chatOverlayActive = true
    chatOverlayDS = {}
    local count = math.min(#chatCache, CHAT_MAX_MESSAGES)
    for i = 1, count do
        local entry = chatCache[i]
        local yPos = CHAT_MSG_BASE_Y - (i - 1) * CHAT_MSG_SPACING

local ok = pcall(function()
            local px, py = TEN.Util.PercentToScreen(CHAT_X, yPos)
            local ds = TEN.Strings.DisplayString(entry.text, Vec2(px, py), CHAT_SCALE, entry.color, false,
                { TEN.Strings.DisplayStringOption.SHADOW })
            TEN.Strings.ShowString(ds)
            chatOverlayDS[#chatOverlayDS + 1] = ds
        end)
        if not ok then
            print("[Multiplayer UI-GUARD] chat-history overlay entry " .. tostring(i) .. " failed to render")
        end
    end
end

local function closeChatOverlay()
    chatOverlayActive = false
    for _, ds in ipairs(chatOverlayDS) do
        pcall(function() TEN.Strings.HideString(ds) end)
    end
    chatOverlayDS = {}
end

local function openChat()
    chatOpen = true
    chatBuffer = ""
    chatMode = "global"
    chatJustOpened = true
    createInputDS()
    refreshInputDS()
    if chatTabHintDS then
        pcall(function() TEN.Strings.HideString(chatTabHintDS) end)
    end
    local px, py = TEN.Util.PercentToScreen(CHAT_INPUT_X, CHAT_TAB_HINT_Y)
    chatTabHintDS = TEN.Strings.DisplayString("[TAB/click] Talk/Shout", Vec2(px, py), CHAT_TAB_HINT_SCALE, MENU_ITEM_COLOR, false,
        { TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW })
    TEN.Strings.ShowString(chatTabHintDS)

HS.register("chat_tabhint", {
        x = CHAT_INPUT_X, y = CHAT_TAB_HINT_Y, scale = CHAT_TAB_HINT_SCALE,
        ds = chatTabHintDS, baseColor = MENU_ITEM_COLOR,
        onClick = function()
            chatMode = chatMode == "local" and "global" or "local"
            refreshInputDS()
        end,
        flashCheck = function() return chatOpen and KeyIsHit(ActionID.TAB) end,
    })
end

local function closeChat()
    chatOpen = false
    chatBuffer = ""
    chatJustOpened = false
    if chatInputDS then
        pcall(function() TEN.Strings.HideString(chatInputDS) end)
    end
    if chatTabHintDS then
        pcall(function() TEN.Strings.HideString(chatTabHintDS) end)
        chatTabHintDS = nil
        HS.unregister("chat_tabhint")
    end
end

local function splitChatLines(text)
    if #text <= 50 then
        return { text }
    else
        local breakAt = 50

        while breakAt > 1 and text:sub(breakAt, breakAt) ~= " " do
            breakAt = breakAt - 1
        end

        if breakAt <= 1 then breakAt = 50 end
        return { text:sub(1, breakAt), text:sub(breakAt + 1) }
    end
end

local function insertChatEntry(line, msgColor)
    local px, py = TEN.Util.PercentToScreen(CHAT_X, CHAT_MSG_BASE_Y)
    local ds = TEN.Strings.DisplayString(line, Vec2(px, py), CHAT_SCALE, msgColor, false,
        {TEN.Strings.DisplayStringOption.SHADOW})
    TEN.Strings.ShowString(ds)

    table.insert(chatLog, 1, { text = line, framesLeft = CHAT_MSG_TTL, ds = ds })
    while #chatLog > CHAT_MAX_MESSAGES do
        local old = table.remove(chatLog)
        pcall(function() TEN.Strings.HideString(old.ds) end)
    end

    table.insert(chatCache, 1, { text = line, color = msgColor })
    while #chatCache > CHAT_MAX_MESSAGES do table.remove(chatCache) end
end


MPUI.CHAT_PALETTE = {
    NAME_INPUT_COLOR,
    CHAT_INPUT_COLOR,
    CHAT_OWN_COLOR,
    LEAVE_COLOR,
    CHAT_REMOTE_COLOR,
    CHAT_INPUT_GLOBAL_COLOR,
    RECPANEL.HEAD_COLOR,
    JOIN_LEAVE_OVERFLOW_COLOR,
}

function MPUI.GetPlayerChatColor(id)
    id = tonumber(id)
    if not id then return nil end
    return MPUI.CHAT_PALETTE[((id - 1) % #MPUI.CHAT_PALETTE) + 1]
end


function MPUI.AddChatMessage(senderName, text, color, isGlobal, senderId)
    text = (text or ""):sub(1, 100)
    local prefix = isGlobal and "[ALL] " or ""
    local displayText = prefix .. "[" .. (senderName or "???") .. "]: " .. text
    local msgColor = color or MPUI.GetPlayerChatColor(senderId)
        or (isGlobal and CHAT_REMOTE_GLOBAL_COLOR or CHAT_REMOTE_COLOR)

    local lines = splitChatLines(displayText)
    for _, line in ipairs(lines) do
        insertChatEntry(line, msgColor)
    end
end


function MPUI.AddSystemChatMessage(text, color)
    local msgColor = color or CHAT_REMOTE_COLOR
    local px, py = TEN.Util.PercentToScreen(CHAT_X, CHAT_MSG_BASE_Y)
    local ds = TEN.Strings.DisplayString(text, Vec2(px, py), CHAT_SCALE, msgColor, false,
        {TEN.Strings.DisplayStringOption.SHADOW})
    TEN.Strings.ShowString(ds)
    table.insert(chatLog, 1, { text = text, framesLeft = CHAT_MSG_TTL, ds = ds })
    while #chatLog > CHAT_MAX_MESSAGES do
        local old = table.remove(chatLog)
        pcall(function() TEN.Strings.HideString(old.ds) end)
    end
    table.insert(chatCache, 1, { text = text, color = msgColor })
    while #chatCache > CHAT_MAX_MESSAGES do table.remove(chatCache) end
end

local function pollChatInput()
    if not chatOpen then return end

    if chatJustOpened then
        chatJustOpened = false
        clearGameplayKeys()
        return
    end

    local shift = KeyIsHeld(ActionID.SHIFT)

    if KeyIsHit(ActionID.TAB) then
        chatMode = chatMode == "local" and "global" or "local"
        refreshInputDS()
        clearGameplayKeys()
        return
    end

    if KeyIsHit(ActionID.RETURN) then
        if #chatBuffer > 0 then

if MPUI._chatCmdHandler and MPUI._chatCmdHandler(chatBuffer) then
                closeChat()
                clearGameplayKeys()
                return
            end
            pendingChatMsg = chatBuffer
            pendingChatGlobal = (chatMode == "global")
            local myName = getLocalNameFn and getLocalNameFn() or "Me"
            local myId = MPUI._localIdFn and MPUI._localIdFn() or nil
            local ownColor = MPUI.GetPlayerChatColor(myId)
                or (pendingChatGlobal and CHAT_OWN_GLOBAL_COLOR or CHAT_OWN_COLOR)
            MPUI.AddChatMessage(myName, chatBuffer, ownColor, pendingChatGlobal, myId)
        end
        closeChat()
        clearGameplayKeys()
        return
    end



    if KeyIsHit(ActionID.BACKSPACE) then
        if #chatBuffer > 0 then
            chatBuffer = chatBuffer:sub(1, #chatBuffer - 1)
        end
    end

    if #chatBuffer < CHAT_MAX_LENGTH then
        for _, mapping in ipairs(activeKeyMap) do
            if KeyIsHit(mapping.action) then
                local ch = shift and mapping.upper or mapping.lower
                chatBuffer = chatBuffer .. ch
            end
        end
    end

    refreshInputDS()

    clearGameplayKeys()
end

local function updateChatLog()
    if chatHeaderText then
        if not chatHeaderDS then
            local px, py = TEN.Util.PercentToScreen(CHAT_X, CHAT_HEADER_Y)
            chatHeaderDS = TEN.Strings.DisplayString(chatHeaderText, Vec2(px, py),
                CHAT_SCALE, CHAT_HEADER_COLOR, false,
                { TEN.Strings.DisplayStringOption.SHADOW })
        end
        TEN.Strings.ShowString(chatHeaderDS, 1/30)
    end

    local i = 1
    while i <= #chatLog do
        local msg = chatLog[i]
        msg.framesLeft = msg.framesLeft - 1
        if msg.framesLeft <= 0 then
            pcall(function() TEN.Strings.HideString(msg.ds) end)
            table.remove(chatLog, i)
        else
            local yPos = CHAT_MSG_BASE_Y - (i - 1) * CHAT_MSG_SPACING
            local px, py = TEN.Util.PercentToScreen(CHAT_X, yPos)
            msg.ds:SetPosition(Vec2(px, py))
            TEN.Strings.ShowString(msg.ds, 1/30)
            i = i + 1
        end
    end
end


function MPUI.SetLocalNameProvider(fn)
    getLocalNameFn = fn
end


function MPUI.SetLocalIdProvider(fn)
    MPUI._localIdFn = fn
end


function MPUI.SetChatCommandHandler(fn)
    MPUI._chatCmdHandler = fn
end

function MPUI.SetConnectedProvider(fn)
    getConnectedFn = fn
end

function MPUI.SetChatHeader(text)
    chatHeaderText = text
    if chatHeaderDS then
        pcall(function() TEN.Strings.HideString(chatHeaderDS) end)
        chatHeaderDS = nil
    end
end

function MPUI.GetPendingChatMessage()
    local msg = pendingChatMsg
    pendingChatMsg = nil
    return msg
end

function MPUI.IsPendingChatGlobal()
    return pendingChatGlobal
end

function MPUI.HasPendingChat()
    return pendingChatMsg ~= nil
end

function MPUI.IsChatOpen()
    return chatOpen
end

function MPUI.IsNameInputOpen()
    return nameInputOpen
end


function MPUI.OpenNameInput()
    openNameInput()
end

function MPUI.IsMenuOpen()
    return #menuStack > 0
end

function MPUI.GetPendingNameChange()
    local name = pendingNameChange
    pendingNameChange = nil
    return name
end


function MPUI.Update()
    uiFrame = uiFrame + 1


readyMenuCommands  = stagedMenuCommands
    stagedMenuCommands = {}

    TEN.Flow.EnableFlyCheat(not chatOpen and not nameInputOpen)


HS.clicks        = {}
    HS.clickConsumed = false
    HS.enterConsumed = false
    HS.leftConsumed  = false

    for key, notif in pairs(activeNotifs) do
        if notif.visible then
            notif.framesTTL = notif.framesTTL - 1
            if notif.framesTTL <= 0 then
                pcall(function() TEN.Strings.HideString(notif.ds) end)
                notif.visible = false
                activeNotifs[key] = nil
                HS.unregister("notif_" .. key)
            else
                TEN.Strings.ShowString(notif.ds, 1/30)
            end
        else
            activeNotifs[key] = nil
            HS.unregister("notif_" .. key)
        end
    end


if MENUS._confirm.open then
        HS.pass(MENUS._confirm.IDS)
        if MENUS._confirm.ds    then TEN.Strings.ShowString(MENUS._confirm.ds,    1/30, false) end
        if MENUS._confirm.yesDS then TEN.Strings.ShowString(MENUS._confirm.yesDS, 1/30, false) end
        if MENUS._confirm.noDS  then TEN.Strings.ShowString(MENUS._confirm.noDS,  1/30, false) end
        if KeyIsHit(ActionID.ACTION) or HS.clicks["confirm_yes"] then

local fn = MENUS._confirm.onConfirm
            MENUS._confirm.close()
            if fn then pcall(fn) end
        elseif KeyIsHit(ActionID.DRAW) or HS.clicks["confirm_no"] then
            MENUS._confirm.close()
        end
        clearGameplayKeys()

        updateChatLog()
        updateMenuBtn()
        return
    end


HS.pass()
    local menuBtnClickHit = HS.menuBtnClicked == true
    HS.menuBtnClicked = false


if chatOpen and menuBtnClickHit then
        closeChat()
        openMenu()
        menuBtnClickHit = false
    end

    if not chatOpen and not nameInputOpen then
        local isConnected = getConnectedFn and getConnectedFn() or false

        local mKeyHit    = KeyIsHit(ActionID.M) or (azertyMode and KeyIsHit(ActionID.SEMICOLON))

if xCycleUIActive then mKeyHit = false end

if menuBtnClickHit then mKeyHit = true end
        local menuKeyHit = mKeyHit or KeyIsHit(ActionID.Z) or KeyIsHit(ActionID.B)
        if not isConnected and menuKeyHit then
            MPUI.ShowNotification("mp_offline", "You must be connected to use the multiplayer menu!", 120,
                { color = Color(255, 160, 0) })
        elseif isConnected then
            if mKeyHit then
                if menuOpen then
                    closeMenu()
                else
                    openMenu()
                end
            end


            if menuOpen and (KeyIsHit(ActionID.Z) or (azertyMode and KeyIsHit(ActionID.W))) then
                openNameInput()
            end


if MPUI.ConsumeMenuCommand("chat") then
                openChat()
            end
            if MPUI.ConsumeMenuCommand("change_name") then
                openNameInput()
            end

            if KeyIsHeld(ActionID.C) then
                chatCKeyFrames = chatCKeyFrames + 1
                if chatCKeyFrames == CHAT_C_HOLD_THRESHOLD then
                    openChatOverlay()
                end
            end
        end


if menuOpen and (KeyIsHit(ActionID.TAB)
            or (KeyIsHit(ActionID.ARROW_LEFT) and not HS.leftConsumed)) then
            if #menuStack > 1 then
                popMenu()
            else
                closeMenu()
            end
        end


if KeyIsHit(ActionID.X) and not menuBtnClickHit then
            MPUI.OpenSocialSettings()
        end


if menuOpen and KeyIsHit(ActionID.V) then
            closeMenu()
        end

        if menuOpen and KeyIsHit(ActionID.T) then
            closeMenu()
        end


        if menuOpen then
            menuInputPass()
        end
    end


    if not KeyIsHeld(ActionID.C) and chatCKeyFrames > 0 then
        if chatOverlayActive then
            closeChatOverlay()
        elseif chatCKeyFrames < CHAT_C_HOLD_THRESHOLD then
            openChat()
        end
        chatCKeyFrames = 0
    end


if menuOpen and uiMouseMode then
        local mEntry  = activeMenuEntry()
        local hovered = mEntry and mEntry.hoverIndex and mEntry.def.items[mEntry.hoverIndex]
        local onChat  = hovered ~= nil and hovered.id == "chat"
        if onChat and KeyIsHeld(ActionID.MOUSE_CLICK_LEFT) then
            chatItemHeldFrames = chatItemHeldFrames + 1
            if chatItemHeldFrames == CHAT_C_HOLD_THRESHOLD and not chatOverlayActive then
                openChatOverlay()
                chatItemOverlayByMouse = true
            end
        else
            if chatItemHeldFrames > 0 then
                if chatItemOverlayByMouse then
                    if chatOverlayActive then closeChatOverlay() end
                    chatItemOverlayByMouse = false
                elseif onChat and chatItemHeldFrames < CHAT_C_HOLD_THRESHOLD then
                    openChat()
                    closeMenu()
                end
            end
            chatItemHeldFrames = 0
        end
    elseif chatItemHeldFrames > 0 then

        if chatItemOverlayByMouse and chatOverlayActive then closeChatOverlay() end
        chatItemOverlayByMouse = false
        chatItemHeldFrames = 0
    end

    pollChatInput()

    if chatOpen and chatTabHintDS then
        TEN.Strings.ShowString(chatTabHintDS, 1/30)
    end



    pollNameInput()

    updateChatLog()

    if menuOpen then
        menuIdleFrames = menuIdleFrames + 1
        if menuIdleFrames >= MENU_AUTO_CLOSE_FRAMES then
            closeMenu()
        end
    end


    local mHeld = KeyIsHeld(ActionID.M) or (azertyMode and KeyIsHeld(ActionID.SEMICOLON))

if xCycleUIActive then mHeld = false end

local holdSpot = HS.spots["menu_hold_hide"]
    if holdSpot and holdSpot.heldFrames > 0 then mHeld = true end
    if not mHeld then

        if menuHoldFrames > 0 then
            menuHoldFrames = 0
            MPUI.HideNotification("menu_hide_warn")
        end
        menuHoldConsumed = false
    elseif not menuHoldConsumed then
        menuHoldFrames = menuHoldFrames + 1
        if menuHoldFrames >= MENU_HIDE_WARN_FRAMES then
            menuHoldFrames   = 0
            menuHoldConsumed = true
            if menuHintVisible then
                menuHintVisible = false
                if menuHintDS then
                    pcall(function() TEN.Strings.HideString(menuHintDS) end)
                    menuHintDS = nil
                    HS.unregister("menu_btn")
                end
                if menuHoldHintDS then
                    pcall(function() TEN.Strings.HideString(menuHoldHintDS) end)
                    menuHoldHintDS = nil
                    HS.unregister("menu_hold_hide")
                end

else
                menuHintVisible = true
            end
            MPUI.HideNotification("menu_hide_warn")
        elseif menuHoldFrames >= MENU_HIDE_START_FRAME
            and (menuHoldFrames - MENU_HIDE_START_FRAME) % 30 == 0 then
            local secondsLeft = math.ceil((MENU_HIDE_WARN_FRAMES - menuHoldFrames) / 30)
            local msg = menuHintVisible and "Hiding visual hint in... " or "Showing visual hint in... "
            MPUI.ShowNotification("menu_hide_warn", msg .. secondsLeft,
                35, { color = Color(255, 220, 80) })
        end
    end

    updateMenuBtn()
    updateRestartHint()
    updateRecordsPanel()
    updateMenuBoard()
    updateCycleBoard()

    if menuOpen then renderMenuStack() end


if menuOpen or xCycleUIActive then
        local mouse = TEN.Input.GetMouseDisplayPosition()
        TEN.View.DisplaySprite(
            TEN.Objects.ObjID.DEFAULT_SPRITES, MENU_CURSOR.SPRITE_ID,
            Vec2(mouse.x + MENU_CURSOR.HOTSPOT.x, mouse.y + MENU_CURSOR.HOTSPOT.y),
            0, MENU_CURSOR.SCALE
        ):Draw()
    end


    if joinLeaveFrames > 0 then
        joinLeaveFrames = joinLeaveFrames - 1
        if joinLeaveFrames == 0 then
            if joinLeaveDS then
                pcall(function() TEN.Strings.HideString(joinLeaveDS) end)
                joinLeaveDS = nil
            end
        else
            if joinLeaveDS then TEN.Strings.ShowString(joinLeaveDS, 1/30) end
        end
    end
    if joinLeaveFrames == 0 and #joinLeaveQueue > 0 then
        local entry = table.remove(joinLeaveQueue, 1)
        local px, py = TEN.Util.PercentToScreen(DEFAULT_POS_X, JOIN_LEAVE_POS_Y)
        joinLeaveDS = TEN.Strings.DisplayString(entry.text, Vec2(px, py), DEFAULT_SCALE, entry.color, false,
            { TEN.Strings.DisplayStringOption.CENTER, TEN.Strings.DisplayStringOption.SHADOW })
        TEN.Strings.ShowString(joinLeaveDS)
        joinLeaveFrames = JOIN_LEAVE_DURATION
    end

end

function MPUI.Cleanup()
    for _, notif in pairs(activeNotifs) do
        pcall(function() TEN.Strings.HideString(notif.ds) end)
    end
    activeNotifs = {}

    closeChat()
    for _, msg in ipairs(chatLog) do
        pcall(function() TEN.Strings.HideString(msg.ds) end)
    end
    chatLog = {}
    pendingChatMsg = nil
    pendingChatGlobal = false

    if chatHeaderDS then
        pcall(function() TEN.Strings.HideString(chatHeaderDS) end)
        chatHeaderDS = nil
    end

    closeMenu()
    stagedMenuCommands = {}
    readyMenuCommands  = {}
    if menuHintDS then
        pcall(function() TEN.Strings.HideString(menuHintDS) end)
        menuHintDS = nil
    end
    if menuHoldHintDS then
        pcall(function() TEN.Strings.HideString(menuHoldHintDS) end)
        menuHoldHintDS = nil
    end


    HS.spots         = {}
    HS.flash         = {}
    HS.clicks        = {}
    HS.clickConsumed = false
    HS.enterConsumed = false
    HS.leftConsumed  = false
    HS.menuBtnClicked = false

    closeNameInput()
    pendingNameChange = nil



cycleBoardProgress = 0

    if joinLeaveDS then
        pcall(function() TEN.Strings.HideString(joinLeaveDS) end)
        joinLeaveDS = nil
    end
    joinLeaveQueue  = {}
    joinLeaveFrames = 0
end



return MPUI
