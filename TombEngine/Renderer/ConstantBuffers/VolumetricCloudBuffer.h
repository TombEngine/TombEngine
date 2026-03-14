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
	};
}
