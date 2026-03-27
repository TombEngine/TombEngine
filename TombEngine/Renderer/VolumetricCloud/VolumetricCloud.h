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
		float CloudBottomHeight = 1536.0f; // World units above camera (matches TEN sky layer offset)
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

		// Fading
		float HorizonFade      = 1.0f;  // Multiplier on horizon atmospheric fade. 0 = no fade, 1 = full fade.
		float DistanceFade     = 1.0f;  // Multiplier on distance-based opacity falloff. 0 = no fade, 1 = full fade.
		float HorizonMeshBleed = 0.0f;  // [0,1] re-composite alpha after horizon mesh (0=no bleed, 1=full bleed).

		// Internal renderer field — not part of saved state.
		// Set by doBleedOverlay to pass bleedStrength into the composite shader.
		// 0.0 = normal pass (HorizonAtmosphericFade applied).
		// >0  = bleed pass  (inverse-fade mask, value carries blendStrength).
		float BleedPassStrength = 0.0f;

		// Cloud type (maps to CloudCategory enum)
		int CloudType = 0;             // 0=None, 1=AltocumulusMid, 2=Aurora

		// Altocumulus-specific appearance tuning (only meaningful for CloudType == 2 / AltocumulusMid)
		float AltoBillowStrength = 0.75f;  // [0,1]      blend toward billow (abs-value) FBM noise
		float AltoCovSoftWidth   = 0.08f;  // [0,0.25]   self-referential coverage soft-threshold width
		float AltoAbsorption      = 1.0f;   // [0.1,5.0] absorption coefficient
		float AltoCloudSize      = 1.0f;   // [0.2,5.0]  feature scale multiplier
		float AltoCloudAmount    = 0.6875f;// [0.0,1.0]  coverage/fill control
		float AltoCloudBrightness = 1.0f;  // [0.1,4.0]  brightness multiplier
		float AltoCloudColorR    = 1.0f;   // [0,1]      color tint red
		float AltoCloudColorG    = 1.0f;   // [0,1]      color tint green
		float AltoCloudColorB    = 1.0f;   // [0,1]      color tint blue
		float AltoFbmLacunarity  = 2.6434f;// [1.5,4.0]  FBM frequency ratio per octave
		float AltoFbmGain        = 0.5f;   // [0.1,0.9]  FBM amplitude scaling per octave
		float AltoThickness      = 1800.0f;// [50,15000] cloud slab thickness
		float AltoCloudColorDarkR = 0.55f; // [0,1]      dark/shadow color tint red
		float AltoCloudColorDarkG = 0.55f; // [0,1]      dark/shadow color tint green
		float AltoCloudColorDarkB = 0.65f; // [0,1]      dark/shadow color tint blue (slightly cool)
		float AltoBottomSoftness  = 0.35f; // [0,1]      0=flat bottom, 1=organic underside

		// Altocumulus sky-height redistribution (only meaningful for CloudType == 2)
		// 0 = uniform. (+) = more/larger toward horizon. (-) = more/larger toward zenith.
		float AltoZenithBias       = 0.0f; // [-1,1]  cloud distribution bias
		float AltoHeightBlendPower = 1.0f; // [0.25,4] exponent on the skyHeight ramp
		float AltoHorizonWidth     = 0.0f; // [0,1]   0=wide (to near horizon), 1=zenith-only cap
		float AltoBleedDepth       = 30.0f; // [0,100] bleed clouds depth (0.01*val*CloudBottomHeight)

		// Compositor hybrid-blend thresholds.
		float BlendThresholdHigh      = 0.85f;   // [0,1]     bright cutoff → screen blend above this
		float BlendThresholdHighWidth = 0.05f;   // [0.005,0.4] half-width of bright→alpha smoothstep
		float BlendThresholdLow       = 0.106f;  // [0,1]     dark  cutoff → screen blend below this

		// Lightning
		int   LightningEnabled     = 0;
		float LightningStrikeFreq  = 0.1f;
		float LightningInternalFreq = 0.5f;
		float LightningSpeed       = 2.5f;
		float LightningInternalSpeed = 5.0f;
		float LightningGlowIntensity = 3.0f;
		float LightningBoltColorR  = 0.3f;
		float LightningBoltColorG  = 0.6f;
		float LightningBoltColorB  = 1.0f;
		float LightningFlashIntensity = 4.0f;
		float LightningAmbientContrib = 0.15f;
		float LightningBoltLengthScale    = 1.0f;
		float LightningBoltThicknessScale = 1.0f;

		// Drift-out: wind-directional dissolution progress [0,1].
		// 0 = normal rendering, 1 = fully dissolved.
		float DriftOutProgress = 0.0f;

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
		float AccumulatedTime  = 0.0f;  // Global time for evolution (EvolutionSpeed pulsing)
		float WindAccumOffset  = 0.0f;  // Pre-integrated wind offset — monotonically non-decreasing.
		                                 // Prevents backwards cloud motion when WindSpeed transitions
		                                 // to a lower value (avoids the CloudTime*WindSpeed artifact).
		int   FrameCounter     = 0;     // Monotonic frame counter for jitter cycling

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
		// Always use High quality regardless of the configured preset.
		preset = CloudQualityPreset::High;

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
