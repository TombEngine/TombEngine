#pragma once

// ============================================================================
// SkyCloudSystem.h — Layered Sky & Cloud Weather System
//
// Architecture:
//   The sky is composed of up to 4 independently enabled layers, rendered
//   in a fixed compositing order:
//
//     1. Legacy SkyLayer1 (bitmap scroll)               — back
//     2. Legacy SkyLayer2 (bitmap scroll)               — over layer 1
//     3. Volumetric Cloud Layer A (higher / thinner)    — over legacy layers
//     4. Volumetric Cloud Layer B (lower / denser)      — front
//
//   Weather presets define target states for all four layers.
//   A transition system interpolates smoothly between preset states.
//   A random weather controller cycles presets automatically.
//
//   The system is fully controllable from Lua and backward-compatible
//   with levels that only use layer1/layer2 bitmap sky.
// ============================================================================

#include <array>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <SimpleMath.h>

#include "Renderer/VolumetricCloud/VolumetricCloud.h"
#include "Scripting/Internal/TEN/Flow/SkyLayer/SkyLayer.h"

namespace TEN::Sky
{
	using namespace DirectX::SimpleMath;
	using namespace TEN::Renderer::VolumetricCloud;

	// ====================================================================
	// Cloud category — conceptual cloud type for visual identity
	// ====================================================================

	enum class CloudCategory
	{
		None,                       // No clouds / clear sky
		CirrusHigh,                 // Thin, wispy, high altitude
		AltocumulusMid,             // Patchy, medium altitude, moderate density
		StratocumulusLow,           // Dense, low altitude, broad coverage
		CumulonimbusVertical,       // Strong vertical development, storm-capable
		CumulonimbusVerticalBuildUp, // Distant horizon tower buildup (pre-storm)
		Aurora                      // High-altitude aurora borealis effect
	};

	// ====================================================================
	// Weather preset type
	// ====================================================================

	enum class WeatherPresetType
	{
		ClearSky,
		FewClouds,
		ScatteredClouds,
		BrokenClouds,
		Overcast,
		Cirrus,
		Altocumulus,
		AuroraBorealis,
		RainSnowOvercast,
		StormBuildUp,
		Thunderstorm,
		StormTransformation,
		Random,                // Meta-preset: activates random weather mode

		Count
	};

	// ====================================================================
	// Easing curve for transitions
	// ====================================================================

	enum class EasingCurve
	{
		Linear,
		SmoothStep,       // Hermite smoothstep (3t^2 - 2t^3)
		EaseInOut,        // Cubic ease-in-out
		EaseIn,           // Quadratic ease-in
		EaseOut           // Quadratic ease-out
	};

	float ApplyEasing(float t, EasingCurve curve);

	// ====================================================================
	// Volumetric cloud layer settings snapshot
	//
	// This mirrors CloudRenderSettings but is a pure data snapshot used
	// for interpolation. All parameters can be lerped numerically.
	// ====================================================================

	struct VolumetricCloudLayerSnapshot
	{
		bool  Enabled         = false;
		CloudCategory Category = CloudCategory::None;

		float Coverage        = 0.0f;    // [0, 1]
		float Density         = 0.0f;
		float BottomHeight    = 1536.0f; // World units above camera
		float Thickness       = 2500.0f;

		float WindDirectionX  = 1.0f;
		float WindDirectionY  = 0.0f;
		float WindSpeed       = 0.003f;
		float EvolutionSpeed  = 0.15f;

		float ShapeScale      = 0.00008f;
		float DetailScale     = 0.0008f;
		float DetailStrength  = 0.35f;

		float Absorption      = 1.1f;
		float AmbientContrib  = 0.35f;
		float SilverliningStr = 0.4f;

		float HorizonFade     = 1.0f;   // Fade near horizon, 0 = none, 1 = full
		float DistanceFade    = 1.0f;   // Distance-based fade factor
		float HorizonMeshBleed = 0.0f;  // [0,1] how much clouds bleed through the opaque horizon mesh after it is drawn

