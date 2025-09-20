#pragma once

enum class ModificationType;
enum GAME_OBJECT_ID : short;
struct LaraInfo;

bool TryAddMiscItem(LaraInfo& lara, GAME_OBJECT_ID objectID);
bool TryRemoveMiscItem(LaraInfo& lara, GAME_OBJECT_ID objectID);
bool TryModifyMiscCount(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount, ModificationType modType);
std::optional<bool> HasMiscItem(LaraInfo& lara, GAME_OBJECT_ID objectID);
