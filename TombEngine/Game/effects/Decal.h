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
		static constexpr auto COUNT_MAX = 48;
		static constexpr auto COUNT_THRESHOLD = COUNT_MAX / 2;
		static constexpr auto LIFE_MAX = 8 * FPS;
		static constexpr auto LIFE_START_FADING = LIFE_MAX / 2;
		static constexpr auto LIFE_QUEUE_FADEOUT = FPS / 2;
		
		BoundingSphere Sphere = {};
		int RoomNumber = NO_VALUE;
		std::array<int, 4> Neighbors = {};

		DecalType Type = DecalType::BulletHole;

		int Life = 0;
		int LifeStartFading = 0;

		float StartOpacity = 0.0f;
		float Opacity = 0.0f;

		void UpdateNeighbors();
	};

	extern std::vector<Decal> Decals;

	void SpawnDecal(Vector3 pos, int roomNumber, DecalType type);

	void UpdateDecals();
	void ClearDecals();
}
