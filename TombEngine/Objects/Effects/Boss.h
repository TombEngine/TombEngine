#pragma once
#include "Game/items.h"

namespace TEN::Effects::Boss
{
	enum class BossItemFlags
	{
		Object = 0,			 // BossFlagValue enum.
		Rotation = 1,		 // Store rotation for use (e.g. Puna when summoning).
		ShieldIsEnabled = 2,
		AttackType = 3,
		AttackCount = 4,	 // Change behaviour after some attack.
		ChargedState = 4,
		DeathCount = 5,
		ItemNumber = 6,		 // Check if summon is dead.
		ExplodeCount = 7
	};

	enum class BossFlagValue
	{
		ShockwaveExplosion = (1 << 0),
		Shield             = (1 << 1),
		Lizard			   = (1 << 2)
	};

	void ShieldControl(int itemNumber);
	void ShockwaveRingControl(int itemNumber);
	void ShockwaveExplosionControl(int itemNumber);

	void ExplodeBoss(int itemNumber, ItemInfo& item, int deathCountToDie, const Vector4& color, const Vector4& explosionColor1, const Vector4& explosionColor2, bool allowExplosion = true);
	void CheckForRequiredObjects(ItemInfo& item);

	void SpawnShield(const ItemInfo& item, const Vector4& color);
	void SpawnShockwaveExplosion(const ItemInfo& item, const Vector4& color);

	void SpawnShieldAndRichochetSparks(const ItemInfo& item, const Vector3& pos, const Vector4& color);
}
