#include "framework.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"

#include <filesystem>

#include "Game/Gui.h"
#include "Game/Lara/lara_fire.h"
#include "Game/pickup/pickup_ammo.h"
#include "Game/pickup/pickup_consumable.h"
#include "Game/savegame.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Scripting/Include/Strings/ScriptInterfaceStringsHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Collision/MaterialTypes.h"
#include "Scripting/Internal/TEN/Collision/Probe.h"
#include "Scripting/Internal/TEN/Flow/Enums/ErrorModes.h"
#include "Scripting/Internal/TEN/Flow/Enums/FreezeModes.h"
#include "Scripting/Internal/TEN/Flow/Enums/GameStatuses.h"
#include "Scripting/Internal/TEN/Flow/InventoryItem/InventoryItem.h"
#include "Scripting/Internal/TEN/Flow/Settings/Settings.h"
#include "Scripting/Internal/TEN/Logic/LevelFunc.h"
#include "Scripting/Internal/TEN/Objects/Lara/WeaponTypes.h"
#include "Scripting/Internal/TEN/Types/Time/Time.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Sound/sound.h"
#include "Specific/trutils.h"

using namespace TEN::Scripting;
using namespace TEN::Scripting::Collision;

/***
Functions that (mostly) don't directly impact in-game mechanics. Used for setup
in gameflow.lua, settings.lua and strings.lua; some can be used in level
scripts too.
@tentable Flow 
@pragma nostrip
*/

ScriptInterfaceGame* g_GameScript;
ScriptInterfaceObjectsHandler* g_GameScriptEntities;
ScriptInterfaceStringsHandler* g_GameStringsHandler;
ScriptInterfaceFlowHandler* g_GameFlow;

