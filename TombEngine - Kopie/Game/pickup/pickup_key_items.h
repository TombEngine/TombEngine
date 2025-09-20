#pragma once

enum class ModificationType;
enum GAME_OBJECT_ID : short;
struct LaraInfo;

bool TryAddingKeyItem(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount = std::nullopt);
bool TryRemovingKeyItem(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount = std::nullopt);
bool TryModifyingKeyItem(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount, ModificationType modType);
std::optional<int> GetKeyItemCount(LaraInfo& lara, GAME_OBJECT_ID objectID);
