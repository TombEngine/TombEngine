#pragma once

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/IO/ChunkId.h"
#include "Specific/IO/ChunkReader.h"
#include "Specific/IO/ChunkWriter.h"
#include "Specific/IO/LEB128.h"
#include "Specific/IO/Streams.h"
#include "Scripting/Internal/TEN/Types/Time/Time.h"
#include "Scripting/Internal/TEN/Flow/Statistics/Statistics.h"

using namespace TEN::Scripting;

constexpr auto SAVEGAME_MAX = 16;

struct GameStats
{
	unsigned int SecretBits = 0;

	Statistics Game	 = {};
	Statistics Level = {};
};

struct SaveGameHeader
{
	std::string LevelName = {};
	int			LevelHash = 0;
	int			Hours	  = 0;
	int			Minutes	  = 0;
	int			Seconds	  = 0;
	int			Level	  = 0;
	int			Timer	  = 0;
	int			Count	  = 0;
	bool		Present	  = false;
};

class SaveGame 
{
private:
	static std::string FullSaveDirectory;
	static int LastSaveGame;
	static std::map<int, std::vector<byte>> Hub;

	static std::string SaveGame::GetSavegameFilename(int slot);
	static bool IsSaveGameSlotValid(int slot);

	static const std::vector<byte> Build();
	static void Parse(const std::vector<byte>& buffer, bool hubMode);

public:
	static GameStats Statistics;
	static SaveGameHeader Infos[SAVEGAME_MAX];

	static void Init(const std::string& dir);
	static bool Load(int slot);
	static bool LoadHeader(int slot, SaveGameHeader* header);
	static void LoadHeaders();
	static bool Save(int slot);
	static void Delete(int slot);

	static bool DoesSaveGameExist(int slot, bool silent = false);
	static bool IsLoadGamePossible();
	static bool IsSaveGameValid(int slot);

	static void SaveHub(int index);
	static void LoadHub(int index);
	static bool IsOnHub(int index);
	static void ResetHub();
};