FlowHandler::FlowHandler(sol::state* lua, sol::table& parent) : _handler(lua)
{
/*** gameflow.lua.
These functions are called in gameflow.lua, a file loosely equivalent to winroomedit's SCRIPT.DAT.
They handle a game's 'metadata'; i.e., things such as level titles, loading screen paths, and default
ambient tracks.
@section Flowlua
*/
	sol::table tableFlow{ _handler.GetState()->lua_state(), sol::create };
	parent.set(ScriptReserved_Flow, tableFlow);

/***
Add a level to the Flow.
@function AddLevel
@tparam Flow.Level level a level object
*/
	tableFlow.set_function(ScriptReserved_AddLevel, &FlowHandler::AddLevel, this);

/*** Image to show when loading the game.
Must be a .jpg or .png image.
@function SetIntroImagePath
@tparam string path the path to the image, relative to the TombEngine exe
*/
	tableFlow.set_function(ScriptReserved_SetIntroImagePath, &FlowHandler::SetIntroImagePath, this);

/*** Video to show when loading the game.
Must be a common video format, such as .mp4, .mkv or .avi.
@function SetIntroVideoPath
@tparam string path the path to the video, relative to the TombEngine exe
*/
	tableFlow.set_function(ScriptReserved_SetIntroVideoPath, &FlowHandler::SetIntroVideoPath, this);

/*** Image to show in the background of the title screen.
Must be a .jpg or .png image.
__(not yet implemented)__
@function SetTitleScreenImagePath
@tparam string path the path to the image, relative to the TombEngine exe
*/
	tableFlow.set_function(ScriptReserved_SetTitleScreenImagePath, &FlowHandler::SetTitleScreenImagePath, this);

/*** Enable or disable Lara drawing in title flyby.
Must be true or false
@function EnableLaraInTitle
@tparam bool enabled true or false
*/
	tableFlow.set_function(ScriptReserved_EnableLaraInTitle, &FlowHandler::EnableLaraInTitle, this);

/*** Enable or disable level selection in title flyby.
Must be true or false
@function EnableLevelSelect
@tparam bool enabled true or false
*/
	tableFlow.set_function(ScriptReserved_EnableLevelSelect, &FlowHandler::EnableLevelSelect, this);

/*** Enable or disable Home Level entry in the main menu.
@function EnableHomeLevel()
@tparam bool enabled True or false.
*/
	tableFlow.set_function(ScriptReserved_EnableHomeLevel, &FlowHandler::EnableHomeLevel, this);

/*** Enable or disable saving and loading of savegames.
@function EnableLoadSave()
@tparam bool enabled True or false.
*/
	tableFlow.set_function(ScriptReserved_EnableLoadSave, &FlowHandler::EnableLoadSave, this);

/*** gameflow.lua or level scripts.
@section FlowluaOrScripts
*/

/*** Enable or disable the fly cheat.
@function EnableFlyCheat()
@tparam bool enabled True or false.
*/
	tableFlow.set_function(ScriptReserved_EnableFlyCheat, &FlowHandler::EnableFlyCheat, this);

/*** Enable or disable point texture filter.
Must be true or false
@function EnablePointFilter
@tparam bool enabled true or false
*/
	tableFlow.set_function(ScriptReserved_EnablePointFilter, &FlowHandler::EnablePointFilter, this);

/*** Enable or disable mass pickup.
Must be true or false
@function EnableMassPickup
@tparam bool enabled true or false
*/
	tableFlow.set_function(ScriptReserved_EnableMassPickup, &FlowHandler::EnableMassPickup, this);

/*** Returns the level by index.
Indices depend on the order in which AddLevel was called; the first added will
have an ID of 0, the second an ID of 1, and so on.
@function GetLevel
@tparam int index of the level
@treturn Flow.Level the level indicated by the id
*/
	tableFlow.set_function(ScriptReserved_GetLevel, &FlowHandler::GetLevel, this);

/*** Returns the level that the game control is running in that moment.
@function GetCurrentLevel
@treturn Flow.Level the current level
*/
	tableFlow.set_function(ScriptReserved_GetCurrentLevel, &FlowHandler::GetCurrentLevel, this);

/*** Finishes the current level, with optional level index and start position index provided.
If level index is not provided or is zero, jumps to next level. If level index is more than
level count, jumps to title. If LARA\_START\_POS objects are present in level, player will be
teleported to such object with OCB similar to provided second argument.
@function EndLevel
@int[opt] index level index (default 0)
@int[opt] startPos player start position (default 0)
*/
	tableFlow.set_function(ScriptReserved_EndLevel, &FlowHandler::EndLevel, this);

/***
Get game or level statistics. For reference about statistics class, see @{Flow.Statistics}.
@function GetStatistics
@tparam bool game if true, returns overall game statistics, otherwise returns current level statistics (default: false)
@treturn Flow.Statistics statistics structure representing game or level statistics
*/
	tableFlow.set_function(ScriptReserved_GetStatistics, &FlowHandler::GetStatistics, this);

/***
Set game or level statistics. For reference about statistics class, see @{Flow.Statistics}.
@function SetStatistics
@tparam Flow.Statistics statistics statistic object to set
@tparam bool game if true, sets overall game statistics, otherwise sets current level statistics (default: false)
*/
	tableFlow.set_function(ScriptReserved_SetStatistics, &FlowHandler::SetStatistics, this);

/***
Get current game status, such as normal game loop, exiting to title, etc.
@function GetGameStatus
@treturn Flow.GameStatus the current game status
*/
	tableFlow.set_function(ScriptReserved_GetGameStatus, &FlowHandler::GetGameStatus, this);

/***
Get current freeze mode, such as none, full, spectator or player.
@function GetFreezeMode
@treturn Flow.FreezeMode the current freeze mode
*/
	tableFlow.set_function(ScriptReserved_GetFreezeMode, &FlowHandler::GetFreezeMode, this);

/***
Set current freeze mode, such as none, full, spectator or player. 
Freeze mode specifies whether game is in normal mode or paused in a particular way to allow
custom menu creation, photo mode or time freeze.
@function SetFreezeMode
@tparam Flow.FreezeMode freezeMode new freeze mode to set.
*/
	tableFlow.set_function(ScriptReserved_SetFreezeMode, &FlowHandler::SetFreezeMode, this);

/***
Save the game to a savegame slot.
@function SaveGame
@tparam int slotID ID of the savegame slot to save to.
*/
	tableFlow.set_function(ScriptReserved_SaveGame, &FlowHandler::SaveGame, this);

/***
Load the game from a savegame slot.
@function LoadGame
@tparam int slotID ID of the savegame slot to load from.
*/
	tableFlow.set_function(ScriptReserved_LoadGame, &FlowHandler::LoadGame, this);

/***
Delete a savegame.
@function DeleteSaveGame
@tparam int slotID ID of the savegame slot to clear.
*/
	tableFlow.set_function(ScriptReserved_DeleteSaveGame, &FlowHandler::DeleteSaveGame, this);

/***
Check if a savegame exists.
@function DoesSaveGameExist
@tparam int slotID ID of the savegame slot to check.
@treturn bool true if the savegame exists, false if not.
*/
	tableFlow.set_function(ScriptReserved_DoesSaveGameExist, &FlowHandler::DoesSaveGameExist, this);

/***
Returns the player's current per-game secret count.
@function GetSecretCount
@treturn int Current game secret count.
*/
	tableFlow.set_function(ScriptReserved_GetSecretCount, &FlowHandler::GetSecretCount, this);

/*** 
Sets the player's current per-game secret count.
@function SetSecretCount
@tparam int count new secret count.
*/
	tableFlow.set_function(ScriptReserved_SetSecretCount, &FlowHandler::SetSecretCount, this);

/***
Adds one secret to current level secret count and also plays secret music track.
The index argument corresponds to the secret's unique ID, the same that would go in a secret trigger's Param.
@function AddSecret
@tparam int index an index of current level's secret (must be from 0 to 31).
*/
	tableFlow.set_function(ScriptReserved_AddSecret, &FlowHandler::AddSecret, this);

/*** Get total number of secrets in the game.
@function GetTotalSecretCount
@treturn int Total number of secrets in the game.
*/
	tableFlow.set_function(ScriptReserved_GetTotalSecretCount, &FlowHandler::GetTotalSecretCount, this);

/*** Set total number of secrets in the game.
Must be an integer value (0 means no secrets).
@function SetTotalSecretCount
@tparam int count Total number of secrets in the game.
*/
	tableFlow.set_function(ScriptReserved_SetTotalSecretCount, &FlowHandler::SetTotalSecretCount, this);
	
/*** Do FlipMap with specific group ID.
@function FlipMap
@tparam int flipmap ID of flipmap group to actuvate / deactivate.
*/
	tableFlow.set_function(ScriptReserved_FlipMap, &FlowHandler::FlipMap, this);
	
/*** Get current FlipMap status for specific group ID.
@function GetFlipMapStatus
@int[opt] index Flipmap group ID to check. If no group specified or group is -1, function returns overall flipmap status (on or off).
@treturn int Status of the flipmap group (true means on, false means off).
*/
	tableFlow.set_function(ScriptReserved_GetFlipMapStatus, &FlowHandler::GetFlipMapStatus, this);
	
/*** settings.lua.
These functions are called in settings.lua, a file which holds global settings, such as system settings, flare color or animation movesets.
@section settingslua
*/
/*** Set provided settings table to an engine.
@function SetSettings
@tparam Flow.Settings settings a settings table 
*/
	tableFlow.set_function(ScriptReserved_SetSettings, &FlowHandler::SetSettings, this);
/*** Get settings table from an engine.
@function GetSettings
@treturn Flow.Settings current settings table 
*/
	tableFlow.set_function(ScriptReserved_GetSettings, &FlowHandler::GetSettings, this);

/*** strings.lua. 
These functions used in strings.lua, which is generated by TombIDE.
You will not need to call them manually.
@section stringslua
*/

/*** Set string variable keys and their translations.
@function SetStrings
@tparam tab table array-style table with strings
*/
	tableFlow.set_function(ScriptReserved_SetStrings, &FlowHandler::SetStrings, this);

/*** Get translated string.
@function GetString
@tparam key string key for translated string 
*/
	tableFlow.set_function(ScriptReserved_GetString, &FlowHandler::GetString, this);

/*** Check if translated string is present.
@function IsStringPresent
@tparam key string key for translated string
*/
	tableFlow.set_function(ScriptReserved_IsStringPresent, &FlowHandler::IsStringPresent, this);

/*** Set language names for translations.
Specify which translations in the strings table correspond to which languages.
@function SetLanguageNames
@tparam tab table array-style table with language names
*/
	tableFlow.set_function(ScriptReserved_SetLanguageNames, &FlowHandler::SetLanguageNames, this);
	
	ScriptColor::Register(parent);
	Rotation::Register(parent);
	Statistics::Register(parent);
	Time::Register(parent);
	Vec2::Register(parent);
	Vec3::Register(parent);
	Level::Register(tableFlow);
	SkyLayer::Register(tableFlow);
	InventoryItem::Register(tableFlow);
	Settings::Register(tableFlow);
	Fog::Register(tableFlow);
	Horizon::Register(tableFlow);
	LensFlare::Register(tableFlow);
	Starfield::Register(tableFlow);

	_handler.MakeReadOnlyTable(tableFlow, ScriptReserved_WeatherType, WEATHER_TYPES);
	_handler.MakeReadOnlyTable(tableFlow, ScriptReserved_LaraType, PLAYER_TYPES);
	_handler.MakeReadOnlyTable(tableFlow, ScriptReserved_RotationAxis, ROTATION_AXES);
	_handler.MakeReadOnlyTable(tableFlow, ScriptReserved_ItemAction, ITEM_MENU_ACTIONS);
	_handler.MakeReadOnlyTable(tableFlow, ScriptReserved_ErrorMode, ERROR_MODES);
	_handler.MakeReadOnlyTable(tableFlow, ScriptReserved_GameStatus, GAME_STATUSES);
	_handler.MakeReadOnlyTable(tableFlow, ScriptReserved_FreezeMode, FREEZE_MODES);
}

