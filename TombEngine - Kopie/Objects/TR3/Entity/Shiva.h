#pragma once

class GameVector;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR3
{
	void InitializeShiva(short itemNumber);
	void ShivaControl(short itemNumber);
	void ShivaHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
}
