#include "framework.h"
#include "Game/effects/Splash.h"

#include "Game/collision/Point.h"
#include "Game/effects/Drip.h"
#include "Game/room.h"
#include "Sound/sound.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Drip;

namespace TEN::Effects::Splash
{
	int												  SplashCount;
	SplashEffectSetup								  SplashSetup;
	std::array<SplashEffect, SPLASH_EFFECT_COUNT_MAX> SplashEffects;

	void SetupSplash(const SplashEffectSetup* const setup, int room)
	{
		constexpr auto SETUP_COUNT_MAX = 3;

		int splashSetupCount = 0;
		float splashVel = 0.0f;

		for (auto& splash : SplashEffects)
		{
			if (splash.isActive)
				continue;

			if (splashSetupCount == 0)
			{
				float splashPower = std::min(256.0f, setup->SplashPower);

				splash.isActive = true;
				splash.Position = setup->Position;
				splash.life = 62;
				splash.isRipple = false;
				splash.InnerRadius = setup->InnerRadius;
				splashVel = splashPower / 16;
				splash.InnerRadialVel = splashVel;
				splash.HeightSpeed = splashPower * 1.2f;
				splash.height = 0;
				splash.HeightVel = -16;
				splash.OuterRadius = setup->InnerRadius / 3;
				splash.outerRadialVel = splashVel * 1.5f;
				splash.SpriteSeqStart = 8; // Splash texture.
				splashSetupCount++;
			}
			else
			{
				float thickness = Random::GenerateFloat(64, 128);

				splash.isActive = true;
				splash.Position = setup->Position;
				splash.isRipple = true;
				float vel = 0.0f;

				if (splashSetupCount == 2)
				{
					vel = (splashVel / 16) + Random::GenerateFloat(2, 4);
				}
				else
				{
					vel = (splashVel / 7) + Random::GenerateFloat(3, 7);
				}

				splash.InnerRadius = 0.0f;
				splash.InnerRadialVel = vel * 1.3f;
				splash.OuterRadius = thickness;
				splash.outerRadialVel = vel * 2.3f;
				splash.HeightSpeed = 128;
				splash.height = 0;
				splash.HeightVel = -16;

				float alpha = (vel / (splashVel / 2)) + 16;
				alpha = std::max(0.0f, std::min(alpha, 1.0f));

				splash.life = Lerp(48.0f, 70.0f, alpha);
				splash.SpriteSeqStart = 4; // Splash texture.
				splash.SpriteSeqEnd = 7; // Splash texture.
				splash.AnimSpeed = fmin(0.6f, (1 / splash.outerRadialVel) * 2);

				splashSetupCount++;
			}

			if (splashSetupCount == SETUP_COUNT_MAX)
				break;
		}

		SpawnSplashDrips(Vector3(setup->Position.x, setup->Position.y - 15, setup->Position.z), room, 32);

		auto soundPose = Pose(Vector3i(setup->Position));
		SoundEffect(SFX_TR4_LARA_SPLASH, &soundPose);
	}

	void UpdateSplashes()
	{
		if (SplashCount)
			SplashCount--;

		for (auto& splash : SplashEffects)
		{
			if (splash.isActive)
			{
				splash.StoreInterpolationData();

				splash.life--;
				if (splash.life <= 0)
					splash.isActive = false;

				splash.HeightSpeed += splash.HeightVel;
				splash.height += splash.HeightSpeed;

				if (splash.height < 0)
				{
					splash.height = 0;
					if (!splash.isRipple)
						splash.isActive = false;
				}

				splash.InnerRadius += splash.InnerRadialVel;
				splash.OuterRadius += splash.outerRadialVel;
				splash.AnimPhase += splash.AnimSpeed;

				int sequenceLength = splash.SpriteSeqEnd - splash.SpriteSeqStart;
				if (splash.AnimPhase > sequenceLength)
					splash.AnimPhase = fmod(splash.AnimPhase, sequenceLength);
			}
		}
	}

	void ClearSplashes()
	{
		SplashCount = 0;

		for (auto& splash : SplashEffects)
			splash = {};
	}

	void Splash(ItemInfo* item)
	{
		int probedRoomNumber = GetPointCollision(*item).GetRoomNumber();
		if (!TestEnvironment(ENV_FLAG_WATER, probedRoomNumber))
			return;

		int waterHeight = GetPointCollision(*item).GetWaterTopHeight();

		SplashSetup.Position = Vector3(item->Pose.Position.x, waterHeight - 1, item->Pose.Position.z);
		SplashSetup.SplashPower = item->Animation.Velocity.y;
		SplashSetup.InnerRadius = 64;
		SetupSplash(&SplashSetup, probedRoomNumber);
	}
}
