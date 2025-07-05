------
-- This module provides functions for creating and managing custom progress bars. It stores bar definitions and configurations in `LevelVars.Engine.CustomBars`, enabling seamless state management. 
-- Each bar is independently controlled through its associated functions.
--
-- Example usage:
--
--	local CustomBar = require("Engine.CustomBar")
--
--	-- Create a table with all the bar properties
--	local barData = {
--		barName             = "water",
--		startValue          = 0,
--		maxValue            = 1000,
--		objectIdBg          = TEN.Objects.ObjID.CUSTOM_BAR_GRAPHIC,
--		spriteIdBg          = 0,
--		colorBg             = TEN.Color(255,255,255),
--		posBg               = TEN.Vec2(20, 20),
--		rotBg               = 0,
--		scaleBg             = TEN.Vec2(19.05, 19.1),
--		alignModeBg         = TEN.View.AlignMode.CENTER_LEFT,
--		scaleModeBg         = TEN.View.ScaleMode.FIT,
--		blendModeBg         = TEN.Effects.BlendID.ALPHABLEND,
--		objectIdBar         = TEN.Objects.ObjID.CUSTOM_BAR_GRAPHIC,
--		spriteIdBar         = 1,
--		colorBar            = TEN.Color(255,0,0),
--		posBar              = TEN.Vec2(20.15, 20),
--		rot                 = 0,
--		scaleBar            = TEN.Vec2(18.7, 18.48),
--		alignMode           = TEN.View.AlignMode.CENTER_LEFT,
--		scaleMode           = TEN.View.ScaleMode.FIT,
--		blendMode           = TEN.Effects.BlendID.ALPHABLEND,
--		text                = "Water Bar",
--		textPos             = TEN.Vec2(20, 15),
--		textOptions         = {TEN.Strings.DisplayStringOption.SHADOW,TEN.Strings.DisplayStringOption.CENTER},
--		textScale           = 1,
--		textColor           = TEN.Color(255,0,0),
--		hideText            = false,
--		alphaBlendSpeed     = 50,
--		blink               = false,
--		blinkLimit          = 0.25
--	}
--
--	-- This function creates the bar.
--	CustomBar.Create(barData)
--
--	-- This method gets the bar with name "water" and stores it in variable bar.
--	local bar = CustomBar.Get("water")
--	-- This method displays the bar
--	bar:SetVisibility(true)
--	-- This method sets the bar value to 1000 over 5 seconds.
--	bar:SetBarValue(1000,5)
--
-- @luautil CustomBar


local CustomBar = {}

CustomBar.__index = CustomBar

LevelFuncs.Engine.CustomBar = {}
LevelVars.Engine.CustomBars = {bars = {}, enemiesHpBar = {status = nil}}

---
-- Creates a custom progress bar with extensive configuration options.
-- @tparam table barData The table that contains all the bar data. Refer to table setup for barData.
--
-- @treturn CustomBar The custombar in its hidden state
--
CustomBar.Create = function (barData)

	local dataName	= barData.barName .. "_bar_data"
	local self = {name = dataName}

	if LevelVars.Engine.CustomBars.bars[dataName] then
        print("Warning: a customBar with name " .. dataName .. " already exists; overwriting it with a new one...")
	end
