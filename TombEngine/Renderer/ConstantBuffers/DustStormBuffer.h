#pragma once

#include <SimpleMath.h>

// ============================================================================
// DustStormBuffer.h - GPU constant buffer for the volumetric dust storm pass.
//
// Bound to register b10. The dust pass runs after all geometry / gun flashes
// and before HUD / glow, so reusing the Hud slot is safe at this stage of the
// frame (same convention as the god ray pass).
// Must match CBDustStorm.hlsli layout exactly.
// ============================================================================

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	constexpr auto DUST_STORM_MAX_OUTDOOR_ROOMS = 32;

	struct alignas(16) CDustStormBuffer
	{
		// Row 0 - color and overall density
		Vector3 Color;
		float   Density;
		//--
		// Row 1 - vertical clamping (world units) + time
		float MinHeight;       // World Y at which dust begins (Y-down: larger = lower).
		float MaxHeight;       // World Y at which dust fades out (smaller = higher).
		float Time;            // Seconds since level start, drives advection.
		float TurbulenceScale; // Noise frequency scale.
		//--
		// Row 2 - wind
		Vector2 WindDirection;  // Normalized world-space (XZ) wind direction.
		float   WindSpeed;      // World units per second (already scaled).
		float   StepCount;      // Raymarch steps (float to ease lerp / LOD).
		//--
		// Row 3 - viewport
		Vector2 ViewSize;
		Vector2 InvViewSize;
		//--
		// Row 4 - camera position + far plane
		Vector3 CameraPos;
		float   FarPlane;
		//--
		// Row 5 - light direction + base step distance
		Vector3 LightDirection; // World-space sun direction (points away from camera toward sun).
		float   BaseStepDist;   // First raymarch step length in world units.
		//--
		// Row 6 - light color + ambient
		Vector3 LightColor;
		float   AmbientStrength;
		//--
		// Row 7 - engine fog blending
		Vector3 FogColor;
		float   FogStartDistance;
		//--
		// Row 8 - extras
		float FogEndDistance;
		float StepGrowth;       // Geometric step ratio (1.8 in reference).
		float IntensityFade;    // [0,1] global fade (e.g. for level transitions).
		float GustMode;         // 1.0 = gust mode, 0.0 = continuous fog.
		//--
		// Rows 9-12 - inverse view-projection for world-pos reconstruction
		Matrix InvViewProjection;
		//--
		// Outdoor room volumes visible in the current view.
		Vector4 OutdoorRoomMins[DUST_STORM_MAX_OUTDOOR_ROOMS];
		Vector4 OutdoorRoomMaxs[DUST_STORM_MAX_OUTDOOR_ROOMS];
		float   OutdoorRoomCount;
		Vector3 Pad1;
	};
}
