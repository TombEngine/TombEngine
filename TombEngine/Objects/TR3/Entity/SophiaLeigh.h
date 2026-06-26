#pragma once

namespace TEN::Entities::Creatures::TR3
{
	void InitializeSophiaLeigh(short itemNumber);
	void SophiaLeighControl(short itemNumber);
	void SophiaLeighHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
	void SpawnSophiaSparks(const Vector3& pos, const Vector3& color, unsigned int count, int unk);
	void ClearSophiaLeighEffects();
}
