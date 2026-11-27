-- Example level script for Engine/Multiplayer.lua. It connects when the level starts or loads, runs the module
-- every frame, reconnects once a second while disconnected, and passes the sounds of the local player on.

local Multiplayer = require("Engine.Multiplayer")

local RECONNECT_INTERVAL = 60

local levelState = {
    isSaving = false
}

LevelFuncs = LevelFuncs or {}

LevelFuncs.OnLoad = function()
    Multiplayer.OnLoad()
    Multiplayer.Reset()
    Multiplayer.Connect()
    levelState.isSaving = false
end

LevelFuncs.OnSave = function()
    Multiplayer.OnSave()
    levelState.isSaving = true

    -- The remote players are not saved, so they are removed before the save.
    Multiplayer.CleanupRemoteEntities()
end

LevelFuncs.OnStart = function()
    Multiplayer.Connect()
end

LevelFuncs.OnLoop = function(dt)
    -- Update() runs while multiplayer is off too, so it can remove the remote players when the option is switched off.
    if not TEN.Flow.IsMultiplayerEnabled() then
        Multiplayer.Update()
        return
    end

    if levelState.isSaving then
        levelState.isSaving = false
        Multiplayer.Connect()
        return
    end

    Multiplayer.Update()

    if not Multiplayer.IsConnected() and Multiplayer.GetDisconnectedTimer() % RECONNECT_INTERVAL == 0 then
        Multiplayer.Connect()
    end
end

LevelFuncs.OnEnd = function()
    Multiplayer.Disconnect()
    Multiplayer.CleanupRemoteEntities()
end

LevelFuncs.OnUseItem = function(item)
    if not Multiplayer.IsConnected() then
        Multiplayer.Connect()
    end
end

-- Called by the engine for every sound of the local player while multiplayer is in effect.
LevelFuncs.OnLaraSoundEffect = function(soundID, frameNumber)
    Multiplayer.HandleLaraSoundEffect(soundID, frameNumber)
end
