#pragma once

namespace TEN::Entities::Effects
{
	enum class MissileType
	{
		SethNormal = 0,
		SethLarge = 1,
		Harpy = 2,
		Demigod3Single = 3,
		Demigod3Radial = 4,
		Demigod2 = 5,
		CrocgodMutant = 6,
		SophiaLeighNormal = 7,
		SophiaLeighLarge = 8,
		ClawMutantPlasma = 9,
		WillardPlasmaBall = 10
	};

	void ControlEnemyMissile(short fxNumber);
	void SpawnWillardScatterPlasmaBall(const Vector3i& pos, int roomNumber, short yaw, short type);
	void TriggerWillardPlasmaBallFlame(short fxNumber, short xVel, short yVel, short zVel);
}