FlowHandler::~FlowHandler()
{
	for (auto& level : Levels)
		delete level;
}

std::string FlowHandler::GetGameDir()
{
	return _gameDir;
}

void FlowHandler::SetGameDir(const std::string& assetDir)
{
	_gameDir = assetDir;
}

void FlowHandler::SetLanguageNames(sol::as_table_t<std::vector<std::string>>&& src)
{
	_languageNames = std::move(src);
}

void FlowHandler::SetStrings(sol::nested<std::unordered_map<std::string, std::vector<std::string>>>&& src)
{
	if (_translationMap.empty())
	{
		_translationMap = std::move(src);
	}
	else
	{
		for (auto& stringPair : src.value())
			_translationMap.insert_or_assign(stringPair.first, stringPair.second);
	}
}

Statistics* FlowHandler::GetStatistics(std::optional<bool> game) const
{
	return (game.value_or(false) ? &SaveGame::Statistics.Game : &SaveGame::Statistics.Level);
}

void FlowHandler::SetStatistics(Statistics const& src, std::optional<bool> game)
{
	if (game.value_or(false))
	{
		SaveGame::Statistics.Game = src;
	}
	else
	{
		SaveGame::Statistics.Level = src;
	}
}

void FlowHandler::SetSettings(Settings const& src)
{
	_settings = src;

	// Copy weapon, ammo and consumable settings to in-game structs.
	InitializeWeaponInfo(_settings);
	InitializeAmmo(_settings);
	InitializeConsumables(_settings);
}

