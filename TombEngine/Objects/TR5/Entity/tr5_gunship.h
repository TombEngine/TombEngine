#pragma once

#include "Game/items.h"

enum class LaraWeaponType : int;

namespace TEN::Entities::Creatures::TR5
{
	void InitializeGunShip(short itemNumber);
	void ControlGunShip(short itemNumber);
	void ControlOriginalGunShip(short itemNumber);
	void HitGunship(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);

	// Fire block: can the shooter fire from the muzzle? (target in the line of fire, own item is skipped.)
	bool CanFireShot(ItemInfo* shooter, const Vector3& muzzlePos, const EulerAngles& orientation, float range);

	// A complete shot (hitscan): fire block + sound + muzzle effects (light/casing/smoke) +
	// tracer + shot ray with hit resolution (static: damage/shatter, item: damage, wall: ricochet).
	void FireShot(ItemInfo* shooter, const Vector3& muzzlePos, const EulerAngles& orientation, float range, int damage, LaraWeaponType weaponType, int sfxID);

	struct GunShipStateInfo
	{
		int currentState = -1;
		int prevState = -1;
		int inertiaTimer = 0;

		bool isDodgingUp = false;
		bool isDodgingDown = false;

		void Reset()
		{
			currentState = -1;
			prevState = -1;
			inertiaTimer = 0;
			isDodgingUp = false;
			isDodgingDown = false;
		}
	};

	struct GunShipMovementInfo
	{
		float currentSpeed = 0.0f;
		float currentYSpeed = 0.0f;
		float targetSpeed = 0.0f;
		float ySpeedTargetGlobal = 0.0f;

		void Reset()
		{
			currentSpeed = 0.0f;
			currentYSpeed = 0.0f;
			targetSpeed = 0.0f;
			ySpeedTargetGlobal = 0.0f;
		}
	};

	struct GunShipOrientationInfo
	{
		float pitchTarget = 0.0f;
		float bankTarget = 0.0f;
		EulerAngles targetOrient = EulerAngles::Identity;

		void Reset()
		{
			pitchTarget = 0.0f;
			bankTarget = 0.0f;
			targetOrient = EulerAngles::Identity;
		}
	};
}
