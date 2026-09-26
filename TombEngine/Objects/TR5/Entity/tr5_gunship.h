#pragma once

#include "Game/items.h"

enum class LaraWeaponType : int;

namespace TEN::Entities::Creatures::TR5
{
	void InitializeGunShip(short itemNumber);
	void ControlGunShip(short itemNumber);

	// Feuersperre: Kann der Schuetzer von der Muenze aus feuern? (Ziel in der Schusslinie, eigenes Item wird uebersprungen.)
	bool CanFireShot(ItemInfo* shooter, const Vector3& muzzlePos, const EulerAngles& orientation, float range);

	// Ein kompletter Schuss (Hitscan): Feuersperre + Schall + Muenzen-Effekte (Licht/Huelse/Rauch) +
	// Tracer + Schuss-Ray mit Treffer-Aufloesung (Static: Schaden/Zerbrechen, Item: Schaden, Wand: Ricochet).
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