void FlowHandler::AddLevel(Level const& level)
{
	Levels.push_back(new Level{ level });
}

void FlowHandler::SetIntroImagePath(const std::string& path)
{
	IntroImagePath = path;
}

void FlowHandler::SetIntroVideoPath(const std::string& path)
{
	IntroVideoPath = path;
}

void FlowHandler::SetTitleScreenImagePath(const std::string& path)
{
	TitleScreenImagePath = path;
}


int FlowHandler::GetTotalSecretCount()
{
	return TotalNumberOfSecrets;
}

void FlowHandler::SetTotalSecretCount(int secretsNumber)
{
	TotalNumberOfSecrets = secretsNumber;
}

void FlowHandler::LoadFlowScript()
{
	TENLog("Loading gameflow script, strings, and settings...", LogLevel::Info);

	Levels.clear();

	_handler.ExecuteScript(_gameDir + "Scripts/Gameflow.lua");
	_handler.ExecuteScript(_gameDir + "Scripts/SystemStrings.lua", true);
	_handler.ExecuteScript(_gameDir + "Scripts/Strings.lua", true);
	_handler.ExecuteScript(_gameDir + "Scripts/Settings.lua", true);

	SetScriptErrorMode(GetSettings()->System.ErrorMode);
	
	// Check if levels exist in Gameflow.lua.
	if (Levels.empty())
	{
		throw TENScriptException("No levels found. Check Gameflow.lua file integrity.");
	}
	else
	{
		TENLog("Level count: " + std::to_string(Levels.size()), LogLevel::Info);
	}
}

