#pragma once

#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	// Must match CBVolumetricCloud.hlsli layout exactly.
	// Bound to register b9.
	struct alignas(16) CVolumetricCloudBuffer
	{
		// Row 0
		float CloudBottomHeight;
		float CloudTopHeight;
		float CloudThickness;
		float Coverage;
		//--
		// Row 1
		float Density;
		float ShapeScale;
		float DetailScale;
		float DetailStrength;
		//--
		// Row 2
		float WeatherScale;
		float Absorption;
		float AmbientContrib;
		float SilverliningStr;
		//--
		// Row 3
		float PhaseForward;
		float PhaseBackward;
		float WindSpeed;
		float EvolutionSpeed;
		//--
		// Row 4
		Vector2 WindDirection;
		float   Time;
		float   JitterStrength;
		//--
		// Row 5
		Vector3 LightDirection;
		float   Padding0;
		//--
		// Row 6
		Vector3 LightColor;
		float   Padding1;
		//--
		// Row 7
		int PrimaryStepCount;
		int ShadowStepCount;
		int DetailNoiseEnabled;
		int DebugView;
		//--
		// Row 8
		Vector2 CloudRenderSize;
		Vector2 InvCloudRenderSize;
		//--
		// Row 9
		int   TemporalEnabled;
		float FrameIndex;
		float EarthRadius;
		float PlanetCenterY;
		//--
		// Row 10
		float HorizonFade;    // Multiplier on horizon atmospheric fade (0 = none, 1 = full).
		float DistanceFade;   // Multiplier on distance-based opacity falloff (0 = none, 1 = full).
		int   CloudType;      // Cloud category enum: 0=None, 1=CirrusHigh, 2=AltocumulusMid, 3=StratocumulusLow, 4=CumulonimbusVertical
		float Padding2;
		//--
		// Row 11 — Altocumulus-specific appearance parameters (only used when CloudType == 2)
		float AltoBillowStrength;  // [0,1]      blend toward billow (abs-value) FBM noise
		float AltoCovSoftWidth;    // [0,0.25]   self-referential coverage soft-threshold width
		float AltoAbsorption;      // [0.1,5.0]  absorption coefficient for Altocumulus
		float AltoCloudSize;       // [0.2,5.0]  feature scale multiplier
		//--
		// Row 12 — Altocumulus extended parameters
		float AltoCloudAmount;     // [0.0,1.0]  coverage/fill control
		float AltoCloudBrightness; // [0.1,4.0]  brightness multiplier
		float AltoFbmLacunarity;   // [1.5,4.0]  FBM frequency ratio per octave
		float AltoFbmGain;         // [0.1,0.9]  FBM amplitude scaling per octave
		//--
		// Row 13 — Altocumulus cloud color
		Vector3 AltoCloudColor;    // [0,1] per channel — RGB tint for cloud top/highlights
		float   AltoThickness;     // [50,15000] cloud slab thickness for Altocumulus
		//--
		// Row 14 — Altocumulus dark / shadow color + underside softness
		Vector3 AltoCloudColorDark;// [0,1] per channel — RGB tint for cloud base/shadow
		float   AltoBottomSoftness;// [0,1] 0=flat slab bottom, 1=organic irregular underside
		//--
		// Row 15 — Altocumulus sky-height redistribution
		// 0 = uniform distribution. (+) = more/larger toward horizon. (-) = more/larger toward zenith.
		float   AltoZenithBias;   // [-1,1]  cloud distribution bias
		float   AltoHtPad3;
		float   AltoHtPad4;
		float   AltoHtPad5;
		//--
		// Row 16 — Altocumulus sky-height modulation (continued)
		float   AltoHeightBlendPower;   // [0.25,4]  exponent on the skyHeight ramp
		float   SunElevation;           // sin(pitch): +1 = zenith, 0 = horizon, -1 = nadir.
		float   CloudNightAmbient;      // [0,0.5] minimum ambient when sun is below horizon.
		float   CloudTwilightAmbient;   // [0,1] ambient contribution during twilight.
		//--
		// Row 17 — Lightning enable + frequencies
		int     LightningEnabled;      // 0 or 1
		float   LightningStrikeFreq;   // [0,1]
		float   LightningInternalFreq; // [0,1]
		float   LightningPad;
		//--
		// Row 18 — Lightning speed + glow
		float   LightningSpeed;        // [0.5,10]
		float   LightningInternalSpeed;// [1,20]
		float   LightningGlowIntensity;// [0.5,10]
		float   LightningFlashIntensity;// [0.5,15]
		//--
		// Row 19 — Lightning bolt color + ambient
		Vector3 LightningBoltColor;    // [0,1] per channel
		float   LightningAmbientContrib;// [0,1]
		//--
		// Row 20 — Sun screen-space UV + bolt scale params.
		// Set to (-1,-1) when no global lens flare is active or sun is behind camera.
		Vector2 SunScreenUV;               // projected sun UV in [0,1] x [0,1]
		float   LightningBoltLengthScale;    // [0.1,5] bolt length multiplier
		float   LightningBoltThicknessScale; // [0.1,5] bolt radius multiplier
		//--
		// Row 21 — Atmospheric sun-lighting multipliers (from AtmosphericSkySettings).
		float CloudSunLightIntensity;       // [0,5]   direct sun brightness multiplier.
		float CloudAmbientIntensity;        // [0,2]   ambient light multiplier.
		float CloudSilverliningStrength;    // [0,3]   silverlining / forward-scatter boost.
		float CloudForwardScatterStrength;  // [0,3]   broad HG scatter strength.
		//--
		// Row 22
		float CloudLightAbsorption;         // [0.1,5] Beer-Lambert absorption exponent (standard clouds).
		float CloudSunWarmthInfluence;      // [0,1]   sun color warmth blend.
		float CloudLtPad1;
		float CloudLtPad2;
	};
}