		// Altocumulus-specific appearance tuning (only meaningful for Category == AltocumulusMid)
		float AltoBillowStrength = 0.75f;  // [0,1]      blend toward billow (abs-value) FBM
		float AltoCovSoftWidth   = 0.08f;  // [0,0.25]   self-referential coverage soft-threshold
		float AltoAbsorption      = 1.0f;   // [0.1,5.0] absorption coefficient
		float AltoCloudSize      = 1.0f;   // [0.2,5.0]  feature scale (1=default, <1=bigger, >1=smaller)
		float AltoCloudAmount    = 0.6875f;// [0.0,1.0]  coverage/fill (0=sparse, 1=overcast)
		float AltoCloudBrightness = 1.0f;  // [0.1,4.0]  brightness multiplier
		float AltoCloudColorR    = 1.0f;   // [0,1]      color tint red
		float AltoCloudColorG    = 1.0f;   // [0,1]      color tint green
		float AltoCloudColorB    = 1.0f;   // [0,1]      color tint blue
		float AltoFbmLacunarity  = 2.6434f;// [1.5,4.0]  FBM frequency ratio per octave
		float AltoFbmGain        = 0.5f;   // [0.1,0.9]  FBM amplitude scaling per octave
		float AltoThickness      = 1800.0f;// [50,5000]  cloud slab thickness
		float AltoCloudColorDarkR = 0.55f; // [0,1]      dark/shadow color tint red
		float AltoCloudColorDarkG = 0.55f; // [0,1]      dark/shadow color tint green
		float AltoCloudColorDarkB = 0.65f; // [0,1]      dark/shadow color tint blue (slightly cool)
		float AltoBottomSoftness  = 0.35f; // [0,1]      0=flat bottom, 1=organic underside

		// Altocumulus sky-height redistribution (only meaningful for Category == AltocumulusMid)
		// 0 = uniform. (+) = more/larger toward horizon. (-) = more/larger toward zenith.
		float AltoZenithBias       = 0.0f; // [-1,1]  cloud distribution bias
		float AltoHeightBlendPower = 1.0f; // [0.25,4] exponent on the skyHeight ramp

		// Lightning parameters (only for AltocumulusMid — internal flash + bolt glow)
		bool  LightningEnabled      = false;
		float LightningStrikeFreq   = 0.1f;  // [0,1]      probability of visible bolt per cycle
		float LightningInternalFreq = 0.5f;  // [0,1]      probability of internal flash per cycle
		float LightningSpeed        = 2.5f;  // [0.5,10]   cycle speed
		float LightningInternalSpeed = 5.0f; // [1,20]     internal flash source movement speed
		float LightningGlowIntensity = 3.0f; // [0.5,10]   glow strength
		float LightningBoltColorR   = 0.3f;  // [0,1]      bolt glow color R
		float LightningBoltColorG   = 0.6f;  // [0,1]      bolt glow color G
		float LightningBoltColorB   = 1.0f;  // [0,1]      bolt glow color B
		float LightningFlashIntensity = 4.0f;// [0.5,15]   internal cloud-flash intensity
		float LightningAmbientContrib = 0.15f;// [0,1]     how much lightning adds to ambient
		float LightningBoltLengthScale    = 1.0f; // [0.1,5]   bolt length multiplier
		float LightningBoltThicknessScale = 1.0f; // [0.1,5]   bolt radius multiplier

		CloudQualityPreset Quality = CloudQualityPreset::Medium;

		// Convert to/from the renderer's CloudRenderSettings.
		CloudRenderSettings ToRenderSettings() const;
		static VolumetricCloudLayerSnapshot FromRenderSettings(const CloudRenderSettings& src);

		// Numerically interpolate all fields (except booleans and enums).
		static VolumetricCloudLayerSnapshot Lerp(
			const VolumetricCloudLayerSnapshot& a,
			const VolumetricCloudLayerSnapshot& b,
			float t);
	};

	// ====================================================================
	// Legacy sky layer snapshot (bitmap scroll)
	// ====================================================================

	struct LegacySkyLayerSnapshot
	{
		bool  Enabled    = false;
		byte  R          = 0;
		byte  G          = 0;
		byte  B          = 0;
		short CloudSpeed = 0;

		static LegacySkyLayerSnapshot Lerp(
			const LegacySkyLayerSnapshot& a,
			const LegacySkyLayerSnapshot& b,
			float t);
	};

	// ====================================================================
	// Full sky state snapshot — entire sky/cloud configuration at one point
	// ====================================================================

	struct SkyCloudSnapshot
	{
		LegacySkyLayerSnapshot        Layer1;   // Legacy bitmap layer 1
		LegacySkyLayerSnapshot        Layer2;   // Legacy bitmap layer 2
		VolumetricCloudLayerSnapshot  CloudA;   // Volumetric cloud layer A (high/thin)
		VolumetricCloudLayerSnapshot  CloudB;   // Volumetric cloud layer B (low/dense)