char const * FlowHandler::GetString(const char* id) const
{
	if (!ScriptAssert(_translationMap.find(id) != _translationMap.end(), std::string{ "Couldn't find string " } + id))
	{
		return id;
	}
	else
	{
		return _translationMap.at(std::string(id)).at(0).c_str();
	}
}

bool FlowHandler::IsStringPresent(const char* id) const
{
	return _translationMap.find(id) != _translationMap.end();
}

Settings* FlowHandler::GetSettings()
{
	return &_settings;
}

Level* FlowHandler::GetLevel(int id)
{
	return Levels[id];
}

Level* FlowHandler::GetCurrentLevel()
{
	return Levels[CurrentLevel];
}

int	FlowHandler::GetNumLevels() const
{
	return (int)Levels.size();
}

int FlowHandler::GetLevelNumber(const std::string& fileName)
{
	if (fileName.empty())
		return -1;

	auto fileNameWithForwardSlashes = fileName;
	std::replace(fileNameWithForwardSlashes.begin(), fileNameWithForwardSlashes.end(), '\\', '/');

	auto requestedPath = std::filesystem::path{ fileName };
	bool isAbsolute = requestedPath.is_absolute();
	if (!isAbsolute)
		requestedPath = std::filesystem::path{ GetGameDir() + fileName };

	if (std::filesystem::is_regular_file(requestedPath))
	{
		auto lcFileName = TEN::Utils::ToLower(fileNameWithForwardSlashes);

		if (isAbsolute)
		{
			for (int i = 0; i < Levels.size(); i++)
			{
				auto lcFullLevelPathFromFlow = TEN::Utils::ToLower(GetGameDir() + GetLevel(i)->FileName);
				std::replace(lcFullLevelPathFromFlow.begin(), lcFullLevelPathFromFlow.end(), '\\', '/');

				if (lcFullLevelPathFromFlow == lcFileName)
					return i;
			}
		}
		else
		{
			for (int i = 0; i < Levels.size(); i++)
			{
				auto lcLevelNameFromFlow = TEN::Utils::ToLower(GetLevel(i)->FileName);
				std::replace(lcLevelNameFromFlow.begin(), lcLevelNameFromFlow.end(), '\\', '/');

				if (lcLevelNameFromFlow == lcFileName)
					return i;
			}
		}
	}
	else
	{
		TENLog("Provided -level arg \"" + fileName + "\" does not exist.");
		return -1;
	}

	TENLog("Provided -level arg \"" + fileName + "\" was not found in gameflow.lua.");
	return -1;
}

void FlowHandler::EndLevel(std::optional<int> nextLevel, std::optional<int> startPosIndex)
{
	int index = (nextLevel.has_value() && nextLevel.value() != 0) ? nextLevel.value() : CurrentLevel + 1;
	NextLevel = index;
	RequiredStartPos = startPosIndex.has_value() ? startPosIndex.value() : 0;
}

