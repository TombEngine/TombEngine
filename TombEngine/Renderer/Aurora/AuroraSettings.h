#pragma once

// ============================================================================
// AuroraSettings.h — Runtime settings for the Aurora Borealis sky effect.
//
// Controls the appearance of the procedural aurora (northern lights) rendered
// as part of the atmospheric sky dome. The aurora is only visible at night or
// during twilight, and fades out when the sun is above the horizon.
//
// All values are updated live via the debug menu and sent to the GPU via
// the atmospheric sky constant buffer.
// ============================================================================

namespace TEN::Renderer::Aurora
{
	// ====================================================================
	// Aurora color preset indices
	// ====================================================================

	enum class AuroraColorPreset
	{
		GreenClassic,       // Green dominant (classic aurora)
		GreenPurple,        // Green + purple
		GreenRedTips,       // Green + red tips at top
		BluePurple,         // Blue/purple (rare aurora)
		StrongMulticolor,   // Full multicolor display
		TurquoiseBluePurple,// Turquoise bottom, blue mid, purple top

		Count
	};

	// ====================================================================
	// Aurora settings
	// ====================================================================

	struct AuroraSettings
	{
		// --- Core ---
		bool  Enabled              = false;
		float Intensity            = 1.0f;     // [0, 3]     Overall brightness multiplier.
		float Brightness           = 1.5f;     // [0, 5]     Base brightness of aurora bands.
		float Height               = 0.45f;    // [0.05, 1.0]  Sky-layer ceiling (fraction of hemisphere: 0 = horizon, 1 = zenith).
		float Spread               = 0.6f;     // [0.1, 1]   Horizontal noise spread across the dome.
		float Speed                = 0.679f;   // [0, 2]     Animation drift speed.

		// --- Color ---
		int   ColorPreset          = 0;        // [0, 5]     AuroraColorPreset index.
		float ColorIntensity       = 1.0f;     // [0, 3]     Color saturation/vibrancy multiplier.
		float Saturation           = 1.0f;     // [0, 2]     Color saturation (1 = normal, 0 = grayscale).

		// --- Shape ---
		float BandSharpness        = 2.0f;     // [0.5, 8]   Sharpness of individual aurora bands.
		float NoiseScale           = 1.0f;     // [0.1, 5]   Scale of the noise pattern.
		float VerticalStretch      = 3.0f;     // [0.5, 10]  Vertical elongation of aurora curtains.
		float DistortionStrength   = 0.3f;     // [0, 1]     Noise distortion / wave warping.

		// --- Visibility ---
		float NightFadeThreshold   = 0.1f;     // [0, 0.5]   Sun elevation below which aurora starts appearing.
		float HorizonFade          = 1.0f;     // [0, 2]     How quickly aurora fades near horizon.
		float SunSuppressionStr    = 8.0f;     // [1, 20]    How strongly sunlight suppresses aurora.

		// --- Advanced ---
		int   LayerCount           = 3;        // [1, 5]     Number of overlapping aurora bands.
		float Softness             = 0.5f;     // [0, 1]     Overall softness / blur of the effect.
	};
}
