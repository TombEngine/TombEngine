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
		float   AtmoSkyEnabled;        // 0 = sky dome disabled (suppress sun disc in moon-only pass), 1 = enabled.

		// Row 12 — Aurora core parameters
		float   AuroraEnabled;         // 0 or 1.
		float   AuroraIntensity;       // Overall brightness multiplier.
		float   AuroraBrightness;      // Base brightness of aurora bands.
		float   AuroraHeight;          // Sky-space height position.
		//--
		// Row 13 — Aurora shape parameters
		float   AuroraSpread;          // Horizontal spread across sky.
		float   AuroraSpeed;           // Animation drift speed.
		float   AuroraBandSharpness;   // Sharpness of individual bands.
		float   AuroraNoiseScale;      // Scale of noise pattern.
		//--
		// Row 14 — Aurora shape continued
		float   AuroraVerticalStretch; // Vertical elongation of curtains.
		float   AuroraDistortionStr;   // Noise distortion / wave warping.
		float   AuroraLayerCount;      // Number of overlapping bands (float for shader).
		float   AuroraSoftness;        // Overall softness of effect.
		//--
		// Row 15 — Aurora color parameters
		float   AuroraColorPreset;     // Color preset index (float for interpolation).
		float   AuroraColorIntensity;  // Color vibrancy multiplier.
		float   AuroraSaturation;      // Color saturation.
		float   AuroraVisibility;      // [0,1] computed night visibility factor.
		//--
		// Row 16 — Aurora visibility parameters
		float   AuroraNightFadeThreshold; // Sun elevation below which aurora appears.
		float   AuroraHorizonFade;        // How quickly aurora fades near horizon.
		float   AuroraSunSuppressionStr;  // How strongly sunlight suppresses aurora.
		float   AuroraTime;               // Accumulated animation time.
		//--
		// Row 17 — Cloud occlusion of sun disc + horizon ground color
		float   CloudDiscOcclusion;       // [0,1] 1 = fully occluded, 0 = clear. Suppresses AtmoSunDisk.
		float   HorizonColorR;            // [0,1] Red channel of horizon ground color.
		float   HorizonColorG;            // [0,1] Green channel of horizon ground color.
		float   HorizonColorB;            // [0,1] Blue channel of horizon ground color.
		//--
		// Row 18 — Underwater sky core
		float   UnderwaterSkyEnabled;     // 0 or 1.
		float   UnderwaterSkyVisibility;  // [0,1] computed fade (preset fade x night damping).
		float   UnderwaterTime;           // Accumulated animation time.
		float   UnderwaterIntensity;      // Overall brightness multiplier.
		//--
		// Row 19 — Underwater wave/color
		float   UnderwaterWaveSize;       // Spatial scale of wave caustics.
		float   UnderwaterWaveSpeed;      // Animation drift speed.
		float   UnderwaterWaveSharpness;  // Contrast of caustic ridges.
		float   UnderwaterColorR;         // Water tint color (R).
		//--
		// Row 20 — Underwater color continued + geometry
		float   UnderwaterColorG;         // Water tint color (G).
		float   UnderwaterColorB;         // Water tint color (B).
		float   UnderwaterLayerHeight;    // [0,1] hemisphere fraction where water-line sits.
		float   UnderwaterHorizonSoftness;// Fade width around the water-line.
		//--
		// Row 21 — Underwater depth/shafts
		float   UnderwaterDepthFadeStr;   // How quickly looking down fades into pit void.
		float   UnderwaterCausticStrength;// Brightness of caustic peaks.
		float   UnderwaterShaftStrength;  // Brightness of god-ray shafts.
		float   UnderwaterShaftSharpness; // Tightness of cone around sun direction.
		//--
		// Row 22 — Underwater distortion + wind advection
		float   UnderwaterDistortionAmount;   // Distortion detail frequency.
		float   UnderwaterDistortionStrength; // Distortion amplitude multiplier.
		float   UnderwaterWindDirX;           // Normalized X drift direction.
		float   UnderwaterWindDirY;           // Normalized Y drift direction.
		//--
		// Row 23 — Per-quality level
		float   SkyQualityLevel;              // 0=Low, 1=Medium, 2=High.
		float   _PadRow23_1 = 0.0f;
		float   _PadRow23_2 = 0.0f;
		float   _PadRow23_3 = 0.0f;
	};
}