GameStatus FlowHandler::GetGameStatus()
{
	return this->LastGameStatus;
}

FreezeMode FlowHandler::GetFreezeMode()
{
	return this->CurrentFreezeMode;
}

void FlowHandler::SetFreezeMode(FreezeMode mode)
{
	this->CurrentFreezeMode = mode;
}

void FlowHandler::FlipMap(int group)
{
	DoFlipMap(group);
}

bool FlowHandler::GetFlipMapStatus(std::optional<int> group)
{
	if (!group.has_value() || group.value() == NO_VALUE)
	{
		return FlipStatus;
	}

	if (group.value() < 0 || group.value() >= MAX_FLIPMAP)
	{
		TENLog("Maximum flipmap group number is " + std::to_string(MAX_FLIPMAP) + ". Please specify another index.", LogLevel::Warning);
		return false;
	}

	return FlipStatus && FlipStats[group.value()];
}

void FlowHandler::SaveGame(int slot)
{
	SaveGame::Save(slot);
}

void FlowHandler::LoadGame(int slot)
{
	if (!SaveGame::DoesSaveGameExist(slot))
		return;

	NextLevel = -(slot + 1);
}

void FlowHandler::DeleteSaveGame(int slot)
{
	SaveGame::Delete(slot);
}

bool FlowHandler::DoesSaveGameExist(int slot)
{
	return SaveGame::DoesSaveGameExist(slot, true);
}

int FlowHandler::GetSecretCount() const
{
	return SaveGame::Statistics.Game.Secrets;
}

void FlowHandler::SetSecretCount(int secretsNum)
{
	if (secretsNum > UCHAR_MAX)
		return;

	SaveGame::Statistics.Game.Secrets = secretsNum;
}

void FlowHandler::AddSecret(int levelSecretIndex)
{
	static constexpr unsigned int maxSecretIndex = CHAR_BIT * sizeof(unsigned int);

	if (levelSecretIndex >= maxSecretIndex)
	{
		TENLog("Current maximum amount of secrets per level is " + std::to_string(maxSecretIndex) + ".", LogLevel::Warning);
		return;
	}

	if (SaveGame::Statistics.SecretBits & (1 << levelSecretIndex))
		return;

	if (SaveGame::Statistics.Game.Secrets >= UINT_MAX)
	{
		TENLog("Maximum amount of level secrets is already reached!", LogLevel::Warning);
		return;
	}

	PlaySecretTrack();
	SaveGame::Statistics.SecretBits |= 1 << levelSecretIndex;
	SaveGame::Statistics.Level.Secrets++;
	SaveGame::Statistics.Game.Secrets++;
}

bool FlowHandler::IsFlyCheatEnabled() const
{
	return FlyCheat;
}

void FlowHandler::EnableFlyCheat(bool enable)
{
	FlyCheat = enable;
}

bool FlowHandler::IsPointFilterEnabled() const
{
	return PointFilter;
}

void FlowHandler::EnablePointFilter(bool enable)
{
	PointFilter = enable;
}

bool FlowHandler::IsMassPickupEnabled() const
{
	return MassPickup;
}

void FlowHandler::EnableMassPickup(bool enable)
{
	MassPickup = enable;
}

bool FlowHandler::IsLaraInTitleEnabled() const
{
	return LaraInTitle;
}

void FlowHandler::EnableLaraInTitle(bool enable)
{
	LaraInTitle = enable;
}

void FlowHandler::EnableLevelSelect(bool enable)
{
	LevelSelect = enable;
}

bool FlowHandler::IsHomeLevelEnabled() const
{
	return HomeLevel;
}

void FlowHandler::EnableHomeLevel(bool enable)
{
	HomeLevel = enable;
}

bool FlowHandler::IsLoadSaveEnabled() const
{
	return LoadSave;
}

void FlowHandler::EnableLoadSave(bool enable)
{
	LoadSave = enable;
}

