#pragma once

class Vector3i;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR5
{
	struct GuardianInfo;

	GuardianInfo& GetGuardianInfo(ItemInfo& item);

	void InitializeGuardian(short itemNumber);
	void ControlGuardian(short itemNumber);

	void DoGuardianLaserAttack(ItemInfo* item);
	void DoGuardianDeath(int itemNumber, ItemInfo& item);
	void SpawnGuardianSparks(const Vector3& pos, const Vector3& color, unsigned int count, int unk = 0);
}