---
-- Table setup for creating custom bar.
-- @table barData
-- @tfield string barName Unique identifier for the bar.
-- @tfield float startValue Initial value of the bar.
-- @tfield float maxValue Maximum value of the bar.
-- @tfield Objects.ObjID objectIdBg Object ID for the bar's background sprite.
-- @tfield int spriteIdBg Sprite ID from the specified object for the bar's background.
-- @tfield Color colorBg Color of bar's background.
-- @tfield Vec2 posBg X,Y position of the bar's background in screen percent (0-100).
-- @tfield float rotBg Rotation of the bar's background sprite (0-360).
-- @tfield Vec2 scaleBg X,Y Scaling factor for the bar's background sprite.
-- @tfield View.AlignMode alignModeBg Alignment for the bar's background.
-- @tfield View.ScaleMode scaleModeBg Scaling for the bar's background.
-- @tfield Effects.BlendID blendModeBg Blending modes for the bar's background.
-- @tfield Objects.ObjID objectIdBar Object ID for the bar sprite.
-- @tfield int spriteIdBar Sprite ID from the specified object for the bar.
-- @tfield Color colorBar Color of the bar.
-- @tfield Vec2 posBar X,Y position of the bar in screen percent (0-100).
-- @tfield float rot rotation of the bar's sprite (0-360).
-- @tfield Vec2 scaleBar X,Y Scaling factor for the bar's sprite.
-- @tfield View.AlignMode alignMode Alignment for the bar.
-- @tfield View.ScaleMode scaleMode Scaling for the bar.
-- @tfield Effects.BlendID blendMode Blending modes for the bar.
-- @tfield string text Text to display on the bar.
-- @tfield Vec2 textPos X,Y position of the text.
-- @tfield Strings.DisplayStringOption textOptions Alignment and effects for the text. Note that text is automatically aligned to the left.
-- @tfield float textScale Scale factor for the text.
-- @tfield Color textColor Color of the text.
-- @tfield bool hideText Whether to hide the text.
-- @tfield int alphaBlendSpeed Speed of alpha blending for bar visibility (0-255).
-- @tfield bool blink Whether the bar blinks.
-- @tfield float blinkLimit Limit below which bar starts blinking (0-1).

	LevelVars.Engine.CustomBars.bars[dataName]			        = {}
	LevelVars.Engine.CustomBars.bars[dataName].name				= dataName
	LevelVars.Engine.CustomBars.bars[dataName].fixedInterval	= 1/30
	LevelVars.Engine.CustomBars.bars[dataName].progress			= barData.startValue / barData.maxValue -- Set initial progress from start value
	LevelVars.Engine.CustomBars.bars[dataName].objectIdBg		= barData.objectIdBg
	LevelVars.Engine.CustomBars.bars[dataName].spriteIdBg		= barData.spriteIdBg
	LevelVars.Engine.CustomBars.bars[dataName].colorBg			= barData.colorBg
	LevelVars.Engine.CustomBars.bars[dataName].posBg			= barData.posBg
	LevelVars.Engine.CustomBars.bars[dataName].scaleBg			= barData.scaleBg
	LevelVars.Engine.CustomBars.bars[dataName].rotBg			= barData.rotBg
	LevelVars.Engine.CustomBars.bars[dataName].alignModeBg	    = barData.alignModeBg
	LevelVars.Engine.CustomBars.bars[dataName].scaleModeBg	    = barData.scaleModeBg
	LevelVars.Engine.CustomBars.bars[dataName].blendModeBg	    = barData.blendModeBg
	LevelVars.Engine.CustomBars.bars[dataName].objectIdBar		= barData.objectIdBar
	LevelVars.Engine.CustomBars.bars[dataName].spriteIdBar		= barData.spriteIdBar
	LevelVars.Engine.CustomBars.bars[dataName].colorBar			= barData.colorBar
	LevelVars.Engine.CustomBars.bars[dataName].posBar			= barData.posBar
	LevelVars.Engine.CustomBars.bars[dataName].scaleBar			= barData.scaleBar
	LevelVars.Engine.CustomBars.bars[dataName].rot				= barData.rot
	LevelVars.Engine.CustomBars.bars[dataName].alignMode		= barData.alignMode
	LevelVars.Engine.CustomBars.bars[dataName].scaleMode		= barData.scaleMode
	LevelVars.Engine.CustomBars.bars[dataName].blendMode		= barData.blendMode
	LevelVars.Engine.CustomBars.bars[dataName].oldValue			= barData.startValue  -- stores the current bar value
	LevelVars.Engine.CustomBars.bars[dataName].targetValue		= barData.startValue  -- target value to reach
	LevelVars.Engine.CustomBars.bars[dataName].maxValue			= barData.maxValue
	LevelVars.Engine.CustomBars.bars[dataName].text				= barData.text
	LevelVars.Engine.CustomBars.bars[dataName].textPos			= barData.textPos
	LevelVars.Engine.CustomBars.bars[dataName].textOptions		= barData.textOptions
	LevelVars.Engine.CustomBars.bars[dataName].textScale		= barData.textScale
	LevelVars.Engine.CustomBars.bars[dataName].textColor		= barData.textColor
	LevelVars.Engine.CustomBars.bars[dataName].hideText			= barData.hideText  -- required to hide bar text
	LevelVars.Engine.CustomBars.bars[dataName].visible			= false
	LevelVars.Engine.CustomBars.bars[dataName].currentAlpha		= 0
	LevelVars.Engine.CustomBars.bars[dataName].targetAlpha		= 0
	LevelVars.Engine.CustomBars.bars[dataName].alphaBlendSpeed	= barData.alphaBlendSpeed
	LevelVars.Engine.CustomBars.bars[dataName].blink			= barData.blink
	LevelVars.Engine.CustomBars.bars[dataName].blinkLimit		= barData.blinkLimit
	LevelVars.Engine.CustomBars.bars[dataName].blinkSpeed		= 8
	LevelVars.Engine.CustomBars.bars[dataName].showBar			= nil --required to hide bar when enemy is not targeted
	LevelVars.Engine.CustomBars.bars[dataName].object			= nil
	LevelVars.Engine.CustomBars.bars[dataName].getActionType	= nil
	LevelVars.Engine.CustomBars.bars[dataName].currentTimer		= 0
    return setmetatable(self, CustomBar)
end

---
-- Creates a bar tied to Players's attributes (Health, Air, Stamina).
-- @tparam table playerBarData The table that contains all the player bar data. Refer to table setup for playerBarData.
-- @treturn CustomBar Player attribute bar.

CustomBar.CreatePlayerBar = function (playerBarData)

	local barName = "Player" .. playerBarData.getActionType
	local dataName	= barName .. "_bar_data"

	if playerBarData.getActionType >= 1 and playerBarData.getActionType <= 3 then
		local startValue = playerBarData.getActionType == 1 and Lara:GetHP() or
		(playerBarData.getActionType == 2 and Lara:GetAir() or
		(playerBarData.getActionType == 3 and Lara:GetStamina()))
		local maxValue = playerBarData.getActionType == 1 and 1000 or (playerBarData.getActionType == 2 and 1800 or (playerBarData.getActionType == 3 and 120))

