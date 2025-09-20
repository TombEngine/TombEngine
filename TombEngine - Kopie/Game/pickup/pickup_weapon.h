#pragma once

enum class ModificationType;
enum GAME_OBJECT_ID : short;
struct LaraInfo;

bool TryAddingWeapon(LaraInfo& lara, GAME_OBJECT_ID objectID);
bool TryRemovingWeapon(LaraInfo& lara, GAME_OBJECT_ID objectID);
bool TryModifyWeapon(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> count, ModificationType type);
std::optional<bool> HasWeapon(LaraInfo&, GAME_OBJECT_ID objectID);
