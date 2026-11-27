-- NOTE: This module will be rewritten at some point for clean code purposes. It is completely functional despite its current flaws.
--
-- LuaSocket is not included. For multiplayer to work, it has to be added by hand:
--   Scripts folder: socket.lua, ftp.lua, http.lua and smtp.lua.
--   Bin folder, in both x64 and x86: socket.dll, mime.dll and socket\core.lua.
-- Without them the module stays offline and prints that LuaSocket is not available.


local Multiplayer = {}

-- Declares multiplayer to the engine, switched on by default. The player decides in the options menu, and a script cannot switch multiplayer back on after the player switched it off. It can run from Gameflow.lua or from a level script; a declaration from a level script ends with that level. -- divezzi, 24.11.26
-- Without any declaration there is no Multiplayer entry in the options menu and the engine refuses LuaSocket. Flow.EnableMultiplayer(false) lists the entry too, with multiplayer off until the player switches it on. -- divezzi, 24.11.26
function Multiplayer.EnableGate()
    TEN.Flow.EnableMultiplayer(true)
end

-- LuaSocket is loaded at the first connect, not here: the engine refuses it while multiplayer is not in effect, and a level script may request multiplayer after it required this module. -- divezzi, 24.11.26
local socket = nil
local socketErrorPrinted = false

local function getSocket()
    if socket then return socket end

    local loaded, library = pcall(require, "socket")
    if loaded then
        socket = library
    elseif not socketErrorPrinted then
        socketErrorPrinted = true
        print("[MP] LuaSocket is not available, multiplayer stays offline: " .. tostring(library))
    end

    return socket
end

local MPUI = require("Engine.MPUI")

GlobalVars.Engine = GlobalVars.Engine or {}
GlobalVars.Engine.Multiplayer = GlobalVars.Engine.Multiplayer or {}

local state = {}

LevelFuncs.Engine = LevelFuncs.Engine or {}
LevelFuncs.Engine.Multiplayer = LevelFuncs.Engine.Multiplayer or {}

-- Replace this with the address of the server (for example the Hamachi IP of the machine that runs it), or call SetHost().
local DEFAULT_HOST = "HAMACHI-IP"
local DEFAULT_PORT = 10001
local DEBUG = true


-- The game version that is announced to the server. A server that checks versions needs it set with SetVersion(). It is not sent while nil.
local gameVersion = nil

local POS_CHANGE_THRESHOLD = 0.05
local ROT_CHANGE_THRESHOLD = 0.05
local FORCE_SYNC_INTERVAL = 1
local DISCONNECTED_MSG_INTERVAL = 30 * 60
local PENDING_ID_TIMEOUT        = 90
local MAX_CONNECT_ATTEMPTS      = 2

-- Number of position updates of a remote player that are held back before they are shown, to even out the arrival of the packets. 0 shows every update at once. -- divezzi, 21.11.26
local JITTER_TICKS = 0

-- The updates are held back only while the local camera moves, which is when an uneven step of a remote player is visible. -- divezzi, 21.11.26
local JITTER_ONLY_WHEN_CAMERA_MOVES = true
local JITTER_CAMERA_EPSILON         = 1.0
local JITTER_CAMERA_HOLD_TICKS      = 15

-- NOTE: Diagnostic for the judder test, prints the number of position updates that arrived in a tick when it is not one. Set it to false or remove it once the test is settled. -- divezzi, 21.11.26
local LOG_RX_TICKS = false

-- Sound 0 (SFX_TR4_LARA_FOOTSTEPS, the footstep of stone and concrete) is also the value that means "no sound" in this script, so it travels as SOUND_WIRE_ALIAS_FOOTSTEPS and is turned back into 0 where it is played.
-- The Obbyv9 script sends the same alias, so the value must stay the same. It is above every sound id and the sound map, so an untranslated alias would be logged as a missing sound instead of playing another sample. -- divezzi, 21.11.26
local SOUND_WIRE_ALIAS_FOOTSTEPS = 100000
local SOUND_WIRE_ALIAS_TARGET    = 0

-- Remote sounds that several players can play at the same moment, so playing one must not stop the copy that another player already has running (StopSound() is global).
-- The values are the footstep sounds of the floor materials (SFX_TR4_LARA_FOOTSTEPS, SFX_TR4_LARA_WET_FEET, SFX_TR4_LARA_FOOTSTEPS_SAND to _SNOW, SFX_TEN_CUSTOM_FOOTSTEPS_1 to _8), keyed by the sound that is played, after the alias is turned back. -- divezzi, 21.11.26
local OVERLAPPING_SOUNDS = {
    [0]    = true,
    [17]   = true,
    [185]  = true,
    [288]  = true,
    [290]  = true,
    [291]  = true,
    [292]  = true,
    [293]  = true,
    [294]  = true,
    [370]  = true,
    [371]  = true,
    [1150] = true,
    [1151] = true,
    [1152] = true,
    [1153] = true,
    [1154] = true,
    [1155] = true,
    [1156] = true,
    [1157] = true,
}

-- Held position updates by player ID (oldest first), the number of updates held back right now, and the state of the camera check.
local jitter = { queues = {}, playing = {}, depth = 0, cameraMovedFrame = -JITTER_CAMERA_HOLD_TICKS, lastCamera = nil }

local NAME_MAX_DIST             = 4 * 1024
local NAME_BASE_SCALE           = 0.45
local NAME_SCALE_LOSS_PER_BLOCK = 0.05

local HP_BAR_Y_OFFSET = -1.8
local HP_BAR_BG_W     = 9.0
local HP_BAR_BG_H     = 25.0
local HP_BAR_FILL_W   = 8.7
local HP_BAR_FILL_H   = 24.4
local HP_BAR_BORDER   = 0.15
local HP_MAX          = 1000

local HP_BLINK_THRESHOLD = 250

local SPRITE_TYPING_FRAME   = 2
local SPRITE_TYPING_SCALE_W = 6.3
local SPRITE_TYPING_SCALE_H = 6.3
local SPRITE_TYPING_Y_NUDGE = -5.0


local FREEZE_SPRITES_ENABLED = true
local FREEZE_SPRITE_SLOT     = TEN.Objects.ObjID.DEFAULT_SPRITES

local FREEZE_SPRITE_IDS      = {
    Paused  = "TODO",
    Saving  = "TODO",
    Loading = "TODO",
}
local FREEZE_SPRITE_SCALE    = 10.0
local FREEZE_SPRITE_Y_NUDGE  = -4.0
local FREEZE_MAX_DIST        = NAME_MAX_DIST
local FREEZE_LABEL_Y_NUDGE   = 3.5

local FREEZE_LABELS          = { PAUSE = "Paused", SAVING = "Saving", LOADING = "Loading" }


local notifiedJoin = {}

local pendingJLLeave = false


local formerSelfIds   = {}
local pendingPreviousId = nil
local FORMER_SELF_TTL = 3600

-- Set by Disconnect(): a socket that this script closed leaves no ghost on the server, and the server hands its id to the next player. The next Connect() must then suppress no id. A lost connection does not go through Disconnect(), so it still arms the suppression. -- divezzi, 24.11.26
local leftOnPurpose = false

-- NOTE: The default factory is the only place in the script that uses the names the engine accepts for a remote player today. -- divezzi, 20.11.26
local puppetFactory = {
    Create = function(id, pos, rot)
        return TEN.Objects.Moveable(TEN.Objects.ObjID.LARA_DOUBLE, "lara_double_" .. id, pos, rot)
    end,
    Name = function(id)
        return "lara_double_" .. id
    end,
    Apply = function(puppet, remoteTable)
        puppet:SetLaraDoubleState(remoteTable)
    end,
    Read = function(puppet)
        return puppet:GetLaraDoubleState()
    end,
    Destroy = function(puppet)
        puppet:Destroy()
    end
}

-- Replaces the object that stands for a remote player. Every member is optional and falls back to the default. Create(id, pos, rot) returns the object or nil, Name(id) returns the name used in logs, Apply(puppet, remoteTable) shows the received state, Read(puppet) returns the shown state (handStatus, flareControlLeft, isUsingBinoculars) and Destroy(puppet) removes it. The object must not depend on the animation state or the items of the local game. -- divezzi, 20.11.26
function Multiplayer.SetPuppetFactory(factory)
    for _, member in ipairs({ "Create", "Name", "Apply", "Read", "Destroy" }) do
        if factory and factory[member] then
            puppetFactory[member] = factory[member]
        end
    end
end

-- When the engine clears the remote players (the option was switched off) it invalidates their handles, and Destroy() on an invalid handle does nothing. -- divezzi, 21.11.26
local function destroyPuppet(puppet)
    return pcall(puppetFactory.Destroy, puppet)
end

-- True when the handle of a puppet was invalidated by the engine, for example when the pool was cleared while no script frame ran. A custom puppet without GetValid() counts as valid. -- divezzi, 21.11.26
local function isPuppetInvalid(puppet)
    local ok, isValid = pcall(function() return puppet:GetValid() end)
    return ok and isValid == false
end

local function initState()
    state.client = nil
    state.pendingClient  = nil
    state.pendingIdTimer = 0
    state.isConnected = false
    state.myId = nil
    jitter.queues = {}
    jitter.playing = {}
    jitter.depth = 0
    jitter.lastCamera = nil
    state.remotePlayers    = state.remotePlayers or {}
    state.remoteVehicles   = state.remoteVehicles or {}
    state.remoteVehicleFailLogged = state.remoteVehicleFailLogged or {}
    state.torchFlames      = state.torchFlames or {}
    state.torchPositions   = state.torchPositions or {}
    state.torchResendCount = 0
    state.firePositions    = state.firePositions or {}
    state.fireResendCount  = 0
    state.remotePlayerRoom = state.remotePlayerRoom or {}

state.remotePlayerLevel = state.remotePlayerLevel or {}

state.remotePlayerLevelName = state.remotePlayerLevelName or {}
    state.firstSeenFrame    = {}
    state.frameCounter = state.frameCounter or 0
    state.lastSentState = nil
    state.pendingSoundID = 0
    state.lastSentSoundID = 0
    state.lastReceivedSoundID = state.lastReceivedSoundID or {}
    state.lastSoundFrame = state.lastSoundFrame or {}
    state.host = state.host or DEFAULT_HOST
    state.port = state.port or DEFAULT_PORT
    state.myName = nil

state.customName = state.customName or GlobalVars.Engine.Multiplayer.customName or nil
    -- The PvP choice stays when the module connects again, for example after Options > Multiplayer is switched off and on. -- divezzi, 21.11.26
    state.pvpEnabled = state.pvpEnabled == true

state.pvpForced          = state.pvpForced == true

state.pvpHarmless          = state.pvpHarmless == true
    state.pvpWindowUntil       = nil
    state.pvpWindowPrevEnabled = false
    state.pvpWindowPrevForced  = false
    state.pvpWindowApplying    = nil
    state.lastPvPAttackerId  = nil
    state.lastPvPAttackFrame = nil
    state.pendingHits = {}
    state.pendingPvPReqs  = {}
    state.pendingPvPReq   = nil
    state.pendingPvPAllow = nil
    state.disconnectedMsgTimer = state.disconnectedMsgTimer or 0
    state.connectAttempts      = state.connectAttempts or 0
    state.connectGaveUp        = state.connectGaveUp or false

state.spawnFailFrame = state.spawnFailFrame or {}

    notifiedJoin   = {}
    pendingJLLeave = false
end


initState()

local SPAWN_RETRY_INTERVAL = 60

local LEVEL_ANNOUNCE_BURST = 5
local LEVEL_UNKNOWN_GRACE  = 90
local LEVEL_REQ_COOLDOWN   = 60

local pvpLastKnownHP = {}
local PVP_FLAT_DAMAGE_BONUS = 8


local PVP_DOUBLE_HP = 1000

local function pvpRearmDouble(hitDouble, hitIndex)
    pcall(function() hitDouble:SetHP(PVP_DOUBLE_HP) end)
    pvpLastKnownHP[hitIndex] = PVP_DOUBLE_HP
end

LevelFuncs.Engine.Multiplayer.OnPvPHit = function(hitDouble)
    local hitIndex = hitDouble:GetIndex()
    local currentHP = hitDouble:GetHP()
    local prevHP = pvpLastKnownHP[hitIndex] or PVP_DOUBLE_HP
    local realDamage = prevHP - currentHP

    if realDamage <= 0 then
        pvpRearmDouble(hitDouble, hitIndex)
        return
    end


    if not state.pvpEnabled then
        pvpRearmDouble(hitDouble, hitIndex)
        return
    end

    local finalDamage = realDamage + PVP_FLAT_DAMAGE_BONUS


local hitId = nil
    for id, player in pairs(state.remotePlayers) do
        if player:GetIndex() == hitIndex then
            hitId = id
            break
        end
    end

    if hitId == nil then
        pvpRearmDouble(hitDouble, hitIndex)
        if DEBUG then
            print("[PVP_HIT_LUA] WARNING: OnPvPHit could not find player for index=" .. tostring(hitIndex))
        end
        return
    end


if Multiplayer.IsPvPDamageHarmlessFor(hitId) then
        pvpRearmDouble(hitDouble, hitIndex)
        if DEBUG then
            print("[PVP_HIT_LUA] hit on player " .. hitId .. " dealt no damage (non-consensual PvP)")
        end
        return
    end

    pvpRearmDouble(hitDouble, hitIndex)
    table.insert(state.pendingHits, { targetId = hitId, damage = finalDamage })
    if DEBUG then
        print("[PVP_HIT_LUA] OnHit fired! targetId=" .. hitId .. " realDamage=" .. realDamage .. " finalDamage=" .. finalDamage .. " (HP " .. prevHP .. " -> " .. currentHP .. ")")
    end
end


local SCRIPT_NAME_MAX = 20
local ENGINE_NAME_MAX = 20

local function sanitizeName(name, maxLen)
    if not name or type(name) ~= "string" or #name == 0 then return nil end
    if #name > maxLen then
        return name:sub(1, maxLen)
    end
    return name
end

local function splitString(inputstr, sep)
    if sep == nil then sep = "%s" end
    local t = {}
    for str in string.gmatch(inputstr, "([^" .. sep .. "]+)") do
        table.insert(t, str)
    end
    return t
end

local nameDisplays = {}
local remotePlayerHP      = {}
local remotePlayerTorchLit = {}
local remotePlayerOnFire   = {}
local remotePlayerNames   = {}
local remotePvPState      = {}
local remotePlayerTyping  = {}
local remotePlayerShooting = {}
local remotePlayerFrozen  = {}
local freezeStrings       = {}
local freezeSpritesAvailable = nil
local lastAppliedPos      = {}

local lastAppliedRoom     = {}

local spawnWarmupUntil    = {}


local showPlayerInfo = true

local function cleanupAllNames()
    for id, entry in pairs(nameDisplays) do
        pcall(function() TEN.Strings.HideString(entry.ds) end)
    end
    nameDisplays = {}
    remotePlayerHP     = {}

