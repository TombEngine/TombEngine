local DragonCutscene = require("Engine.TR2_DRAGON_Cutscene")

LevelFuncs.OnStart = function()
    DragonCutscene.Init()
end

LevelFuncs.OnLoop = function()
    DragonCutscene.Update()
end