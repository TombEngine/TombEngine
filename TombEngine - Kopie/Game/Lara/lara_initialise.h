#pragma once
#include "Game/Lara/lara_struct.h"

void BackupLara();
void InitializeLara(bool restore);
void InitializeLaraLoad(short itemNumber);
void InitializeLaraMeshes(ItemInfo* item);
void InitializeLaraAnims(ItemInfo* item);
void InitializeLaraStartPosition(ItemInfo& playerItem);
void InitializeLaraLevelJump(ItemInfo* item, LaraInfo* playerBackup);
void InitializeLaraDefaultInventory(ItemInfo& item);
void InitializePlayerVehicle(ItemInfo& playerItem);