void FlowHandler::PrepareInventoryObjects()
{
	const auto& level = *Levels[CurrentLevel];
	for (const auto& refInvItem : level.InventoryObjects)
	{
		if (refInvItem.ObjectID < 0 || refInvItem.ObjectID >= INVENTORY_TABLE_SIZE)
			continue;

		auto& invItem = InventoryObjectTable[refInvItem.ObjectID];

		invItem.ObjectName = refInvItem.Name.c_str();
		invItem.Scale1 = refInvItem.Scale;
		invItem.YOffset = refInvItem.YOffset;
		invItem.Orientation = EulerAngles(ANGLE(refInvItem.Rot.x), ANGLE(refInvItem.Rot.y), ANGLE(refInvItem.Rot.z));
		invItem.MeshBits = refInvItem.MeshBits;
		invItem.Options = refInvItem.MenuAction;
		invItem.RotFlags = refInvItem.RotFlags;
	}
}

bool FlowHandler::DoFlow()
{
	// We start with the title level, if no other index is specified
	if (CurrentLevel == -1)
		CurrentLevel = SystemNameHash = 0;

	SelectedLevelForNewGame = 0;
	SelectedSaveGame = 0;
	SaveGameHeader header;

	// We loop indefinitely, looking for return values of DoLevel
	bool loadFromSavegame = false;

	while (DoTheGame)
	{
		// Check if called level exists in script.
		if (CurrentLevel >= Levels.size())
		{
			TENLog("Level not found. Check Gameflow.lua file integrity.", LogLevel::Error, LogConfig::All);
			CurrentLevel = 0;
		}

		GameStatus status;

		if (CurrentLevel == 0)
		{
			try
			{
				status = DoLevel(CurrentLevel);
			}
			catch (TENScriptException const& e)
			{
				std::string msg = std::string{ "A Lua error occurred while running the title level; " } + __func__ + ": " + e.what();
				TENLog(msg, LogLevel::Error, LogConfig::All);
				ShutdownTENLog();
				throw;
			}
		}
		else
		{
			try
			{
				PrepareInventoryObjects();
				status = DoLevel(CurrentLevel, loadFromSavegame);
			}
			catch (TENScriptException const& e) 
			{
				std::string msg = std::string{ "A Lua error occurred while running a level; " } + __func__ + ": " + e.what();
				TENLog(msg, LogLevel::Error, LogConfig::All);
				status = GameStatus::ExitToTitle;
			}

			loadFromSavegame = false;
		}

		switch (status)
		{
		case GameStatus::ExitGame:
			DoTheGame = false;
			break;

		case GameStatus::ExitToTitle:
		case GameStatus::LaraDead:
			CurrentLevel = 0;
			break;

		case GameStatus::NewGame:
			// NOTE: 0 reserved for title level and 1 reserved for home level.
			CurrentLevel = (SelectedLevelForNewGame != 0) ? SelectedLevelForNewGame : (IsHomeLevelEnabled() ? 2 : 1);
			RequiredStartPos = 0;
			SelectedLevelForNewGame = 0;
			InitializeGame = true;
			break;

		case GameStatus::HomeLevel:
			CurrentLevel = 1;
			RequiredStartPos = 0;
			InitializeGame = true;
			break;

		case GameStatus::LoadGame:
			// Load header of savegame to get level to load.
			SaveGame::LoadHeader(SelectedSaveGame, &header);

			// Load level.
			CurrentLevel = header.Level;
			NextLevel = 0;
			loadFromSavegame = true;
			break;

		case GameStatus::LevelComplete:
			if (NextLevel >= Levels.size())
			{
				CurrentLevel = 0; // TODO: Final credits.
			}
			else
			{
				CurrentLevel = NextLevel;

				// Reset hub if next level has it set.
				if (g_GameFlow->GetLevel(CurrentLevel)->GetResetHubEnabled())
					SaveGame::ResetHub();
			}

			NextLevel = 0;
			break;
		}
	}

	g_GameScript->ResetScripts(true);
	return true;
}

bool FlowHandler::IsLevelSelectEnabled() const
{
	return LevelSelect;
}
