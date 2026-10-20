#pragma once

#include "Math/SimpleMath.h"

// ============================================================================
// GodRayBuffer.h - Uniform block for the god ray shader pass.
//
// Filled by UpdateGodRayBuffer, copied into Backend::GodRayDraw and pushed by
// the scene renderer as pixel uniforms of the god ray pass.
// Must match CBGodRay.hlsli layout exactly.
// ============================================================================

namespace TEN::Renderer::ConstantBuffers
{
	using namespace TEN::Math::Library;

	struct alignas(16) CGodRayBuffer
	{
		// Row 0 - Sun position and ray parameters
		Vector2 SunScreenPos;     // Projected sun UV in [0,1] x [0,1].
		float   RayLength;        // Max radial reach in UV space.
		float   Intensity;        // Overall brightness multiplier.
		//--
		// Row 1 - Sampling and automatic strength
		float   Decay;            // Per-sample exponential decay.
		int     SampleCount;      // Number of radial samples.
		float   SunElevation;     // sin(pitch): +1 = zenith, 0 = horizon, -1 = nadir.
		float   AutoStrength;     // Computed per-frame automatic strength [0,1].
		//--
		// Row 2 - Sun color and softness
		Vector3 SunColor;         // Sun color for tinting rays.
		float   Softness;         // Sun-glow source falloff multiplier.
		//--
		// Row 3 - View info
		Vector2 ViewSize;         // Full screen resolution.
		Vector2 InvViewSize;      // 1.0 / ViewSize.
		//--
		// Row 4 - Underwater shaft mode
		float   UnderwaterShaftActive;     // 0 = off, 1 = on. Switches sample march to procedural caustic mask.
		float   UnderwaterShaftBrightness; // Overall multiplier for procedural mask (visibility * ShaftStrength).
		float   UnderwaterShaftTime;       // Accumulated time for wave drift.
		float   UnderwaterShaftSharpness;  // Mask power (1 = soft, 4+ = thin shafts).
		//--
		// Row 5 - Underwater wind drift direction and ray params
		float   UnderwaterWindX;           // Normalized X wind component (same as atmospheric sky uniforms).
		float   UnderwaterWindY;           // Normalized Y wind component.
		float   UnderwaterRayLength;       // Underwater-specific march reach in UV space.
		float   UnderwaterRayDecay;        // Underwater-specific per-sample exponential decay.
		//--
		// Row 6 - Underwater ray appearance
		float   UnderwaterRayIntensity;    // Final brightness multiplier for underwater god rays.
		float   _pad5;
		float   _pad6;
		int     UnderwaterSampleCount;     // Radial sample count for the underwater march.
	};
}
