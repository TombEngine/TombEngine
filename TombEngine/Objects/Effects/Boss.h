#pragma once
#include <array>
#include "Game/items.h"
#include "Specific/clock.h"

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

	// Color pattern matching the classic TR3 per-boss explosion ring colors.
	enum class BossExplosionRingColor
	{
		Tony = 0,
		Sophia = 1,
		Puna = 2,
		Willard = 3
	};

	// Procedural explosion ring effect spawned during boss death, matching classic TR3.
	constexpr auto MAX_BOSS_EXPLOSION_RINGS     = 12;
	constexpr auto BOSS_EXPLOSION_RING_LIFE_MAX = 1.0f * FPS;

	struct BossExplosionRing
	{
		bool IsActive = false;
		BossExplosionRingColor ColorType = BossExplosionRingColor::Willard;
		int Life = 0;
		int Speed = 0;
		int Radius = 0;
		short XRot = 0;
		short ZRot = 0;
		Vector3 Position = Vector3::Zero;

		int PrevRadius = 0;
		int PrevLife = 0;

		void StoreInterpolationData()
		{
			PrevRadius = Radius;
			PrevLife = Life;
		}
	};

	extern std::array<BossExplosionRing, MAX_BOSS_EXPLOSION_RINGS> BossExplosionRings;

	void ShieldControl(int itemNumber);
	void ShockwaveRingControl(int itemNumber);
	void ShockwaveExplosionControl(int itemNumber);

	void ExplodeBoss(ItemInfo& item, int deathCountToDie, const Vector4& color, const Vector4& explosionColor1, const Vector4& explosionColor2, bool allowExplosion = true);
	void CheckForRequiredObjects(ItemInfo& item);

	void SpawnShield(const ItemInfo& item, const Vector4& color);
	void SpawnShockwaveExplosion(const ItemInfo& item, const Vector4& color);

	void TriggerBossExplosionRing(const Vector3& pos, int speed, BossExplosionRingColor colorType);
	void UpdateBossExplosionRings();

	void SpawnShieldAndRichochetSparks(const ItemInfo& item, const Vector3& pos, const Vector4& color);
}