---
-- Table setup for creating custom player attribute bar.
-- @table playerBarData
-- @tfield int getActionType Determines the bar type: 1: Health, 2: Air, 3: Stamina.
-- @tfield Objects.ObjID objectIdBg Object ID for the bar's background sprite.
-- @tfield int spriteIdBg Sprite ID from the specified object for the bar's background.
-- @tfield Color colorBg Color of bar's background.
-- @tfield Vec2 posBg X,Y position of the bar's background in screen percent (0-100).
-- @tfield float rotBg Rotation of the bar's background. sprite (0-360).
-- @tfield Vec2 scaleBg X,Y Scaling factor for the bar's background sprite.
-- @tfield View.AlignMode alignModeBg Alignment for the bar's background.
-- @tfield View.ScaleMode scaleModeBg Scaling for the bar's background.
-- @tfield Effects.BlendID blendModeBg Blending modes for the bar's background.
-- @tfield Objects.ObjID objectIdBar Object ID for the bar sprite.
-- @tfield int spriteIdBar Sprite ID from the specified object for the bar.
-- @tfield Color colorBar Color of the bar.
-- @tfield Vec2 posBar X,Y position of the bar in screen percent (0-100).
-- @tfield float rot Rotation of the bar's sprite (0-360).
-- @tfield Vec2 scaleBar X,Y Scaling factor for the bar's sprite.
-- @tfield View.AlignMode alignMode Alignment for the bar.
-- @tfield View.ScaleMode scaleMode Scaling for the bar.
-- @tfield Effects.BlendID blendMode Blending modes for the bar.
-- @tfield int alphaBlendSpeed Speed of alpha blending for bar visibility (0-255).
-- @tfield bool showBar Option to always show the bar. If set to false, the bars will automatically hide when they stop updating.
-- @tfield bool blink Whether the bar blinks.
-- @tfield float blinkLimit Limit below which bar starts blinking (0-1).

		local playerBar = {
			barName			= barName,
			startValue		= startValue,
			maxValue		= maxValue,
			objectIdBg		= playerBarData.objectIdBg,
			spriteIdBg		= playerBarData.spriteIdBg,
			colorBg			= playerBarData.colorBg,
			posBg			= playerBarData.posBg,
			rotBg			= playerBarData.rotBg,
			scaleBg			= playerBarData.scaleBg,
			alignModeBg		= playerBarData.alignModeBg,
			scaleModeBg		= playerBarData.scaleModeBg,
			blendModeBg		= playerBarData.blendModeBg,
			objectIdBar		= playerBarData.objectIdBar,
			spriteIdBar		= playerBarData.spriteIdBar,
			colorBar		= playerBarData.colorBar,
			posBar			= playerBarData.posBar,
			rot				= playerBarData.rot,
			scaleBar		= playerBarData.scaleBar,
			alignMode		= playerBarData.alignMode,
			scaleMode		= playerBarData.scaleMode,
			blendMode		= playerBarData.blendMode,
			text			= "BLANK",
			textPos			= TEN.Vec2(0,0),
			textOptions		= {},
			textScale		= 0,
			textColor		= TEN.Color(0,0,0),
			hideText		= true,
			alphaBlendSpeed	= playerBarData.alphaBlendSpeed,
			blink			= playerBarData.blink,
			blinkLimit		= playerBarData.blinkLimit,
		}

		CustomBar.Create(playerBar)

	end

	LevelVars.Engine.CustomBars.bars[dataName].getActionType	= playerBarData.getActionType
	LevelVars.Engine.CustomBars.bars[dataName].showBar			= playerBarData.showBar
	LevelVars.Engine.CustomBars.bars[dataName].visible			= true
	LevelVars.Engine.CustomBars.bars[dataName].targetAlpha		= 255

end

---
-- Creates a custom health bar for a specific enemy (like a boss). Ensure this function is called before Lara aims at the enemy if using generic enemy HP bars as well.
-- Also be sure to call this function after increasing the HP of the enemy via LUA.
-- @tparam table enemyBarData The table that contains all the enemy bar data. Refer to table setup for enemyBarData.
-- @treturn CustomBar Enemy health bar.
CustomBar.CreateEnemyHpBar = function (enemyBarData)

	local dataName	= enemyBarData.barName .. "_bar_data"
	local enemyHP = TEN.Objects.GetMoveableByName(enemyBarData.object):GetHP()

---
-- Table setup for creating a specific enemy health bar.
-- @table enemyBarData
-- @tfield string barName Unique identifier for the bar.
-- @tfield Objects.ObjID objectIdBg Object ID for the bar's background sprite.
-- @tfield int spriteIdBg Sprite ID from the specified object for the bar's background.
-- @tfield Color colorBg Color of bar's background.
-- @tfield Vec2 posBg X,Y position of the bar's background in screen percent (0-100).
-- @tfield float rotBg Rotation of the bar's background sprite (0-360).
-- @tfield Vec2 scaleBg X,Y Scaling factor for the bar's background sprite.
-- @tfield View.AlignMode alignModeBg Alignment for the bar's background.
-- @tfield View.ScaleMode scaleModeBg Scaling for the bar's background.
-- @tfield Effects.BlendID blendModeBg Blending modes for the bar's background.
-- @tfield Objects.ObjID objectIdBar Object ID for the bar sprite.
-- @tfield int spriteIdBar Sprite ID from the specified object for the bar.
-- @tfield Color colorBar Color of the bar.
-- @tfield Vec2 posBar X,Y position of the bar in screen percent (0-100).
-- @tfield float rot Rotation of the bar's sprite (0-360).
-- @tfield Vec2 scaleBar X,Y Scaling factor for the bar's sprite.
-- @tfield View.AlignMode alignMode Alignment for the bar.
-- @tfield View.ScaleMode scaleMode Scaling for the bar.
-- @tfield Effects.BlendID blendMode Blending modes for the bar.
-- @tfield string text Text to display for the enemy.
-- @tfield Vec2 textPos X,Y position of the text.
-- @tfield Strings.DisplayStringOption textOptions Alignment and effects for the text. Note text is automatically aligned to the left.
-- @tfield float textScale Scale factor for the text.
-- @tfield Color textColor Color of the text.
-- @tfield bool hideText Whether to hide the text.
-- @tfield int alphaBlendSpeed Speed of alpha blending for bar visibility (0-255).
-- @tfield string object Enemy name set in Editor for which to create HP for.
-- @tfield bool showBar Option to always show the bar whether the enemy is current target or not. Useful for boss health bars.
-- @tfield bool blink Whether the bar blinks.
-- @tfield float blinkLimit Limit below which bar starts blinking (0-1).
	
	local enemyBar = {
		barName			= enemyBarData.barName,
		startValue		= enemyHP,
		maxValue		= enemyHP,
		objectIdBg		= enemyBarData.objectIdBg,
		spriteIdBg		= enemyBarData.spriteIdBg,
		colorBg			= enemyBarData.colorBg,
		posBg			= enemyBarData.posBg,
		rotBg			= enemyBarData.rotBg,
		scaleBg			= enemyBarData.scaleBg,
		alignModeBg		= enemyBarData.alignModeBg,
		scaleModeBg		= enemyBarData.scaleModeBg,
		blendModeBg		= enemyBarData.blendModeBg,
		objectIdBar		= enemyBarData.objectIdBar,
		spriteIdBar		= enemyBarData.spriteIdBar,
		colorBar		= enemyBarData.colorBar,
		posBar			= enemyBarData.posBar,
		rot				= enemyBarData.rot,
		scaleBar		= enemyBarData.scaleBar,
		alignMode		= enemyBarData.alignMode,
		scaleMode		= enemyBarData.scaleMode,
		blendMode		= enemyBarData.blendMode,
		text			= enemyBarData.text,
		textPos			= enemyBarData.textPos,
		textOptions		= enemyBarData.textOptions,
		textScale		= enemyBarData.textScale,
		textColor		= enemyBarData.textColor,
		hideText		= enemyBarData.hideText,
		alphaBlendSpeed	= enemyBarData.alphaBlendSpeed,
		blink			= enemyBarData.blink,
		blinkLimit		= enemyBarData.blinkLimit
	}

	CustomBar.Create(enemyBar)

	LevelVars.Engine.CustomBars.bars[dataName].showBar			= enemyBarData.showBar
	LevelVars.Engine.CustomBars.bars[dataName].object			= enemyBarData.object
	LevelVars.Engine.CustomBars.bars[dataName].getActionType	= 0
	LevelVars.Engine.CustomBars.bars[dataName].visible			= true
	LevelVars.Engine.CustomBars.bars[dataName].fixedInterval	= 1/3
	LevelVars.Engine.CustomBars.bars[dataName].currentAlpha		= 0
	LevelVars.Engine.CustomBars.bars[dataName].targetAlpha		= 255