		static SkyCloudSnapshot Lerp(
			const SkyCloudSnapshot& a,
			const SkyCloudSnapshot& b,
			float t);
	};

	// ====================================================================
	// Weather preset definition
	// ====================================================================

	struct WeatherPresetDefinition
	{
		WeatherPresetType Type        = WeatherPresetType::ClearSky;
		std::string       Name        = "ClearSky";
		SkyCloudSnapshot  TargetState = {};

		// Transition defaults (seconds).
		float DefaultTransitionDuration = 30.0f;

		// Per-layer transition durations (seconds).
		// Negative value = use DefaultTransitionDuration as fallback.
		float TransitionDurationA = -1.0f;  // CloudA transition time. < 0 means inherit DefaultTransitionDuration.
		float TransitionDurationB = -1.0f;  // CloudB transition time. < 0 means inherit DefaultTransitionDuration.

		// Random weather config.
		float RandomWeight              = 1.0f;    // Probability weight for random selection.
		bool  AllowInRandom             = true;     // Can this preset be picked by random mode?

		// Optional transition hints (legacy).
		// If the transition FROM this preset should stage certain params first
		// (e.g., cirrus appears before lower clouds fill in), define a staging factor.
		// 0.0 = everything transitions together (default).
		// > 0 = higher-altitude layers lead by this fraction of the total duration.
		// Superseded by TransitionDurationA / TransitionDurationB when those are >= 0.
		float HighLayerLeadFraction     = 0.0f;

		// Auto-chain: once this preset becomes active, immediately start transitioning to NextPreset.
		// Empty string = no chaining.
		std::string NextPreset                    = "";
		float       NextPresetTransitionDuration  = 30.0f; // seconds for the chained transition.
		float       NextPresetTransitionDurationA = -1.0f; // < 0 = inherit NextPresetTransitionDuration.
		float       NextPresetTransitionDurationB = -1.0f; // < 0 = inherit NextPresetTransitionDuration.
	};

	// ====================================================================
	// Transition state
	// ====================================================================

	struct WeatherTransitionState
	{
		bool  Active                = false;
		WeatherPresetType Source    = WeatherPresetType::ClearSky;
		WeatherPresetType Target   = WeatherPresetType::ClearSky;
		SkyCloudSnapshot  SourceSnapshot = {};
		SkyCloudSnapshot  TargetSnapshot = {};
		float Duration              = 30.0f;   // Total transition time = max(DurationA, DurationB).
		float DurationA             = 30.0f;   // CloudA transition duration (seconds).
		float DurationB             = 30.0f;   // CloudB transition duration (seconds).
		float Elapsed               = 0.0f;    // Time elapsed.
		float Progress              = 0.0f;    // [0, 1] eased (based on Duration).
		EasingCurve Curve           = EasingCurve::SmoothStep;
		float HighLayerLeadFraction = 0.0f;    // Legacy; overridden by DurationA/B when both >= 0.
	};

	// ====================================================================
	// Random weather controller state
	// ====================================================================

	struct RandomWeatherState
	{
		bool   Active          = false;
		float  DwellTime       = 120.0f;  // Seconds to stay in a preset before switching.
		float  DwellElapsed    = 0.0f;    // Time spent in current preset.
		float  TransitionTime  = 60.0f;   // Seconds for each transition.
		EasingCurve Curve      = EasingCurve::SmoothStep;
		std::vector<WeatherPresetType> ExcludedPresets;
		std::mt19937 RNG;
		bool   Seeded          = false;
		uint32_t Seed          = 0;
	};

	// ====================================================================
	// Combined Sky Cloud System — the main manager
	// ====================================================================

	class SkyCloudSystem
	{
	public:
		SkyCloudSystem();

		// --- Initialization ---
		void Initialize();
		void InitializePresets();

		// --- Per-frame update (call from game loop) ---
		void Update(float deltaTime);

		// --- Preset control ---
		void SetPresetImmediate(WeatherPresetType preset);
		void TransitionToPreset(WeatherPresetType preset, float durationSeconds,
		                         EasingCurve curve = EasingCurve::SmoothStep);
		// Per-layer duration overload: CloudA and CloudB transition independently.
		void TransitionToPreset(WeatherPresetType preset, float durationASeconds, float durationBSeconds,
		                        EasingCurve curve = EasingCurve::SmoothStep);
		void InterruptTransition(); // Stop mid-transition, keep current blended state.

