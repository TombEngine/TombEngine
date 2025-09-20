#pragma once

class GameVector;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR3
{
	void InitializePuna(short itemNumber);
	void PunaControl(short itemNumber);
	void PunaHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
}