end

---
-- Creates health bars for all enemies. A new bar is generated whenever Lara targets an enemy. If the "hide text" option is disabled, the enemy's name (as set in the editor) is displayed. 
-- Multiple enemies can share the same name by appending _number to the name in the editor. If adjusting an enemy's max HP, ensure this is done before Lara targets the enemy.
-- To create health bars for specific enemies, use CustomBar.CreateEnemyHpBar, ensuring the bar is created prior to targeting.
-- @tparam table enemiesBarData The table that contains all the enemies bar data. Refer to table setup for enemiesBarData.
-- @treturn CustomBar Enemy health bars.

CustomBar.SetEnemiesHpGenericBar = function (enemiesBarData)

	if LevelVars.Engine.CustomBars.enemiesHpBar.objectIdBg then
        print("Warning: Overwriting enemy HP bar definitions")
	end

---
-- Table setup for creating health bars for all enemies.
-- @table enemiesBarData
-- @tfield Objects.ObjID objectIdBg Object ID for the bar's background sprite.
-- @tfield int spriteIdBg Sprite ID from the specified object for the bar's background.
-- @tfield Color colorBg Color of bar's background.
-- @tfield Vec2 posBg X,Y position of the bar's background in screen percent (0-100).
-- @tfield float rotBg Rotation of the bar's background sprite (0-360).
-- @tfield Vec2 scaleBg X,Y Scaling factor for the bar's background sprite.
-- @tfield View.AlignMode alignModeBg Alignment for the bar's background.
-- @tfield View.ScaleMode scaleModeBg Scaling for the bar's background.
-- @tfield Effects.BlendID blendModeBg Blending modes for the bar's background.
-- @tfield Objects.ObjID objectIdBar Object ID for the bar sprite.
-- @tfield int spriteIdBar Sprite ID from the specified object for the bar.
-- @tfield Color colorBar Color of the bar.
-- @tfield Vec2 posBar X,Y position of the bar in screen percent (0-100).
-- @tfield float rot Rotation of the bar's sprite (0-360).
-- @tfield Vec2 scaleBar X,Y Scaling factor for the bar's sprite.
-- @tfield View.AlignMode alignMode Alignment for the bar.
-- @tfield View.ScaleMode scaleMode Scaling for the bar.
-- @tfield Effects.BlendID blendMode Blending modes for the bar.
-- @tfield int textPos X position of the text.
-- @tfield Strings.DisplayStringOption textOptions Alignment and effects for the text. Note that text is automatically aligned to the left.
-- @tfield float textScale Scale factor for the text.
-- @tfield Color textColor Color of the text.
-- @tfield bool hideText Whether to hide the enemy name text.
-- @tfield int alphaBlendSpeed Speed of alpha blending for bar visibility (0-255).
-- @tfield bool blink Whether the bar blinks.
-- @tfield float blinkLimit Limit below which bar starts blinking (0-1).

	LevelVars.Engine.CustomBars.enemiesHpBar.objectIdBg			= enemiesBarData.objectIdBg
	LevelVars.Engine.CustomBars.enemiesHpBar.spriteIdBg			= enemiesBarData.spriteIdBg
	LevelVars.Engine.CustomBars.enemiesHpBar.colorBg			= enemiesBarData.colorBg
	LevelVars.Engine.CustomBars.enemiesHpBar.posBg				= enemiesBarData.posBg
	LevelVars.Engine.CustomBars.enemiesHpBar.scaleBg			= enemiesBarData.scaleBg
	LevelVars.Engine.CustomBars.enemiesHpBar.rotBg				= enemiesBarData.rotBg
	LevelVars.Engine.CustomBars.enemiesHpBar.alignModeBg	    = enemiesBarData.alignModeBg
	LevelVars.Engine.CustomBars.enemiesHpBar.scaleModeBg	    = enemiesBarData.scaleModeBg
	LevelVars.Engine.CustomBars.enemiesHpBar.blendModeBg	    = enemiesBarData.blendModeBg
	LevelVars.Engine.CustomBars.enemiesHpBar.objectIdBar		= enemiesBarData.objectIdBar
	LevelVars.Engine.CustomBars.enemiesHpBar.spriteIdBar		= enemiesBarData.spriteIdBar
	LevelVars.Engine.CustomBars.enemiesHpBar.colorBar			= enemiesBarData.colorBar
	LevelVars.Engine.CustomBars.enemiesHpBar.posBar				= enemiesBarData.posBar
	LevelVars.Engine.CustomBars.enemiesHpBar.scaleBar			= enemiesBarData.scaleBar
	LevelVars.Engine.CustomBars.enemiesHpBar.rot				= enemiesBarData.rot
	LevelVars.Engine.CustomBars.enemiesHpBar.alignMode			= enemiesBarData.alignMode
	LevelVars.Engine.CustomBars.enemiesHpBar.scaleMode			= enemiesBarData.scaleMode
	LevelVars.Engine.CustomBars.enemiesHpBar.blendMode			= enemiesBarData.blendMode
	LevelVars.Engine.CustomBars.enemiesHpBar.textPos			= enemiesBarData.textPos
	LevelVars.Engine.CustomBars.enemiesHpBar.textOptions		= enemiesBarData.textOptions
	LevelVars.Engine.CustomBars.enemiesHpBar.textScale			= enemiesBarData.textScale
	LevelVars.Engine.CustomBars.enemiesHpBar.textColor			= enemiesBarData.textColor
	LevelVars.Engine.CustomBars.enemiesHpBar.hideText			= enemiesBarData.hideText
	LevelVars.Engine.CustomBars.enemiesHpBar.alphaBlendSpeed	= enemiesBarData.alphaBlendSpeed
	LevelVars.Engine.CustomBars.enemiesHpBar.blink				= enemiesBarData.blink
	LevelVars.Engine.CustomBars.enemiesHpBar.blinkLimit			= enemiesBarData.blinkLimit


	LevelVars.Engine.CustomBars.enemiesHpBar.status = true

