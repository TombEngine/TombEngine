#pragma once

// ============================================================================
// AtmosphericSkySettings.h — Runtime settings for the atmospheric sky dome.
//
// These parameters control the visual appearance of the atmospheric scattering
// sky dome. They are used to fill the CAtmosphericSkyBuffer constant buffer
// each frame. All values have sensible defaults matching the reference shader.
// ============================================================================

namespace TEN::Renderer
{
	struct AtmosphericSkySettings
	{
		// --- Enable ---
		bool Enabled = false; // When false, the legacy bitmap sky is used instead.

		// --- Scattering parameters (adapted from reference shader) ---
		float SkyColorR              = 0.065f;   // [0,1]   Base sky color R (Rayleigh tint).
		float SkyColorG              = 0.145f;   // [0,1]   Base sky color G.
		float SkyColorB              = 0.422f;   // [0,1]   Base sky color B.
		float Density                = 0.830f;   // [0.1,3] Atmospheric density factor.
		float ZenithOffset           = 0.039f;   // [0,0.5] Vertical offset for zenith density.
		float MultiScatterPhase      = 0.902f;   // [0,1]   Multi-scatter sun elevation influence.
		float AnisotropicIntensity   = 0.00f;   // [0,2]   Mie anisotropic scattering strength.

		// --- Glow and brightness ---
		float MieIntensity           = 0.00f;   // [0,5]   Mie glow disk intensity around sun.
		float RayleighIntensity      = 0.00f;   // [0,5]   Rayleigh brightness multiplier.
		float SunGlowIntensity       = 0.217f;   // [0,10]  Broad sun glow field intensity.
		float HorizonDarkeningStr    = 2.361f;   // [0.1,5] Horizon darkening exponent.
		float ExposureMultiplier     = 2.032f;   // [0.1,5] Tone mapping exposure control.

		// --- Night sky ---
		float NightSkyBrightness     = 5.0f;   // [0,5]   Night sky base brightness.
		float TwilightOffset         = 0.0648f;   // [0,0.3] Sun elevation where twilight starts (radians).
		float NightBlendSpeed        = 6.29f;   // [1,20]  How quickly night blends in.

		// --- Cloud sun-lighting integration ---
		float CloudSunLightIntensity       = 1.00f;   // [0,5]   Direct sun light on clouds.
		float CloudAmbientIntensity        = 0.0f;   // [0,2]   Ambient light on clouds.
		float CloudSilverliningStrength    = 0.40f;   // [0,3]   Forward scattering / silverlining.
		float CloudForwardScatterStrength  = 0.60f;   // [0,3]   Broad forward scatter (HG-like).
		float CloudLightAbsorption         = 1.10f;   // [0.1,5] Beer-Lambert absorption coefficient.
		float CloudSunWarmthInfluence      = 0.30f;   // [0,1]   How much sun color tints clouds.
		float CloudTwilightAmbient         = 0.432f;   // [0,1]   Twilight ambient contribution.
		float CloudNightAmbient            = 0.0f;   // [0,0.5] Night ambient minimum.

		// --- Sun elevation color ramp ---
		// Controls the "2-color mode" gradient: white (zenith) <-> warm sun color (horizon).
		//   SunElevationRampSpeed: how quickly the warm tint fades as the sun rises.
		//     1.0 = tint gone at sunY=1.0 (90°), 2.0 = tint gone at sunY=0.5 (30°).
		//   SunWarmInfluence: max blend weight toward AtmoSunColor at horizon (0=always white, 1=full color).
		float SunElevationRampSpeed  = 5.0f;   // [0.1,5] Ramp speed: higher = white zone starts sooner.
		float SunWarmInfluence       = 1.0f;   // [0,1]   Max warmth at sun color at horizon.

		// --- Shader sun disk ---
		// Rendered inside the atmospheric scattering pass so the bottom half is
		// darkened by the same horizon band — giving a natural half-set appearance.
		float SunDiskSize      = 1.62f;    // [0.1,10] Apparent half-angle in degrees.
		float SunDiskIntensity = 103.6f;   // [1,200]  Brightness before tone mapping.
	};
}
