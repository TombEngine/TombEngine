#pragma once

#include <SimpleMath.h>

namespace TEN::Renderer::VolumetricCloud
{
	using namespace DirectX::SimpleMath;

	// ========================================================================
	// Quality presets
	// ========================================================================

	enum class CloudQualityPreset
	{
		Low,
		Medium,
		High,
		Count
	};

	// ========================================================================
	// Debug visualization modes
	// ========================================================================

	enum class CloudDebugView
	{
		None,
		CoverageMask,          // Grayscale coverage noise
		DensityField,          // Cloud density before lighting
		StepCountHeatmap,      // Number of march steps taken
		TransmittanceOnly,     // Raw transmittance output
		LensFlareOcclusion,    // Flare visibility factor
		NoDetailNoise,         // Shape noise only, no erosion
		Count
	};

	// ========================================================================
	// Cloud layer mode — backward-compatible with old SkyLayer
	// ========================================================================

	enum class CloudLayerMode
	{
		LegacyBitmap,  // Old flat scrolling layer (default)
		Volumetric     // New volumetric cloud renderer
	};

	// ========================================================================
	// Per-quality-preset parameters
	// ========================================================================

	struct CloudQualityParams
	{
		int   PrimaryStepCount        = 12;
		int   ShadowStepCount         = 4;
		int   OcclusionSampleSteps    = 6;
		float RenderResolutionScale   = 0.5f;    // 0.5 = half-res
		bool  DetailNoiseEnabled      = true;
		bool  TemporalReprojection    = true;
		bool  BlueNoiseJitter         = true;
	};

	// ========================================================================
	// Cloud noise parameters (procedural only — no 3D textures)
	// ========================================================================

	struct CloudNoiseParams
	{
		float ShapeScale     = 0.00008f;  // Low-frequency FBM scale
		float DetailScale    = 0.0008f;   // High-frequency erosion scale
		float DetailStrength = 0.35f;     // How much detail erodes shape
		float WeatherScale   = 0.00002f;  // Scale for coverage weather map
	};

	// ========================================================================
	// Main cloud render settings — tunable from Lua
	// ========================================================================

	struct CloudRenderSettings
	{
		bool           Enabled            = false;
		CloudLayerMode Mode               = CloudLayerMode::LegacyBitmap;

		// Atmosphere
		float Coverage          = 0.55f;   // [0,1] — global cloud coverage
		float Density           = 0.8f;    // Density multiplier
		float CloudBottomHeight = 8000.0f; // World units above camera horizon
		float CloudThickness    = 2500.0f; // Vertical extent of cloud slab

		// Wind & evolution
		Vector2 WindDirection   = Vector2(1.0f, 0.0f);
		float   WindSpeed       = 0.003f;
		float   EvolutionSpeed  = 0.15f;

		// Noise
		CloudNoiseParams Noise = {};

		// Lighting
		float Absorption        = 1.1f;    // Beer-Lambert extinction
		float AmbientContrib    = 0.35f;   // Sky ambient added to cloud base
		float SilverliningStr   = 0.4f;    // Forward scattering / powder
		float PhaseForward      = 0.6f;    // HG phase function forward lobe
		float PhaseBackward     = 0.3f;    // HG phase function backward lobe

		// Quality
		CloudQualityPreset Quality  = CloudQualityPreset::Medium;
		float JitterStrength        = 1.0f;

		// Override for direct light direction (if no lens flare is set).
		// If all zero, derive from existing lens flare orientation.
		Vector3 LightDirection = Vector3::Zero;
	};

	// ========================================================================
	// Lens flare occlusion state
	// ========================================================================

	struct LensFlareCloudOcclusionState
	{
		float CloudTransmittance   = 1.0f;  // [0,1] — 1 = fully visible
		float SmoothedTransmittance = 1.0f; // Temporally smoothed value
		int   CacheValidFrames     = 0;     // Frames since last recalculation
	};

	// ========================================================================
	// Cloud runtime state — maintained per frame by the renderer
	// ========================================================================

	struct CloudRuntimeState
	{
		float AccumulatedTime = 0.0f;  // Global time for wind/evolution
		int   FrameCounter    = 0;     // Monotonic frame counter for jitter cycling

		// Current packed quality params
		CloudQualityParams ActiveQuality = {};

		// Lens flare occlusion
		LensFlareCloudOcclusionState FlareOcclusion = {};

		// Debug
		CloudDebugView DebugView   = CloudDebugView::None;
		bool FreezeEvolution       = false;
		bool FreezeWind            = false;
		bool ShowVolumeBounds      = false;
	};

	// ========================================================================
	// Quality preset table
	// ========================================================================

	inline CloudQualityParams GetQualityParams(CloudQualityPreset preset)
	{
		switch (preset)
		{
		case CloudQualityPreset::Low:
			return CloudQualityParams{
				/*PrimaryStepCount=*/    8,
				/*ShadowStepCount=*/     0,   // No shadow march on Low
				/*OcclusionSampleSteps=*/4,
				/*RenderResolutionScale=*/0.25f,
				/*DetailNoiseEnabled=*/  false,
				/*TemporalReprojection=*/false,
				/*BlueNoiseJitter=*/     true
			};

		case CloudQualityPreset::High:
			return CloudQualityParams{
				/*PrimaryStepCount=*/    24,
				/*ShadowStepCount=*/     6,
				/*OcclusionSampleSteps=*/8,
				/*RenderResolutionScale=*/0.5f,
				/*DetailNoiseEnabled=*/  true,
				/*TemporalReprojection=*/true,
				/*BlueNoiseJitter=*/     true
			};

		case CloudQualityPreset::Medium:
		default:
			return CloudQualityParams{
				/*PrimaryStepCount=*/    12,
				/*ShadowStepCount=*/     4,
				/*OcclusionSampleSteps=*/6,
				/*RenderResolutionScale=*/0.5f,
				/*DetailNoiseEnabled=*/  true,
				/*TemporalReprojection=*/true,
				/*BlueNoiseJitter=*/     true
			};
		}
	}
}
