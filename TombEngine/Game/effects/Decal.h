#pragma once
#include "Specific/clock.h"

namespace TEN::Effects::Decal
{
	enum class DecalType
	{
		BulletHole,
		Explosion
	};

	struct Decal
	{
		static constexpr auto COUNT_MAX = 32;
		static constexpr auto LIFE_MAX = 5.0f * FPS;
		static constexpr auto LIFE_START_FADING = LIFE_MAX / 2;
		
		BoundingSphere Sphere = {};
		int RoomNumber = NO_VALUE;

		DecalType Type = DecalType::BulletHole;

		float Life = 0.0f;
		float StartOpacity = 0.0f;
		float Opacity = 0.0f;
	};

	extern std::vector<Decal> Decals;

	void SpawnDecal(Vector3 pos, int roomNumber, DecalType type);

	void UpdateDecals();
	void ClearDecals();
}
