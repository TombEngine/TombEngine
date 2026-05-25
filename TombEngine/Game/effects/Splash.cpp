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

		unsigned int splashDripCount = 32;
		float splashDripScale = 1.0f;
		if (setup->InnerRadius >= 224.0f)
		{
			splashDripCount = 448;
			splashDripScale = 2.5f;
		}
		else if (setup->InnerRadius >= 128.0f)
		{
			splashDripCount = 320;
			splashDripScale = 2.0f;
		}
		else if (setup->InnerRadius >= 64.0f)
		{
			splashDripCount = 192;
			splashDripScale = 1.35f;
		}

		int splashSetupCount = 0;
		float splashPower = std::min(256.0f, setup->SplashPower);
		float splashVel = splashPower / 16;

		for (auto& splash : SplashEffects)
		{
			if (splash.isActive)
				continue;

			splash = {};

			if (splashSetupCount == 0)
			{
				splash.isActive = true;
				splash.Position = setup->Position;
				splash.life = 62;
				splash.isRipple = true;
				splash.InnerRadius = setup->InnerRadius * 0.5f;
				splash.InnerRadialVel = splashVel;
				splash.HeightSpeed = splashPower * 1.2f;
				splash.height = 0;
				splash.HeightVel = -16;
				splash.OuterRadius = setup->InnerRadius;
				splash.outerRadialVel = splashVel * 1.5f;
				splash.AnimPhase = 0.0f;
				splash.StoreInterpolationData();
				splashSetupCount++;
			}
			else
			{
				float thickness = Random::GenerateFloat(64.0f, 128.0f);
				float vel = (splashSetupCount == 2) ?
					((splashVel / 16) + Random::GenerateFloat(2, 4)) :
					((splashVel / 7) + Random::GenerateFloat(3, 7));

				splash.isActive = true;
				splash.Position = setup->Position;
				splash.isRipple = true;
				splash.InnerRadius = thickness * 0.5f;
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
				splash.AnimPhase = 0.0f;
				splash.StoreInterpolationData();
				splashSetupCount++;
			}

			if (splashSetupCount == SETUP_COUNT_MAX)
				break;
		}

		SpawnSplashDrips(Vector3(setup->Position.x, setup->Position.y - 15, setup->Position.z), room, splashDripCount, false, splashDripScale);

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