end

--- The function retrieves an existing bar instance by its unique identifier (barName). This function is useful when you need to access or manipulate a bar that has already been created.
-- @string barName The unique identifier assigned to the bar when it was created using CustomBar.New
CustomBar.Get = function(barName)
	local dataName = barName .. "_bar_data"
    if LevelVars.Engine.CustomBars.bars[dataName] then
        local self = {name = dataName}
        return setmetatable(self, CustomBar)
    end
end

--- The function removes a custom bar and its associated data from the system. It ensures that the bar is no longer tracked or accessible in the LevelVars.Engine.CustomBars.bars table.
-- @string barName The name of the custom bar to be deleted.
CustomBar.Delete = function (barName)
    local dataName = barName .. "_bar_data"
	if LevelVars.Engine.CustomBars.bars[dataName] then
		LevelVars.Engine.CustomBars.bars[dataName] = nil
	end
end

--- The function sets the value of a custom bar over a specified time period.
-- @number value The new target to which the bar's current value should transition. (Must be a non-negative number; between 0 and the bar's maxValue.
-- @number time The time (in seconds) over which the bar's value should transition to the target value.
function CustomBar:SetBarValue(value, time)
	if LevelVars.Engine.CustomBars.bars[self.name] then
		if type(value) =="number" and value >= 0 then
			local currentValue = LevelVars.Engine.CustomBars.bars[self.name].oldValue
			local maxValue = LevelVars.Engine.CustomBars.bars[self.name].maxValue
			local newTargetValue = math.max(0, math.min(maxValue, value))
			LevelVars.Engine.CustomBars.bars[self.name].targetValue = newTargetValue
			LevelVars.Engine.CustomBars.bars[self.name].fixedInterval = (newTargetValue - currentValue) / (time * 30)
		end
	end
end

--- The function adjusts the bar's value relative to its current or target value over a specified time span.
-- @number value The relative value to add (positive or negative) to the current bar value.
-- @number time The duration (in seconds) over which the change should occur.
function CustomBar:ChangeBarValueOverTimespan(value, time)
    -- Check if bar data and timer exist
	if LevelVars.Engine.CustomBars.bars[self.name] then

		-- Get the current target value or old value if no target value exists
		local currentValue = LevelVars.Engine.CustomBars.bars[self.name].oldValue
		local maxValue = LevelVars.Engine.CustomBars.bars[self.name].maxValue
		local currentTarget = LevelVars.Engine.CustomBars.bars[self.name].targetValue or currentValue

		-- Calculate new target value by adding the relative 'value' and clamp between 0 and 1000
		local newTargetValue = math.max(0, math.min(maxValue, currentTarget + value))

		-- Set the new target value
		LevelVars.Engine.CustomBars.bars[self.name].targetValue = newTargetValue

		-- Calculate total frames based on time and FPS (30 FPS)
		local totalFrames = time * 30

		-- Calculate the fixed interval for the entire transition
		LevelVars.Engine.CustomBars.bars[self.name].fixedInterval = (newTargetValue - currentValue) / totalFrames
	end
end

--- The function controls the visibility of a custom bar.
-- @bool visible `true` makes the bar visible, `false` hides the bar.
function CustomBar:SetVisibility(visible)
    --the visible variable is a boolean
	if LevelVars.Engine.CustomBars.bars[self.name] then
		if visible and type(visible) == "boolean" then
			LevelVars.Engine.CustomBars.bars[self.name].targetAlpha = 255
			LevelVars.Engine.CustomBars.bars[self.name].visible = true
		else
			LevelVars.Engine.CustomBars.bars[self.name].targetAlpha = 0
		end
	end
