-- Place in this Lua script all the levels of your game
-- Title is mandatory and must be the first level.

-- Intro image is a splash screen which appears before actual loading screen.
-- If you don't want it to appear, just remove this line.

Flow.SetIntroImagePath("Screens\\intro.jpg")

-- Intro video plays right after or instead of intro image, if specified.
-- If you don't want it to appear, just remove this line.

Flow.SetIntroVideoPath("Fmv\\intro.mp4")

-- Set overall amount of secrets in game.
-- If set to 0, secrets won't be displayed in statistics.

Flow.SetTotalSecretCount(5)

-- Enable/Disable Point Filter (square, unsmoothed pixels).

Flow.EnablePointFilter(false)

-- Enable/Disable saving and loading of savegames.

Flow.EnableLoadSave(true)

-- Disable/enable flycheat globally

Flow.EnableFlyCheat(true)

-- Disable/enable Lara drawing in title level

Flow.EnableLaraInTitle(false)

-- Disable/enable level selection in title level

Flow.EnableLevelSelect(true)

-- Disable/enable mass pickup (collect all pickups at once)

Flow.EnableMassPickup(true)

--------------------------------------------------

-- Title level

title = Level.new()

title.ambientTrack = "108"
title.levelFile = "Data\\title.ten"
title.scriptFile = "Scripts\\Levels\\title.lua"
title.loadScreenFile = "Screens\\main.png"

Flow.AddLevel(title)

--------------------------------------------------