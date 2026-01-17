#pragma once

#include <optional>
#include "Game/collision/collide_room.h"

struct ItemInfo;

namespace TEN::Entities::TR4
{
	void InitializeHorse(short itemNumber);
	void InitializeHorseman(short itemNumber);
	void HorsemanHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
	void HorsemanControl(short itemNumber);
}