end

--- The function checks whether a custom bar is currently visible.
-- @treturn bool `true` if the bar is visible and `false` if it is not.
function CustomBar:IsVisible()
    
	if LevelVars.Engine.CustomBars.bars[self.name] then
		if LevelVars.Engine.CustomBars.bars[self.name].visible then
			return true
		else
			return false
		end
	end
end

--- The function retrieves the current value of a custom bar.
-- @treturn float Returns the current value of a custom bar.
function CustomBar:GetValue()
	
    if LevelVars.Engine.CustomBars.bars[self.name] then
        return LevelVars.Engine.CustomBars.bars[self.name].oldValue
	end
end

--- The function deletes all custom bars.
CustomBar.DeleteAllBars = function ()
	for _, customBar in pairs (LevelVars.Engine.CustomBars.bars) do
		LevelVars.Engine.CustomBars.bars[customBar.name] = nil
	end
end

--- This function prevents the creation of new health bars for enemies when set to false. However, it does not affect the health bars that have already been created.
-- @bool value Specifies whether new health bars for enemies should be created. 
CustomBar.ShowEnemiesHpGenericBar = function(value)
	if type(value) == "boolean" then
		LevelVars.Engine.CustomBars.enemiesHpBar.status = value
	end
end

--- The function deletes all the enemy health bars excluding those created by CustomBar.CreateEnemyHpBar.
CustomBar.DeleteExistingHpGenericBars = function ()
	for _, customBar in pairs (LevelVars.Engine.CustomBars.bars) do
		if customBar.getActionType == 4 then
			LevelVars.Engine.CustomBars.bars[customBar.name] = nil
		end
	end
end

--- Sets the custom bar background sprite position.
-- @tparam Vec2 pos X,Y position of the bar's background in screen percent (0-100).
--
function CustomBar:SetBackgroundPosition(pos)
	if pos and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].posBg = pos
	end
end

--- Sets the custom bar background sprite rotation.
-- @tparam float rot Rotation of the bar's background sprite (0-360).
--
function CustomBar:SetBackgroundRotation(rot)
	if rot and  LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].rotBg = rot
	end
end

-- Sets the custom bar background sprite color.
-- @tparam Color color Color of bar's background.
--
function CustomBar:SetBackgroundColor(color)
	if color and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].colorBg = color
	end
end

---
-- Sets the custom bar background sprite scale.
-- @tparam Vec2 scale X,Y Scaling factor for the bar's background sprite.
--
function CustomBar:SetBackgroundScale(scale)
	if scale and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].scaleBg = scale
	end
end

---
-- Sets the custom bar background sprite slot and sprite ID.
-- @tparam Objects.ObjID slot Object ID for the bar's background sprite.
-- @tparam int id Sprite ID from the specified object for the bar's background.
--
function CustomBar:SetBackgroundSpriteSlot(slot, id)
	if slot and id and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].objectIdBg = slot
		LevelVars.Engine.CustomBars.bars[self.name].spriteIdBg = id
	end
end

---
-- Sets the custom bar background sprite align mode.
-- @tparam View.AlignMode alignMode Alignment for the bar's background.
--
function CustomBar:SetBackgroundAlignMode(alignMode)
	if alignMode and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].alignModeBg = alignMode
	end
end
---
-- Sets the custom bar background sprite scale mode.
-- @tparam View.ScaleMode scaleMode Scaling for the bar's background.
--
function CustomBar:SetBackgroundScaleMode(scaleMode)
	if scaleMode and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].scaleModeBg = scaleMode
	end
end
---
-- Sets the custom bar background sprite blend mode.
-- @tparam Effects.BlendID blendMode Blending modes for the bar's background.
--
function CustomBar:SetBackgroundBlendMode(blendMode)
	if blendMode and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].blendModeBg = blendMode
	end
end

---
-- Sets the custom bar sprite position.
-- @tparam Vec2 pos X,Y position of the bar in screen percent (0-100).
--
function CustomBar:SetBarPosition(pos)
	if pos and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].posBar = pos
	end
end
---
-- Sets the custom bar sprite rotation.
-- @tparam float rot Rotation of the bar's sprite (0-360).
--
function CustomBar:SetBarRotation(rot)
	if rot and  LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].rot = rot
	end
end
---
-- Sets the custom bar sprite color.
-- @tparam Color color Color of the bar.
--
function CustomBar:SetBarColor(color)
	if color and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].colorBar = color
	end
end
---
-- Sets the custom bar sprite scale.
-- @tparam Vec2 scale X,Y Scaling factor for the bar's sprite.
--
function CustomBar:SetBarScale(scale)
	if scale and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].scaleBar = scale
	end
end
---
-- Sets the custom bar sprite slot and sprite ID.
-- @tparam Objects.ObjID slot Object ID for the bar sprite.
-- @tparam int id Sprite ID from the specified object for the bar.
--
function CustomBar:SetBarSpriteSlot(slot, id)
	if slot and id and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].objectIdBar = slot
		LevelVars.Engine.CustomBars.bars[self.name].spriteIdBar = id
	end
end
---
-- Sets the custom bar sprite alignment mode.
-- @tparam View.AlignMode alignMode Alignment for the bar.
--
function CustomBar:SetBarAlignMode(alignMode)
	if alignMode and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].alignMode = alignMode
	end
end
---
-- Sets the custom bar sprite scale mode.
-- @tparam View.ScaleMode scaleMode Scaling for the bar.
--
function CustomBar:SetBarScaleMode(scaleMode)
	if scaleMode and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].scaleMode = scaleMode
	end