remotePvPState          = {}
    remotePlayerTyping      = {}
    for id, entry in pairs(freezeStrings) do
        pcall(function() TEN.Strings.HideString(entry.ds) end)
    end
    freezeStrings           = {}
    remotePlayerFrozen      = {}
    lastAppliedPos          = {}
    lastAppliedRoom         = {}
    state.remotePlayerRoom  = {}
end

local NAME_POOL = {
    "Kurtis",   "Winston",  "Natla",    "Larson",
    "Pierre",   "Qualopec", "Tihocan",  "Sophia",   "Von Croy",
    "Zip",      "Alister",  "Amanda",   "Jean-Yves","Willard",
}

local function resolvePoolName(id)
    local index = ((id - 1) % #NAME_POOL) + 1
    local baseName = NAME_POOL[index]
    local cycle = math.floor((id - 1) / #NAME_POOL)
    if cycle > 0 then
        return baseName .. " " .. (cycle + 1)
    end
    return baseName
end


local nameDupeSfx  = {}
local nameSetDirty = true

local function baseNameFor(id)
    if id == state.myId then
        return state.customName or state.myName or resolvePoolName(id)
    end

return remotePlayerNames[id] or resolvePoolName(id)
end

local function dupeSuffixFor(id)
    if not id then return "" end
    if nameSetDirty then
        nameSetDirty = false
        nameDupeSfx  = {}
        local holders = {}
        local ids     = {}
        if state.myId then ids[#ids + 1] = state.myId end
        for pid in pairs(state.remotePlayers) do
            if pid ~= state.myId then ids[#ids + 1] = pid end
        end
        for i = 1, #ids do
            local n = baseNameFor(ids[i])
            if n then
                holders[n] = holders[n] or {}
                table.insert(holders[n], ids[i])
            end
        end
        for _, held in pairs(holders) do
            if #held > 1 then
                for i = 1, #held do nameDupeSfx[held[i]] = " (" .. held[i] .. ")" end
            end
        end
    end
    return nameDupeSfx[id] or ""
end


local function displayNameFor(id)
    if not id then return nil end
    return baseNameFor(id) .. dupeSuffixFor(id)
end

local function updatePlayerNames()
    if not showPlayerInfo then
        for id, entry in pairs(nameDisplays) do
            if entry.visible then
                pcall(function() TEN.Strings.HideString(entry.ds) end)
                entry.visible = false
            end
        end
        return
    end

    local laraPos = Lara:GetPosition()

    for id, player in pairs(state.remotePlayers) do
        local displayName = displayNameFor(id)

        if not nameDisplays[id] or nameDisplays[id].name ~= displayName then
            if nameDisplays[id] then
                pcall(function() TEN.Strings.HideString(nameDisplays[id].ds) end)
            end
            local ok, ds = pcall(function()
                return TEN.Strings.DisplayString(displayName, Vec2(0, 0), NAME_BASE_SCALE, Color(255, 255, 255), false, {TEN.Strings.DisplayStringOption.CENTER})
            end)
            if ok and ds then
                nameDisplays[id] = { ds = ds, visible = false, name = displayName }
            end
        end

        local entry = nameDisplays[id]
        if entry then
            local shouldShow = false
            local displayPos = nil
            local dist = 0

            pcall(function()
                local pos = player:GetPosition()

                local dx = pos.x - laraPos.x
                local dy = pos.y - laraPos.y
                local dz = pos.z - laraPos.z
                dist = math.sqrt(dx * dx + dy * dy + dz * dz)

                if dist <= NAME_MAX_DIST then
                    local aboveHead = Vec3(pos.x, pos.y - 900, pos.z)
                    displayPos = TEN.Util.GetDisplayPosition(aboveHead)
                    if displayPos then
                        shouldShow = true
                    end
                end
            end)

            if shouldShow and displayPos then
                local px, py = TEN.Util.PercentToScreen(displayPos.x, displayPos.y)
                entry.ds:SetPosition(Vec2(px, py))

                local blocks = dist / 1024
                local scale = NAME_BASE_SCALE * (1.0 - NAME_SCALE_LOSS_PER_BLOCK * blocks)
                if scale < 0.05 then scale = 0.05 end
                entry.ds:SetScale(scale)

                if not entry.visible then
                    TEN.Strings.ShowString(entry.ds)
                    entry.visible = true
                end

                local distFactor = scale / NAME_BASE_SCALE

                local hp = remotePlayerHP[id] or HP_MAX
                local doubleInDanger = hp < (HP_MAX * 0.30)

local showHPBar = doubleInDanger
                if Multiplayer.HasLocalPvPConsent() then
                    showHPBar = showHPBar or (remotePlayerShooting[id] == true)
                    pcall(function()
                        local doubleState = puppetFactory.Read(player)
                        local weaponDrawn = (doubleState.handStatus or 0) ~= 0
                        local isFlare     = doubleState.flareControlLeft == true
                        local isBinos     = doubleState.isUsingBinoculars == true
                        showHPBar = showHPBar or (weaponDrawn and not isFlare and not isBinos)
                    end)
                end

                if showHPBar then
                    local hpRatio = math.max(0, math.min(hp / HP_MAX, 1.0))

                    local bgW = HP_BAR_BG_W * distFactor
                    local bgH = HP_BAR_BG_H * distFactor
                    local barLeftX = displayPos.x - bgW / 2
                    local barY = displayPos.y + HP_BAR_Y_OFFSET * distFactor

                    local posBg = TEN.Vec2(barLeftX, barY)
                    local scaleBg = TEN.Vec2(bgW, bgH)
                    local bgSprite = TEN.DisplaySprite(TEN.Objects.ObjID.CUSTOM_BAR_GRAPHICS, 0, posBg, 0, scaleBg, TEN.Color(255, 255, 255))
                    bgSprite:Draw(0, TEN.View.AlignMode.CENTER_LEFT, TEN.View.ScaleMode.FIT, TEN.Effects.BlendID.ALPHABLEND)

                    local blinkVisible = not doubleInDanger or ((state.frameCounter % 8) < 4)
                    if blinkVisible then
                        local border = HP_BAR_BORDER * distFactor
                        local fillW = HP_BAR_FILL_W * distFactor * hpRatio
                        local fillH = HP_BAR_FILL_H * distFactor

                        local posBar = TEN.Vec2(barLeftX + border, barY)
                        local scaleBar = TEN.Vec2(fillW, fillH)
                        local barSprite = TEN.DisplaySprite(TEN.Objects.ObjID.CUSTOM_BAR_GRAPHICS, 1, posBar, 0, scaleBar, TEN.Color(208, 0, 0))
                        barSprite:Draw(1, TEN.View.AlignMode.CENTER_LEFT, TEN.View.ScaleMode.FIT, TEN.Effects.BlendID.ALPHABLEND)
                    end
                end

                if remotePlayerTyping[id] then
                    local spriteY     = displayPos.y + (HP_BAR_Y_OFFSET + SPRITE_TYPING_Y_NUDGE) * distFactor
                    local sw          = SPRITE_TYPING_SCALE_W * distFactor
                    local sh          = SPRITE_TYPING_SCALE_H * distFactor
                    local spritePos   = TEN.Vec2(displayPos.x, spriteY)
                    local spriteScale = TEN.Vec2(sw, sh)
                    local typingSprite = TEN.DisplaySprite(
                        TEN.Objects.ObjID.INTERACTION_SPRITES, SPRITE_TYPING_FRAME,
                        spritePos, 0, spriteScale, TEN.Color(255, 255, 255))
                    typingSprite:Draw(2, TEN.View.AlignMode.CENTER, TEN.View.ScaleMode.FIT, TEN.Effects.BlendID.ALPHABLEND)
                end
            else
                if entry.visible then
                    pcall(function() TEN.Strings.HideString(entry.ds) end)
                    entry.visible = false
                end
            end
        end
    end

    for id, entry in pairs(nameDisplays) do
        if not state.remotePlayers[id] then
            pcall(function() TEN.Strings.HideString(entry.ds) end)
            nameDisplays[id]        = nil
            remotePlayerHP[id]      = nil
            remotePlayerTorchLit[id] = nil
            remotePlayerOnFire[id]  = nil
            remotePlayerTyping[id]  = nil
            remotePlayerShooting[id] = nil
        end
    end
end


local function updateFreezeIndicators()
    if not FREEZE_SPRITES_ENABLED then
        for id, entry in pairs(freezeStrings) do
            if entry.visible then
                pcall(function() TEN.Strings.HideString(entry.ds) end)
                entry.visible = false
            end
        end
        return
    end

    local okLp, laraPos = pcall(function() return Lara:GetPosition() end)
    if not (okLp and laraPos) then return end

    for id, label in pairs(remotePlayerFrozen) do
        local double = state.remotePlayers[id]
        local pos    = nil
        if double then
            local okp, p = pcall(function() return double:GetPosition() end)
            if okp then pos = p end
        end

        local inRange, displayPos, dist = false, nil, 0
        if pos then
            local dx, dy, dz = pos.x - laraPos.x, pos.y - laraPos.y, pos.z - laraPos.z
            dist = math.sqrt(dx * dx + dy * dy + dz * dz)
            local aboveHead = Vec3(pos.x, pos.y - 900, pos.z)
            displayPos = TEN.Util.GetDisplayPosition(aboveHead)
            inRange = (displayPos ~= nil) and (dist <= FREEZE_MAX_DIST)
        end


local usedSprite = false
        local frame = FREEZE_SPRITE_IDS[label]
        if inRange and type(frame) == "number" and freezeSpritesAvailable ~= false then
            local distFactor = math.max(0.05, 1.0 - dist / FREEZE_MAX_DIST)
            local s          = FREEZE_SPRITE_SCALE * distFactor
            local spriteY    = displayPos.y + FREEZE_SPRITE_Y_NUDGE * distFactor
            local okDraw = pcall(function()
                TEN.View.DisplaySprite(
                    FREEZE_SPRITE_SLOT, frame,
                    Vec2(displayPos.x, spriteY), 0, Vec2(s, s),
                    Color(255, 255, 255)
                ):Draw(10, TEN.View.AlignMode.CENTER, TEN.View.ScaleMode.FIT, TEN.Effects.BlendID.ALPHABLEND)
            end)
            if freezeSpritesAvailable == nil then freezeSpritesAvailable = okDraw end
            usedSprite = okDraw
        end

        if usedSprite then
            local entry = freezeStrings[id]
            if entry and entry.visible then
                pcall(function() TEN.Strings.HideString(entry.ds) end)
                entry.visible = false
            end
        else

            if not freezeStrings[id] or freezeStrings[id].label ~= label then
                if freezeStrings[id] then
                    pcall(function() TEN.Strings.HideString(freezeStrings[id].ds) end)
                end
                local okDs, ds = pcall(function()
                    return TEN.Strings.DisplayString(label, Vec2(0, 0), NAME_BASE_SCALE,
                        Color(255, 160, 0), false, { TEN.Strings.DisplayStringOption.CENTER })
                end)
                if okDs and ds then
                    freezeStrings[id] = { ds = ds, visible = false, label = label }
                end
            end
            local entry = freezeStrings[id]
            if entry then
                if inRange then
                    local blocks = dist / 1024
                    local scale  = NAME_BASE_SCALE * (1.0 - NAME_SCALE_LOSS_PER_BLOCK * blocks)
                    if scale < 0.05 then scale = 0.05 end

                    local labelY = displayPos.y + FREEZE_LABEL_Y_NUDGE * (scale / NAME_BASE_SCALE)
                    local px, py = TEN.Util.PercentToScreen(displayPos.x, labelY)
                    entry.ds:SetPosition(Vec2(px, py))
                    entry.ds:SetScale(scale)
                    if not entry.visible then
                        TEN.Strings.ShowString(entry.ds)
                        entry.visible = true
                    end
                elseif entry.visible then
                    pcall(function() TEN.Strings.HideString(entry.ds) end)
                    entry.visible = false
                end
            end
        end
    end


    for id, entry in pairs(freezeStrings) do
        if not remotePlayerFrozen[id] then
            pcall(function() TEN.Strings.HideString(entry.ds) end)
            freezeStrings[id] = nil
        end
    end
end

-- Reader of the local player, the default source of the state that is sent. Everything the sender needs from the game comes from a reader, so a script can replace it with SetReader().
-- The table that a reader returns is called localTable: it describes the local player or a moveable of the level and only leaves the game, it is never applied back. The table built from a received packet is called remoteTable and is applied to the remote player only. -- divezzi, 21.11.26
local function readLara(lara)
    local objID = lara:GetObjectID()
    if objID ~= 0 then
        if DEBUG then print("[MP] The Lara reader was given object ID " .. objID .. ".") end
        return nil
    end

    local ok, localTable = pcall(function() return lara:GetFullState() end)
    if not ok or not localTable then
        if DEBUG then print("[MP] GetFullState failed.") end
        return nil
    end

    local pos = lara:GetPosition()
    local rot = lara:GetRotation()
    localTable.posX = pos.x
    localTable.posY = pos.y
    localTable.posZ = pos.z
    localTable.roomNumber = lara:GetRoomNumber()
    localTable.rotY = rot.y
    localTable.rotX = rot.x or 0
    localTable.rotZ = rot.z or 0
    localTable.health = lara:GetHP()
    localTable.ammo = lara:GetAmmoCount()
    localTable.inputForward = KeyIsHeld(ActionID.FORWARD)
    localTable.inputBack = KeyIsHeld(ActionID.BACK)
    localTable.inputLeft = KeyIsHeld(ActionID.LEFT)
    localTable.inputRight = KeyIsHeld(ActionID.RIGHT)
    localTable.inputJump = KeyIsHeld(ActionID.JUMP)
    localTable.inputAction = KeyIsHeld(ActionID.ACTION)
    localTable.inputCrouch = KeyIsHeld(ActionID.CROUCH)
    localTable.inputWalk = KeyIsHeld(ActionID.WALK)
    localTable.inputSprint = KeyIsHeld(ActionID.SPRINT)
    localTable.inputRoll = KeyIsHeld(ActionID.ROLL)
    localTable.inputLook = KeyIsHeld(ActionID.LOOK)
    localTable.inputFlare = KeyIsHeld(ActionID.FLARE)
    localTable.inputStepLeft = false
    localTable.inputStepRight = false
    localTable.inputDraw = KeyIsHeld(ActionID.DRAW)
    localTable.torchIsLit = lara:IsTorchLit()
    localTable.isOnFire   = (lara:GetEffect() == TEN.Effects.EffectID.FIRE)
    localTable.vehicle = lara:GetVehicle()

    local armsOk, arms = pcall(function()
        return {
            left = lara:GetLeftArmOrientation(),
            right = lara:GetRightArmOrientation(),
            target = lara:GetTargetArmOrientation(),
            leftLocked = lara:GetLeftArmLocked(),
            rightLocked = lara:GetRightArmLocked()
        }
    end)
    if armsOk then
        localTable.arms = arms
    elseif DEBUG then
        print("[MP] Reading the arm orientations failed: " .. tostring(arms))
    end

    local headOk, headRot = pcall(function() return lara:GetExtraHeadRotation() end)
    if headOk then
        localTable.headRot = headRot
    elseif DEBUG then
        print("[MP] Reading the head rotation failed: " .. tostring(headRot))
    end

    if localTable.torchIsLit then
        local torchOk, torchPos = pcall(function() return lara:GetJointPosition(13, Vec3(-10, 54, 156)) end)
        if torchOk then
            localTable.torchPos = torchPos
        end
    end

    if localTable.isOnFire then
        local fireOk, firePos = pcall(function() return lara:GetJointPosition(0, Vec3(0, 0, 0)) end)
        if fireOk then
            localTable.firePos = firePos
        end
    end

    return localTable
end

-- Reader of any other moveable, for example an Animating slot or an enemy. It sends the pose through the animation object, animation, frame and state numbers, so the puppet plays the same animation. Weapons, arms and input are left out; the puppet keeps its defaults for them. -- divezzi, 20.11.26
local function readMoveable(moveable)
    local pos = moveable:GetPosition()
    local rot = moveable:GetRotation()

    return {
        animObjectID = moveable:GetAnimSlot(),
        animNumber = moveable:GetAnim(),
        frameNumber = moveable:GetFrame(),
        activeState = moveable:GetState(),
        targetState = moveable:GetTargetState(),
        posX = pos.x,
        posY = pos.y,
        posZ = pos.z,
        rotX = rot.x or 0,
        rotY = rot.y,
        rotZ = rot.z or 0,
        roomNumber = moveable:GetRoomNumber(),
        health = moveable:GetHP()
    }
end

Multiplayer.Readers = {
    Lara = {
        GetMoveable = function() return Lara end,
        Read = readLara
    }
}

-- Returns a reader for the moveable that getMoveable() returns, for example Multiplayer.Readers.Moveable(function() return TEN.Objects.GetMoveableByName("my_npc") end).
function Multiplayer.Readers.Moveable(getMoveable)
    return {
        GetMoveable = getMoveable,
        Read = readMoveable,
        IsGeneric = true
    }
end

local activeReader = Multiplayer.Readers.Lara

-- Replaces the source of the state that is sent. A reader has GetMoveable(), which returns the moveable to describe (or nil), and Read(moveable), which returns the localTable that the sender expects. Without a call the local player is read. -- divezzi, 20.11.26
function Multiplayer.SetReader(reader)
    activeReader = reader or Multiplayer.Readers.Lara
    state.lastSentState = nil
end

local function getLocalState()
    local moveable = activeReader.GetMoveable()
    if not moveable then return nil end

    return activeReader.Read(moveable)
end

local function getLocalVehicleState(localTable)
    local vehicle = localTable.vehicle
    if not vehicle then return nil end

    local rot = vehicle:GetRotation()
    local vehicleState = {
        objectID = vehicle:GetObjectID(),
        posX = math.floor(vehicle:GetPosition().x),
        posY = math.floor(vehicle:GetPosition().y),
        posZ = math.floor(vehicle:GetPosition().z),
        rotX = rot.x,
        rotY = rot.y,
        rotZ = rot.z
    }

    if DEBUG and state.frameCounter % 60 == 0 then
        print(string.format("[Multiplayer VEH_SEND] ObjID=%d Rot=(%.2f, %.2f, %.2f)",
            vehicleState.objectID, vehicleState.rotX, vehicleState.rotY, vehicleState.rotZ))
    end

    return vehicleState
end

local function deserializeVehicleState(parts, startIndex)
    if not parts[startIndex] or parts[startIndex] ~= "VEH" then
        return nil
    end

    local objectID = tonumber(parts[startIndex + 1])
    local posX = tonumber(parts[startIndex + 2])
    local posY = tonumber(parts[startIndex + 3])
    local posZ = tonumber(parts[startIndex + 4])
    local rotX = tonumber(parts[startIndex + 5])
    local rotY = tonumber(parts[startIndex + 6])
    local rotZ = tonumber(parts[startIndex + 7])

    if not (objectID and posX and posY and posZ and rotX and rotY and rotZ) then
        if DEBUG then
            print("[Multiplayer VEH_ERROR] Invalid vehicle data at index " .. startIndex)
        end
        return nil
    end

    if DEBUG and state.frameCounter % 60 == 0 then
        print(string.format("[Multiplayer VEH_RECV] ObjID=%d Rot=(%.2f, %.2f, %.2f)",
            objectID, rotX, rotY, rotZ))
    end

    return {
        objectID = objectID,
        posX = posX,
        posY = posY,
        posZ = posZ,
        rotX = rotX,
        rotY = rotY,
        rotZ = rotZ
    }
end

local function updateRemoteVehicle(playerId, vehicleState)
    if not vehicleState then
        if state.remoteVehicles[playerId] then
            pcall(function() state.remoteVehicles[playerId]:Destroy() end)
            state.remoteVehicles[playerId] = nil
        end
        return
    end

    local vehicle = state.remoteVehicles[playerId]
    if vehicle and isPuppetInvalid(vehicle) then
        state.remoteVehicles[playerId] = nil
        vehicle = nil
    end

    if not vehicle then
        local vehicleName = "remote_vehicle_" .. playerId
        local okCreate, created = pcall(function()
            return TEN.Objects.CreateRemoteVehicle(
                vehicleState.objectID,
                vehicleName,
                Vec3(vehicleState.posX, vehicleState.posY, vehicleState.posZ),
                Rotation(vehicleState.rotX, vehicleState.rotY, vehicleState.rotZ)
            )
        end)

        if okCreate and created then
            state.remoteVehicles[playerId] = created
            vehicle = created
            print(string.format("[REMVEH] Created for player %d - ObjID=%d", playerId, vehicleState.objectID))
        else
            -- The creation is retried on every packet that carries a vehicle, so only the first failure of a player is logged. -- divezzi, 20.11.26
            state.remoteVehicleFailLogged = state.remoteVehicleFailLogged or {}
            if not state.remoteVehicleFailLogged[playerId] then
                state.remoteVehicleFailLogged[playerId] = true
                print(string.format("[REMVEH] Failed to create vehicle for player %d - ObjID=%d ok=%s err=%s",
                    playerId, vehicleState.objectID, tostring(okCreate), tostring(created)))
            end
            return
        end
    end

    pcall(function()
        vehicle:SetRemoteVehiclePose(
            Vec3(vehicleState.posX, vehicleState.posY, vehicleState.posZ),
            Rotation(vehicleState.rotX, vehicleState.rotY, vehicleState.rotZ)
        )
    end)
end

-- Vehicle sounds of a remote player are played at the position of its puppet, which is the rider. A looped sound is remembered for VEHICLE_LOOP_TTL ticks after its last packet and replayed each tick by updateRemoteVehicleLoops(). -- divezzi, 20.11.26
local VEHICLE_LOOP_TTL = 6

local function playRemoteVehicleSound(double, soundID, pitch, env)
    if not TEN.Objects.PlayRemoteVehicleSound then return end
    pcall(function()
        TEN.Objects.PlayRemoteVehicleSound(soundID, double:GetPosition(), pitch, env)
    end)
end

local function receiveRemoteVehicleSound(playerId, double, soundID, pitch, env, looped)
    if looped then
        state.remoteVehicleLoops = state.remoteVehicleLoops or {}
        local loops = state.remoteVehicleLoops[playerId]
        if not loops then
            loops = {}
            state.remoteVehicleLoops[playerId] = loops
        end
        loops[soundID] = { pitch = pitch, env = env, ttl = VEHICLE_LOOP_TTL }
    end

    playRemoteVehicleSound(double, soundID, pitch, env)
end

local function updateRemoteVehicleLoops()
    if not state.remoteVehicleLoops then return end

    for playerId, loops in pairs(state.remoteVehicleLoops) do
        local double = state.remotePlayers[playerId]
        for soundID, loop in pairs(loops) do
            loop.ttl = loop.ttl - 1
            if loop.ttl <= 0 or not double then
                loops[soundID] = nil
            else
                playRemoteVehicleSound(double, soundID, loop.pitch, loop.env)
            end
        end

        if next(loops) == nil then
            state.remoteVehicleLoops[playerId] = nil
        end
    end
end

local lastSentTyping     = nil
local lastSentName       = nil
local lastSentLevel      = nil
local levelAnnounceBurst = 0
local pendingLevelReq    = false

local lastResolvedLevelFile = nil


local function getCurrentLevelSafe()
    return TEN.Flow.GetCurrentLevel()
end
local lastLevelReqFrame  = -1000
local lastSentPvP        = nil
local lastSentHP         = nil
local lastSentTorchLit   = nil
local lastSentOnFire     = nil
local lastSentAimSuffix  = nil
local lastSentLookSuffix = nil
local lastSentRoom       = nil
local lastSentJL         = nil

local function sendPlayerData(additionalSuffixes)
    if not (state.isConnected and state.client) then return end

    local localTable = getLocalState()
    if not localTable then return end

    local stateChanged = false
    if not state.lastSentState then
        stateChanged = true
    else
        if (localTable.animNumber ~= state.lastSentState.animNumber or
            math.abs(localTable.posX - state.lastSentState.posX) > POS_CHANGE_THRESHOLD or
            math.abs(localTable.posY - state.lastSentState.posY) > POS_CHANGE_THRESHOLD or
            math.abs(localTable.posZ - state.lastSentState.posZ) > POS_CHANGE_THRESHOLD or
            math.abs(localTable.rotY - state.lastSentState.rotY) > ROT_CHANGE_THRESHOLD or
            math.abs(localTable.rotX - (state.lastSentState.rotX or 0)) > ROT_CHANGE_THRESHOLD or
            math.abs(localTable.rotZ - (state.lastSentState.rotZ or 0)) > ROT_CHANGE_THRESHOLD or
            localTable.inputForward ~= state.lastSentState.inputForward or
            localTable.inputBack ~= state.lastSentState.inputBack or
            localTable.inputLeft ~= state.lastSentState.inputLeft or
            localTable.inputRight ~= state.lastSentState.inputRight or
            localTable.inputJump ~= state.lastSentState.inputJump or
            localTable.inputAction ~= state.lastSentState.inputAction or
            localTable.inputCrouch ~= state.lastSentState.inputCrouch or
            localTable.inputWalk ~= state.lastSentState.inputWalk or
            localTable.inputSprint ~= state.lastSentState.inputSprint or
            localTable.inputRoll ~= state.lastSentState.inputRoll or
            localTable.inputFlare ~= state.lastSentState.inputFlare or
            localTable.inputDraw ~= state.lastSentState.inputDraw) then
            stateChanged = true
        end
    end

    local hasPendingHits = state.pendingHits and #state.pendingHits > 0
    local hasPendingChat = MPUI.GetPendingChatMessage ~= nil and MPUI.HasPendingChat()
    if not (stateChanged or hasPendingHits or hasPendingChat or (state.frameCounter % FORCE_SYNC_INTERVAL == 0)) then
        return
    end

    state.lastSentState = {
        posX = localTable.posX,
        posY = localTable.posY,
        posZ = localTable.posZ,
        rotY = localTable.rotY,
        rotX = localTable.rotX,
        rotZ = localTable.rotZ,
        animNumber = localTable.animNumber,
        inputForward = localTable.inputForward,
        inputBack = localTable.inputBack,
        inputLeft = localTable.inputLeft,
        inputRight = localTable.inputRight,
        inputJump = localTable.inputJump,
        inputAction = localTable.inputAction,
        inputCrouch = localTable.inputCrouch,
        inputWalk = localTable.inputWalk,
        inputSprint = localTable.inputSprint,
        inputRoll = localTable.inputRoll,
        inputFlare = localTable.inputFlare,
        inputDraw = localTable.inputDraw
    }

    local meshString = ""
    if localTable.meshIndices then
        meshString = table.concat(localTable.meshIndices, ",")
    end

    local vehicleState = getLocalVehicleState(localTable)
    local vehicleSuffix = ""
    if vehicleState then
        vehicleSuffix = string.format(":VEH:%d:%d:%d:%d:%.2f:%.2f:%.2f",
            vehicleState.objectID,
            vehicleState.posX,
            vehicleState.posY,
            vehicleState.posZ,
            vehicleState.rotX,
            vehicleState.rotY,
            vehicleState.rotZ
        )
    end

    local rotSuffix = ""
    if math.abs(localTable.rotX or 0) > 0.01 or math.abs(localTable.rotZ or 0) > 0.01 then
        rotSuffix = string.format(":ROT:%.2f:%.2f", localTable.rotX, localTable.rotZ)
    end

    local soundSuffix = ""
    if state.pendingSoundID ~= 0 then
        soundSuffix = ":SND:" .. state.pendingSoundID
    end
    state.pendingSoundID = 0

    -- Sounds of the vehicle of the local player. Four numbers per sound: id, pitch times 100, environment and looped flag.
    local vehicleSoundSuffix = ""
    if TEN.Objects.PopLocalVehicleSounds then
        local vehicleSounds = TEN.Objects.PopLocalVehicleSounds()
        for i = 1, #vehicleSounds - 3, 4 do
            vehicleSoundSuffix = vehicleSoundSuffix .. string.format(":VSND:%d:%d:%d:%d",
                vehicleSounds[i], vehicleSounds[i + 1], vehicleSounds[i + 2], vehicleSounds[i + 3])
        end
    end

    local aimSuffix = ""
    local gunsDrawn = (localTable.handStatus or 0) ~= 0
    if localTable.arms then
        local arms = localTable.arms
        local candidate = string.format(
            ":AIM:%.2f:%.2f:%.2f:%.2f:%.2f:%.2f:%.2f:%.2f:%.2f:%d:%d",
            arms.left.x, arms.left.y, arms.left.z,
            arms.right.x, arms.right.y, arms.right.z,
            arms.target.x, arms.target.y, arms.target.z,
            arms.leftLocked and 1 or 0, arms.rightLocked and 1 or 0
        )
        if gunsDrawn or candidate ~= lastSentAimSuffix then
            aimSuffix = candidate
            lastSentAimSuffix = candidate
        end
    end

    local lookSuffix = ""
    if localTable.headRot then
        local headRot = localTable.headRot
        local lookActive = math.abs(headRot.x) > 0.5 or math.abs(headRot.y) > 0.5

        if lookActive or localTable.isUsingBinoculars then
            local candidate = string.format(":LOOK:%d:%.2f:%.2f", 1, headRot.x * 2, headRot.y * 2)
            lookSuffix = candidate
            lastSentLookSuffix = candidate
        else
            local candidate = ":LOOK:0:0.00:0.00"
            if candidate ~= lastSentLookSuffix then
                lookSuffix = candidate
                lastSentLookSuffix = candidate
            end
        end
    end

    -- NOTE: SRC is sent only by a reader other than the local player. A parser that finds its tokens by keyword skips it, so other versions are not affected. -- divezzi, 20.11.26
    local sourceSuffix = ""
    if activeReader.IsGeneric then
        sourceSuffix = ":SRC:" .. (tonumber(localTable.animObjectID) or 0)
    end

    local holsterSuffix = ""
    if localTable.leftHolster or localTable.rightHolster or localTable.backHolster then
        holsterSuffix = string.format(":H:%d:%d:%d",
            tonumber(localTable.leftHolster) or 0,
            tonumber(localTable.rightHolster) or 0,
            tonumber(localTable.backHolster) or 0)
    end

    local hpSuffix = ""
    local currentHP = localTable.health or 0
    if currentHP ~= lastSentHP then
        hpSuffix = ":HP:" .. currentHP
        lastSentHP = currentHP
    end

    local flmSuffix = ""
    local currentTorchLit = localTable.torchIsLit and 1 or 0

    local forceTorchResend = (state.torchResendCount or 0) > 0
    if forceTorchResend then state.torchResendCount = state.torchResendCount - 1 end

    if currentTorchLit == 1 then
        local handPos = localTable.torchPos
        if handPos then
            flmSuffix = string.format(":FLM:1:%d:%d:%d",
                math.floor(handPos.x), math.floor(handPos.y), math.floor(handPos.z))
        else
            flmSuffix = ":FLM:1"
        end
        lastSentTorchLit = 1
    elseif currentTorchLit ~= lastSentTorchLit or forceTorchResend then
        flmSuffix = ":FLM:0"
        lastSentTorchLit = 0
    end

    -- The life of the flare is sent with every packet while a flare burns in the hand, in every pose, so that the remote player can light it with the settings of the level. The life is zero when no flare is lit, and the token is absent then. -- divezzi, 22.11.26
    local flrSuffix = ""
    local flareLife = tonumber(localTable.flareLife) or 0
    if flareLife > 0 then
        flrSuffix = ":FLR:" .. math.floor(flareLife)
    end

    local fireSuffix = ""
    local currentOnFire = localTable.isOnFire and 1 or 0
    local forceFireResend = (state.fireResendCount or 0) > 0
    if forceFireResend then state.fireResendCount = state.fireResendCount - 1 end

    if currentOnFire == 1 then
        local hipPos = localTable.firePos
        if hipPos then
            fireSuffix = string.format(":FIRE:1:%d:%d:%d",
                math.floor(hipPos.x), math.floor(hipPos.y), math.floor(hipPos.z))
        else
            fireSuffix = ":FIRE:1"
        end
        lastSentOnFire = 1
    elseif currentOnFire ~= lastSentOnFire or forceFireResend then
        fireSuffix = ":FIRE:0"
        lastSentOnFire = 0
    end


local jlSuffix = ""
    if pendingJLLeave then
        jlSuffix = ":JL:0"
        lastSentJL = "done"
        pendingJLLeave = false
    elseif lastSentJL ~= "done" then
        jlSuffix = ":JL:1"
        local prevJL = lastSentJL
        lastSentJL = (lastSentJL or 0) + 1
        if lastSentJL >= 60 then
            lastSentJL = "done"
        elseif prevJL == nil then
        end
    end

    local nameSuffix = ""
    local currentName = state.myName or "Unknown"
    if currentName ~= lastSentName then
        nameSuffix = ":NAME:" .. currentName
        lastSentName = currentName

end


local lvlSuffix = ""
    local currentLevel = state.myLevelFile
    if currentLevel and (currentLevel ~= lastSentLevel or levelAnnounceBurst > 0) then
        lvlSuffix = ":LVLANN:" .. currentLevel

if state.myLevelName and #state.myLevelName > 0 then
            lvlSuffix = lvlSuffix .. ":LVLNAME:" .. state.myLevelName
        end
        lastSentLevel = currentLevel
        if levelAnnounceBurst > 0 then levelAnnounceBurst = levelAnnounceBurst - 1 end
    end


local levelReqSuffix = ""
    if pendingLevelReq then
        levelReqSuffix = ":LEVELREQ:1"
        pendingLevelReq = false
    end

    local pvpSuffix = ""
    local currentPvP = state.pvpEnabled and "1" or "0"
    if currentPvP ~= lastSentPvP then
        pvpSuffix = ":PVP:" .. currentPvP
        lastSentPvP = currentPvP
    end
    if state.pendingPvPReq then
        pvpSuffix = pvpSuffix .. ":PVP_REQ:1"
        state.pendingPvPReq = nil
    end
    if state.pendingPvPAllow then
        pvpSuffix = pvpSuffix .. ":PVP_ALLOW:" .. state.pendingPvPAllow
        state.pendingPvPAllow = nil
    end

    local roomSuffix = ""
    local currentRoom = tostring(localTable.roomNumber or "")
    if currentRoom ~= lastSentRoom then
        roomSuffix = ":ROOM:" .. currentRoom
        lastSentRoom = currentRoom
    end

    local typingSuffix = ""
    local isTyping = MPUI.IsChatOpen()
    if isTyping ~= lastSentTyping then
        typingSuffix = ":TYPING:" .. (isTyping and "1" or "0")
        lastSentTyping = isTyping
    end

    local hitSuffix = ""
    if state.pendingHits and #state.pendingHits > 0 then
        for _, hit in ipairs(state.pendingHits) do
            hitSuffix = hitSuffix .. ":HIT:" .. hit.targetId .. ":" .. hit.damage
            if DEBUG then
                print("[PVP_HIT_SEND] Sending :HIT: targetId=" .. hit.targetId .. " damage=" .. hit.damage)
            end
        end
        state.pendingHits = {}
    end

    local chatSuffix = ""
    local chatMsg = MPUI.GetPendingChatMessage()
    if chatMsg then
        local safeMsg = chatMsg:gsub(":", ";")
        local isGlobal = MPUI.IsPendingChatGlobal and MPUI.IsPendingChatGlobal()
        local tag = isGlobal and "CHATG" or "CHAT"
        chatSuffix = ":" .. tag .. ":" .. safeMsg
        if DEBUG then
            print("[CHAT_SEND] Sending :" .. tag .. ":" .. safeMsg)
        end
    end

    local customSuffix = (additionalSuffixes or "")

    local pauseSuffix = ""
    if state.pendingPauseNotify then
        pauseSuffix = ":" .. state.pendingPauseNotify .. ":"
        if DEBUG then print("[Multiplayer:PrePause] emitting " .. pauseSuffix .. " token") end
        state.pendingPauseNotify = nil
    end

    -- NOTE: Fields 32, 33, 36 and 37 are sent as zero and no version reads them, so they are available for future expansion. They cannot be removed: the fields after them (34, 35 and the mesh list) are read by position in every version, so removing them would shift those. -- divezzi, 21.11.26
    local msg = string.format(
        "INPUT:HYBRID:%d:%d:%d:%.2f:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%s%s%s%s\n",
        math.floor(localTable.posX),
        math.floor(localTable.posY),
        math.floor(localTable.posZ),
        localTable.rotY,
        localTable.animNumber,
        localTable.frameNumber,
        tonumber(localTable.gunType) or 0,
        tonumber(localTable.handStatus) or 0,
        (localTable.isLow and 1 or 0),
        (localTable.inputForward and 1 or 0),
        (localTable.inputBack and 1 or 0),
        (localTable.inputLeft and 1 or 0),
        (localTable.inputRight and 1 or 0),
        (localTable.inputJump and 1 or 0),
        (localTable.inputAction and 1 or 0),
        (localTable.inputCrouch and 1 or 0),
        (localTable.inputWalk and 1 or 0),
        (localTable.inputSprint and 1 or 0),
        (localTable.inputRoll and 1 or 0),
        (localTable.inputDraw and 1 or 0),
        (localTable.inputFlare and 1 or 0),
        tonumber(localTable.leftArmAnim) or 0,
        tonumber(localTable.leftArmFrame) or 0,
        tonumber(localTable.leftArmFrameBase) or 0,
        tonumber(localTable.rightArmAnim) or 0,
        tonumber(localTable.rightArmFrame) or 0,
        tonumber(localTable.rightArmFrameBase) or 0,
        tonumber(localTable.animObjectID) or 0,
        tonumber(localTable.frameBase) or 0,
        (localTable.isUsingBinoculars and 1 or 0),
        (localTable.flareControlLeft and 1 or 0),
        0,
        0,
        tonumber(localTable.leftArmGunFlash) or 0,
        tonumber(localTable.rightArmGunFlash) or 0,
        0,
        0,
        meshString,
        rotSuffix,
        vehicleSuffix,
        customSuffix .. jlSuffix .. nameSuffix .. lvlSuffix .. levelReqSuffix .. soundSuffix .. vehicleSoundSuffix .. aimSuffix .. lookSuffix .. holsterSuffix .. sourceSuffix .. hpSuffix .. flmSuffix .. flrSuffix .. fireSuffix .. pvpSuffix .. roomSuffix .. typingSuffix .. hitSuffix .. chatSuffix .. pauseSuffix
    )

    local okSend, sendErr = pcall(function() return state.client:send(msg) end)
    if not okSend then
        print('[Multiplayer] Send failed: ' .. tostring(sendErr))
        state.isConnected = false
        pcall(function() if state.client then state.client:close() end end)
        state.client = nil
    elseif not sendErr then
        print('[Multiplayer] Send returned nil - connection broken')
        state.isConnected = false
        pcall(function() if state.client then state.client:close() end end)
        state.client = nil
    end
end

local function processMessage(msg, customMessageHandler)
    if msg:match("^LEAVE:") then
        local id = tonumber(msg:match("LEAVE:(%d+)"))
        if id then

formerSelfIds[id] = nil
            if DEBUG then print("[Multiplayer] Player " .. id .. " disconnected") end

if notifiedJoin[id] then
                local leaveName = displayNameFor(id)
                MPUI.ShowLeaveNotification(leaveName)
            else
            end
            notifiedJoin[id] = nil


            if state.remotePlayers[id] then
                pcall(function()
                    TEN.Objects.SetPvPTargetable(state.remotePlayers[id]:GetIndex(), false)
                    pvpLastKnownHP[state.remotePlayers[id]:GetIndex()] = nil
                end)
                destroyPuppet(state.remotePlayers[id])
                state.remotePlayers[id] = nil
                nameSetDirty = true
            end

            if state.torchFlames and state.torchFlames[id] then
                pcall(function() state.torchFlames[id]:Destroy() end)
                state.torchFlames[id] = nil
            end

            if state.torchPositions then state.torchPositions[id] = nil end

            if state.firePositions then state.firePositions[id] = nil end

            if state.remoteVehicles[id] then
                pcall(function() state.remoteVehicles[id]:Destroy() end)
                state.remoteVehicles[id] = nil
            end
            if state.remoteVehicleFailLogged then state.remoteVehicleFailLogged[id] = nil end
            if state.remoteVehicleLoops then state.remoteVehicleLoops[id] = nil end

            remotePlayerHP[id]      = nil
            remotePlayerTorchLit[id] = nil
            remotePlayerOnFire[id]  = nil
            remotePvPState[id]      = nil
            remotePlayerTyping[id]  = nil
            remotePlayerShooting[id] = nil

remotePlayerFrozen[id]  = nil
            if freezeStrings[id] then
                pcall(function() TEN.Strings.HideString(freezeStrings[id].ds) end)
                freezeStrings[id] = nil
            end
            lastAppliedPos[id]     = nil
            lastAppliedRoom[id]    = nil
            spawnWarmupUntil[id]   = nil
            if state.remotePlayerRoom then state.remotePlayerRoom[id] = nil end

if state.remotePlayerLevel then state.remotePlayerLevel[id] = nil end
            if state.remotePlayerLevelName then state.remotePlayerLevelName[id] = nil end
            if state.firstSeenFrame then state.firstSeenFrame[id] = nil end
            if state.spawnFailFrame then state.spawnFailFrame[id] = nil end
            if state.pendingPvPReqs then state.pendingPvPReqs[id] = nil end
        end
        return
    end

    if msg:match("^VERSION:") then
        local flag = msg:match("VERSION:(%d+)")
        if flag == "0" then
            MPUI.ShowNotification("version_outdated",
                "This game version is outdated. Please update",
                600, { color = Color(255, 80, 80) })
            if DEBUG then print("[Multiplayer] VERSION:0 received - client is outdated") end
        elseif flag == "1" then
            if DEBUG then print("[Multiplayer] VERSION:1 received - client is up to date") end
        end
        return
    end

    if customMessageHandler and customMessageHandler(msg) then
        return
    end

    local id, slot, payload = msg:match("^(%d+):(%d+):INPUT:(HYBRID:.*)$")
    if not payload then
        if DEBUG then print("[Multiplayer] Malformed message: " .. msg:sub(1, 80)) end
        return
    end

    id = tonumber(id)
    if state.myId and id == state.myId then return end


if formerSelfIds[id] then
        if (state.frameCounter or 0) > formerSelfIds[id] then
            formerSelfIds[id] = nil
        else
            return
        end
    end

    payload = payload:sub(8)
    local parts = splitString(payload, ":")

    for i = 1, #parts - 1 do
        if parts[i] == "NAME" then
            local rawName = parts[i + 1]
            if rawName and #rawName > 0 then
                local sanitized = sanitizeName(rawName, ENGINE_NAME_MAX)
                if sanitized and id then
                    if remotePlayerNames[id] ~= sanitized then nameSetDirty = true end
                    remotePlayerNames[id] = sanitized

end
            end
            break
        end
    end


if payload:find("LVL", 1, true) then
        for i = 1, #parts - 1 do
            local key = parts[i]
            if key == "LVLANN" then
                local lvl = parts[i + 1]
                if lvl and #lvl > 0 and id then
                    if state.remotePlayerLevel[id] ~= lvl then
                        state.remotePlayerLevel[id] = lvl
                        levelAnnounceBurst = LEVEL_ANNOUNCE_BURST

end
                end
            elseif key == "LVLNAME" then
                local nm = parts[i + 1]
                if nm and #nm > 0 and id then state.remotePlayerLevelName[id] = nm end
            end
        end
    end


for i = 1, #parts - 1 do
        if parts[i] == "LEVELREQ" then
            if (state.frameCounter - lastLevelReqFrame) >= LEVEL_REQ_COOLDOWN then
                lastLevelReqFrame  = state.frameCounter
                levelAnnounceBurst = LEVEL_ANNOUNCE_BURST
                if DEBUG then print("[LVL_REQ] player " .. tostring(id) .. " requested levels - re-announcing mine") end
            end
            break
        end
    end


if not notifiedJoin[id] then
        notifiedJoin[id] = true
        local joinName = displayNameFor(id)
        MPUI.ShowJoinNotification(joinName)

lastSentPvP  = nil

lastSentName = nil

        lastSentLevel      = nil
        levelAnnounceBurst = LEVEL_ANNOUNCE_BURST

end


for i = 1, #parts - 1 do
        if parts[i] == "JL" then
            local jlVal = tonumber(parts[i + 1])
            if jlVal == 0 and id then
                if notifiedJoin[id] then
                    local leaveName = displayNameFor(id)
                    MPUI.ShowLeaveNotification(leaveName)
                    notifiedJoin[id] = nil
                else
                end
            end
            break
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "CHATG" then
            local chatText = parts[i + 1]
            if chatText and #chatText > 0 then
                chatText = chatText:gsub(";", ":")
                local senderName = displayNameFor(id)
                MPUI.AddChatMessage(senderName, chatText, nil, true, id)
                if DEBUG then
                    print("[CHATG_RECV] From player " .. id .. " (" .. senderName .. "): " .. chatText)
                end
            end
            break
        end
    end


do
        local myLvl   = state.myLevelFile
        local peerLvl = state.remotePlayerLevel[id]
        if myLvl and peerLvl then
            if peerLvl ~= myLvl then
                return
            end
            state.firstSeenFrame[id] = nil
        else
            local firstSeen = state.firstSeenFrame[id]
            if not firstSeen then
                state.firstSeenFrame[id] = state.frameCounter
                firstSeen = state.frameCounter
            end
            if (state.frameCounter - firstSeen) < LEVEL_UNKNOWN_GRACE then
                return
            end
        end
    end



local freezeLabel = nil
    if payload:find("PAUSE", 1, true) or payload:find("SAVING", 1, true)
        or payload:find("LOADING", 1, true) then
        for i = 1, #parts do
            local lbl = FREEZE_LABELS[parts[i]]
            if lbl then freezeLabel = lbl; break end
        end
    end
    if freezeLabel ~= remotePlayerFrozen[id] then
        remotePlayerFrozen[id] = freezeLabel
        if freezeLabel then

pcall(function()
                TEN.Objects.SetPvPTargetable(state.remotePlayers[id]:GetIndex(), false)
            end)
        end
        if DEBUG then
            local px, py, pz = 0, 0, 0
            pcall(function()
                local p = state.remotePlayers[id]:GetPosition()
                px, py, pz = p.x, p.y, p.z
            end)
            print(string.format("[FREEZE] player=%d state=%s pos=(%.0f,%.0f,%.0f) hp=%s (the puppet is kept)",
                id, tostring(freezeLabel), px, py, pz, tostring(remotePlayerHP[id] or "?")))
        end
    end

    local double = state.remotePlayers[id]
    if double and isPuppetInvalid(double) then
        print("[REMOTE] The puppet of player " .. id .. " was invalidated by the engine, it is created again.")
        state.remotePlayers[id] = nil
        nameSetDirty = true
        double = nil
    end

    if not double then
        local lastFail = state.spawnFailFrame[id] or 0
        if (state.frameCounter - lastFail) < SPAWN_RETRY_INTERVAL then
            return
        end

        if DEBUG then print("[Multiplayer] New player ID=" .. id) end

        local doubleName = puppetFactory.Name(id)
        local newPos = Vec3(tonumber(parts[1]), tonumber(parts[2]), tonumber(parts[3]))
        local rotY = tonumber(parts[4])
        local rotX = 0
        local rotZ = 0

        for i = 1, #parts - 2 do
            if parts[i] == "ROT" then
                rotX = tonumber(parts[i + 1]) or 0
                rotZ = tonumber(parts[i + 2]) or 0
                break
            end
        end

        local newRot = Rotation(rotX, rotY, rotZ)

        local okCreate, created = pcall(puppetFactory.Create, id, newPos, newRot)

        if okCreate and created then
            state.spawnFailFrame[id] = nil
            state.remotePlayers[id] = created
            nameSetDirty = true
            double = created
            spawnWarmupUntil[id] = state.frameCounter + 10
            local spawnIdx = -1
            pcall(function() spawnIdx = created:GetIndex() end)




state.torchResendCount = 10
            state.fireResendCount  = 10

            lastSentName       = nil
            lastSentTyping     = nil
            lastSentPvP        = nil
            lastSentHP         = nil
            lastSentTorchLit   = nil
            lastSentOnFire     = nil
            lastSentAimSuffix  = nil
            lastSentLookSuffix = nil
            lastSentJL         = nil


if spawnIdx and spawnIdx >= 0 then
                pvpRearmDouble(created, spawnIdx)
            end

            local okHit, hitErr = pcall(function()
                created:SetOnHit(LevelFuncs.Engine.Multiplayer.OnPvPHit)
            end)
            if okHit then

else
                print("[PVP_HIT_LUA] ERROR registering OnHit for " .. doubleName .. ": " .. tostring(hitErr))
            end
        else
            state.spawnFailFrame[id] = state.frameCounter
            local knownName = displayNameFor(id)
            print(string.format(
                "[Multiplayer] CROSS-LEVEL? Failed to spawn %s (player %d / \"%s\") at pos=(%.0f, %.0f, %.0f). " ..
                "Retrying in %d frames. If this repeats, player is in a different level.",
                doubleName, id, knownName, newPos.x, newPos.y, newPos.z, SPAWN_RETRY_INTERVAL
            ))
            return
        end
    end

    local remoteTable = {
        animNumber = tonumber(parts[5]),
        frameNumber = tonumber(parts[6]),
        gunType = tonumber(parts[7]),
        handStatus = tonumber(parts[8]),
        isLow = (tonumber(parts[9]) == 1),
        -- NOTE: Deprecated. Of the input flags and frame bases only inputAction is used (the shooting test below); SetRemotePlayerState() reads none of the others. They are read so the table has the same keys as the sent one, and the fields stay in the packet for other versions; remove them when those versions are retired. -- divezzi, 21.11.26
        inputForward = (tonumber(parts[10]) == 1),
        inputBack = (tonumber(parts[11]) == 1),
        inputLeft = (tonumber(parts[12]) == 1),
        inputRight = (tonumber(parts[13]) == 1),
        inputJump = (tonumber(parts[14]) == 1),
        inputAction = (tonumber(parts[15]) == 1),
        inputCrouch = (tonumber(parts[16]) == 1),
        inputWalk = (tonumber(parts[17]) == 1),
        inputSprint = (tonumber(parts[18]) == 1),
        inputRoll = (tonumber(parts[19]) == 1),
        inputDraw = (tonumber(parts[20]) == 1),
        inputFlare = (tonumber(parts[21]) == 1),
        leftArmAnim = tonumber(parts[22]),
        leftArmFrame = tonumber(parts[23]),
        leftArmFrameBase = tonumber(parts[24]),
        rightArmAnim = tonumber(parts[25]),
        rightArmFrame = tonumber(parts[26]),
        rightArmFrameBase = tonumber(parts[27]),
        animObjectID = tonumber(parts[28]),
        frameBase = tonumber(parts[29]),
        isUsingBinoculars = (tonumber(parts[30]) == 1),
        flareControlLeft = (tonumber(parts[31]) == 1),
        leftArmGunFlash = tonumber(parts[34]),
        rightArmGunFlash = tonumber(parts[35]),

meshIndices = {}
    }

    local newPos = Vec3(tonumber(parts[1]), tonumber(parts[2]), tonumber(parts[3]))
    local rotY = tonumber(parts[4])
    local rotX = 0
    local rotZ = 0

    for i = 1, #parts - 2 do
        if parts[i] == "ROT" then
            rotX = tonumber(parts[i + 1]) or 0
            rotZ = tonumber(parts[i + 2]) or 0
            break
        end
    end

    local aimLeftArmRot = nil
    for i = 1, #parts - 10 do
        if parts[i] == "AIM" then
            aimLeftArmRot = Rotation(
                tonumber(parts[i + 1]) or 0,
                tonumber(parts[i + 2]) or 0,
                tonumber(parts[i + 3]) or 0
            )
            remoteTable.rightArmOrientation = Rotation(
                tonumber(parts[i + 4]) or 0,
                tonumber(parts[i + 5]) or 0,
                tonumber(parts[i + 6]) or 0
            )
            remoteTable.targetArmOrientation = Rotation(
                tonumber(parts[i + 7]) or 0,
                tonumber(parts[i + 8]) or 0,
                tonumber(parts[i + 9]) or 0
            )
            remoteTable.leftArmLocked = tonumber(parts[i + 10]) == 1
            remoteTable.rightArmLocked = tonumber(parts[i + 11]) == 1
            break
        end
    end

    if aimLeftArmRot then
        remoteTable.leftArmOrientation = aimLeftArmRot
    end

    for i = 1, #parts - 2 do
        if parts[i] == "LOOK" then
            remoteTable.lookActive = tonumber(parts[i + 1]) == 1
            remoteTable.lookOrientX = tonumber(parts[i + 2]) or 0
            remoteTable.lookOrientY = tonumber(parts[i + 3]) or 0
            break
        end
    end

    for i = 1, #parts - 3 do
        if parts[i] == "H" then
            remoteTable.leftHolster = tonumber(parts[i + 1]) or 0
            remoteTable.rightHolster = tonumber(parts[i + 2]) or 0
            remoteTable.backHolster = tonumber(parts[i + 3]) or 0
            break
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "HP" then
            remoteTable.health = tonumber(parts[i + 1]) or 0
            break
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "FLR" then
            remoteTable.flareLife = tonumber(parts[i + 1]) or 0
            break
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "FLM" then
            local litVal = tonumber(parts[i + 1])
            remotePlayerTorchLit[id] = (litVal == 1)
            if litVal == 1 then

                local fx = tonumber(parts[i + 2])
                local fy = tonumber(parts[i + 3])
                local fz = tonumber(parts[i + 4])
                if fx and fy and fz then
                    state.torchPositions[id] = Vec3(fx, fy, fz)
                end
            else
                state.torchPositions[id] = nil
            end
            break
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "FIRE" then
            local litVal = tonumber(parts[i + 1])
            remotePlayerOnFire[id] = (litVal == 1)
            if litVal == 1 then

                local fx = tonumber(parts[i + 2])
                local fy = tonumber(parts[i + 3])
                local fz = tonumber(parts[i + 4])
                if fx and fy and fz then
                    state.firePositions[id] = Vec3(fx, fy, fz)
                end
            else
                state.firePositions[id] = nil
            end
            break
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "NAME" then
            remoteTable.playerName = parts[i + 1] or nil
            break
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "PVP" then
            remoteTable.pvpEnabled = tonumber(parts[i + 1]) == 1
            break
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "ROOM" then
            local rn = tonumber(parts[i + 1])
            if rn and id and state.remotePlayerRoom then
                state.remotePlayerRoom[id] = rn
            end
            break
        end
    end

    local newRot = Rotation(rotX, rotY, rotZ)


local lp = lastAppliedPos[id]
    local inWarmup = spawnWarmupUntil[id] and state.frameCounter <= spawnWarmupUntil[id]
    if newPos and (inWarmup or not lp or lp.x ~= newPos.x or lp.y ~= newPos.y or lp.z ~= newPos.z) then
        local knownRoom = state.remotePlayerRoom and state.remotePlayerRoom[id]
        if knownRoom then
            pcall(function() double:SetPosition(newPos, false) end)
            if id and knownRoom ~= lastAppliedRoom[id] then
                pcall(function() double:SetRoomNumber(knownRoom) end)
                lastAppliedRoom[id] = knownRoom
            end
        else
            pcall(function() double:SetPosition(newPos) end)
        end
        lastAppliedPos[id] = { x = newPos.x, y = newPos.y, z = newPos.z }
    end
    pcall(function() double:SetRotation(newRot) end)

    if parts[38] and #parts[38] > 0 then
        local meshParts = splitString(parts[38], ",")
        for i=1, #meshParts do
            remoteTable.meshIndices[i] = tonumber(meshParts[i])
        end
    end

    -- NOTE: Deprecated. remoteTable.soundID is never read; the sounds are played from the :SND: loop further down. Remove it with the next change of this function. -- divezzi, 21.11.26
    for i = 1, #parts - 1 do
        if parts[i] == "SND" then
            local soundID = tonumber(parts[i + 1])
            if soundID then
                remoteTable.soundID = soundID
                break
            end
        end
    end


    for i = 1, #parts - 1 do
        if parts[i] == "SRC" then
            remoteTable.sourceObjectID = tonumber(parts[i + 1])
            break
        end
    end

    local okSet, setErr = pcall(puppetFactory.Apply, double, remoteTable)
    if not okSet then
        print("[MP] Showing the state of player " .. tostring(id) ..
              " failed: handStatus=" .. tostring(remoteTable.handStatus) ..
              " gunType=" .. tostring(remoteTable.gunType) ..
              " err=" .. tostring(setErr))
    end



    if remoteTable.health then
        remotePlayerHP[id] = remoteTable.health
    elseif not remotePlayerHP[id] then
        remotePlayerHP[id] = 1000
    end

    if remoteTable.playerName then
        local sn = sanitizeName(remoteTable.playerName, ENGINE_NAME_MAX)
        if remotePlayerNames[id] ~= sn then nameSetDirty = true end
        remotePlayerNames[id] = sn
    end

    if remoteTable.pvpEnabled ~= nil then
        remotePvPState[id] = remoteTable.pvpEnabled
    end


local hs = tonumber(remoteTable.handStatus) or 0
    local weaponInHand = hs ~= 0 and not remoteTable.flareControlLeft and not remoteTable.isUsingBinoculars
    remotePlayerShooting[id] = weaponInHand and (remoteTable.inputAction == true)

local shouldTarget = state.pvpEnabled and (state.pvpForced or remotePvPState[id]) and (remotePlayerFrozen[id] == nil)
    pcall(function()
        TEN.Objects.SetPvPTargetable(double:GetIndex(), shouldTarget == true)
    end)

    for i = 1, #parts - 1 do
        if parts[i] == "TYPING" then
            remotePlayerTyping[id] = (tonumber(parts[i + 1]) == 1)
            break
        end
    end

    for i = 1, #parts - 2 do
        if parts[i] == "HIT" then
            local hitTargetId = tonumber(parts[i + 1])
            local hitDamage = tonumber(parts[i + 2])
            if hitTargetId and hitDamage then
                if DEBUG then
                    print("[PVP_HIT_RECV] From player " .. id .. ": HIT targetId=" .. hitTargetId .. " damage=" .. hitDamage .. " myId=" .. tostring(state.myId))
                end
                if hitTargetId == state.myId then
                    local currentHP = Lara:GetHP()
                    local newHP = math.max(0, currentHP - hitDamage)
                    Lara:SetHP(newHP)

state.lastPvPAttackerId  = id
                    state.lastPvPAttackFrame = state.frameCounter

if Multiplayer.IsPvPDamageHarmless() then
                        Lara:SetHP(currentHP)
                        if DEBUG then
                            print("[PVP_HIT_APPLY] harmless mode - hit by player " .. id
                                .. " for " .. hitDamage .. ", HP restored to " .. currentHP)
                        end
                    elseif DEBUG then
                        print("[PVP_HIT_APPLY] I GOT HIT by player " .. id .. "! damage=" .. hitDamage .. " HP: " .. currentHP .. " -> " .. newHP)
                    end
                end
            end
        end
    end

    for i = 1, #parts - 1 do
        if parts[i] == "CHAT" then
            local chatText = parts[i + 1]
            if chatText and #chatText > 0 then
                chatText = chatText:gsub(";", ":")
                local senderName = displayNameFor(id)
                MPUI.AddChatMessage(senderName, chatText, nil, false, id)
                if DEBUG then
                    print("[CHAT_RECV] From player " .. id .. " (" .. senderName .. "): " .. chatText)
                end
            end
            break
        end
    end


    for i = 1, #parts do
        if parts[i] == "PVP_REQ" then
            local reqName = remotePlayerNames[id] or ("Player " .. id)
            if state.pvpEnabled then

state.pendingPvPAllow = id
                lastSentPvP = nil
                MPUI.ShowNotification(
                    "pvpreq_" .. id,
                    reqName .. " just activated PvP too, be cautious!",
                    180,
                    { color = Color(50, 255, 50) }
                )
                if DEBUG then
                    print(string.format("[PVP_REQ] Auto-accepted from %d (%s) - local PvP already ON, sending ALLOW + re-broadcasting :PVP:1", id, reqName))
                end
            else

                if id and state.pendingPvPReqs then
                    state.pendingPvPReqs[id] = { name = reqName, timer = 300 }
                end
                MPUI.ShowNotification(
                    "pvpreq_" .. id,
                    reqName .. " wants to enable PvP! Press ENTER or click to accept",
                    300,
                    { color = Color(255, 160, 0), interactive = true,
                      flashCheck = function() return KeyIsHit(ActionID.RETURN) end }
                )
                if DEBUG then
                    print(string.format("[PVP_REQ] From player %d (%s) - queued, local PvP OFF", id, reqName))
                end
            end
            break
        end
    end


    for i = 1, #parts - 1 do
        if parts[i] == "PVP_ALLOW" then
            local toId = tonumber(parts[i + 1])
            if toId and toId == state.myId then
                Multiplayer.SetPvPEnabled(true)
                local allowName = remotePlayerNames[id] or ("Player " .. id)
                MPUI.ShowNotification(
                    "pvpallow_" .. id,
                    allowName .. " accepted your PvP request!",
                    120,
                    { color = Color(50, 255, 50) }
                )
                if DEBUG then
                    print(string.format("[PVP_ALLOW] Player %d (%s) accepted our PvP request", id, allowName))
                end
            end
            break
        end
    end

    local vehicleState = nil
    for i = 1, #parts - 7 do
        if parts[i] == "VEH" then
            vehicleState = deserializeVehicleState(parts, i)
            break
        end
    end
    updateRemoteVehicle(id, vehicleState)

    -- NOTE: Deprecated. This version sends one :SND: per packet; the loop plays every one because the Obbyv9 script sends up to three. Remove the loop over several tokens when that version is retired. -- divezzi, 21.11.26
    for i = 1, #parts - 1 do
        if parts[i] == "SND" then
            local soundID = tonumber(parts[i + 1]) or 0
            if soundID > 0 then
                if soundID == SOUND_WIRE_ALIAS_FOOTSTEPS then
                    soundID = SOUND_WIRE_ALIAS_TARGET
                end

                local pos = double:GetPosition()
                if not OVERLAPPING_SOUNDS[soundID] then
                    TEN.Sound.StopSound(soundID)
                end
                TEN.Sound.PlaySound(soundID, pos)
            end
        end
    end

    -- Vehicle sounds: VSND, sound id, pitch times 100, environment, looped flag.
    for i = 1, #parts - 4 do
        if parts[i] == "VSND" then
            local soundID = tonumber(parts[i + 1]) or 0
            if soundID > 0 then
                receiveRemoteVehicleSound(id, double, soundID,
                    tonumber(parts[i + 2]) or 100, tonumber(parts[i + 3]) or 0, parts[i + 4] == "1")
            end
        end
    end
end

local function processReceivedLine(line, customMessageHandler)
    local ok, processErr = pcall(processMessage, line, customMessageHandler)
    if not ok then
        print("[Multiplayer] Error processing message: " .. tostring(processErr))
    end
end

-- True when the local camera moved since the last call.
local function hasCameraMoved()
    local ok, cameraValues = pcall(function()
        local position = TEN.View.GetCameraPosition()
        local target = TEN.View.GetCameraTarget()
        return { position.x, position.y, position.z, target.x, target.y, target.z }
    end)
    if not ok then return false end

    local lastValues = jitter.lastCamera
    jitter.lastCamera = cameraValues
    if not lastValues then return false end

    for i = 1, #cameraValues do
        if math.abs(cameraValues[i] - lastValues[i]) > JITTER_CAMERA_EPSILON then
            return true
        end
    end

    return false
end

-- Moves the number of held updates one step toward its target, so it never jumps. -- divezzi, 21.11.26
local function updateJitterDepth()
    local targetDepth = JITTER_TICKS

    if JITTER_ONLY_WHEN_CAMERA_MOVES then
        if hasCameraMoved() then
            jitter.cameraMovedFrame = state.frameCounter
        end

        if (state.frameCounter - jitter.cameraMovedFrame) >= JITTER_CAMERA_HOLD_TICKS then
            targetDepth = 0
        end
    end

    if jitter.depth < targetDepth then
        jitter.depth = jitter.depth + 1
    elseif jitter.depth > targetDepth then
        jitter.depth = jitter.depth - 1
    end
end

-- Shows the held updates of a player. With a depth of 0 everything is shown at once. Otherwise the player waits until the depth is reached, then one update is shown per tick,
-- so a tick without a packet is covered by the update held in reserve, two when more than the depth is held, and all but the depth after a burst. The wait starts again when the queue runs empty. -- divezzi, 21.11.26
local function releaseHeldUpdates(playerId, customMessageHandler, releaseAll)
    local queue = jitter.queues[playerId]
    if not queue then return end

    local count = #queue
    if not releaseAll and jitter.depth > 0 then
        if not jitter.playing[playerId] then
            if count < jitter.depth then return end
            jitter.playing[playerId] = true
        end

        if count > jitter.depth + 3 then
            count = count - jitter.depth
        elseif count > jitter.depth + 1 then
            count = 2
        else
            count = math.min(count, 1)
        end
    end

    for _ = 1, count do
        processReceivedLine(table.remove(queue, 1), customMessageHandler)
    end

    if #queue == 0 then
        jitter.playing[playerId] = nil
    end
end

local function receive(customMessageHandler)
    if not (state.isConnected and state.client) then return end
    state.client:settimeout(0)
    updateJitterDepth()

    local msgCount = 0
    local maxMessages = 50
    local updatesThisTick = {}
    while msgCount < maxMessages do
        local data, err = state.client:receive("*l")
        if data then
            msgCount = msgCount + 1
            data = data:gsub("[\r\n]+$", "")
            if #data > 0 then
                local inputId = tonumber(data:match("^(%d+):%d+:INPUT:HYBRID:"))
                if inputId then
                    updatesThisTick[inputId] = (updatesThisTick[inputId] or 0) + 1
                end

                if inputId and JITTER_TICKS > 0 then
                    jitter.queues[inputId] = jitter.queues[inputId] or {}
                    table.insert(jitter.queues[inputId], data)
                else
                    local leftId = tonumber(data:match("^LEAVE:(%d+)"))
                    if leftId then
                        releaseHeldUpdates(leftId, customMessageHandler, true)
                        jitter.queues[leftId] = nil
                        jitter.playing[leftId] = nil
                    end

                    processReceivedLine(data, customMessageHandler)
                end
            end
        elseif err == "timeout" or err == "wantread" then
            break
        elseif err then
            print("[Multiplayer] Receive err: ".. tostring(err))
            state.isConnected = false
            pcall(function() if state.client then state.client:close() end end)
            state.client = nil
            break
        else
            print("[Multiplayer] Server closed connection")
            state.isConnected = false
            pcall(function() if state.client then state.client:close() end end)
            state.client = nil
            break
        end
    end

    if not state.isConnected then
        jitter.queues = {}
        jitter.playing = {}
        return
    end

    for playerId, _ in pairs(jitter.queues) do
        releaseHeldUpdates(playerId, customMessageHandler, false)
    end

    if LOG_RX_TICKS then
        for playerId, _ in pairs(state.remotePlayers) do
            local updateCount = updatesThisTick[playerId] or 0
            if updateCount ~= 1 then
                print("[REMOTE] Rx frame=" .. state.frameCounter .. " id=" .. playerId .. " msgs=" .. updateCount .. " held=" .. #(jitter.queues[playerId] or {}) .. " depth=" .. jitter.depth)
            end
        end
    end
end


local function applyIdMessage(s, idMsg)
    idMsg = idMsg:gsub("[\r\n]+$", "")
    if DEBUG then print("[Multiplayer] Received: " .. idMsg) end

    if idMsg == "FULL" then
        print("[Multiplayer] Server full!")
        pcall(function() s:close() end)
        return false
    end

    if idMsg:match("^ID:") then
        local id, slot, maxPlayers = idMsg:match("ID:(%d+):SLOT:(%d+):MAX:(%d+)")
        if id then
            state.client = s
            state.myId = tonumber(id)

if pendingPreviousId and pendingPreviousId ~= state.myId then
                formerSelfIds[pendingPreviousId] = (state.frameCounter or 0) + FORMER_SELF_TTL
                if DEBUG then
                    print(string.format("[Multiplayer] Reconnected as id=%d; suppressing self-ghost id=%d",
                        state.myId, pendingPreviousId))
                end
            end
            pendingPreviousId = nil
            state.serverMaxPlayers = tonumber(maxPlayers) or 10
            state.isConnected = true
            lastSentName       = nil
            lastSentTyping     = nil
            lastSentPvP        = nil
            lastSentHP         = nil
            lastSentTorchLit   = nil
            lastSentOnFire     = nil
            lastSentAimSuffix  = nil
            lastSentLookSuffix = nil
            lastSentJL         = nil

            lastSentLevel      = nil
            levelAnnounceBurst = LEVEL_ANNOUNCE_BURST

lastResolvedLevelFile = nil
            MPUI.HideNotification("disconnected")
            MPUI.HideNotification("disconnected_hint")
            if (state.disconnectedMsgTimer or 0) > 0 then
                MPUI.ShowNotification("connected", "You're connected now!", 180, { color = Color(50, 255, 50) })
            end
            state.myName = state.customName or resolvePoolName(state.myId)
            nameSetDirty = true
            MPUI.SetLocalNameProvider(function() return state.myName end)
            MPUI.SetLocalIdProvider(function() return state.myId end)
            if DEBUG then print("[Multiplayer] ID assigned: " .. state.myId .. ", SLOT: " .. slot .. ", Name: " .. state.myName .. ", VERSION: " .. tostring(gameVersion)) end
            if gameVersion then s:send("VERSION:" .. gameVersion .. "\n") end
            local lvlOk, lvl = pcall(function() return TEN.Flow.GetCurrentLevel() end)
            if lvlOk and lvl and lvl.levelFile then
                s:send("LEVEL:" .. lvl.levelFile .. "\n")

                state.myLevelFile = lvl.levelFile:gsub(":", "")
                if DEBUG then print("[Multiplayer] Sent LEVEL: " .. lvl.levelFile) end
            end
            return true
        end
    end

    pcall(function() s:close() end)
    return false
end

local function pollPendingId()
    if not state.pendingClient then return end

    state.pendingIdTimer = state.pendingIdTimer + 1
    if state.pendingIdTimer > PENDING_ID_TIMEOUT then
        if DEBUG then print("[Multiplayer] Timeout awaiting ID - giving up") end
        pcall(function() state.pendingClient:close() end)
        state.pendingClient = nil
        state.pendingIdTimer = 0
        return
    end

    local idMsg, idErr = state.pendingClient:receive("*l")
    if idMsg then
        local s = state.pendingClient
        state.pendingClient = nil
        state.pendingIdTimer = 0
        applyIdMessage(s, idMsg)
    elseif idErr ~= "timeout" then
        if DEBUG then print("[Multiplayer] ID receive error: " .. tostring(idErr)) end
        pcall(function() state.pendingClient:close() end)
        state.pendingClient = nil
        state.pendingIdTimer = 0
    end
end

local gateReasonPrinted = nil

local function printGateReason()
    if not TEN.Flow.IsMultiplayerDeclared then return end

    local reason = TEN.Flow.IsMultiplayerDeclared() and "off" or "undeclared"
    if reason == gateReasonPrinted then return end
    gateReasonPrinted = reason

    if reason == "undeclared" then
        print("[Multiplayer] No script called Multiplayer.EnableGate() or Flow.EnableMultiplayer(). Multiplayer is off.")
    else
        print("[Multiplayer] Multiplayer is off. It can be switched on in Options > Multiplayer. See the [MP-GATE] lines.")
    end
end

function Multiplayer.Connect(host, port)
    if state.isConnected then return true end
    if state.pendingClient then return false end
    if state.connectGaveUp then return false end
    if not TEN.Flow.IsMultiplayerEnabled() then
        printGateReason()
        return false
    end
    if not getSocket() then return false end

    if leftOnPurpose then
        pendingPreviousId = nil
        formerSelfIds     = {}
        leftOnPurpose     = false
        if DEBUG and state.myId then print("[Multiplayer] Reconnecting after a deliberate disconnect (was id " .. tostring(state.myId) .. "): no id is suppressed.") end
    else
        pendingPreviousId = state.myId
    end

    initState()
    state.host = host or state.host
    state.port = port or state.port

    local s, serr = socket.tcp()
    if not s then
        if DEBUG then print("[Multiplayer] Error creating TCP socket: " .. tostring(serr)) end
        return false
    end

    s:settimeout(2)

    -- The packets are small and sent every tick, so they should not wait to be joined by the system. -- divezzi, 21.11.26
    pcall(function() s:setoption("tcp-nodelay", true) end)

    local ok, cerr = s:connect(state.host, state.port)

    if not ok then
        state.connectAttempts = state.connectAttempts + 1
        if DEBUG then print("[Multiplayer] Error connecting: " .. tostring(cerr) .. " (attempt " .. state.connectAttempts .. "/" .. MAX_CONNECT_ATTEMPTS .. ")") end
        pcall(function() s:close() end)
        if state.connectAttempts >= MAX_CONNECT_ATTEMPTS then
            state.connectGaveUp = true
            print("[Multiplayer] Gave up connecting after " .. MAX_CONNECT_ATTEMPTS .. " attempts. Restart the game to retry.")
        end
        return false
    end


    state.connectAttempts = 0
    state.connectGaveUp   = false

    if DEBUG then print("[Multiplayer] Connected! Awaiting ID...") end

    s:settimeout(0)
    local idMsg, idErr = s:receive("*l")
    if idMsg then
        return applyIdMessage(s, idMsg)
    elseif idErr == "timeout" then
        state.pendingClient  = s
        state.pendingIdTimer = 0
        return false
    else
        if DEBUG then print("[Multiplayer] ID receive error: " .. tostring(idErr)) end
        pcall(function() s:close() end)
        return false
    end
end

function Multiplayer.Disconnect()
    if state.pendingClient then
        pcall(function() state.pendingClient:close() end)
        state.pendingClient  = nil
        state.pendingIdTimer = 0
    end

if state.isConnected and state.client then
        pendingJLLeave = true
        pcall(sendPlayerData)
    end
    if state.client then
        pcall(function() state.client:close() end)
    end
    state.isConnected = false
    state.client = nil

    leftOnPurpose = true

end


local NOTIFY_CYCLE = {
    {
        getLabel        = function() return "Players' info: " .. (showPlayerInfo and "On" or "Off") end,
        apply           = function() showPlayerInfo = not showPlayerInfo end,
        getConfirmColor = function() return showPlayerInfo and MPUI.CONFIRM_ON_COLOR or MPUI.CONFIRM_OFF_COLOR end,
        isActive        = function() return false end,
        getLabelColor   = function() return showPlayerInfo and MPUI.CONFIRM_ON_COLOR or MPUI.CONFIRM_OFF_COLOR end,
    },
    {
        getLabel        = function() return "Join/leave notifications: On top"  end,
        apply           = function() MPUI.SetJoinLeaveMode(MPUI.JL_MODE_TOP)  end,
        getConfirmColor = function() return MPUI.CONFIRM_ON_COLOR end,
        isActive        = function() return MPUI.GetJoinLeaveMode() == MPUI.JL_MODE_TOP  end,
        getLabelColor   = function() return MPUI.GetJoinLeaveMode() == MPUI.JL_MODE_TOP  and MPUI.CONFIRM_ON_COLOR or Color(255, 220, 80) end,
    },
    {
        getLabel        = function() return "Join/leave notifications: On chat" end,
        apply           = function() MPUI.SetJoinLeaveMode(MPUI.JL_MODE_CHAT) end,
        getConfirmColor = function() return MPUI.CONFIRM_ON_COLOR end,
        isActive        = function() return MPUI.GetJoinLeaveMode() == MPUI.JL_MODE_CHAT end,
        getLabelColor   = function() return MPUI.GetJoinLeaveMode() == MPUI.JL_MODE_CHAT and MPUI.CONFIRM_ON_COLOR or Color(255, 220, 80) end,
    },
    {
        getLabel        = function()
            if MPUI.GetJoinLeaveMode() == MPUI.JL_MODE_OFF then
                return "Join/leave notifications: Currently Off"
            else
                return "Join/leave notifications: Currently On"
            end
        end,
        apply           = function() MPUI.SetJoinLeaveMode(MPUI.JL_MODE_OFF)  end,
        getConfirmColor = function() return MPUI.CONFIRM_OFF_COLOR end,
        isActive        = function() return MPUI.GetJoinLeaveMode() == MPUI.JL_MODE_OFF  end,
        getLabelColor   = function() return MPUI.GetJoinLeaveMode() == MPUI.JL_MODE_OFF  and MPUI.CONFIRM_OFF_COLOR or MPUI.CONFIRM_ON_COLOR end,
        rollToIndex     = 2,
    },

{
        getLabel        = function() return "Keyboard layout: " .. (MPUI.GetAzertyMode() and "AZERTY" or "QWERTY") end,
        apply           = function() MPUI.SetAzertyMode(not MPUI.GetAzertyMode()) end,
        getConfirmColor = function() return MPUI.GetAzertyMode() and MPUI.CONFIRM_ON_COLOR or MPUI.CONFIRM_OFF_COLOR end,
        isActive        = function() return false end,
        getLabelColor   = function() return Color(255, 220, 80) end,
    },

}


local SOCIAL_SETTINGS = {
    {
        name          = "players' info",
        getValue      = function() return showPlayerInfo and "on" or "off" end,
        getValueColor = function() return showPlayerInfo and MPUI.CONFIRM_ON_COLOR or MPUI.CONFIRM_OFF_COLOR end,
        cycle         = function() showPlayerInfo = not showPlayerInfo end,
    },
    {
        name          = "join/leave",
        getValue      = function()
            local m = MPUI.GetJoinLeaveMode()
            if     m == MPUI.JL_MODE_TOP  then return "on top"
            elseif m == MPUI.JL_MODE_CHAT then return "on chat"
            else                               return "off" end
        end,
        getValueColor = function()
            return (MPUI.GetJoinLeaveMode() == MPUI.JL_MODE_OFF)
                and MPUI.CONFIRM_OFF_COLOR or MPUI.CONFIRM_ON_COLOR
        end,
        cycle         = function()
            local m = MPUI.GetJoinLeaveMode()
            if     m == MPUI.JL_MODE_TOP  then MPUI.SetJoinLeaveMode(MPUI.JL_MODE_CHAT)
            elseif m == MPUI.JL_MODE_CHAT then MPUI.SetJoinLeaveMode(MPUI.JL_MODE_OFF)
            else                               MPUI.SetJoinLeaveMode(MPUI.JL_MODE_TOP) end
        end,
    },

    {
        name          = "keyboard",

getValue      = function() return MPUI.GetAzertyMode() and "AZERTY" or "QWERTY" end,
        getValueColor = function() return Color(255, 220, 80) end,
        cycle         = function() MPUI.SetAzertyMode(not MPUI.GetAzertyMode()) end,
    },

}
MPUI.SetSocialSettings(SOCIAL_SETTINGS)

local X_CYCLE_TIMEOUT  = 150
local xCycleActive     = false
local xCycleIndex      = 1
local xCycleLastFrame  = 0


local promptConfirmConsumed = false


local PVP_CONFIRM_WINDOW = 90
local pvpFirstPressFrame = nil


local function checkNameHPKeyToggle()
    do return end

end

local TELEPORT_NOTIFY_TTL = 90
local teleportActive      = false
local teleportIds         = {}
local teleportIndex       = 0
local teleportFramesLeft  = 0
local teleportGateFn      = nil
local teleportGateMsg     = nil


Multiplayer.SetTeleportGate = function(fn, msg)
    teleportGateFn  = fn
    teleportGateMsg = msg
end

Multiplayer.ClearTeleportGate = function()
    teleportGateFn  = nil
    teleportGateMsg = nil
end

local function checkTeleportKeyToggle()
    if MPUI.IsChatOpen() or MPUI.IsNameInputOpen() then return end
    if not state.isConnected then return end

    local ids = {}
    for id in pairs(state.remotePlayers) do ids[#ids + 1] = id end
    table.sort(ids)


    if KeyIsHit(ActionID.T) or MPUI.ConsumeMenuCommand("teleport") then

pvpFirstPressFrame = nil
        if teleportGateFn ~= nil then
            local ok, allowed = pcall(teleportGateFn)
            if ok and not allowed then
                MPUI.ShowNotification("teleport", teleportGateMsg or "Teleport locked", 120,
                    { color = Color(255, 160, 0) })
                teleportActive = false
                return
            end
        end
        if #ids == 0 then
            MPUI.ShowNotification("teleport", "No other players in this level!", 90,
                { color = Color(255, 160, 0) })
            teleportActive = false
            return
        end
        teleportIds = ids
        if not teleportActive then
            teleportIndex = 1
        else
            teleportIndex = (teleportIndex % #teleportIds) + 1
        end
        teleportActive     = true
        teleportFramesLeft = TELEPORT_NOTIFY_TTL
        local targetId = teleportIds[teleportIndex]
        local name = displayNameFor(targetId)

MPUI.ShowNotification("teleport",
            "Teleport to " .. name .. "? [ENTER] to confirm", TELEPORT_NOTIFY_TTL,
            { color = Color(255, 160, 0), interactive = true,
              flashCheck = function() return KeyIsHit(ActionID.RETURN) end })
        if DEBUG then print("[TELEPORT] Cycling to player " .. targetId .. " (" .. name .. ")") end
    end

    if teleportActive then
        teleportFramesLeft = teleportFramesLeft - 1
        if teleportFramesLeft <= 0 then
            teleportActive = false
            if DEBUG then print("[TELEPORT] Prompt expired - no action") end
        end
    end


if teleportActive and not promptConfirmConsumed
        and KeyIsHit(ActionID.RETURN) and not MPUI.WasEnterConsumedByUI() then
        promptConfirmConsumed = true
        local targetId = teleportIds[teleportIndex]
        local target   = state.remotePlayers[targetId]
        if target then
            local ok = pcall(function()
                local targetPos = target:GetPosition()
                local targetRot = target:GetRotation()


if state.remoteVehicles[targetId] then
                    local rotYRad = math.rad(targetRot.y)

local behindX = -math.sin(rotYRad) * 2048
                    local behindZ = -math.cos(rotYRad) * 2048

                    local spawnPos = Vec3(
                        targetPos.x + behindX,
                        targetPos.y - 1024,
                        targetPos.z + behindZ
                    )
                    Lara:SetPosition(spawnPos)
                    if DEBUG then
                        print(string.format("[TELEPORT] Target %d is in a vehicle - offset to behind+above: (%.0f, %.0f, %.0f)",
                            targetId, spawnPos.x, spawnPos.y, spawnPos.z))
                    end
                else
                    Lara:SetPosition(targetPos)
                end
                Lara:SetRotation(targetRot)
            end)
            local name = displayNameFor(targetId)
            if ok then
                MPUI.ShowNotification("teleport", "Teleported to " .. name .. "!", 90,
                    { color = Color(50, 255, 50) })
                if DEBUG then print("[TELEPORT] Teleported to player " .. targetId .. " (" .. name .. ")") end
            else
                MPUI.ShowNotification("teleport", "Teleport failed!", 90,
                    { color = Color(255, 80, 80) })
                if DEBUG then print("[TELEPORT] SetPosition/SetRotation failed for player " .. targetId) end
            end
        else
            MPUI.ShowNotification("teleport", "Player no longer available!", 90,
                { color = Color(255, 80, 80) })
            if DEBUG then print("[TELEPORT] Target player " .. targetId .. " no longer in state.remotePlayers") end
        end
        teleportActive = false
    end
end

local function checkPvPKeyToggle()
    if MPUI.IsChatOpen() or MPUI.IsNameInputOpen() then return end
    if not state.isConnected then return end


    if state.pendingPvPReqs then
        for fromId, req in pairs(state.pendingPvPReqs) do
            req.timer = req.timer - 1
            if req.timer <= 0 then
                state.pendingPvPReqs[fromId] = nil
            end
        end
    end

    local currentFrame = state.frameCounter or 0


    if pvpFirstPressFrame and (currentFrame - pvpFirstPressFrame) > PVP_CONFIRM_WINDOW then
        pvpFirstPressFrame = nil
    end


local firstReqIdForClick = state.pendingPvPReqs and next(state.pendingPvPReqs)
    local confirmHit = ((KeyIsHit(ActionID.RETURN) and not MPUI.WasEnterConsumedByUI())
        or (KeyIsHit(ActionID.MOUSE_CLICK_LEFT) and not MPUI.WasClickConsumedByUI())
        or MPUI.UIClicked("notif_pvp")
        or (firstReqIdForClick and MPUI.UIClicked("notif_pvpreq_" .. tostring(firstReqIdForClick))))
        and not promptConfirmConsumed


local firstReqId = state.pendingPvPReqs and next(state.pendingPvPReqs)
    if firstReqId and confirmHit and not teleportActive then
        promptConfirmConsumed = true
        local req = state.pendingPvPReqs[firstReqId]
        state.pendingPvPAllow = firstReqId
        state.pendingPvPReqs[firstReqId] = nil
        Multiplayer.SetPvPEnabled(true)
        local name = (req and req.name) or ("Player " .. firstReqId)
        MPUI.ShowNotification("pvp", "PvP accepted with " .. name .. "!", 90, { color = Color(50, 255, 50) })
        if DEBUG then print("[Multiplayer PVP] Accepted PvP request from player " .. firstReqId) end
        pvpFirstPressFrame = nil
        return
    end


    if pvpFirstPressFrame and confirmHit then
        promptConfirmConsumed = true
        Multiplayer.TogglePvP()
        pvpFirstPressFrame = nil
        if state.pvpEnabled then
            MPUI.ShowNotification("pvp", "PvP Enabled!", 90, { color = Color(50, 255, 50) })

            state.pendingPvPReq = true
            if DEBUG then print("[Multiplayer PVP] PvP enabled - broadcasting PVP_REQ to session") end
        else
            MPUI.ShowNotification("pvp", "PvP Disabled!", 90, { color = Color(255, 80, 80) })
            if DEBUG then print("[Multiplayer PVP] PvP disabled") end
        end
        return
    end


    if not (KeyIsHit(ActionID.V) or MPUI.ConsumeMenuCommand("toggle_pvp")) then return end

    if firstReqId then

local req = state.pendingPvPReqs[firstReqId]
        local name = (req and req.name) or ("Player " .. firstReqId)
        MPUI.ShowNotification("pvp", name .. " wants to enable PvP! Press ENTER or click to accept",
            (req and req.timer) or PVP_CONFIRM_WINDOW,
            { color = Color(255, 160, 0), interactive = true,
              flashCheck = function() return KeyIsHit(ActionID.RETURN) end })
        return
    end


teleportActive = false
    pvpFirstPressFrame = currentFrame
    local action = state.pvpEnabled and "DISABLE" or "ENABLE"
    MPUI.ShowNotification("pvp", "Press ENTER or click to " .. action .. " PvP", PVP_CONFIRM_WINDOW,
        { color = Color(255, 160, 0), interactive = true,
          flashCheck = function() return KeyIsHit(ActionID.RETURN) end })
end


local function guardUI(label, fn)
    local ok, err = pcall(fn)
    if not ok then
        print("[Multiplayer UI-GUARD] " .. tostring(label) .. " raised: " .. tostring(err))
    end
end

function Multiplayer.Update(additionalSuffixes, customMessageHandler)
    if not TEN.Flow.IsMultiplayerEnabled() then
        if state.isConnected or next(state.remotePlayers) ~= nil or next(state.remoteVehicles) ~= nil then
            Multiplayer.CleanupRemoteEntities()
            Multiplayer.Disconnect()
        end
        return
    end

    guardUI("MPUI.Update", MPUI.Update)
    promptConfirmConsumed = false
    pollPendingId()

    if not state.isConnected then
        state.disconnectedMsgTimer = (state.disconnectedMsgTimer or 0) + 1
        guardUI("checkNameHPKeyToggle (disconnected)", checkNameHPKeyToggle)
        if showPlayerInfo then
            if state.disconnectedMsgTimer == 1 or state.disconnectedMsgTimer % DISCONNECTED_MSG_INTERVAL == 0 then
                MPUI.ShowNotification("disconnected",
                    "You are disconnected. Check permissions and connection. Restart the game if needed.",
                    540, { color = Color(255, 160, 0) })

MPUI.ShowNotification("disconnected_hint",
                    "Press X for my vibe",
                    540, { overlay = true, x = 95, y = 95, align = "right", color = Color(180, 180, 180),
                           interactive = true,
                           flashCheck = function()
                               return KeyIsHit(ActionID.X) or KeyIsHit(ActionID.RETURN)
                           end })
            end
        else
            MPUI.HideNotification("disconnected")
            MPUI.HideNotification("disconnected_hint")
        end
        return
    end

    state.disconnectedMsgTimer = 0
    state.frameCounter = (state.frameCounter or 0) + 1

    Multiplayer.UpdatePvPWindow()
    updateRemoteVehicleLoops()


do
        local okLvl, lvl = pcall(getCurrentLevelSafe)
        if okLvl and lvl and lvl.levelFile and lvl.levelFile ~= lastResolvedLevelFile then
            lastResolvedLevelFile = lvl.levelFile
            state.myLevelFile = lvl.levelFile:gsub(":", "")

local label
            local key = lvl.nameKey
            if key and #key > 0 and TEN.Flow.IsStringPresent(key) then
                local okStr, s = pcall(TEN.Flow.GetString, key)
                if okStr and s and #s > 0 then label = s end
            end
            if not label then
                label = state.myLevelFile:gsub("^.*[\\/]", ""):gsub("%.ten$", "")
            end
            label = label:gsub("[:\r\n]", " "):gsub("%s+", " "):match("^%s*(.-)%s*$")
            state.myLevelName = (#label > 0) and label or state.myLevelFile
        end
    end


do
        local freezeToken = nil
        if KeyIsHit(ActionID.PAUSE) or KeyIsHit(ActionID.INVENTORY) then
            freezeToken = "PAUSE"
        elseif KeyIsHit(ActionID.SAVE) then
            freezeToken = "SAVING"
        elseif KeyIsHit(ActionID.LOAD) then
            freezeToken = "LOADING"
        end
        if freezeToken then
            state.pendingPauseNotify = freezeToken
            if DEBUG then print("[Multiplayer:PrePause] flagging " .. freezeToken .. " for remote players") end
        end
    end

    guardUI("checkNameHPKeyToggle", checkNameHPKeyToggle)
    guardUI("checkPvPKeyToggle", checkPvPKeyToggle)
    guardUI("checkTeleportKeyToggle", checkTeleportKeyToggle)

    guardUI("name-change", function()
        local newName = MPUI.GetPendingNameChange and MPUI.GetPendingNameChange()
        if newName then
            Multiplayer.SetPlayerName(newName)
            state.myName = sanitizeName(newName, SCRIPT_NAME_MAX) or state.myName
            nameSetDirty = true
            MPUI.ShowNotification("namechange", "Name: " .. (state.myName or newName), 120,
                { color = Color(100, 200, 255) })
            if DEBUG then print("[Multiplayer NAME] Name changed to: " .. tostring(state.myName)) end
        end
    end)

    receive(customMessageHandler)
    sendPlayerData(additionalSuffixes)
    updatePlayerNames()

guardUI("updateFreezeIndicators", updateFreezeIndicators)



for _, torchPos in pairs(state.torchPositions) do
        pcall(function() TEN.Effects.EmitFire(torchPos, 0.5) end)
    end


local FIRE_SIZE = 3.0
    for _, firePos in pairs(state.firePositions) do
        pcall(function() TEN.Effects.EmitFire(firePos, FIRE_SIZE) end)
    end
end

function Multiplayer.IsConnected()
    return state.isConnected == true
end

function Multiplayer.GetMyId()
    return state.myId
end

function Multiplayer.GetRemotePlayer(playerId)
    return state.remotePlayers[playerId]
end

function Multiplayer.GetRemotePlayers()
    return state.remotePlayers
end


function Multiplayer.GetMyLevelFile()
    return state.myLevelFile
end

function Multiplayer.GetRemotePlayerLevel(id)
    return state.remotePlayerLevel and state.remotePlayerLevel[id]
end

function Multiplayer.GetRemotePlayerLevels()
    return state.remotePlayerLevel or {}
end


function Multiplayer.RequestLevels()
    if not state.isConnected then return end
    pendingLevelReq = true
end


function Multiplayer.GetPlayerRoster()
    local roster = {}
    local myId   = state.myId
    local myLvl  = state.myLevelFile
    roster[#roster + 1] = {
        id         = myId,
        name       = (myId and displayNameFor(myId))
                     or state.customName or state.myName or "You",
        levelLabel = myLvl or "?",
        levelName  = state.myLevelName or myLvl or "?",
        isSelf     = true,
        sameLevel  = true,
    }
    local ids = {}
    for id in pairs(remotePlayerNames)               do if id ~= myId then ids[id] = true end end
    for id in pairs(state.remotePlayerLevel or {})   do if id ~= myId then ids[id] = true end end
    for id in pairs(state.remotePlayers or {})       do if id ~= myId then ids[id] = true end end
    local ordered = {}
    for id in pairs(ids) do ordered[#ordered + 1] = id end
    table.sort(ordered)
    for _, id in ipairs(ordered) do
        local lvl  = state.remotePlayerLevel and state.remotePlayerLevel[id]
        local lvlN = state.remotePlayerLevelName and state.remotePlayerLevelName[id]
        roster[#roster + 1] = {
            id         = id,
            name       = displayNameFor(id),
            levelLabel = lvl or "unknown",
            levelName  = lvlN or lvl or "unknown",
            isSelf     = false,
            sameLevel  = (lvl ~= nil and lvl == myLvl),
        }
    end
    return roster
end


function Multiplayer.CleanupRemoteEntities()
    local playerCount = 0
    for _ in pairs(state.remotePlayers) do playerCount = playerCount + 1 end
    local vehicleCount = 0
    for _ in pairs(state.remoteVehicles) do vehicleCount = vehicleCount + 1 end
    print("[CLEANUP] CleanupRemoteEntities called: remotePlayers=" .. playerCount ..
          " remoteVehicles=" .. vehicleCount ..
          " frame=" .. tostring(state.frameCounter))

    for id, player in pairs(state.remotePlayers) do
        local ok, err = destroyPuppet(player)
        if not ok then
            print("[MP] Removing player " .. tostring(id) .. " failed: " .. tostring(err))
        end
    end
    state.remotePlayers = {}
    nameSetDirty = true

    for id, vehicle in pairs(state.remoteVehicles) do
        local ok, err = true, nil
        if TEN.Flow.IsMultiplayerEnabled() then
            ok, err = pcall(function() vehicle:Destroy() end)
        end
        if not ok then
            print("[CLEANUP ERROR] vehicle " .. tostring(id) .. " Destroy() failed: " .. tostring(err))
        end
    end
    state.remoteVehicles = {}
    state.remoteVehicleFailLogged = {}
    state.remoteVehicleLoops = {}

    state.torchFlames      = {}
    state.torchPositions   = {}
    state.firePositions    = {}
    remotePlayerTorchLit   = {}
    remotePlayerOnFire     = {}

    cleanupAllNames()
    MPUI.Cleanup()
end

function Multiplayer.Reset()
    Multiplayer.CleanupRemoteEntities()
    state.myId = nil
    state.myName = nil
    state.frameCounter = 0
    state.lastSentState = nil
    state.pendingSoundID = 0
    state.lastSentSoundID = 0
    state.lastReceivedSoundID = {}
    state.lastSoundFrame = {}
    state.pendingHits = {}
    lastSentTyping     = nil
    lastSentName       = nil
    lastSentPvP        = nil
    lastSentHP         = nil
    lastSentTorchLit   = nil
    lastSentOnFire     = nil
    lastSentAimSuffix  = nil
    lastSentLookSuffix = nil
    lastSentJL         = nil
    teleportActive     = false
    teleportIds        = {}
    teleportIndex      = 0
    teleportFramesLeft = 0
    state.pendingPvPReqs           = {}
    state.pendingPvPReq            = nil
    state.pendingPvPAllow          = nil
    Multiplayer.Disconnect()
end


-- The player name lives in GlobalVars, so a save has nothing of its own to store. The function stays for the level scripts that call it.
function Multiplayer.OnSave()
end

function Multiplayer.OnLoad()
    local saved = GlobalVars.Engine.Multiplayer.customName
    if saved and #saved > 0 then
        state.customName = saved
    end
end

function Multiplayer.SetPendingSound(soundID)
    state.pendingSoundID = soundID or 0
end

function Multiplayer.GetFrameCounter()
    return state.frameCounter or 0
end

function Multiplayer.GetDisconnectedTimer()
    return state.disconnectedMsgTimer or 0
end

function Multiplayer.SetPlayerName(name)
    state.customName = sanitizeName(name, SCRIPT_NAME_MAX)
    GlobalVars.Engine.Multiplayer.customName = state.customName

    lastSentName = nil
    if DEBUG then
        print(string.format("[MP] Player name set to '%s'.", tostring(state.customName)))
    end
end

function Multiplayer.GetPlayerName()
    return state.myName
end


-- Sets the address of the server for the next Connect(). The port is optional.
function Multiplayer.SetHost(host, port)
    state.host = host or state.host
    state.port = port or state.port
end

-- Sets the game version that is announced to the server at connect. Call it before the first Connect().
function Multiplayer.SetVersion(version)
    gameVersion = version
end

function Multiplayer.GetVersion()
    return gameVersion
end


function Multiplayer.IsDefaultName()
    return state.customName == nil
end


function Multiplayer.SendServerLine(line)
    if type(line) ~= "string" or #line == 0 then return false end
    if not (state.isConnected and state.client) then return false end
    if line:sub(-1) ~= "\n" then line = line .. "\n" end
    local ok, err = pcall(function() return state.client:send(line) end)
    if not ok then
        if DEBUG then print("[Multiplayer] SendServerLine failed: " .. tostring(err)) end
        return false
    end
    return true
end


function Multiplayer.SetPvPEnabled(enabled)
    state.pvpEnabled = enabled == true
    for id, player in pairs(state.remotePlayers) do

        local shouldTarget = state.pvpEnabled and (state.pvpForced or remotePvPState[id])
        pcall(function()
            TEN.Objects.SetPvPTargetable(player:GetIndex(), shouldTarget == true)
        end)
    end

if state.pvpWindowUntil ~= nil and state.pvpWindowApplying ~= true then
        state.pvpWindowPrevEnabled = state.pvpEnabled == true
    end
    if DEBUG then print("[Multiplayer PVP] Local PvP = " .. tostring(state.pvpEnabled)) end
end

function Multiplayer.TogglePvP()
    Multiplayer.SetPvPEnabled(not state.pvpEnabled)
end

function Multiplayer.IsPvPEnabled()
    return state.pvpEnabled == true
end




function Multiplayer.SetPvPForced(forced)
    state.pvpForced = forced == true
    if forced then Multiplayer.SetPvPEnabled(true) end
end


function Multiplayer.SetPvPHarmless(enabled)
    state.pvpHarmless = enabled == true
    if DEBUG then print("[Multiplayer PVP] harmless PvP = " .. tostring(state.pvpHarmless)) end
end

function Multiplayer.IsPvPHarmless()
    return state.pvpHarmless == true
end


function Multiplayer.HasLocalPvPConsent()
    if state.pvpWindowUntil ~= nil then
        return state.pvpWindowPrevEnabled == true
    end
    return state.pvpEnabled == true
end


function Multiplayer.IsPvPDamageHarmless()
    if state.pvpHarmless ~= true then return false end
    return not Multiplayer.HasLocalPvPConsent()
end


function Multiplayer.IsPvPDamageHarmlessFor(id)
    if state.pvpHarmless ~= true then return false end
    if not Multiplayer.HasLocalPvPConsent() then return true end
    return remotePvPState[id] ~= true
end


function Multiplayer.BeginPvPWindow(frames)
    frames = tonumber(frames) or 90
    local now = state.frameCounter or 0
    if state.pvpWindowUntil == nil then
        state.pvpWindowPrevEnabled = state.pvpEnabled == true
        state.pvpWindowPrevForced  = state.pvpForced == true
    end
    if state.pvpWindowUntil == nil or (now + frames) > state.pvpWindowUntil then
        state.pvpWindowUntil = now + frames
    end
    state.pvpWindowApplying = true
    Multiplayer.SetPvPForced(true)
    state.pvpWindowApplying = nil
    if DEBUG then print("[Multiplayer PVP] forced PvP window until frame " .. state.pvpWindowUntil) end
end

function Multiplayer.IsPvPWindowActive()
    return state.pvpWindowUntil ~= nil
end


function Multiplayer.UpdatePvPWindow()
    if state.pvpWindowUntil == nil then return end
    if (state.frameCounter or 0) < state.pvpWindowUntil then return end
    state.pvpWindowUntil = nil
    state.pvpForced = state.pvpWindowPrevForced == true
    Multiplayer.SetPvPEnabled(state.pvpWindowPrevEnabled == true)
    if DEBUG then print("[Multiplayer PVP] forced PvP window expired") end
end


function Multiplayer.GetLastPvPAttacker()
    return state.lastPvPAttackerId, state.lastPvPAttackFrame
end

function Multiplayer.ClearLastPvPAttacker()
    state.lastPvPAttackerId  = nil
    state.lastPvPAttackFrame = nil
end


function Multiplayer.GetRemotePlayerName(id)
    if not id then return nil end
    if id == state.myId then return (state.myName or "") .. dupeSuffixFor(id) end
    return (remotePlayerNames[id] or ("Player " .. tostring(id))) .. dupeSuffixFor(id)
end




function Multiplayer.HandleLaraSoundEffect(soundID, frameNumber)
    soundID = tonumber(soundID)
    frameNumber = tonumber(frameNumber)
    if soundID == SOUND_WIRE_ALIAS_TARGET then
        soundID = SOUND_WIRE_ALIAS_FOOTSTEPS
    end

    if not soundID or soundID <= 0 then return end

    Multiplayer.SetPendingSound(soundID)
end


local fakeDeath = {
    supported = (TEN and TEN.Util and TEN.Util.EnableFakeDeath) and true or false,
    enabled   = false,
}

function Multiplayer.EnableFakeDeath(enable)
    if enable == nil then enable = true end
    enable = enable and true or false
    if not fakeDeath.supported then
        print("[Multiplayer] EnableFakeDeath: TEN.Util.EnableFakeDeath missing (old engine build?)")
        return false
    end
    TEN.Util.EnableFakeDeath(enable)
    fakeDeath.enabled = enable
    return true
end


function Multiplayer.IsFakeDeathEnabled()
    return fakeDeath.enabled
end

Multiplayer.UI = MPUI

MPUI.SetConnectedProvider(function() return state.isConnected end)


if MPUI.SetPlayerRosterProvider then
    MPUI.SetPlayerRosterProvider(
        function() return Multiplayer.GetPlayerRoster() end,
        function() Multiplayer.RequestLevels() end
    )
end

return Multiplayer
