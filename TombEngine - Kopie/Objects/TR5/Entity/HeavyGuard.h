#pragma once

class EulerAngles;
class GameVector;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR5
{
	void InitializeHeavyGuard(short itemNumber);
	void HeavyGuardControl(short itemNumber);
	void HeavyGuardHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
}
