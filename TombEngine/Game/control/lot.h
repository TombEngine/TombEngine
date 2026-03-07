#pragma once
#include "Game/control/box.h"

const std::vector<GAME_OBJECT_ID> FriendlyCreatures = 
{
	ID_LARA,
	ID_TROOPS,
	ID_CIVVY,
	ID_GUIDE,
	ID_VON_CROY,
	ID_SCIENTIST,
	ID_MONK1,
	ID_MONK2,
	ID_WHALE,
	ID_WINSTON
};

extern std::vector<int> ActiveCreatures;

void InitializeLOTarray(int allocMem);
bool EnableEntityAI(short itemNum, bool always, bool makeTarget = true);
void InitializeSlot(short itemNum, bool makeTarget);
void SetEntityTarget(short itemNum, short target);
void TargetNearestEntity(ItemInfo& item, const std::vector<GAME_OBJECT_ID>& keyObjectIds = {}, bool ignoreKeyObjectIds = true);
void DisableEntityAI(short itemNumber);
void ClearLOT(LOTInfo* LOT);
void CreateZone(ItemInfo* item);
