#pragma once
#include "Specific/clock.h"

namespace TEN::Effects::Decal
{
	enum class DecalType
	{
		BulletHole,
		Explosion,
		Dirt,
		Blood
	};

	struct Decal
	{
		static constexpr auto COUNT_MAX = 64;
		static constexpr auto LIFE_MAX = 20.0f * FPS;
		static constexpr auto LIFE_START_FADING = LIFE_MAX / 2;

		Vector3 Position = {};
		int RoomNumber = NO_VALUE;

		DecalType Type = DecalType::BulletHole;
		float Radius = 0.0f;

		float Life = 0.0f;
		float StartOpacity = 0.0f;
		float Opacity = 0.0f;
	};

	extern std::vector<Decal> Decals;

	void SpawnDecal(Vector3 pos, int roomNumber, DecalType type);

	void UpdateDecals();
	void ClearDecals();
}
