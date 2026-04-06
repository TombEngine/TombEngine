#pragma once

// ============================================================================
// MoonSettings.h — Runtime settings for the moon system.
//
// Controls moon appearance, position, glow, and moon-specific god rays.
// The moon phase is computed automatically from the sun-moon angular
// relationship — no manual texture swap needed.
// ============================================================================

namespace TEN::Renderer::Moon
{
	struct MoonGodRaySettings
	{
		bool  Enabled      = true;

		float Length        = 0.242f;    // [0.05, 1.5]  Max ray reach in UV space.
		float Intensity    = 0.005f;    // [0.0, 1.0]   Overall brightness (dimmer than sun).
		float Decay        = 0.9593f;    // [0.90, 1.0]   Per-sample exponential decay.
		int   SampleCount  = 43;       // [16, 128]    Radial sample count.
		float Softness     = 3.0f;     // [0.1, 3.0]    Moon-glow source falloff.
		float AutoStrength = 1.0f;     // [0.0, 1.0]   Blend between manual and auto strength.
	};

	struct MoonSettings
	{
		// --- Enable ---
		bool Enabled = true;

		// --- Position (degrees, same convention as sun) ---
		float Pitch = 45.0f;   // [-10, 90]   Elevation angle (90 = zenith, 0 = horizon).
		float Yaw   = 180.0f;  // [0, 360]    Compass direction.

		// --- Appearance ---
		float DiskSize       = 1.4f;    // [0.1, 10]   Apparent half-angle in degrees.
		float DiskIntensity  = 8.0f;    // [1, 100]    Brightness before tone mapping.
		float BaseColorR     = 0.75f;   // [0, 1]      Moon surface base color R.
		float BaseColorG     = 0.80f;   // [0, 1]      Moon surface base color G.
		float BaseColorB     = 0.90f;   // [0, 1]      Moon surface base color B.

		// --- Moon glow (local sky illumination) ---
		float GlowIntensity  = 0.162f;   // [0, 2]      Halo/glow brightness around moon.
		float GlowFalloff    = 100.0f;    // [1, 100]    How quickly glow fades from moon center.

		// --- Moonlight strength for clouds ---
		float CloudLightIntensity = 0.089f;  // [0, 2]   Direct moonlight on clouds at night.
		float CloudAmbientBoost   = 0.285f;  // [0, 0.5] Additional ambient for moonlit clouds.

		// --- Moon god rays ---
		MoonGodRaySettings GodRays;
	};
}
