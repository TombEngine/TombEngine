#pragma once

class GameVector;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR5
{
	void InitializeGuard(short itemNumber);
	void GuardControl(short itemNumber);

	void InitializeSniper(short itemNumber);
	void SniperControl(short itemNumberber);

	void InitializeGuardLaser(short itemNumber);
	void ControlGuardLaser(short itemNumberber);

	void InitializeMafia2(short itemNumber);
	void Mafia2Control(short itemNumber);

	void GuardHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
}
