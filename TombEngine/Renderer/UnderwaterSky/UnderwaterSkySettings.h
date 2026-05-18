#pragma once

// ============================================================================
// UnderwaterSkySettings.h - Runtime settings for the Underwater Sky effect.
//
// When active, the atmospheric sky dome is replaced by a stylized underwater
// surface as seen from below: animated wave caustics overhead, dark void
// pit below the water-line, and god-ray-like light shafts at the sun
// direction. Mutually exclusive with the Aurora preset (both occupy
// Cloud Layer A in the SkyCloudSystem).
//
// All values are updated live via the debug menu and sent to the GPU via
// the atmospheric sky constant buffer.
// ============================================================================

namespace TEN::Renderer::UnderwaterSky
{
	// ====================================================================
	// Underwater sky settings
	// ====================================================================

	struct UnderwaterSkySettings
	{
		// --- Core ---
		bool  Enabled              = false;
		float Intensity            = 1.0f;    // [0, 3]      Overall brightness multiplier.

		// --- Wave appearance ---
		float WaveSize             = 1.0f;    // [0.1, 5]    Spatial scale of wave caustics.
		float WaveSpeed            = 0.4f;    // [0, 3]      Animation drift speed.
		float WaveSharpness        = 2.5f;    // [0.5, 8]    Contrast of caustic ridges.
		float DistortionAmount     = 2.2f;    // [0.5, 8]    Frequency of UV distortion details (higher = tighter/denser).
		float DistortionStrength   = 1.0f;    // [0, 2]      Distortion amplitude multiplier.

		// --- Color (default cyan/teal) ---
		float ColorR               = 0.10f;   // [0, 1]
		float ColorG               = 0.45f;   // [0, 1]
		float ColorB               = 0.65f;   // [0, 1]

		// --- Geometry ---
		float LayerHeight          = 0.35f;   // [0.05, 1.0] Hemisphere fraction where water-line sits (0 = horizon, 1 = zenith).
		float HorizonSoftness      = 0.20f;   // [0.01, 1.0] Fade width around the water-line.
		float DepthFadeStrength    = 1.3f;    // [0.1, 4.0]  How quickly looking down fades into the pit void.

		// --- Light shafts (god rays from the sun direction) ---
		float CausticStrength      = 1.8f;    // [0, 4]      Brightness of bright caustic peaks.
		float ShaftStrength         = 1.2f;   // [0, 4]      Brightness of god-ray light shafts piercing the surface.
		float ShaftSharpness       = 12.0f;   // [1, 64]     Tightness of the cone around the sun direction.

		// --- Underwater god ray appearance (independent from normal god ray settings) ---
		float RayLength            = 0.65f;   // [0.05, 1.5] March reach in UV space.
		float RayDecay             = 0.966f;  // [0.90, 1.0] Per-sample exponential decay.
		float RayIntensity         = 2.5f;    // [0.0, 8.0]  Final brightness multiplier.
		int   RaySampleCount       = 48;      // [8, 128]    Radial sample count.

		// --- Night damping ---
		float NightDarken          = 0.85f;   // [0, 1]      How much to darken the whole effect at full night.
	};
}
