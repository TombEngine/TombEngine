#include "framework.h"
#include "Game/effects/Decal.h"

#include "Game/effects/effects.h"

namespace TEN::Effects::Decal
{
	std::vector<Decal> Decals;

	void SpawnDecal(Vector3 pos, int roomNumber, DecalType type)
	{
		auto& decal = GetNewEffect(Decals, Decal::COUNT_MAX);

		auto radius = 1.0f;
		auto opacity = 1.0f;
		auto life = Decal::LIFE_MAX;

		switch (type)
		{
			default:
			case DecalType::BulletHole:
				radius = CLICK(0.15f) * Random::GenerateFloat(0.9f, 1.1f);
				opacity = Random::GenerateFloat(0.4f, 0.6f) * Random::GenerateFloat(0.8f, 1.2f);
				break;

			case DecalType::Explosion:
				radius = CLICK(3.0f) * Random::GenerateFloat(0.7f, 1.3f);
				opacity = Random::GenerateFloat(0.9f, 1.0f) * Random::GenerateFloat(0.8f, 1.2f);
				life *= 2.0f;
				break;
		}

		decal.Type = type;
		decal.Sphere.Center = pos;
		decal.Sphere.Radius = radius;
		decal.RoomNumber = roomNumber;
		decal.StartOpacity = opacity;
		decal.Life = life;
	}

	void UpdateDecals()
	{
		if (Decals.empty())
			return;

		for (auto& decal : Decals)
		{
			if (decal.Life <= 0.0f)
				continue;

			// Update opacity.
			if (decal.Life <= Decal::LIFE_START_FADING)
			{
				float alpha = 1.0f - (decal.Life / Decal::LIFE_START_FADING);
				decal.Opacity = Lerp(decal.StartOpacity, 0.0f, alpha);
			}
			else
				decal.Opacity = decal.StartOpacity;

			// Update life.
			decal.Life -= 1.0f;
		}

		ClearInactiveEffects(Decals);
	}

	void ClearDecals()
	{
		Decals.clear();
	}
}
