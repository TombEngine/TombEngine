#pragma once
#include <string_view>

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Internal/LanguageScript.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Time/Time.h"
#include "Scripting/Internal/TEN/Logic/LogicHandler.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Scripting/Internal/TEN/Flow/Settings/Settings.h"
#include "Scripting/Internal/TEN/Flow/Statistics/Statistics.h"

class FlowHandler : public ScriptInterfaceFlowHandler
{
private:
	LuaHandler	_handler;
	std::string	_gameDir  = {};
	Settings	_settings = {};

	std::map<int, int> _moveableMap = {};

	std::unordered_map<std::string, std::vector<std::string>> _translationMap = {};
	std::vector<std::string>								  _languageNames  = {};

	void PrepareInventoryObjects();

public:
	bool LevelSelect = true;
	bool HomeLevel	 = false;
	bool LoadSave	 = true;
	bool FlyCheat	 = true;
	bool PointFilter = false;
	bool MassPickup	 = true;
	bool LaraInTitle = false;
	bool DebugMode	 = false;

	std::vector<Level*>	Levels;

	FlowHandler(sol::state* lua, sol::table& parent);
	~FlowHandler() override;

	std::string	GetGameDir() override;
	void		SetGameDir(const std::string& assetDir) override;
	void		AddLevel(Level const& level);
	void		LoadFlowScript();
	char const*	GetString(const char* id) const;
	bool		IsStringPresent(const char* id) const;
	void		SetStrings(sol::nested<std::unordered_map<std::string, std::vector<std::string>>>&& src);
	void		SetLanguageNames(sol::as_table_t<std::vector<std::string>>&& src);
	Level*		GetLevel(int id);
	Level*		GetCurrentLevel();
	int			GetLevelNumber(const std::string& flieName);
	int			GetNumLevels() const;
	void		EndLevel(std::optional<int> nextLevel, std::optional<int> startPosIndex);
	GameStatus	GetGameStatus();
	FreezeMode	GetFreezeMode();
	void		SetFreezeMode(FreezeMode mode);
	void		FlipMap(int group);
	bool		GetFlipMapStatus(std::optional<int> group);
	void		SaveGame(int slot);
	void		LoadGame(int slot);
	void		DeleteSaveGame(int slot);
	bool		DoesSaveGameExist(int slot);
	Statistics* GetStatistics(std::optional<bool> game) const;
	void		SetStatistics(const Statistics& src, std::optional<bool> game);
	int			GetSecretCount() const;
	void		SetSecretCount(int secretsNum);
	void		AddSecret(int levelSecretIndex);
	void		SetIntroImagePath(const std::string& path);
	void		SetIntroVideoPath(const std::string& path);
	void		SetTitleScreenImagePath(const std::string& path);
	int			GetTotalSecretCount();
	void		SetTotalSecretCount(int secretsNumber);
	bool		IsFlyCheatEnabled() const;
	void		EnableFlyCheat(bool enable);
	bool		IsPointFilterEnabled() const;
	void		EnablePointFilter(bool enable);
	bool		IsMassPickupEnabled() const;
	void		EnableMassPickup(bool enable);
	bool		IsLaraInTitleEnabled() const;
	void		EnableLaraInTitle(bool enable);
	bool		IsLevelSelectEnabled() const;
	void		EnableLevelSelect(bool enable);
	bool		IsHomeLevelEnabled() const;
	void		EnableHomeLevel(bool enable);
	bool		IsLoadSaveEnabled() const;
	void		EnableLoadSave(bool enable);
	
	Settings*	GetSettings();
	void		SetSettings(const Settings& src);

	bool DoFlow() override;
};
