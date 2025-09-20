#pragma once

class GameVector;
class Vector3i;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR2
{
	void InitializeSpearGuardian(short itemNumber);
	void SpearGuardianControl(short itemNumber);
	void SpearGuardianHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
	
	void SpawnSpearGuardianEffect(const Vector3& pos, int roomNumber);
}
