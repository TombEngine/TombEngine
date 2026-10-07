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
		int   CameraIsOutdoor;  // 1 when camera room has ENV_FLAG_SKYBOX; 0 otherwise.
		//--
		// Row 9 - screen-space wind direction (kept for padding / future use)
		Vector2 WindScreenDir;
		Vector2 _windScreenPad;
		//--
		// Rows 10-13 - inverse view-projection for world-pos reconstruction
		Matrix InvViewProjection;
		//--
		// Rows 14-17 - forward view-projection for world -> UV reprojection (bleed march)
		Matrix ViewProjection;
		//--
		// Row 18 - world-space portal bleed parameters
		float BleedTopDepthRatio;
		float BleedEdgeFadeStart;
		float BleedDepthFadeStart;
		int   NumBleedVolumes;
		//--
		// Rows 19-22 - center.xyz + strength per bleed volume
		Vector4 BleedVolumeCenterAndStrength[4];
		// Rows 23-26 - inverse basis row 0 per bleed volume
		Vector4 BleedVolumeInvBasis0[4];
		// Rows 27-30 - inverse basis row 1 per bleed volume
		Vector4 BleedVolumeInvBasis1[4];
		// Rows 31-34 - inverse basis row 2 per bleed volume
		Vector4 BleedVolumeInvBasis2[4];
	};
}
