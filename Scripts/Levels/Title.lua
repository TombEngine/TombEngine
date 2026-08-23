-- Title script file.
-- Demonstrates a fully script-driven title screen that replaces the stock menu.

-- Custom title menu: hide the stock menu and react to a few hotkeys.
--  1        -> RequestTitleNewGame
--  2 (or L) -> RequestTitleLoadGame  (opens the stock Load/Save menu)
--  3        -> RequestTitleExitGame

local titleTitle  = nil
local titleNew    = nil
local titleLoad   = nil
local titleExit   = nil
local titleHint   = nil

LevelFuncs.OnStart = function()
	-- Hide the stock title menu; the script draws its own instead.
	TEN.Flow.EnableTitleMenu(false)

	local cx = 50 -- horizontal percent (centered text)

	titleTitle = TEN.Strings.DisplayString("S C R I P T   T I T L E", TEN.Vec2(TEN.Util.PercentToScreen(50, 25)), 1.5,
		TEN.Color(255, 220, 120), false, { TEN.Strings.DisplayStringOption.CENTER })
	titleNew  = TEN.Strings.DisplayString("1 - New Game", TEN.Vec2(TEN.Util.PercentToScreen(cx, 45)), 1.0,
		TEN.Color(255, 255, 255), false, { TEN.Strings.DisplayStringOption.CENTER })
	titleLoad = TEN.Strings.DisplayString("2 - Load Game", TEN.Vec2(TEN.Util.PercentToScreen(cx, 50)), 1.0,
		TEN.Color(255, 255, 255), false, { TEN.Strings.DisplayStringOption.CENTER })
	titleExit = TEN.Strings.DisplayString("3 - Exit Game", TEN.Vec2(TEN.Util.PercentToScreen(cx, 55)), 1.0,
		TEN.Color(255, 255, 255), false, { TEN.Strings.DisplayStringOption.CENTER })
	titleHint = TEN.Strings.DisplayString("Press a number key", TEN.Vec2(TEN.Util.PercentToScreen(50, 70)), 0.8,
		TEN.Color(160, 160, 160), false, { TEN.Strings.DisplayStringOption.CENTER })

	-- Omit the time argument to keep strings displayed indefinitely.
	TEN.Strings.ShowString(titleTitle)
	TEN.Strings.ShowString(titleNew)
	TEN.Strings.ShowString(titleLoad)
	TEN.Strings.ShowString(titleExit)
	TEN.Strings.ShowString(titleHint)
end

LevelFuncs.OnLoop = function()
	if TEN.Input.KeyIsHit(TEN.Input.ActionID.NUM1) then
		TEN.Flow.RequestTitleNewGame()
	elseif TEN.Input.KeyIsHit(TEN.Input.ActionID.NUM2) or TEN.Input.KeyIsHit(TEN.Input.ActionID.L) then
		TEN.Flow.RequestTitleLoadGame()
	elseif TEN.Input.KeyIsHit(TEN.Input.ActionID.NUM3) then
		TEN.Flow.RequestTitleExitGame()
	end
end

LevelFuncs.OnLoad = function() end
LevelFuncs.OnSave = function() end
LevelFuncs.OnEnd = function()
	for _, s in ipairs({ titleTitle, titleNew, titleLoad, titleExit, titleHint }) do
		if s then TEN.Strings.HideString(s) end
	end
end