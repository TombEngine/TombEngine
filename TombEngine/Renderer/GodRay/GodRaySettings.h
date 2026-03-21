#pragma once

// ============================================================================
// GodRaySettings.h — Runtime settings for lightweight screen-space god rays.
//
// God rays are driven automatically by the existing sun (lens flare) position
// and volumetric cloud occlusion.  Only a small number of user-facing controls
// are exposed; everything else self-adjusts each frame.
// ============================================================================

namespace TEN::Renderer::GodRay
{
	struct GodRaySettings
	{
		bool  Enabled         = true;

		// --- User-facing controls (debug sliders) ---
		float Length           = 0.5f;     // [0.05, 1.5]  max ray reach in UV space.
		float Intensity        = 0.8f;     // [0.0,  3.0]  overall brightness multiplier.
		float Decay            = 0.97f;    // [0.90, 1.0]  per-sample exponential decay.
		int   SampleCount      = 48;       // [16,   128]  radial sample count.
		float Softness         = 1.0f;     // [0.1,  3.0]  sun-glow source falloff scale.
		float AutoStrengthMix  = 1.0f;     // [0.0,  1.0]  blend between manual (0) and auto (1) strength.
	};
}
