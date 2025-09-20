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
		ClawMutantPlasma = 9
	};

	void ControlEnemyMissile(short fxNumber);
}
