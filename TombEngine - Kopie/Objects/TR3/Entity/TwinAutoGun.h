#pragma once

class GameVector;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR3
{
	void InitializeTwinAutoGun(short itemNumber);
	void ControlTwinAutoGun(short itemNumber);
	void HitTwinAutoGun(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
}
