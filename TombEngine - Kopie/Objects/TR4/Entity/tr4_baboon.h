#pragma once
#include "Math/Math.h"
#include "Game/items.h"

namespace TEN::Entities::TR4
{
	struct BaboonRespawnData
	{
		int ID;
		Pose Pose;
		unsigned int Count;
		unsigned int MaxCount;  // Used to limit the number of respawns.
	};

	class BaboonRespawner
	{
	private:
		std::vector<BaboonRespawnData> BaboonRespawnArray;

	public:
		void Free(void);
		void Add(ItemInfo* item, unsigned int maxCount);
		void Remove(int ID);
		int GetBaboonFreePlace(void);
		BaboonRespawnData* GetBaboonRespawn(int ID);
		int GetCount(int ID);
		int GetCountMax(int ID);
	};

	extern BaboonRespawner BaboonRespawn;

	extern void InitializeBaboon(short itemNumber);
	extern void BaboonControl(short itemNumber);
}