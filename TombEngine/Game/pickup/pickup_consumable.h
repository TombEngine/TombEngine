#pragma once

#include "Scripting/Internal/TEN/Flow/Settings/Settings.h"

enum class ModificationType;
enum GAME_OBJECT_ID : short;
struct LaraInfo;

void InitializeConsumables(const Settings& settings);
bool TryAddingConsumable(LaraInfo&, GAME_OBJECT_ID objectID, std::optional<int> amount = 0);
bool TryRemovingConsumable(LaraInfo&, GAME_OBJECT_ID objectID, std::optional<int> amount = 0);
bool TryModifyingConsumable(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount, ModificationType modType);
std::optional<int> GetConsumableCount(LaraInfo&, GAME_OBJECT_ID objectID);
int GetDefaultConsumableCount(GAME_OBJECT_ID objectID);
