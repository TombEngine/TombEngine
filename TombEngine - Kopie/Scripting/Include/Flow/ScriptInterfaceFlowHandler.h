#pragma once

#include "Game/control/control.h"
#include "Scripting/Internal/TEN/Flow/Settings/Settings.h"

class ScriptInterfaceLevel;

class ScriptInterfaceFlowHandler
{
public:
	GameStatus	LastGameStatus		= GameStatus::Normal;
	FreezeMode	LastFreezeMode		= FreezeMode::None;
	FreezeMode	CurrentFreezeMode	= FreezeMode::None;

	std::string	IntroImagePath		 = {};
	std::string	IntroVideoPath		 = {};
	std::string	TitleScreenImagePath = {};

	int	 SelectedLevelForNewGame = 0;
	int	 SelectedSaveGame		 = 0;

	int TotalNumberOfSecrets = 0;

	virtual ~ScriptInterfaceFlowHandler() = default;

	virtual void LoadFlowScript() = 0;
	virtual Settings* GetSettings() = 0;

	virtual void SetGameDir(const std::string& assetDir) = 0;
	virtual std::string GetGameDir() = 0;
	virtual int	GetNumLevels() const = 0;
	virtual char const* GetString(const char* id) const = 0;

	virtual bool IsFlyCheatEnabled() const = 0;
	virtual bool IsMassPickupEnabled() const = 0;
	virtual bool IsPointFilterEnabled() const = 0;
	virtual bool IsLaraInTitleEnabled() const = 0;
	virtual bool IsHomeLevelEnabled() const = 0;
	virtual bool IsLoadSaveEnabled() const = 0;

	virtual ScriptInterfaceLevel * GetLevel(int level) = 0;
	virtual int	GetLevelNumber(std::string const& fileName) = 0;
	virtual bool IsLevelSelectEnabled() const = 0;

	virtual bool DoFlow() = 0;
};

extern ScriptInterfaceFlowHandler* g_GameFlow;
