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
		// Row 7 — Sun elevation color ramp
		float   SunElevationRampSpeed; // Controls how quickly warm tint fades as sun rises.
		float   SunWarmInfluence;      // Max blend weight toward sun color at horizon.
		float   _Pad0;                 // Padding for 16-byte alignment.
		float   _Pad1;
	};
}