end
---
-- Sets the custom bar sprite blend mode.
-- @tparam Effects.BlendID blendMode Blending modes for the bar.
--
function CustomBar:SetBarBlendMode(blendMode)
	if blendMode and LevelVars.Engine.CustomBars.bars[self.name] then
		LevelVars.Engine.CustomBars.bars[self.name].blendMode = blendMode
	end
end

LevelFuncs.Engine.CustomBar.UpdateCustomBars = function()
	
	local playerTarget = Lara:GetTarget()
	
	if playerTarget ~= nil and LevelVars.Engine.CustomBars.enemiesHpBar.status then
		local playerTargetName = playerTarget:GetName()
		local displayName = LevelFuncs.Engine.Node.SplitString(playerTargetName, "_")
		local enemytable = playerTargetName .. "_bar_data"
		if LevelVars.Engine.CustomBars.bars[enemytable] == nil then
			local eB = LevelVars.Engine.CustomBars.enemiesHpBar

			local enemyBar = {
				barName			= playerTargetName,
				objectIdBg		= eB.objectIdBg,
				spriteIdBg		= eB.spriteIdBg,
				colorBg			= eB.colorBg,
				posBg			= eB.posBg,
				rotBg			= eB.rotBg,
				scaleBg			= eB.scaleBg,
				alignModeBg		= eB.alignModeBg,
				scaleModeBg		= eB.scaleModeBg,
				blendModeBg		= eB.blendModeBg,
				objectIdBar		= eB.objectIdBar,
				spriteIdBar		= eB.spriteIdBar,
				colorBar		= eB.colorBar,
				posBar			= eB.posBar,
				rot				= eB.rot,
				scaleBar		= eB.scaleBar,
				alignMode		= eB.alignMode,
				scaleMode		= eB.scaleMode,
				blendMode		= eB.blendMode,
				text			= displayName[1],
				textPos			= eB.textPos,
				textOptions		= eB.textOptions,
				textScale		= eB.textScale,
				textColor		= eB.textColor,
				hideText		= eB.hideText,
				alphaBlendSpeed	= eB.alphaBlendSpeed,
				blink			= eB.blink,
				blinkLimit		= eB.blinkLimit,
				showBar 		= false,
				object			= playerTargetName
			}
			
			CustomBar.CreateEnemyHpBar(enemyBar)
			LevelVars.Engine.CustomBars.bars[enemytable].getActionType = 4

		end
	end
	
	for _, customBar in pairs (LevelVars.Engine.CustomBars.bars) do

		if customBar ~= nil then
			-- Smoothly transition to target value
			local currentValue = customBar.oldValue or 0
			local targetValue = customBar.targetValue or 0
			local delta = customBar.fixedInterval

			if customBar.object ~=nil and (customBar.getActionType == 0 or customBar.getActionType == 4) then
				local enemy = GetMoveableByName(customBar.object)
				currentValue = enemy:GetHP()

				targetValue = currentValue

				customBar.progress = math.max(0, math.min(currentValue / customBar.maxValue, 1))

				if customBar.showBar == true then
					-- If showBar is true, the bar is always visible at full alpha
					customBar.targetAlpha = 255
					customBar.visible = true
				else
					-- If showBar is false, only show the bar if the enemy is the player's current target
					if playerTarget == enemy then
						customBar.targetAlpha = 255  -- Set to full alpha if this enemy is the target
						customBar.visible = true
					else
						customBar.targetAlpha = 0  -- Set to 0 alpha if this enemy is not the target
					end
				end

				if currentValue <= 0 then
					customBar.targetAlpha = 0
				end
				
				-- When Alpha reaches 0 set visibility to false
				if currentValue <= 0 and customBar.currentAlpha == 0 then
					customBar.visible = false
					LevelVars.Engine.CustomBars.bars[customBar.name] = nil
				end
			end

			if customBar.getActionType == 1 then
								
				currentValue = Lara:GetHP()
				targetValue = currentValue
				
				customBar.progress = math.max(0, math.min(currentValue / customBar.maxValue, 1))

				-- Check if `hideBar` is true, which overrides all other behaviors
				if customBar.showBar == true then
					customBar.targetAlpha = 255  -- Bar is always visible
					customBar.visible = true

				elseif currentValue ~= (customBar.oldValue or currentValue) then
					customBar.targetAlpha = 255  -- Show the bar if value changes
					customBar.visible = true

					customBar.currentTimer = customBar.currentTimer + 1

						if customBar.currentTimer >= 90 then
							customBar.oldValue = currentValue
							customBar.currentTimer = 0
						end
				
				elseif Lara:GetHandStatus() == 0 and currentValue >= customBar.blinkLimit*1000 then
						-- Hide bar if hands are free and HP is 200 or more
						customBar.targetAlpha = 0
						customBar.visible = false

				elseif Lara:GetHandStatus() == 2 or Lara:GetHandStatus() == 3 or Lara:GetHandStatus() == 4 then
						-- Show bar if hand status is 2, 3, or 4 (weapon drawn)
						customBar.targetAlpha = 255
						customBar.visible = true

				elseif Lara:GetHandStatus() == 0 and currentValue < customBar.blinkLimit*1000 then
						-- Show bar if hands are free and HP is less than 200
						customBar.targetAlpha = 255
						customBar.visible = true
				end

			elseif customBar.getActionType == 2 then
				currentValue = Lara:GetAir()
				targetValue = currentValue

				customBar.progress = math.max(0, math.min(currentValue / customBar.maxValue, 1))

				if customBar.showBar == true then
					-- If showBar is true, the bar is always visible
					customBar.targetAlpha = 255
					customBar.visible = true
				else
					-- If showBar is false, hide the bar when currentValue is at max
					if currentValue == customBar.maxValue then
						customBar.targetAlpha = 0  -- Hide the bar when at max value
					else
						customBar.targetAlpha = 255  -- Show the bar if currentValue is not max
						customBar.visible = true
					end
				end
				
			elseif customBar.getActionType == 3 then 
				currentValue = Lara:GetStamina()
				targetValue = currentValue

				customBar.progress = math.max(0, math.min(currentValue / customBar.maxValue, 1))
				
				if customBar.showBar == true then
					-- If showBar is true, the bar is always visible
					customBar.targetAlpha = 255
					customBar.visible = true
				else
					-- If showBar is false, hide the bar when currentValue is at max
					if currentValue == customBar.maxValue then
						customBar.targetAlpha = 0  -- Hide the bar when at max value
					else
						customBar.targetAlpha = 255  -- Show the bar if currentValue is not max
						customBar.visible = true
					end
				end

			end

			if currentValue ~= targetValue then
				-- Update current value by delta (increment or decrement)
				if currentValue < targetValue then
					currentValue = math.min(currentValue + delta, targetValue)
				else
					currentValue = math.max(currentValue + delta, targetValue)
				end

				-- Update the bar's progress (0-1 scale)
				customBar.oldValue = currentValue
				customBar.progress = currentValue / customBar.maxValue
			end
			-- Smoothly transition alpha
			if customBar.currentAlpha ~= customBar.targetAlpha then
				local alphaDelta = customBar.alphaBlendSpeed
				if customBar.currentAlpha < customBar.targetAlpha then
					customBar.currentAlpha = math.floor(math.min(customBar.currentAlpha + alphaDelta, customBar.targetAlpha))
				else
					customBar.currentAlpha = math.floor(math.max(customBar.currentAlpha - alphaDelta, customBar.targetAlpha))
				end
			end
			
			-- Set parameters to draw the background
			local posBg = customBar.posBg
			local scaleBg = customBar.scaleBg
			local rotBg = customBar.rotBg
			local alignMBg = LevelFuncs.Engine.Node.GetDisplaySpriteAlignMode(customBar.alignModeBg)
			local scaleMBg = LevelFuncs.Engine.Node.GetDisplaySpriteScaleMode(customBar.scaleModeBg)
			local blendIdBg = LevelFuncs.Engine.Node.GetBlendMode(customBar.blendModeBg)

			-- Adjust color with alpha blending
			local bgColor = Color(customBar.colorBg.r,customBar.colorBg.g,customBar.colorBg.b,customBar.currentAlpha)
			
			-- Set parameters to draw the bar
			local pos = customBar.posBar
			local rot = customBar.rot
			local alignM = LevelFuncs.Engine.Node.GetDisplaySpriteAlignMode(customBar.alignMode)
			local scaleM = LevelFuncs.Engine.Node.GetDisplaySpriteScaleMode(customBar.scaleMode)
			local blendID = LevelFuncs.Engine.Node.GetBlendMode(customBar.blendMode)
			local barColor = TEN.Color(customBar.colorBar.r,customBar.colorBar.g,customBar.colorBar.b,customBar.currentAlpha)
			
			-- when Alpha reaches 0 set visibility to false
			if customBar.currentAlpha > 0 then
				customBar.visible = true
			elseif customBar.currentAlpha == 0 then
				customBar.visible = false
			end
			
			--draw bar if alpha is greater than 1 and visibility is true
			if customBar.visible and customBar.currentAlpha > 0 then
				-- Draw background sprite
				local bgSprite = TEN.DisplaySprite(customBar.objectIdBg, customBar.spriteIdBg, posBg, rotBg, scaleBg, bgColor)
				bgSprite:Draw(0, alignMBg, scaleMBg, blendIdBg)

				-- Draw foreground sprite (the bar itself) proportional to Progress
				local barScale = TEN.Vec2(customBar.scaleBar.x * customBar.progress, customBar.scaleBar.y)
				local barSprite = TEN.DisplaySprite(customBar.objectIdBar, customBar.spriteIdBar, pos, rot, barScale, barColor)
				
				if customBar.frameCounter == nil then
					customBar.frameCounter = 0
				end
				
				-- Calculate HP percentage
				local Percentage = (currentValue / customBar.maxValue)
				
				-- Update frame counter
				customBar.frameCounter = customBar.frameCounter + 1
				
				-- Check if blink is enabled and value is below blinkLimit
				if customBar.blink == true and Percentage <= customBar.blinkLimit then
					-- Only draw the sprite every other frame
					if customBar.frameCounter % (customBar.blinkSpeed * 2) < customBar.blinkSpeed then
						barSprite:Draw(1, alignM, scaleM, blendID)
					end
				else
					-- Draw the sprite normally if blink is off or value is above blinkLimit
					barSprite:Draw(1, alignM, scaleM, blendID)
				end
				
				-- Reset the frame counter if it reaches the blinkSpeed limit to prevent overflow
				if customBar.frameCounter >= customBar.blinkSpeed * 2 then
					customBar.frameCounter = 0
				end
			
				if customBar.hideText == false then
					-- Draw text (enemy name and health)
					local barText = tostring(customBar.text) --debug text	 .. " (" .. currentHP .. " / " .. totalHP .. ")"
					local textColor = TEN.Color(customBar.textColor.r, customBar.textColor.g, customBar.textColor.b, customBar.currentAlpha)
					local posInPixel = TEN.Vec2(TEN.Util.PercentToScreen(customBar.textPos.x, customBar.textPos.y))
					local IsString = TEN.Flow.IsStringPresent(barText)
					local myText = TEN.Strings.DisplayString(barText, posInPixel, customBar.textScale, textColor, IsString, customBar.textOptions)
					TEN.Strings.ShowString(myText, 1/30)
				end
			end
		end
	end
end

TEN.Logic.AddCallback(TEN.Logic.CallbackPoint.PRELOOP, LevelFuncs.Engine.CustomBar.UpdateCustomBars)

return CustomBar
