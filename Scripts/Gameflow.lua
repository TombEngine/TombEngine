-- Place in this Lua script all the levels of your game
-- Title is mandatory and must be the first level.

-- Intro image is a splash screen which appears before actual loading screen.
-- If you don't want it to appear, just remove this line.

Flow.SetIntroImagePath("Screens\\main.jpg")

-- This image should be used for static title screen background (as in TR1-TR3).
-- For now it is not implemented.

Flow.SetTitleScreenImagePath("Screens\\main.jpg")

-- Set overall amount of secrets in game.
-- If set to 0, secrets won't be displayed in statistics.

Flow.SetTotalSecretCount(5)
Flow.EnableLaraInTitle(false)
-- Disable/enable flycheat globally

Flow.EnableFlyCheat(true)
Flow.EnableLevelSelect(true)
-- Disable/enable mass pickup (collect all pickups at once)

Flow.EnableMassPickup(true)


--------------------------------------------------

-- ============================================================================
-- Weather Preset Definitions
-- Paste into Gameflow.lua (after level definitions) to customize cloud types.
-- Only define presets you want to CHANGE — unmodified presets use C++ defaults.
-- ============================================================================

-- ClearSky: Wolkenloser Himmel
--Flow.DefineWeatherPreset("ClearSky", {
 --   transitionDuration = 60,
 --   randomWeight       = 2.0
    -- Keine cloudA/cloudB = komplett klar
--})

-- FewClouds: Vereinzelte hohe Federwolken


-- ScatteredClouds: Verstreute Haufenwolken(schäfchen)


-- BrokenClouds: Aufgelockerte Bewölkung (zwei Schichten)


-- Title level

title = Level.new()

title.ambientTrack = "108"
title.levelFile = "Data\\title.ten"
title.scriptFile = "Scripts\\title.lua"
title.loadScreenFile = "Screens\\Main.png"

Flow.AddLevel(title)

--------------------------------------------------

-- First test level

test = Level.new()

test.nameKey = "test"
test.scriptFile = "Scripts\\test.lua"
test.ambientTrack = "108"
test.levelFile = "Data\\test.ten"
test.loadScreenFile = "Screens\\rome.jpg"

-- 0 is no weather, 1 is rain, 2 is snow.
-- Strength varies from 0 to 1 (floating-point value, e.g. 0.5 means half-strength).

test.weather = 0
test.weatherStrength = 1.0
test.lensFlare = Flow.LensFlare(36,124) --Flow.LensFlare(36,124,Color(128,128,128))
test.lensFlare.spriteID = 32

-- Per-level dynamic sky configuration ----------------------------------------
test.dynamicSky.realisticskydome  = true
test.dynamicSky.blackvoidcolor    = Color(0, 0, 0)
test.dynamicSky.horizonbottomfade = 0.0

-- Aurora borealis (replaces bitmap sky layer 1 when enabled).
test.dynamicSky.Aurora.enabled = false
test.dynamicSky.Aurora.color   = "GreenClassic"

-- Volumetric clouds (replaces bitmap sky layer 1 when enabled).
test.dynamicSky.Clouds.enabled           = true
test.dynamicSky.Clouds.startPreset       = "Altocumulus"
test.dynamicSky.Clouds.quality           = "Medium"   -- "Low", "Medium", or "High"
test.dynamicSky.Clouds.windSpeed         = 0.3
test.dynamicSky.Clouds.windDirectionX    = 1.0
test.dynamicSky.Clouds.windDirectionZ    = 0.0
test.dynamicSky.Clouds.transformDuration = 60

-- Random preset rotation. duration = wie lange ein Preset bleibt,
-- percent = relative Wahrscheinlichkeit beim Wechsel.
-- test.dynamicSky.Clouds.changePresets = {
--     ClearSky      = { duration = 30, percent = 50 },
--     Cirrustratus  = { duration = 60, percent = 30 },
--     Altocumulus   = { duration = 45, percent = 20 },
-- }

test.horizon = true
test.farView = 20
--test.layer1 = Flow.SkyLayer.new(Color.new(255, 0, 0), 15)
test.fog = Flow.Fog.new(Color.new(112, 0, 112), 12, 15)

-- Presets for inventory item placement.

test.objects = {
	InventoryItem.new(
		"tut1_ba_cartouche1",
		ObjID.PUZZLE_ITEM3_COMBO1,
		0,
		0.5,
		Rotation.new(0, 0, 0),
		RotationAxis.Y,
		-1,
		ItemAction.USE
	),
	InventoryItem.new(
		"tut1_ba_cartouche2",
		ObjID.PUZZLE_ITEM3_COMBO2,
		0,
		0.5,
		Rotation.new(0, 0, 0),
		RotationAxis.Y,
		-1,
		ItemAction.USE
	),
	InventoryItem.new(
		"tut1_ba_cartouche",
		ObjID.PUZZLE_ITEM3,
		0,
		0.5,
		Rotation.new(0, 0, 0),
		RotationAxis.Y,
		-1,
		ItemAction.USE
	),
	InventoryItem.new(
		"tut1_hand_orion",
		ObjID.PUZZLE_ITEM6,
		0,
		0.5,
		Rotation.new(270, 180, 0),
		RotationAxis.Y,
		-1,
		ItemAction.USE
	),
	InventoryItem.new(
		"tut1_hand_sirius",
		ObjID.PUZZLE_ITEM8,
		0,
		0.5,
		Rotation.new(270, 180, 0),
		RotationAxis.X,
		-1,
		ItemAction.USE
	)
}

Flow.AddLevel(test)

Maya4a = Level.new()

Maya4a.nameKey = "maya"
Maya4a.scriptFile = "Scripts\\Maya4a.lua"
Maya4a.ambientTrack = "108"
Maya4a.levelFile = "Data\\Maya4a.ten"
Maya4a.loadScreenFile = "Screens\\rome.jpg"

-- 0 is no weather, 1 is rain, 2 is snow.
-- Strength varies from 0 to 1 (floating-point value, e.g. 0.5 means half-strength).

Maya4a.weather = 1
Maya4a.weatherStrength = 0.5

Maya4a.horizon = true
Maya4a.farView = 20
--Maya4a.layer1 = Flow.SkyLayer.new(Color.new(255, 0, 0), 15)
Maya4a.fog = Flow.Fog.new(Color.new(80, 112, 104), 8, 20)

-- Presets for inventory item placement.

Maya4a.objects = {
	InventoryItem.new(
		"tut1_ba_cartouche1",
		ObjID.PUZZLE_ITEM3_COMBO1,
		0,
		0.5,
		Rotation.new(0, 0, 0),
		RotationAxis.Y,
		-1,
		ItemAction.USE
	),
	
	
}

Flow.AddLevel(Maya4a)