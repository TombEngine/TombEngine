#include "framework.h"
#include "Game/itemdata/creature_info.h"

#include "Game/items.h"

bool CreatureInfo::IsTargetAlive()
{
    return ((Enemy != nullptr) && (Enemy->HitPoints > 0));
}
