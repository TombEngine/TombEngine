#pragma once

class GameVector;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR2
{
	void InitializeSwordGuardian(short itemNumber);
	void SwordGuardianControl(short itemNumber);
	void SwordGuardianHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
}
