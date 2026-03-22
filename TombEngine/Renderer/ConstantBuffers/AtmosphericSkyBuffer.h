#pragma once

#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	// Must match CBAtmosphericSky.hlsli layout exactly.
	// Bound to register b14.
	struct alignas(16) CAtmosphericSkyBuffer
	{
		// Row 0 — Sun direction and elevation
		Vector3 SunDirection;      // Normalized world-space sun direction (from lens flare system).
		float   SunElevation;      // sin(pitch): +1 = zenith, 0 = horizon, -1 = nadir.
		//--
		// Row 1 — Sun color from lens flare system
		Vector3 SunColor;          // Effective sun color (auto-realistic or user-set).
		float   DayNightBlend;     // [0,1] 0 = full day, 1 = full night.
		//--
		// Row 2 — Sky base color and density
		Vector3 SkyColor;          // Base sky color (Rayleigh-like tint).
		float   Density;           // Atmospheric density factor.
		//--
		// Row 3 — Scattering parameters
		float   ZenithOffset;      // Vertical offset for zenith density calculation.
		float   MultiScatterPhase; // Multi-scatter distance influence on sun absorption.
		float   AnisotropicIntensity; // Mie anisotropic scattering intensity.
		float   MieIntensity;      // Mie glow intensity multiplier.
		//--
		// Row 4 — Rayleigh and glow
		float   RayleighIntensity; // Rayleigh brightness multiplier.
		float   SunGlowIntensity;  // Sun glow field intensity.
		float   HorizonDarkeningStr; // Horizon darkening strength.
		float   ExposureMultiplier;  // Tone mapping exposure control.
		//--
		// Row 5 — Night sky parameters
		float   NightSkyBrightness;  // Base brightness of night sky background.
		float   StarfieldVisibility; // [0,1] computed starfield fade factor.
		float   TwilightOffset;      // Sun elevation angle where twilight begins (radians).
		float   NightBlendSpeed;     // How quickly night blends in as sun descends.
		//--
		// Row 6 — Viewport info
		Vector2 ViewSize;            // Render target size.
		Vector2 InvViewSize;         // 1.0 / ViewSize.
		//--
		// Row 7 — Sun elevation color ramp + shader sun disk
		float   SunElevationRampSpeed; // Controls how quickly warm tint fades as sun rises.
		float   SunWarmInfluence;      // Max blend weight toward sun color at horizon.
		float   SunDiskCosRadius;      // cos(half_angle): precomputed for sun disk threshold.
		float   SunDiskIntensity;      // Sun disk brightness multiplier before tone mapping.
		//--
		// Row 8 — Moon direction and elevation
		Vector3 MoonDirection;         // Normalized world-space moon direction.
		float   MoonElevation;         // sin(pitch): +1 = zenith, 0 = horizon, -1 = nadir.
		//--
		// Row 9 — Moon color and phase
		Vector3 MoonColor;             // Moon surface color (tinted by sun illumination).
		float   MoonPhase;             // [0,1] 0 = new moon, 0.5 = full moon, 1 = new moon (cycle).
		//--
		// Row 10 — Moon disk and glow
		float   MoonDiskCosRadius;     // cos(half_angle): precomputed for moon disk threshold.
		float   MoonDiskIntensity;     // Moon disk brightness before tone mapping.
		float   MoonGlowIntensity;     // Halo/glow brightness around moon in sky.
		float   MoonGlowFalloff;       // How quickly glow fades from moon center.
		//--
		// Row 11 — Moon enable/visibility + phase illumination
		float   MoonEnabled;           // 0 or 1.
		float   MoonPhaseBrightness;   // [0,1] computed brightness from phase (full=1, new=0).
		float   MoonVisibility;        // [0,1] computed visibility (fades in as sky darkens).
		float   MoonPad0;
	};
}
