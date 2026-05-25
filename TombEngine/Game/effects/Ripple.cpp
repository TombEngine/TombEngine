#include "framework.h"
#include "Game/effects/Ripple.h"

#include "Game/effects/effects.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Specific/clock.h"

using namespace TEN::Math;

namespace TEN::Effects::Ripple
{
	constexpr auto RIPPLE_COUNT_MAX	  = 1024;
	constexpr auto RIPPLE_OPACITY_MAX = 0.5f;

	std::vector<Ripple> Ripples = {};

	void SpawnRipple(const Vector3& pos, int roomNumber, float size, int flags, const Vector3& normal)
	{
		constexpr auto LIFE_WATER_SURFACE_MAX = 2.0f;
		constexpr auto LIFE_WATER_SURFACE_MIN = 1.25f;
		constexpr auto LIFE_GROUND_MAX		  = 0.4f;
		constexpr auto LIFE_GROUND_MIN		  = LIFE_GROUND_MAX / 2;
		constexpr auto FADE_FAST_COEFF		  = 0.5f;
		constexpr auto FADE_SLOW_COEFF		  = 0.75f;
		constexpr auto COLOR_WHITE			  = Vector4(1.0f, 1.0f, 1.0f, 0.0f);

		auto& ripple = GetNewEffect(Ripples, RIPPLE_COUNT_MAX);

		float lifeInSec = (flags & (int)RippleFlags::OnGround) ?
			Random::GenerateFloat(LIFE_GROUND_MIN, LIFE_GROUND_MAX) :
			Random::GenerateFloat(LIFE_WATER_SURFACE_MIN, LIFE_WATER_SURFACE_MAX);
		float fadeDurationInSec = lifeInSec * ((flags & (int)RippleFlags::SlowFade) ? FADE_SLOW_COEFF : FADE_FAST_COEFF);

		ripple.SpriteIndex = Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_RIPPLES;
		ripple.Position = pos;
		ripple.RoomNumber = roomNumber;
		ripple.Normal = normal;
		ripple.Color = COLOR_WHITE;
		ripple.Life =
		ripple.LifeMax = round(lifeInSec * FPS);
		ripple.Size = size;
		ripple.FadeDuration = round(fadeDurationInSec * FPS);
		ripple.Flags = flags;
		ripple.StoreInterpolationData();
	}

	void UpdateRipples()
	{
		constexpr auto RIPPLE_SIZE_MAX = BLOCK(1.0f);
		constexpr auto SIZE_STEP_LARGE = 4.0f;
		constexpr auto SIZE_STEP_SMALL = 2.0f;

		if (Ripples.empty())
			return;

		for (auto& ripple : Ripples)
		{
			if (ripple.Life <= 0.0f)
				continue;

			ripple.StoreInterpolationData();

			// Update size.
			if (ripple.Size < RIPPLE_SIZE_MAX)
				ripple.Size += (ripple.Flags & ((int)RippleFlags::SlowFade | (int)RippleFlags::OnGround)) ? SIZE_STEP_SMALL : SIZE_STEP_LARGE;

			float lifeFullOpacity = ripple.LifeMax - (ripple.FadeDuration * 0.75f);
			float lifeStartFading = ripple.FadeDuration;

			// Update opacity.
			if (ripple.Life >= lifeFullOpacity)
			{
				float alpha = 1.0f - ((ripple.Life - lifeFullOpacity) / (ripple.LifeMax - lifeFullOpacity));
				ripple.Color.w = Lerp(0.0f, RIPPLE_OPACITY_MAX, alpha);
			}
			else if (ripple.Life <= lifeStartFading)
			{
				float alpha = 1.0f - (ripple.Life / lifeStartFading);
				ripple.Color.w = Lerp(RIPPLE_OPACITY_MAX, 0.0f, alpha);
			}

			// Update life.
			ripple.Life -= 1.0f;
		}

		ClearInactiveEffects(Ripples);
	}

	void ClearRipples()
	{
		Ripples.clear();
	}
}
