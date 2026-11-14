#pragma once

// ============================================================================
// DustStormSettings.h - Runtime settings for the volumetric dust storm pass.
//
// The dust storm is a screen-space raymarched volumetric effect that fills the
// outdoor air with a wind-driven sand layer. It is rendered after all opaque
// and transparent geometry so that the depth buffer can clamp marching to the
// scene, and is gated by camera-room outdoor status to match rain / snow.
// All user-facing parameters live here; everything else is internal.
// ============================================================================

namespace TEN::Renderer::DustStorm
{
	struct DustStormSettings
	{
		bool Enabled = false;

		// User-facing controls (debug sliders).
		float Density   = 0.65f;   // [0,2]   overall opacity multiplier.
		float MinHeight = 0.0f;    // [0,1]   normalized base height (0 = ground).
		float MaxHeight = 0.65f;   // [0,1]   normalized cap height (1 = top of weather column).
		float ColorR    = 0.85f;
		float ColorG    = 0.65f;
		float ColorB    = 0.50f;

		// Tuning (kept simple - exposed only if needed).
		float WindSpeedScale = 1.0f;   // [0,4]   scales coupling to base wind strength.
		float Turbulence     = 1.0f;   // [0,2]   noise intensity multiplier.
		int   StepCount      = 7;      // [3,12]  raymarch step count (engine LOD reduces this at distance).
	};
}