		// --- Random weather ---
		void StartRandomWeather(float dwellTime, float transitionTime,
		                        EasingCurve curve = EasingCurve::SmoothStep);
		void StopRandomWeather();
		void SetRandomSeed(uint32_t seed);
		void SetRandomExclusions(const std::vector<WeatherPresetType>& exclusions);

		// --- Manual layer override ---
		void SetVolumetricLayerA(const VolumetricCloudLayerSnapshot& snapshot);
		void SetVolumetricLayerB(const VolumetricCloudLayerSnapshot& snapshot);
		void SetLegacyLayer1(const LegacySkyLayerSnapshot& snapshot);
		void SetLegacyLayer2(const LegacySkyLayerSnapshot& snapshot);
		void ClearManualOverrides();

		// --- Queries ---
		WeatherPresetType GetCurrentPreset() const;
		WeatherPresetType GetTargetPreset() const;
		float             GetTransitionProgress() const;
		bool              IsTransitioning() const;
		bool              IsRandomWeatherActive() const;

		// --- Current blended state access (for renderer & debug) ---
		const SkyCloudSnapshot& GetCurrentState() const;
		SkyCloudSnapshot&       GetMutableCurrentState();
		CloudRenderSettings     GetCloudARenderSettings() const;
		CloudRenderSettings     GetCloudBRenderSettings() const;
		bool                    IsCloudAActive() const;
		bool                    IsCloudBActive() const;
		bool                    IsAuroraPresetActive() const;
		bool                    IsLegacyLayer1Active() const;
		bool                    IsLegacyLayer2Active() const;

		// --- Lens flare occlusion ---
		// Combined transmittance from both volumetric layers.
		float GetCombinedCloudTransmittance() const;
		void  SetLayerTransmittance(int layerIndex, float transmittance);

		// --- Preset registry ---
		const WeatherPresetDefinition* GetPresetDefinition(WeatherPresetType type) const;
		WeatherPresetDefinition*       GetMutablePresetDefinition(WeatherPresetType type);
		std::vector<WeatherPresetType> GetAllPresetTypes() const;
		void OverridePreset(WeatherPresetType type, const WeatherPresetDefinition& def);
		static CloudCategory CategoryFromString(const std::string& name);
		static const char* PresetTypeToString(WeatherPresetType type);
		static WeatherPresetType StringToPresetType(const std::string& name);

		// --- Debug ---
		struct DebugInfo
		{
			WeatherPresetType CurrentPreset   = WeatherPresetType::ClearSky;
			WeatherPresetType TargetPreset    = WeatherPresetType::ClearSky;
			float TransitionProgress          = 0.0f;
			std::string NextPreset            = "";   // Name of the chained preset (empty = none).
			bool  RandomModeActive            = false;
			float RandomDwellRemaining        = 0.0f;
			bool  Layer1Enabled               = false;
			bool  Layer2Enabled               = false;
			bool  CloudAEnabled               = false;
			bool  CloudBEnabled               = false;
			float CloudATransmittance         = 1.0f;
			float CloudBTransmittance         = 1.0f;
			float CombinedTransmittance       = 1.0f;
			CloudCategory CloudACategory      = CloudCategory::None;
			CloudCategory CloudBCategory      = CloudCategory::None;
		};

		DebugInfo GetDebugInfo() const;

	private:
		// --- Internal ---
		void UpdateTransition(float deltaTime);
		void UpdateRandomWeather(float deltaTime);
		void ApplySnapshot(const SkyCloudSnapshot& snapshot);
		WeatherPresetType PickRandomPreset();

		// --- Data ---
		std::unordered_map<WeatherPresetType, WeatherPresetDefinition> _presets;
		SkyCloudSnapshot       _currentState;
		WeatherPresetType      _currentPreset  = WeatherPresetType::ClearSky;
		WeatherTransitionState _transition;
		RandomWeatherState     _randomWeather;

		// Manual override flags.
		bool _manualOverrideCloudA  = false;
		bool _manualOverrideCloudB  = false;
		bool _manualOverrideLayer1  = false;
		bool _manualOverrideLayer2  = false;

		// Lens flare occlusion from each volumetric layer.
		float _cloudATransmittance = 1.0f;
		float _cloudBTransmittance = 1.0f;
	};

	// Global instance.
	extern SkyCloudSystem g_SkyCloudSystem;
}
