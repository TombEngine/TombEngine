--- Internal file used by the RingInventory module.
-- @module RingInventory.Statistics
-- @local

-- ============================================================================
-- Statistics - Handles statistics function and data for ring inventory
-- ============================================================================

--External Modules
local Settings = require("Engine.RingInventory.Settings")
local Text = require("Engine.RingInventory.Text")

--Pointers to tables
local COLOR_MAP = Settings.ColorMap

--Class Start
local Stats = {}

local statisticsType = false
local endStatistics = false

local LEVEL_HEADER_POS = TEN.Vec2(50, 36)
local HEADER_TEXT_POS = TEN.Vec2(22.4, 43)
local STATS_TEXT_POS = TEN.Vec2(65, 43)
local STATS_TEXT_SCALE = 1

local LEVEL_HEADER_TEXT = 
{
	name = "LEVEL_HEADER_TEXT",                 
	text = "",               
	position = LEVEL_HEADER_POS,                   
	scale = STATS_TEXT_SCALE,                             
	color = COLOR_MAP.headerText,        
	visible = false,                           
	flags = 
	{
		TEN.Strings.DisplayStringOption.CENTER,
		TEN.Strings.DisplayStringOption.SHADOW
	},
	translate = false
}

local HEADER_TEXT = 
{
	name = "HEADER_TEXT",                 
	text = "",               
	position = HEADER_TEXT_POS,                   
	scale = STATS_TEXT_SCALE,                             
	color = COLOR_MAP.plainText,        
	visible = false,                           
	flags = 
	{
		TEN.Strings.DisplayStringOption.SHADOW
	},
	translate = false
}

local STATS_TEXT = 
{
	name = "STATS_TEXT",                 
	text = "",               
	position = STATS_TEXT_POS,                   
	scale = STATS_TEXT_SCALE,                             
	color = COLOR_MAP.plainText,        
	visible = false,                           
	flags = 
	{
		TEN.Strings.DisplayStringOption.SHADOW
	},
	translate = false
}

local GetLevelHeader = function(type)
    if type then
        return TEN.Flow.GetString("game_title")
    else
        local level = TEN.Flow.GetCurrentLevel()
        return TEN.Flow.GetString(level.nameKey)
    end
end

local GetStatistics = function(type)
    local secretCount = type and TEN.Flow.GetTotalSecretCount() or TEN.Flow.GetCurrentLevel().secrets
    local levelStats = TEN.Flow.GetStatistics(type)
    local statistics = tostring(levelStats.timeTaken):sub(1, -4) .. "\n" ..
        tostring(levelStats.secrets) .. " / " .. tostring(secretCount) .. "\n" ..
        tostring(levelStats.pickups) .. "\n" ..
        tostring(levelStats.kills) .. "\n" ..
        tostring(levelStats.ammoUsed) .. "\n" ..
        tostring(levelStats.healthPacksUsed) .. "\n" ..
        string.format("%.1f", levelStats.distanceTraveled / 420) .. " m"

    return statistics
end

local GetLabels = function(type)
    local secretsText = type and TEN.Flow.GetString("total_secrets_found") or TEN.Flow.GetString("level_secrets_found")

    local headings = 
        TEN.Flow.GetString("time_taken").."\n"..
        secretsText.."\n"..
        TEN.Flow.GetString("pickups").."\n"..
        TEN.Flow.GetString("kills").."\n"..
        TEN.Flow.GetString("ammo_used").."\n"..
        TEN.Flow.GetString("used_medipacks").."\n"..
        TEN.Flow.GetString("distance_travelled")

    return headings
end

function Stats.SetupStats()
    Text.Create(LEVEL_HEADER_TEXT)
    Text.Create(HEADER_TEXT) 
    Text.Create(STATS_TEXT)
    Text.AddToGroup("STATISTICS", "LEVEL_HEADER_TEXT")
    Text.AddToGroup("STATISTICS", "HEADER_TEXT")
    Text.AddToGroup("STATISTICS", "STATS_TEXT")

    local header = GetLevelHeader(statisticsType)
    local headings = GetLabels(statisticsType)
    local statistics = GetStatistics(statisticsType)
    
    Text.SetText("LEVEL_HEADER_TEXT", header, false)
    Text.SetText("HEADER_TEXT", headings, false)
    Text.SetText("STATS_TEXT", statistics, false)
end

function Stats.Show()
    Text.ShowGroup("STATISTICS")
end

function Stats.Hide()
    Text.HideGroup("STATISTICS")
end

function Stats.UpdateStatistics(type, transitionType)
    local header = GetLevelHeader(type)
    local headings = GetLabels(type)
    local statistics = GetStatistics(type)
    Text.SetText("LEVEL_HEADER_TEXT", header, true, transitionType)
    Text.SetText("HEADER_TEXT", headings, true, transitionType)
    Text.SetText("STATS_TEXT", statistics, true, transitionType)
end

function Stats.ToggleType(transitionType)
    statisticsType = not statisticsType
    Stats.UpdateStatistics(statisticsType, transitionType)
end

function Stats.GetType()
    return statisticsType
end

function Stats.SetType(type)
    statisticsType = type
end

function Stats.IsEndStatisticsEnabled()
    return endStatistics == true
end

function Stats.SetEndStatistics(value)
    endStatistics = value
end


function Stats.UpdateIngameTime()
    if Settings.Statistics.progressTime then
        TEN.Flow.GetStatistics(true).timeTaken = TEN.Flow.GetStatistics(true).timeTaken + 1
        TEN.Flow.GetStatistics(false).timeTaken = TEN.Flow.GetStatistics(false).timeTaken + 1
    end
end

return Stats