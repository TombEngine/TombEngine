#pragma once

namespace TEN::Effects::Splash
{
	constexpr auto SPLASH_EFFECT_COUNT_MAX = 8;

	// TODO: Refactor everything here to use new effect handling patterns.

	struct SplashEffectSetup
	{
		Vector3 Position	= Vector3::Zero;
		int		RoomNumber	= 0;
		float	SplashPower = 0.0f;
		float	InnerRadius = 0.0f;
	};

	struct SplashEffect
	{
		Vector3 Position = Vector3::Zero;

		bool isRipple;
		bool isActive;

		float InnerRadius;
		float InnerRadialVel;
		float HeightVel;
		float HeightSpeed;
		float height;
		float OuterRadius;
		float outerRadialVel;
		float AnimSpeed;
		float AnimPhase;
		int SpriteSeqStart;
		int SpriteSeqEnd;
		unsigned int life;

		Vector3		 PrevPosition	 = Vector3::Zero;
		float		 PrevInnerRadius = 0.0f;
		float		 PrevOuterRadius = 0.0f;
		float		 PrevHeight		 = 0.0f;
		float		 PrevHeightSpeed = 0.0f;
		float		 PrevAnimPhase	 = 0.0f;
		unsigned int PrevLife		 = 0;

		void StoreInterpolationData()
		{
			PrevPosition = Position;
			PrevInnerRadius = InnerRadius;
			PrevOuterRadius = OuterRadius;
			PrevHeight = height;
			PrevHeightSpeed = HeightSpeed;
			PrevAnimPhase = AnimPhase;
			PrevLife = life;
		}
	};

	extern int												 SplashCount;
	extern SplashEffectSetup								 SplashSetup;
	extern std::array<SplashEffect, SPLASH_EFFECT_COUNT_MAX> SplashEffects;

	void SetupSplash(const SplashEffectSetup* const setup, int room);
	void UpdateSplashes();
	void ClearSplashes();
	void Splash(ItemInfo* item);
}
