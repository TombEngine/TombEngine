#pragma once

struct CreatureBiteInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeElectricBall(short itemNumber);
	void ControlElectricBall(short itemNumber);
	void InitializeElectricBallImpactPoint(short itemNumber);

	void SpawnElectricBallLightning(ItemInfo& item, const Vector3& pos, const CreatureBiteInfo& bite);
	void SpawnElectricBallShockwaveAttackSparks(int x, int y, int z, byte r, byte g, byte b, byte size);	
}
