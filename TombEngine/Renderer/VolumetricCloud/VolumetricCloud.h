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
		float RenderResolutionScale   = 0.25f;   // 0.25 = quarter-res
		bool  DetailNoiseEnabled      = true;
		bool  TemporalReprojection    = true;
		bool  BlueNoiseJitter         = true;
		// Base EMA blend factor for temporal accumulation when the camera is still.
		// Lower = more temporal smoothing (less per-frame noise at edges, more ghosting).
		// Higher = faster convergence (sharper edges, more per-frame noise/flickering).
		// windEvoBoost in the renderer ramps this up dynamically when clouds move.
		float TemporalBaseBlend       = 0.15f;
	};

	// ========================================================================
	// Cloud noise parameters (only relevant for non-AltocumulusMid types)
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
		float Coverage          = 0.55f;   // [0,1] — cloud coverage (used for transitions; not read by shader for AltocumulusMid)
		float Density           = 0.8f;    // No-op for AltocumulusMid (shader ignores for this cloud type)
		float CloudBottomHeight = 1536.0f; // World units above camera (matches TEN sky layer offset)
		float CloudThickness    = 2500.0f; // Vertical extent of cloud slab

		// Wind & evolution
		Vector2 WindDirection   = Vector2(1.0f, 0.0f);
		float   WindSpeed       = 0.003f;
		float   EvolutionSpeed  = 0.15f;

		// Noise (no-op for AltocumulusMid — kept for Lua API compatibility)
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
		float AltoAbsorption      = 1.0f;   // [0.0,5.0] absorption coefficient
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
		float AltoHorizonGradientFade = 0.0f; // [0,1] top-to-bottom alpha gradient on horizon mesh

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

		// Transform dissolve phase [0,1]. Set by TransformPresets (CloudMorph).
		// 0 = no dissolve (shader dissolve logic skipped), 1 = fully dissolved.
		float DissolvePhase = 0.0f;

		// Transform formation phase [0,1]. Reverse of dissolve — clouds appear.
		float FormationPhase = 0.0f;

		// CloudMorph dual-density morph source params (only used during morph transitions).
		float MorphActive          = 0.0f;
		float MorphSrcCloudSize    = 1.0f;
		float MorphSrcCloudAmount  = 0.6875f;
		float MorphSrcBillowStr    = 0.75f;
		float MorphSrcCovSoftWidth = 0.08f;
		float MorphSrcFbmLac       = 2.6434f;
		float MorphSrcFbmGain      = 0.5f;
		float MorphSrcBottomSoft   = 0.35f;
		float MorphSrcZenithBias   = 0.0f;
		float MorphSrcEvolutionSpd = 0.15f;
		float MorphSrcHorizonWidth = 0.0f;

		float AltoFbmScale               = 2.032f; // FBM input pre-scale (2.032=reference); lower=coarser
		float CurlWarpStrength           = 1.0f;   // [0,2] curl domain-warp amplitude multiplier (0 = no warp)
		float JitterStrength             = 0.3f;
		float UpsampleSpatialSigma2      = 2.0f;   // bilateral upsampler spatial spread (2*sigma^2) — 5x5 kernel
		float TemporalAlphaLow           = 0.05f;  // below this alpha: temporal reuse OK (clear sky)
		float TemporalAlphaHigh          = 0.95f;  // above this alpha: temporal reuse OK (cloud core)
		float AltoJitterAbsCap           = 5.0f;   // absorption cap used only for jitter amplitude

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
		float EvoAccumOffset   = 0.0f;  // Pre-integrated evolution offset — monotonically non-decreasing.
		                                 // Analogous to WindAccumOffset: accumulated as EvolutionSpeed*dt*0.05
		                                 // so that when EvolutionSpeed transitions to a lower value (or zero)
		                                 // the clouds don't drift backwards. Used in place of the old
		                                 // ap.EvolutionSpd*(CloudTime*0.05+WindSpeed*0.15) evoOfs formula.
		float FlowAccumOffset  = 0.0f;  // Pre-integrated flow time — monotonically non-decreasing.
		                                 // Accumulated as EvolutionSpeed*dt*0.16 (same guard as EvoAccumOffset).
		                                 // Prevents curl-warp and windBias from reversing when EvolutionSpeed
		                                 // transitions to a lower value; replaces CloudTime*EvSpd*0.16 in shader.
		float WindAccumOffsetScaled = 0.0f; // Pre-integrated WindSpeed * AltoFbmScale * dt — used in Alto FBM p_advect.
		                                     // Bakes AltoFbmScale into the integral so on-the-fly FbmScale changes
		                                     // (preset transitions) only affect future frames, not the rescaling of
		                                     // all accumulated past motion (which would read as time-lapse).
		float EvoAccumOffsetScaled  = 0.0f; // Pre-integrated EvolutionSpeed * AltoFbmScale * dt * 0.05 — analogous.
		int   FrameCounter     = 0;     // Monotonic frame counter for jitter cycling

		// Current packed quality params
		CloudQualityParams ActiveQuality = {};

		// Previous-frame values for temporal-disable guards.
		//
		// Camera rotation: if the camera has rotated since last frame, temporal is
		// disabled so every pixel is freshly raymarched — avoids stale-UV swimming.
		//
		// Cloud motion: AccumulatedTime and WindAccumOffset are compared to their
		// previous values to approximate the noise-space displacement per frame.
		// If the displacement exceeds ~0.05 noise units the checkerboard becomes
		// visible (adjacent pixels show the cloud at two distinct moments in time).
		Vector3 PrevCameraForward      = Vector3(0.0f, 0.0f, 1.0f);
		float   PrevAccumulatedTime    = 0.0f;
		float   PrevWindAccumOffset    = 0.0f;
		float   PrevEvoAccumOffset     = 0.0f;

		// Previous-frame morph phase values. During CloudMorph the on-screen
		// content evolves continuously (Dissolve/Formation phase ramps every
		// frame), so feeding the per-frame phase delta into the EMA blend factor
		// makes temporal accumulation track the morph instead of averaging
		// successive morph stages together (which reads as blur / smear).
		float   PrevDissolvePhase      = 0.0f;
		float   PrevFormationPhase     = 0.0f;

		// Previous frame's ViewProjection matrix for temporal reprojection.
		// Clouds are at infinite distance so only rotation matters; translation
		// is negligible at sky-dome scale (1e6 world units).
		Matrix  PrevViewProjection     = Matrix::Identity;

		// Lens flare occlusion
		LensFlareCloudOcclusionState FlareOcclusion = {};

		// Previous cloud type — used to detect preset switches for temporal invalidation.
		int PrevCloudType = -1;

		// Lightning thunder tracking.
		float LightningPrevFlashCycle  = -1.0f; // Flash cycle at previous frame; -1 = not initialized.
		float LightningThunderCountdown = -1.0f; // Seconds until next thunder sound; -1 = none pending.

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
				/*BlueNoiseJitter=*/     true,
				/*TemporalBaseBlend=*/   0.30f  // Temporal disabled — value unused
			};

		case CloudQualityPreset::High:
			return CloudQualityParams{
				/*PrimaryStepCount=*/    32,
				/*ShadowStepCount=*/     6,
				/*OcclusionSampleSteps=*/8,
				/*RenderResolutionScale=*/0.5f,   // was 0.75 — now half-res (balanced)
				/*DetailNoiseEnabled=*/  true,
				/*TemporalReprojection=*/true,
				/*BlueNoiseJitter=*/     true,
				/*TemporalBaseBlend=*/   0.10f  // Half-res: moderate smoothing still appropriate
			};

		case CloudQualityPreset::Medium:
		default:
			return CloudQualityParams{
				/*PrimaryStepCount=*/    12,
				/*ShadowStepCount=*/     4,
				/*OcclusionSampleSteps=*/6,
				/*RenderResolutionScale=*/0.25f,  // was 0.5 — now quarter-res (4x fewer pixels)
				/*DetailNoiseEnabled=*/  true,
				/*TemporalReprojection=*/true,
				/*BlueNoiseJitter=*/     true,
				/*TemporalBaseBlend=*/   0.10f  // Quarter-res: low EMA blend reduces per-frame noise at thin edges;
				                               // windEvoBoost ramps this up dynamically when clouds are moving.
			};
		}
	}
}
