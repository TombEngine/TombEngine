-- Title script file.
-- To include other script files, you can use require("filename") command.

-- Called when entering a level, either after leveljump, new game or loading game
LevelFuncs.OnStart = function() end

-- Called after loading from a save
LevelFuncs.OnLoad = function() end

-- Called after saving game
LevelFuncs.OnSave = function() end

-- Called on every frame of the game
LevelFuncs.OnLoop = function() end

-- Called when level is ended, either after leveljump, quitting to title or loading game
LevelFuncs.OnEnd = function() end