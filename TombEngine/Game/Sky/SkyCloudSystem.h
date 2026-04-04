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
		AltocumulusMid,             // Patchy, medium altitude, moderate density
		Aurora                      // High-altitude aurora borealis effect
	};

	// ====================================================================
	// Weather preset type
	// ====================================================================

	enum class WeatherPresetType
	{
		ClearSky,
		ClearSkyHigh,          // Clear sky with faint high-altitude wisps.
		ClearSkyLow,           // Clear sky with thin horizon haze layer.
		CirrocumulusClear,     // Open sky with very few scattered cirrocumulus patches.
		CirrocumulusLots,      // Dense rippled mackerel-sky cirrocumulus.
		CirrocumulusFew,       // Sparse patches of rippled cirrocumulus.
		Cirrustratus,          // Thin translucent veil of cirrus covering the sky.
		StormBuildUpHigh,
		BrokenClouds,
		Overcast,
		Altocumulus,
		AltocumulusHigh,       // Altocumulus pushed to higher altitude, thinner.
		AuroraBorealis,
		RainSnowOvercast,
		StormBuildUp,
		StormTransformation,   // Bridge preset: rapid shift to full thunderstorm.
		Thunderstorm,

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
		float AltoHorizonWidth     = 0.0f; // [0,1]   0=wide (to near horizon), 1=zenith-only cap
		float AltoBleedDepth       = 0.0f; // [0,100] bleed clouds depth (0.01*val*CloudBottomHeight)
		float AltoHorizonGradientFade = 0.0f; // [0,1] top-to-bottom alpha gradient on horizon mesh (0=none, 1=full)

		// Compositor hybrid-blend thresholds (only meaningful for AltocumulusMid)
		// Luminance above BlendThresholdHigh → screen blend (bright clouds, no halos).
		// Luminance below BlendThresholdLow  → screen blend (dark cloud edges, no halos).
		// Luminance in-between              → alpha blend  (dense/mid clouds absorb properly).
		float BlendThresholdHigh      = 0.85f;  // [0,1]     bright cutoff (left  arrow on gradient bar)
		float BlendThresholdHighWidth = 0.05f;  // [0.005,0.4] half-width of the bright→alpha transition zone
		float BlendThresholdLow       = 0.106f; // [0,1]     dark  cutoff (right arrow on gradient bar)

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

	// Per-entry in a probabilistic next-preset list.
	// Used when nextPreset is defined as a table in Lua instead of a single string.
	struct NextPresetCandidate
	{
		std::string Name;
		float       Weight              = 1.0f;  // Relative probability weight during day (>= 0).
		float       WeightNight         = -1.0f; // Weight during night. < 0 = same as Weight (no day/night split).
		float       TransitionDuration  = 30.0f; // Transition duration in seconds.
		float       TransitionDurationA = -1.0f; // Per-layer; < 0 = inherit TransitionDuration.
		float       TransitionDurationB = -1.0f; // Per-layer; < 0 = inherit TransitionDuration.
	};

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

		// Optional transition hints (legacy).
		// If the transition FROM this preset should stage certain params first
		// (e.g., cirrus appears before lower clouds fill in), define a staging factor.
		// 0.0 = everything transitions together (default).
		// > 0 = higher-altitude layers lead by this fraction of the total duration.
		// Superseded by TransitionDurationA / TransitionDurationB when those are >= 0.
		float HighLayerLeadFraction     = 0.0f;

		// Auto-chain: once this preset becomes active, start transitioning to NextPreset.
		// Empty string = no chaining.
		std::string NextPreset                    = "";
		float       NextPresetTransitionDuration  = 30.0f; // seconds for the chained transition.
		float       NextPresetTransitionDurationA = -1.0f; // < 0 = inherit NextPresetTransitionDuration.
		float       NextPresetTransitionDurationB = -1.0f; // < 0 = inherit NextPresetTransitionDuration.

		// Dwell duration: how long (seconds) to stay at this preset before chaining to NextPreset.
		// < 0 = chain immediately (backward-compatible default).
		// If DwellDurationMin >= 0 and DwellDurationMax >= DwellDurationMin, a random value
		// in [DwellDurationMin, DwellDurationMax] is rolled each time the preset becomes active.
		float NextPresetDwellDuration    = -1.0f;
		float NextPresetDwellDurationMin = -1.0f;
		float NextPresetDwellDurationMax = -1.0f;

		// Probabilistic next-preset list (set when nextPreset / nextPresetAB is a Lua table).
		// When non-empty, a candidate is picked by weighted random instead of using NextPreset.
		// Each entry carries its own transition duration so different follow-ups can have
		// different transition speeds.
		std::vector<NextPresetCandidate> NextPresetCandidates;

		// Layer-A-only auto-chain: only CloudA transitions to the target preset's CloudA snapshot.
		// Does NOT change CloudB or _currentPreset — layers stay independent.
		// nextPresetA = "PresetName"  or  nextPresetA = { PresetName = {weight, duration}, ... }
		std::string NextPresetA         = "";
		float       NextPresetADuration = 30.0f;
		std::vector<NextPresetCandidate> NextPresetACandidates;

		// Layer-B-only auto-chain: only CloudB transitions to the target preset's CloudB snapshot.
		// nextPresetB = "PresetName"  or  nextPresetB = { PresetName = {weight, duration}, ... }
		std::string NextPresetB         = "";
		float       NextPresetBDuration = 30.0f;
		std::vector<NextPresetCandidate> NextPresetBCandidates;
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
	// Independent per-layer transition state (volumetric cloud layers only)
	//
	// Allows CloudA and CloudB to each transition toward the CloudA/CloudB
	// snapshot of *different* presets, completely independently of each
	// other and of the full-preset WeatherTransitionState.
	// ====================================================================

	struct LayerTransitionState
	{
		bool  Active   = false;
		WeatherPresetType TargetPreset = WeatherPresetType::ClearSky;
		VolumetricCloudLayerSnapshot Source = {};
		VolumetricCloudLayerSnapshot Target = {};
		float Duration = 30.0f;   // Total transition time in seconds.
		float Elapsed  = 0.0f;
		float Progress = 0.0f;    // [0, 1] eased progress.
		EasingCurve Curve = EasingCurve::SmoothStep;
	};

	// ====================================================================
	// Drift-out state — wind-directional dissolution when no NextPreset
	// ====================================================================

	struct DriftOutState
	{
		bool  Active   = false;
		float Duration = 60.0f;  // Seconds for clouds to fully dissolve.
		float Elapsed  = 0.0f;
		float Progress = 0.0f;   // [0, 1] smoothstepped dissolution progress.
		VolumetricCloudLayerSnapshot StartSnapshot = {};
	};

	struct LayerDwellState
	{
		float Elapsed = 0.0f;
		float Target  = -1.0f; // < 0 = no dwell pending.
		bool  Paused  = false;
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
		void InterruptTransition();      // Stop mid-transition, keep current blended state.
		void StopAllTransitions();       // Cancel ALL active transitions, dwell, and drift-out.

		// --- Independent per-layer preset control ---
		// These target only the CloudA or CloudB snapshot of a preset, leaving
		// the other layer completely unaffected. Each runs its own timer.
		void TransitionLayerAToPreset(WeatherPresetType preset, float durationSeconds,
		                              EasingCurve curve = EasingCurve::SmoothStep);
		void TransitionLayerBToPreset(WeatherPresetType preset, float durationSeconds,
		                              EasingCurve curve = EasingCurve::SmoothStep);
		void SetLayerAPresetImmediate(WeatherPresetType preset);
		void SetLayerBPresetImmediate(WeatherPresetType preset);
		void InterruptLayerATransition();
		void InterruptLayerBTransition();
		void PauseLayerADwell();
		void PauseLayerBDwell();
		void ResumeLayerADwell();
		void ResumeLayerBDwell();
		bool IsLayerADwellPaused() const;
		bool IsLayerBDwellPaused() const;

		// Progress queries for per-layer transitions.
		bool  IsLayerATransitioning() const;
		bool  IsLayerBTransitioning() const;
		float GetLayerATransitionProgress() const;
		float GetLayerBTransitionProgress() const;

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

		// --- Drift-out queries ---
		bool  IsCloudADriftingOut() const;
		bool  IsCloudBDriftingOut() const;
		float GetCloudADriftOutProgress() const;
		float GetCloudBDriftOutProgress() const;

		// --- Night blend ---
		// Called by the renderer each frame with the current moon/starfield visibility [0..1].
		// Controls day-vs-night weight blending in probabilistic next-preset chains.
		void  SetNightBlend(float blend);
		float GetNightBlend() const;

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

			bool  Layer1Enabled               = false;
			bool  Layer2Enabled               = false;
			bool  CloudAEnabled               = false;
			bool  CloudBEnabled               = false;
			float CloudATransmittance         = 1.0f;
			float CloudBTransmittance         = 1.0f;
			float CombinedTransmittance       = 1.0f;
			CloudCategory CloudACategory      = CloudCategory::None;
			CloudCategory CloudBCategory      = CloudCategory::None;

			// Per-layer independent transition state.
			bool  LayerATransitioning         = false;
			bool  LayerBTransitioning         = false;
			float LayerATransitionProgress    = 0.0f;
			float LayerBTransitionProgress    = 0.0f;

			// Per-layer active and target preset.
			WeatherPresetType LayerAPreset       = WeatherPresetType::ClearSky;
			WeatherPresetType LayerBPreset       = WeatherPresetType::ClearSky;
			WeatherPresetType LayerATargetPreset = WeatherPresetType::ClearSky;
			WeatherPresetType LayerBTargetPreset = WeatherPresetType::ClearSky;

			// Dwell timer (how long until next-preset chain fires).
			float DwellElapsed = 0.0f;
			float DwellTarget  = -1.0f; // < 0 = no dwell pending.
			float LayerADwellElapsed = 0.0f;
			float LayerADwellTarget  = -1.0f;
			float LayerBDwellElapsed = 0.0f;
			float LayerBDwellTarget  = -1.0f;
			bool  LayerADwellPaused  = false;
			bool  LayerBDwellPaused  = false;
		};

		DebugInfo GetDebugInfo() const;

	private:
		// --- Internal ---
		void UpdateTransition(float deltaTime);
		void ApplySnapshot(const SkyCloudSnapshot& snapshot);

		// --- Per-layer independent transition ---
		// Advances one layer's transition and writes the result back to `current`.
		bool UpdateLayerTransition(float deltaTime, LayerTransitionState& layerTr,
		                          VolumetricCloudLayerSnapshot& current);

		// --- Data ---
		std::unordered_map<WeatherPresetType, WeatherPresetDefinition> _presets;
		SkyCloudSnapshot       _currentState;
		WeatherPresetType      _currentPreset  = WeatherPresetType::ClearSky;
		WeatherTransitionState _transition;

		// Per-layer independent transition states.
		LayerTransitionState   _layerTransitionA;
		LayerTransitionState   _layerTransitionB;

		// Manual override flags.
		bool _manualOverrideCloudA  = false;
		bool _manualOverrideCloudB  = false;
		bool _manualOverrideLayer1  = false;
		bool _manualOverrideLayer2  = false;

		// Per-layer active preset (diverges from _currentPreset when layers transition independently).
		WeatherPresetType _layerAPreset = WeatherPresetType::ClearSky;
		WeatherPresetType _layerBPreset = WeatherPresetType::ClearSky;

		// Lens flare occlusion from each volumetric layer.
		float _cloudATransmittance = 1.0f;
		float _cloudBTransmittance = 1.0f;

		// Dwell state: countdown before firing a deferred NextPreset chain.
		// _nextPresetDwellTarget < 0 means no dwell is pending.
		float _nextPresetDwellElapsed = 0.0f;
		float _nextPresetDwellTarget  = -1.0f;
		LayerDwellState _layerDwellA;
		LayerDwellState _layerDwellB;
		std::mt19937 _dwellRNG; // separate RNG for dwell randomization

		// Night blend factor [0 = full day, 1 = full night].
		// Set each frame by the renderer via SetNightBlend().
		// Used to interpolate between Weight and WeightNight in NextPresetCandidate.
		float _nightBlend = 0.0f;

		// Drift-out: wind-directional dissolution (per layer).
		DriftOutState _driftOutA;
		DriftOutState _driftOutB;

		float ResolveNextPresetDwell(const WeatherPresetDefinition& def);
		void  UpdatePresetDwell(float deltaTime);
		void  StartNextPresetDwell(const WeatherPresetDefinition& def);
		void  StartLayerDwell(WeatherPresetType preset, LayerDwellState& dwellState, bool isLayerA);
		void  UpdateLayerDwell(float deltaTime, LayerDwellState& dwellState, WeatherPresetType preset, bool isLayerA);
		void  FireNextPresetChains(const WeatherPresetDefinition& def);
		const NextPresetCandidate* PickNextPresetCandidate(const std::vector<NextPresetCandidate>& candidates);
		void  StartDriftOut(DriftOutState& state, const VolumetricCloudLayerSnapshot& current);
		void  UpdateDriftOut(float deltaTime, DriftOutState& state, VolumetricCloudLayerSnapshot& current);
	};

	// Global instance.
	extern SkyCloudSystem g_SkyCloudSystem;
}
