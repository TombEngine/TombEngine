#pragma once

#include "Scripting/Internal/TEN/Flow/Settings/Settings.h"

enum class ModificationType;
enum GAME_OBJECT_ID : short;
struct LaraInfo;

void InitializeAmmo(const Settings& settings);
bool TryAddingAmmo(LaraInfo&, GAME_OBJECT_ID objectID, std::optional<int> amount = std::nullopt);
bool TryRemovingAmmo(LaraInfo&, GAME_OBJECT_ID objectID, std::optional<int> amount = std::nullopt);
bool TryModifyingAmmo(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount, ModificationType modType);
std::optional<int> GetAmmoCount(LaraInfo&, GAME_OBJECT_ID objectID);
int GetDefaultAmmoCount(GAME_OBJECT_ID objectID);
