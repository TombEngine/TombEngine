// ============================================================================
// SkyCloudSystem.cpp � Layered Sky & Cloud Weather System Implementation
// ============================================================================

#include "framework.h"
#include "Game/Sky/SkyCloudSystem.h"

#include <algorithm>
#include "Game/control/control.h"
#include "Game/effects/weather.h"
#include "Math/Utils.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Scripting/Internal/TEN/Flow/DynamicSky/DynamicSky.h"
#include <cmath>
#include <numeric>

using TEN::Math::Smoothstep;
using namespace TEN::Renderer;
using namespace TEN::Renderer::Aurora;

namespace TEN::Sky
{
	// ====================================================================
	// Global instance
	// ====================================================================

	SkyCloudSystem g_SkyCloudSystem;

	// ====================================================================
	// Easing functions
	// ====================================================================

	float ApplyEasing(float t, EasingCurve curve)
	{
		t = std::clamp(t, 0.0f, 1.0f);

		switch (curve)
		{
		case EasingCurve::Linear:
			return t;

		case EasingCurve::SmoothStep:
			return Smoothstep(t);

		case EasingCurve::EaseInOut:
			return TEN::Math::EaseInOutSine(t);

		case EasingCurve::EaseIn:
			return TEN::Math::EaseInSine(t);

		case EasingCurve::EaseOut:
			return TEN::Math::EaseOutSine(t);

		default:
			return t;
		}
	}

	// Copy only interpolated appearance fields (color, lightning, blend thresholds)
	// from a Lerp'd snapshot into a morph-frozen shape snapshot.
	static void CopyMorphAppearance(VolumetricCloudLayerSnapshot& dst,
	                                const VolumetricCloudLayerSnapshot& src);

	// Apply CloudMorph transition to a single cloud layer.
	static void ApplyMorphToLayer(
		VolumetricCloudLayerSnapshot& out,
		const VolumetricCloudLayerSnapshot& source,
		const VolumetricCloudLayerSnapshot& target,
		const VolumetricCloudLayerSnapshot& lerpAppearance,
		float layerT);

	// ====================================================================
	// VolumetricCloudLayerSnapshot
	// ====================================================================

	CloudRenderSettings VolumetricCloudLayerSnapshot::ToRenderSettings() const
	{
		CloudRenderSettings s;
		s.Enabled         = Enabled;
		s.Mode            = Enabled ? CloudLayerMode::Volumetric : CloudLayerMode::LegacyBitmap;
		s.Coverage        = Coverage;
		s.Density         = 1.0f;      // No-op for AltocumulusMid; kept for Lua API compatibility.
		s.CloudBottomHeight = BottomHeight;
		s.CloudThickness  = Thickness;
		s.EvolutionSpeed  = EvolutionSpeed;
		s.Absorption      = 1.1f;          // Default — unused (Alto has AltoAbsorption).
		s.AmbientContrib  = 0.35f;
		s.SilverliningStr = 0.4f;
		s.HorizonFade     = HorizonFade;
		s.DistanceFade    = DistanceFade;
		s.HorizonMeshBleed = HorizonMeshBleed;
		s.CloudType       = static_cast<int>(Category);
		s.AltoBillowStrength = AltoBillowStrength;
		s.AltoCovSoftWidth   = AltoCovSoftWidth;
		s.AltoAbsorption      = AltoAbsorption;
		s.AltoCloudSize      = AltoCloudSize;
		s.AltoCloudAmount    = AltoCloudAmount;
		s.AltoCloudBrightness = AltoCloudBrightness;
		s.AltoCloudColorR    = AltoCloudColorR;
		s.AltoCloudColorG    = AltoCloudColorG;
		s.AltoCloudColorB    = AltoCloudColorB;
		s.AltoFbmLacunarity  = AltoFbmLacunarity;
		s.AltoFbmGain        = AltoFbmGain;
		s.AltoThickness      = AltoThickness;
		s.AltoCloudColorDarkR = AltoCloudColorDarkR;
		s.AltoCloudColorDarkG = AltoCloudColorDarkG;
		s.AltoCloudColorDarkB = AltoCloudColorDarkB;
		s.AltoBottomSoftness  = AltoBottomSoftness;

		s.AltoZenithBias       = AltoZenithBias;
		s.AltoHeightBlendPower  = AltoHeightBlendPower;
		s.AltoHorizonWidth      = AltoHorizonWidth;
		s.AltoBleedDepth        = AltoBleedDepth;
		s.AltoHorizonGradientFade = AltoHorizonGradientFade;

		// Lightning
		s.LightningEnabled      = LightningEnabled ? 1 : 0;
		s.LightningStrikeFreq   = LightningStrikeFreq;
		s.LightningInternalFreq = LightningInternalFreq;
		s.LightningSpeed        = LightningSpeed;
		s.LightningInternalSpeed = LightningInternalSpeed;
		s.LightningGlowIntensity = LightningGlowIntensity;
		s.LightningBoltColorR   = LightningBoltColorR;
		s.LightningBoltColorG   = LightningBoltColorG;
		s.LightningBoltColorB   = LightningBoltColorB;
		s.LightningFlashIntensity = LightningFlashIntensity;
		s.LightningAmbientContrib = LightningAmbientContrib;
		s.LightningBoltLengthScale    = LightningBoltLengthScale;
		s.LightningBoltThicknessScale = LightningBoltThicknessScale;

		// Blend thresholds
		s.BlendThresholdHigh      = BlendThresholdHigh;
		s.BlendThresholdHighWidth = BlendThresholdHighWidth;
		s.BlendThresholdLow       = BlendThresholdLow;

		// Transform dissolve
		s.DissolvePhase = DissolvePhase;
		s.FormationPhase = FormationPhase;

		// CloudMorph dual-density source params
		s.MorphActive          = MorphActive;
		s.MorphSrcCloudSize    = MorphSrcCloudSize;
		s.MorphSrcCloudAmount  = MorphSrcCloudAmount;
		s.MorphSrcBillowStr    = MorphSrcBillowStr;
		s.MorphSrcCovSoftWidth = MorphSrcCovSoftWidth;
		s.MorphSrcFbmLac       = MorphSrcFbmLac;
		s.MorphSrcFbmGain      = MorphSrcFbmGain;
		s.MorphSrcBottomSoft   = MorphSrcBottomSoft;
		s.MorphSrcZenithBias   = MorphSrcZenithBias;
		s.MorphSrcEvolutionSpd = MorphSrcEvolutionSpd;
		s.MorphSrcHorizonWidth = MorphSrcHorizonWidth;

		// Edge quality tuning
		s.AltoFbmScale          = AltoFbmScale;
		s.CurlWarpStrength      = CurlWarpStrength;
		s.JitterStrength        = JitterStrength;
		s.UpsampleSpatialSigma2 = UpsampleSpatialSigma2;
		s.TemporalAlphaLow      = TemporalAlphaLow;
		s.TemporalAlphaHigh     = TemporalAlphaHigh;
		s.AltoJitterAbsCap      = AltoJitterAbsCap;

		return s;
	}

	VolumetricCloudLayerSnapshot VolumetricCloudLayerSnapshot::FromRenderSettings(
		const CloudRenderSettings& src)
	{
		VolumetricCloudLayerSnapshot snap;
		snap.Enabled       = src.Enabled;
		snap.Category      = static_cast<CloudCategory>(src.CloudType);
		snap.Coverage      = src.Coverage;
		snap.BottomHeight  = src.CloudBottomHeight;
		snap.Thickness     = src.CloudThickness;
		snap.EvolutionSpeed = src.EvolutionSpeed;
		snap.HorizonFade     = src.HorizonFade;
		snap.DistanceFade    = src.DistanceFade;
		snap.HorizonMeshBleed = src.HorizonMeshBleed;
		snap.AltoBillowStrength = src.AltoBillowStrength;
		snap.AltoCovSoftWidth   = src.AltoCovSoftWidth;
		snap.AltoAbsorption      = src.AltoAbsorption;
		snap.AltoCloudSize      = src.AltoCloudSize;
		snap.AltoCloudAmount    = src.AltoCloudAmount;
		snap.AltoCloudBrightness = src.AltoCloudBrightness;
		snap.AltoCloudColorR    = src.AltoCloudColorR;
		snap.AltoCloudColorG    = src.AltoCloudColorG;
		snap.AltoCloudColorB    = src.AltoCloudColorB;
		snap.AltoFbmLacunarity  = src.AltoFbmLacunarity;
		snap.AltoFbmGain        = src.AltoFbmGain;
		snap.AltoThickness      = src.AltoThickness;
		snap.AltoCloudColorDarkR = src.AltoCloudColorDarkR;
		snap.AltoCloudColorDarkG = src.AltoCloudColorDarkG;
		snap.AltoCloudColorDarkB = src.AltoCloudColorDarkB;
		snap.AltoBottomSoftness  = src.AltoBottomSoftness;

		snap.AltoZenithBias       = src.AltoZenithBias;
		snap.AltoHeightBlendPower  = src.AltoHeightBlendPower;
		snap.AltoHorizonWidth      = src.AltoHorizonWidth;
		snap.AltoBleedDepth        = src.AltoBleedDepth;
		snap.AltoHorizonGradientFade = src.AltoHorizonGradientFade;

		// Lightning
		snap.LightningEnabled      = (src.LightningEnabled != 0);
		snap.LightningStrikeFreq   = src.LightningStrikeFreq;
		snap.LightningInternalFreq = src.LightningInternalFreq;
		snap.LightningSpeed        = src.LightningSpeed;
		snap.LightningInternalSpeed = src.LightningInternalSpeed;
		snap.LightningGlowIntensity = src.LightningGlowIntensity;
		snap.LightningBoltColorR   = src.LightningBoltColorR;
		snap.LightningBoltColorG   = src.LightningBoltColorG;
		snap.LightningBoltColorB   = src.LightningBoltColorB;
		snap.LightningFlashIntensity = src.LightningFlashIntensity;
		snap.LightningAmbientContrib = src.LightningAmbientContrib;
		snap.LightningBoltLengthScale    = src.LightningBoltLengthScale;
		snap.LightningBoltThicknessScale = src.LightningBoltThicknessScale;

		// Blend thresholds
		snap.BlendThresholdHigh      = src.BlendThresholdHigh;
		snap.BlendThresholdHighWidth = src.BlendThresholdHighWidth;
		snap.BlendThresholdLow       = src.BlendThresholdLow;

		// Transform dissolve
		snap.DissolvePhase = src.DissolvePhase;
		snap.FormationPhase = src.FormationPhase;

		// CloudMorph dual-density source params
		snap.MorphActive          = src.MorphActive;
		snap.MorphSrcCloudSize    = src.MorphSrcCloudSize;
		snap.MorphSrcCloudAmount  = src.MorphSrcCloudAmount;
		snap.MorphSrcBillowStr    = src.MorphSrcBillowStr;
		snap.MorphSrcCovSoftWidth = src.MorphSrcCovSoftWidth;
		snap.MorphSrcFbmLac       = src.MorphSrcFbmLac;
		snap.MorphSrcFbmGain      = src.MorphSrcFbmGain;
		snap.MorphSrcBottomSoft   = src.MorphSrcBottomSoft;
		snap.MorphSrcZenithBias   = src.MorphSrcZenithBias;
		snap.MorphSrcEvolutionSpd = src.MorphSrcEvolutionSpd;
		snap.MorphSrcHorizonWidth = src.MorphSrcHorizonWidth;

		// Edge quality tuning
		snap.AltoFbmScale          = src.AltoFbmScale;
		snap.CurlWarpStrength      = src.CurlWarpStrength;
		snap.JitterStrength        = src.JitterStrength;
		snap.UpsampleSpatialSigma2 = src.UpsampleSpatialSigma2;
		snap.TemporalAlphaLow      = src.TemporalAlphaLow;
		snap.TemporalAlphaHigh     = src.TemporalAlphaHigh;
		snap.AltoJitterAbsCap      = src.AltoJitterAbsCap;

		return snap;
	}

	static byte LerpByte(byte a, byte b, float t)
	{
		return static_cast<byte>(std::clamp(
			static_cast<int>(std::round(a + (b - a) * t)), 0, 255));
	}

	static short LerpShort(short a, short b, float t)
	{
		return static_cast<short>(std::round(a + (b - a) * t));
	}

	VolumetricCloudLayerSnapshot VolumetricCloudLayerSnapshot::Lerp(
		const VolumetricCloudLayerSnapshot& a,
		const VolumetricCloudLayerSnapshot& b,
		float t)
	{
		VolumetricCloudLayerSnapshot result;

		// Boolean: crossfade by enabling the target once we pass the halfway point,
		// but use coverage to smoothly fade in/out.
		result.Enabled = (t < 0.5f) ? a.Enabled : b.Enabled;

		// If transitioning from disabled to enabled (or vice versa), fade coverage.
		// When going from enabled to disabled: coverage fades to 0 over full duration.
		// When going from disabled to enabled: coverage fades from 0 to target.
		if (a.Enabled && !b.Enabled)
		{
			// Fading out: keep enabled until coverage is ~0.
			result.Enabled = (t < 0.95f);
			result.Coverage = TEN::Math::Lerp(a.Coverage, 0.0f, t);
		}
		else if (!a.Enabled && b.Enabled)
		{
			// Fading in: enable early, ramp coverage from 0.
			result.Enabled = (t > 0.05f);
			result.Coverage = TEN::Math::Lerp(0.0f, b.Coverage, t);
		}
		else
		{
			result.Coverage = TEN::Math::Lerp(a.Coverage, b.Coverage, t);
		}

		// Category: snap at halfway.
		result.Category = (t < 0.5f) ? a.Category : b.Category;

		result.BottomHeight    = TEN::Math::Lerp(a.BottomHeight, b.BottomHeight, t);
		result.Thickness       = TEN::Math::Lerp(a.Thickness, b.Thickness, t);
		result.EvolutionSpeed  = TEN::Math::Lerp(a.EvolutionSpeed, b.EvolutionSpeed, t);
		result.CurlWarpStrength = TEN::Math::Lerp(a.CurlWarpStrength, b.CurlWarpStrength, t);
		result.AltoFbmScale    = TEN::Math::Lerp(a.AltoFbmScale,    b.AltoFbmScale,    t);
		result.HorizonFade        = TEN::Math::Lerp(a.HorizonFade,        b.HorizonFade,        t);
		result.DistanceFade       = TEN::Math::Lerp(a.DistanceFade,       b.DistanceFade,       t);
		result.HorizonMeshBleed   = TEN::Math::Lerp(a.HorizonMeshBleed,   b.HorizonMeshBleed,   t);
		result.AltoBillowStrength = TEN::Math::Lerp(a.AltoBillowStrength, b.AltoBillowStrength, t);
		result.AltoCovSoftWidth   = TEN::Math::Lerp(a.AltoCovSoftWidth,   b.AltoCovSoftWidth,   t);
		result.AltoAbsorption      = TEN::Math::Lerp(a.AltoAbsorption,      b.AltoAbsorption,      t);
		result.AltoCloudSize      = TEN::Math::Lerp(a.AltoCloudSize,      b.AltoCloudSize,      t);
		result.AltoCloudAmount    = TEN::Math::Lerp(a.AltoCloudAmount,    b.AltoCloudAmount,    t);
		result.AltoCloudBrightness = TEN::Math::Lerp(a.AltoCloudBrightness, b.AltoCloudBrightness, t);
		result.AltoCloudColorR    = TEN::Math::Lerp(a.AltoCloudColorR,    b.AltoCloudColorR,    t);
		result.AltoCloudColorG    = TEN::Math::Lerp(a.AltoCloudColorG,    b.AltoCloudColorG,    t);
		result.AltoCloudColorB    = TEN::Math::Lerp(a.AltoCloudColorB,    b.AltoCloudColorB,    t);
		result.AltoFbmLacunarity  = TEN::Math::Lerp(a.AltoFbmLacunarity,  b.AltoFbmLacunarity,  t);
		result.AltoFbmGain        = TEN::Math::Lerp(a.AltoFbmGain,        b.AltoFbmGain,        t);
		result.AltoThickness      = TEN::Math::Lerp(a.AltoThickness,      b.AltoThickness,      t);
		result.AltoCloudColorDarkR = TEN::Math::Lerp(a.AltoCloudColorDarkR, b.AltoCloudColorDarkR, t);
		result.AltoCloudColorDarkG = TEN::Math::Lerp(a.AltoCloudColorDarkG, b.AltoCloudColorDarkG, t);
		result.AltoCloudColorDarkB = TEN::Math::Lerp(a.AltoCloudColorDarkB, b.AltoCloudColorDarkB, t);
		result.AltoBottomSoftness  = TEN::Math::Lerp(a.AltoBottomSoftness,  b.AltoBottomSoftness,  t);

		result.AltoZenithBias       = TEN::Math::Lerp(a.AltoZenithBias,       b.AltoZenithBias,       t);
		result.AltoHeightBlendPower  = TEN::Math::Lerp(a.AltoHeightBlendPower,  b.AltoHeightBlendPower,  t);
		result.AltoHorizonWidth      = TEN::Math::Lerp(a.AltoHorizonWidth,      b.AltoHorizonWidth,      t);
		result.AltoBleedDepth        = TEN::Math::Lerp(a.AltoBleedDepth,        b.AltoBleedDepth,        t);
		result.AltoHorizonGradientFade = TEN::Math::Lerp(a.AltoHorizonGradientFade, b.AltoHorizonGradientFade, t);

		// Lightning
		result.LightningEnabled      = (t < 0.5f) ? a.LightningEnabled : b.LightningEnabled;
		result.LightningStrikeFreq   = TEN::Math::Lerp(a.LightningStrikeFreq,   b.LightningStrikeFreq,   t);
		result.LightningInternalFreq = TEN::Math::Lerp(a.LightningInternalFreq, b.LightningInternalFreq, t);
		result.LightningSpeed        = TEN::Math::Lerp(a.LightningSpeed,        b.LightningSpeed,        t);
		result.LightningInternalSpeed = TEN::Math::Lerp(a.LightningInternalSpeed, b.LightningInternalSpeed, t);
		result.LightningGlowIntensity = TEN::Math::Lerp(a.LightningGlowIntensity, b.LightningGlowIntensity, t);
		result.LightningBoltColorR   = TEN::Math::Lerp(a.LightningBoltColorR,   b.LightningBoltColorR,   t);
		result.LightningBoltColorG   = TEN::Math::Lerp(a.LightningBoltColorG,   b.LightningBoltColorG,   t);
		result.LightningBoltColorB   = TEN::Math::Lerp(a.LightningBoltColorB,   b.LightningBoltColorB,   t);
		result.LightningFlashIntensity = TEN::Math::Lerp(a.LightningFlashIntensity, b.LightningFlashIntensity, t);
		result.LightningAmbientContrib = TEN::Math::Lerp(a.LightningAmbientContrib, b.LightningAmbientContrib, t);
		result.LightningBoltLengthScale    = TEN::Math::Lerp(a.LightningBoltLengthScale,    b.LightningBoltLengthScale,    t);
		result.LightningBoltThicknessScale = TEN::Math::Lerp(a.LightningBoltThicknessScale, b.LightningBoltThicknessScale, t);

		// Blend thresholds
		result.BlendThresholdHigh      = TEN::Math::Lerp(a.BlendThresholdHigh,      b.BlendThresholdHigh,      t);
		result.BlendThresholdHighWidth = TEN::Math::Lerp(a.BlendThresholdHighWidth, b.BlendThresholdHighWidth, t);
		result.BlendThresholdLow       = TEN::Math::Lerp(a.BlendThresholdLow,       b.BlendThresholdLow,       t);

		// Edge quality tuning: interpolated so transitions don't snap jitter/temporal params.
		result.JitterStrength        = TEN::Math::Lerp(a.JitterStrength,        b.JitterStrength,        t);
		result.UpsampleSpatialSigma2 = TEN::Math::Lerp(a.UpsampleSpatialSigma2, b.UpsampleSpatialSigma2, t);
		result.TemporalAlphaLow      = TEN::Math::Lerp(a.TemporalAlphaLow,      b.TemporalAlphaLow,      t);
		result.TemporalAlphaHigh     = TEN::Math::Lerp(a.TemporalAlphaHigh,     b.TemporalAlphaHigh,     t);
		result.AltoJitterAbsCap      = TEN::Math::Lerp(a.AltoJitterAbsCap,      b.AltoJitterAbsCap,      t);

		// DissolvePhase: NOT interpolated. Set directly by UpdateTransition
		// for CloudMorph transforms; always 0 for normal transitions.
		result.DissolvePhase = 0.0f;
		result.FormationPhase = 0.0f;

		// Morph source fields: NOT interpolated — set directly by ApplyMorphToLayer.
		result.MorphActive          = 0.0f;
		result.MorphSrcCloudSize    = a.MorphSrcCloudSize;
		result.MorphSrcCloudAmount  = a.MorphSrcCloudAmount;
		result.MorphSrcBillowStr    = a.MorphSrcBillowStr;
		result.MorphSrcCovSoftWidth = a.MorphSrcCovSoftWidth;
		result.MorphSrcFbmLac       = a.MorphSrcFbmLac;
		result.MorphSrcFbmGain      = a.MorphSrcFbmGain;
		result.MorphSrcBottomSoft   = a.MorphSrcBottomSoft;
		result.MorphSrcZenithBias   = a.MorphSrcZenithBias;
		result.MorphSrcEvolutionSpd = a.MorphSrcEvolutionSpd;
		result.MorphSrcHorizonWidth = a.MorphSrcHorizonWidth;

		return result;
	}

	// ====================================================================
	// LegacySkyLayerSnapshot
	// ====================================================================

	LegacySkyLayerSnapshot LegacySkyLayerSnapshot::Lerp(
		const LegacySkyLayerSnapshot& a,
		const LegacySkyLayerSnapshot& b,
		float t)
	{
		LegacySkyLayerSnapshot result;

		// Crossfade enabled state.
		if (a.Enabled && !b.Enabled)
		{
			// Fading out: stay enabled until near end.
			result.Enabled = (t < 0.95f);
		}
		else if (!a.Enabled && b.Enabled)
		{
			// Fading in: enable early.
			result.Enabled = (t > 0.05f);
		}
		else
		{
			result.Enabled = (t < 0.5f) ? a.Enabled : b.Enabled;
		}

		result.R = LerpByte(a.R, b.R, t);
		result.G = LerpByte(a.G, b.G, t);
		result.B = LerpByte(a.B, b.B, t);
		result.CloudSpeed = LerpShort(a.CloudSpeed, b.CloudSpeed, t);

		return result;
	}

	// ====================================================================
	// SkyCloudSnapshot
	// ====================================================================

	SkyCloudSnapshot SkyCloudSnapshot::Lerp(
		const SkyCloudSnapshot& a,
		const SkyCloudSnapshot& b,
		float t)
	{
		SkyCloudSnapshot result;
		result.Layer1 = LegacySkyLayerSnapshot::Lerp(a.Layer1, b.Layer1, t);
		result.Layer2 = LegacySkyLayerSnapshot::Lerp(a.Layer2, b.Layer2, t);
		result.CloudA = VolumetricCloudLayerSnapshot::Lerp(a.CloudA, b.CloudA, t);
		result.CloudB = VolumetricCloudLayerSnapshot::Lerp(a.CloudB, b.CloudB, t);
		return result;
	}

	// ====================================================================
	// SkyCloudSystem � Construction & Initialization
	// ====================================================================

	SkyCloudSystem::SkyCloudSystem()
	{
		InitializePresets();
	}

	static EasingCurve EasingFromString(const std::string& s)
	{
		if (s == "Linear")    return EasingCurve::Linear;
		if (s == "EaseIn")    return EasingCurve::EaseIn;
		if (s == "EaseOut")   return EasingCurve::EaseOut;
		if (s == "EaseInOut") return EasingCurve::EaseInOut;
		return EasingCurve::SmoothStep;
	}

	void SkyCloudSystem::FinalizeBasePresets()
	{
		// Snapshot all presets as they exist after WeatherPresets.lua has run.
		// This becomes the restore point used at every level load.
		_basePresets = _presets;
	}

	void SkyCloudSystem::Initialize()
	{
		// Restore presets to the post-WeatherPresets.lua baseline so that
		// per-level overrides from previous levels do not leak into this one.
		// Falls back to engine defaults if FinalizeBasePresets() was never called.
		if (!_basePresets.empty())
			_presets = _basePresets;
		else
			InitializePresets();

		_currentPreset = WeatherPresetType::ClearSky;
		_currentState  = _presets[WeatherPresetType::ClearSky].TargetState;
		_transition    = {};
		_manualOverrideCloudA = false;
		_manualOverrideCloudB = false;
		_manualOverrideLayer1 = false;
		_manualOverrideLayer2 = false;
		_cloudATransmittance  = 1.0f;
		_cloudBTransmittance  = 1.0f;
		_nextPresetDwellElapsed = 0.0f;
		_nextPresetDwellTarget  = -1.0f;
		_layerDwellB = {};
		_driftOutA = {};
		_driftOutB = {};
		_dynamicSkyAuroraForced = false;
		_dwellRNG.seed(std::random_device{}());

		// Apply weather config from Gameflow.lua (level.dynamicSky.*).
		auto* level = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));
		if (level)
		{
			const auto& dyn    = level->DynamicSky;
			const auto& clouds = dyn.Clouds;
			const auto& aurora = dyn.Aurora;

			// --- Atmospheric sky dome ---
			auto& atmo = g_Renderer.GetAtmosphericSkySettings();
			atmo.Enabled = dyn.RealisticSkyDome;

			atmo.HorizonColorR = std::clamp(dyn.BlackVoidColor.GetR() / 255.0f, 0.0f, 1.0f);
			atmo.HorizonColorG = std::clamp(dyn.BlackVoidColor.GetG() / 255.0f, 0.0f, 1.0f);
			atmo.HorizonColorB = std::clamp(dyn.BlackVoidColor.GetB() / 255.0f, 0.0f, 1.0f);

			float horizonRise = std::clamp(dyn.HorizonBottomFade, 0.0f, 1.0f);
			atmo.HorizonGradientRise[0] = horizonRise;
			atmo.HorizonGradientRise[1] = horizonRise;

			// --- Aurora ---
			auto& auroraSettings = g_Renderer.GetAuroraSettings();
			auroraSettings.Enabled = aurora.Enabled;
			_dynamicSkyAuroraForced = aurora.Enabled;
			if (aurora.Enabled)
			{
				static const std::pair<const char*, AuroraColorPreset> AURORA_COLOR_NAMES[] =
				{
					{ "GreenClassic",        AuroraColorPreset::GreenClassic        },
					{ "GreenPurple",         AuroraColorPreset::GreenPurple         },
					{ "GreenRedTips",        AuroraColorPreset::GreenRedTips        },
					{ "BluePurple",          AuroraColorPreset::BluePurple          },
					{ "StrongMulticolor",    AuroraColorPreset::StrongMulticolor    },
					{ "TurquoiseBluePurple", AuroraColorPreset::TurquoiseBluePurple }
				};
				for (const auto& kv : AURORA_COLOR_NAMES)
				{
					if (aurora.Color == kv.first)
					{
						auroraSettings.ColorPreset = (int)kv.second;
						break;
					}
				}

				if (aurora.Speed >= 0.0f)
					auroraSettings.Speed = aurora.Speed;

				// Aurora effect implies disabling legacy bitmap sky layer 1.
				level->Layer1.Enabled = false;
			}

			// --- Volumetric clouds ---
			if (clouds.Enabled)
			{
				// Cloud effect implies disabling legacy bitmap sky layer 1.
				level->Layer1.Enabled = false;

				// Per-level quality override.
				if (!clouds.Quality.empty())
				{
					if (clouds.Quality == "Low")
						SetGlobalQuality(CloudQualityPreset::Low);
					else if (clouds.Quality == "Medium")
						SetGlobalQuality(CloudQualityPreset::Medium);
					else if (clouds.Quality == "High")
						SetGlobalQuality(CloudQualityPreset::High);
				}

				if (!clouds.StartPreset.empty())
					SetPresetImmediate(StringToPresetType(clouds.StartPreset));

				// Per-level wind override via level.windSpeed / windDirection.
				// A negative windSpeed means "no override" — keep whatever was set globally
				// from Settings.lua via Flow.SetCloudWind().
				if (level->WindSpeed >= 0.0f)
					SetGlobalWind(level->WindDirection.x, level->WindDirection.y, level->WindSpeed);

				// Per-level cloud wind speed override (independent from base wind magnitude).
				// Negative = derive from base wind as usual.
				if (clouds.WindSpeed >= 0.0f)
					_cloudWindSpeed = std::clamp(clouds.WindSpeed, 0.0f, 8.0f);
				else
					_cloudWindSpeed = -1.0f;

				// Per-level CloudMorph transform duration override.
				if (clouds.TransformDuration >= 0.0f)
				{
					for (auto& [type, def] : _presets)
					{
						if (def.Transform == TransformType::CloudMorph)
							def.TransformDuration = clouds.TransformDuration;
					}
				}

				// changePresets: build a random rotation table among the listed presets.
				// Each entry: { name, duration, percent }.
				// duration = how long the preset stays before chaining (dwell).
				// percent  = relative weight when picking the next preset.
				// When changePresets is not provided, strip every baked-in chain so
				// the level stays on its startPreset without auto-cycling.
				if (clouds.ChangePresets.empty())
				{
					for (auto& [type, def] : _presets)
					{
						def.NextPreset  = "";
						def.NextPresetB = "";
						def.NextPresetCandidates.clear();
						def.NextPresetBCandidates.clear();
						def.NextPresetDwellDuration    = -1.0f;
						def.NextPresetDwellDurationMin = -1.0f;
						def.NextPresetDwellDurationMax = -1.0f;
					}
				}
				else
				{
					for (const auto& src : clouds.ChangePresets)
					{
						auto srcType = StringToPresetType(src.Name);
						auto* srcDef = GetMutablePresetDefinition(srcType);
						if (srcDef == nullptr)
							continue;

						srcDef->NextPresetDwellDuration    = std::max(src.Duration, 0.0f);
						srcDef->NextPresetDwellDurationMin = -1.0f;
						srcDef->NextPresetDwellDurationMax = -1.0f;
						srcDef->NextPresetCandidates.clear();
						srcDef->NextPreset = "";

						for (const auto& tgt : clouds.ChangePresets)
						{
							if (tgt.Name == src.Name || tgt.Percent <= 0.0f)
								continue;

							NextPresetCandidate cand;
							cand.Name               = tgt.Name;
							cand.Weight             = std::max(tgt.Percent, 0.0f);
							cand.WeightNight        = -1.0f;
							cand.TransitionDuration = (clouds.TransformDuration >= 0.0f)
								? clouds.TransformDuration : srcDef->DefaultTransitionDuration;
							srcDef->NextPresetCandidates.push_back(std::move(cand));
						}
					}
				}
			}

			// changePresets are applied AFTER SetPresetImmediate, so the dwell timer that
			// started inside SetPresetImmediate used the baked-in dwell range rather than the
			// Lua-specified duration. Reset it now so the very first cycle uses the correct value.
			if (!clouds.ChangePresets.empty())
			{
				auto it = _presets.find(_currentPreset);
				if (it != _presets.end())
				{
					_nextPresetDwellTarget  = -1.0f;
					_nextPresetDwellElapsed = 0.0f;
					StartNextPresetDwell(it->second);
				}
			}

			// --- Per-level alto cloud color overrides ---
			// Applied to every preset definition AND the current snapshot so that
			// every preset transition shares the level's chosen color palette.
			ApplyCloudColorOverrides(clouds);

			// --- God rays ---
			g_Renderer.GetGodRaySettings().Enabled = dyn.GodRays.Enabled;

			// --- Moon (level.moonLens) ---
			ApplyMoonLensOverride(level->MoonLens);

			// --- Dust storm (level.dustStorm) ---
			ApplyDustStormOverride(level->DustStorm);

			// --- Underwater sky (level.underwaterSky) ---
			ApplyUnderwaterSkyOverride(level->UnderwaterSky);
		}
	}

	// ====================================================================
	// Per-level Lua override appliers
	// ====================================================================

	// Convert a ScriptColor (0..255 channels) to floating-point 0..1 RGB.
	static void ScriptColorToFloatRGB(const TEN::Scripting::Types::ScriptColor& src,
	                                  float& r, float& g, float& b)
	{
		r = std::clamp(src.GetR() / 255.0f, 0.0f, 1.0f);
		g = std::clamp(src.GetG() / 255.0f, 0.0f, 1.0f);
		b = std::clamp(src.GetB() / 255.0f, 0.0f, 1.0f);
	}

	static void ApplyAltoColorToLayer(VolumetricCloudLayerSnapshot& layer,
	                                  bool hasColor, float r, float g, float b,
	                                  bool hasDarkColor, float dr, float dg, float db)
	{
		if (hasColor)
		{
			layer.AltoCloudColorR = r;
			layer.AltoCloudColorG = g;
			layer.AltoCloudColorB = b;
		}
		if (hasDarkColor)
		{
			layer.AltoCloudColorDarkR = dr;
			layer.AltoCloudColorDarkG = dg;
			layer.AltoCloudColorDarkB = db;
		}
	}

	void SkyCloudSystem::ApplyCloudColorOverrides(const TEN::Scripting::DynamicSkyClouds& clouds)
	{
		if (!clouds.HasColor && !clouds.HasDarkColor)
			return;

		float r = 0.0f, g = 0.0f, b = 0.0f;
		float dr = 0.0f, dg = 0.0f, db = 0.0f;
		if (clouds.HasColor)
			ScriptColorToFloatRGB(clouds.Color, r, g, b);
		if (clouds.HasDarkColor)
			ScriptColorToFloatRGB(clouds.DarkColor, dr, dg, db);

		// Apply to every preset's TargetState so all future transitions inherit them.
		for (auto& [type, def] : _presets)
		{
			ApplyAltoColorToLayer(def.TargetState.CloudA,
				clouds.HasColor, r, g, b, clouds.HasDarkColor, dr, dg, db);
			ApplyAltoColorToLayer(def.TargetState.CloudB,
				clouds.HasColor, r, g, b, clouds.HasDarkColor, dr, dg, db);
		}

		// Also apply to the active snapshot so the level starts with the new color
		// (the snapshot was captured before this override ran).
		ApplyAltoColorToLayer(_currentState.CloudA,
			clouds.HasColor, r, g, b, clouds.HasDarkColor, dr, dg, db);
		ApplyAltoColorToLayer(_currentState.CloudB,
			clouds.HasColor, r, g, b, clouds.HasDarkColor, dr, dg, db);
	}

	void SkyCloudSystem::ApplyMoonLensOverride(const TEN::Scripting::MoonLens& moon)
	{
		auto& settings = g_Renderer.GetMoonSettings();

		settings.Enabled = moon.GetEnabled();
		if (!moon.GetEnabled())
			return;

		// Same convention as the lens flare sun: pitch in degrees from horizon
		// (0 = horizon, 90 = zenith), yaw in degrees compass direction.
		settings.Pitch = std::clamp(moon.GetPitch(), -10.0f, 90.0f);
		settings.Yaw   = moon.GetYaw();
	}

	void SkyCloudSystem::ApplyDustStormOverride(const TEN::Scripting::LevelDustStorm& dust)
	{
		auto& settings = g_Renderer.GetDustStormSettings();

		settings.Enabled = dust.Enabled;

		if (dust.Density >= 0.0f)
			settings.Density = std::clamp(dust.Density, 0.0f, 2.0f);

		if (dust.MinHeight >= 0.0f)
			settings.MinHeight = std::clamp(dust.MinHeight, 0.0f, 1.0f);

		if (dust.MaxHeight >= 0.0f)
			settings.MaxHeight = std::clamp(dust.MaxHeight, 0.0f, 1.0f);

		if (dust.WindCoupling >= 0.0f)
			settings.WindSpeedScale = std::clamp(dust.WindCoupling, 0.0f, 4.0f);

		if (dust.HasColor)
			ScriptColorToFloatRGB(dust.Color, settings.ColorR, settings.ColorG, settings.ColorB);
	}

	void SkyCloudSystem::ApplyUnderwaterSkyOverride(const TEN::Scripting::LevelUnderwaterSky& uw)
	{
		auto& settings = g_Renderer.GetUnderwaterSkySettings();

		// Enabled toggles the WaterSurface Layer A preset on/off.
		if (uw.HasEnabled)
		{
			if (uw.Enabled)
				SetLayerAPresetImmediate(WeatherPresetType::WaterSurface);
			else if (_layerAPreset == WeatherPresetType::WaterSurface)
				SetLayerAPresetImmediate(WeatherPresetType::Nothing);
		}

		if (uw.WaveSpeed >= 0.0f)
			settings.WaveSpeed = std::clamp(uw.WaveSpeed, 0.0f, 3.0f);

		if (uw.HasColor)
			ScriptColorToFloatRGB(uw.Color, settings.ColorR, settings.ColorG, settings.ColorB);
	}

	// ====================================================================
	// Preset Registry
	// ====================================================================

	void SkyCloudSystem::InitializePresets()
	{
		_presets.clear();

		// ----- ClearSky -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::ClearSky;
			def.Name = "ClearSky";
			def.DefaultTransitionDuration = 60.0f;
			def.NextPresetDwellDuration   = 1.0f;
			def.NextPresetBCandidates = {
				{ "Altocumulus", 1.0f, 1.0f, 1.0f }
			};

			auto& a = def.TargetState.CloudA;
			a.Enabled             = false;
			a.Category            = CloudCategory::AltocumulusMid;
			a.Coverage            = 0.0f;
			a.BottomHeight        = 2500.0f;
			a.EvolutionSpeed      = 4.532f;
			a.HorizonFade         = 1.0f;
			a.DistanceFade        = 0.0f;
			a.AltoBillowStrength  = 0.458f;
			a.AltoCovSoftWidth    = 0.165f;
			a.AltoAbsorption      = 0.1f;
			a.AltoCloudSize       = 0.412f;
			a.AltoCloudAmount     = 0.451f;
			a.AltoCloudBrightness = 0.769f;
			a.AltoCloudColorR     = 1.0f;
			a.AltoCloudColorG     = 1.0f;
			a.AltoCloudColorB     = 1.0f;
			a.AltoCloudColorDarkR = 0.693f;
			a.AltoCloudColorDarkG = 0.693f;
			a.AltoCloudColorDarkB = 0.873f;
			a.AltoFbmLacunarity   = 4.0f;
			a.AltoFbmGain         = 0.587f;
			a.AltoThickness       = 5000.0f;
			a.AltoBottomSoftness  = 0.153f;
			a.AltoZenithBias      = 1.0f;
			a.AltoHeightBlendPower = 1.564f;

			auto& b = def.TargetState.CloudB;
			b.Enabled             = false;
			b.Category            = CloudCategory::AltocumulusMid;
			b.Coverage            = 0.0f;
			b.BottomHeight        = 2127.0f;
			b.Thickness           = 3252.0f;
			b.EvolutionSpeed      = 5.0f;
			b.HorizonFade         = 1.0f;
			b.DistanceFade        = 0.0f;
			b.AltoBillowStrength  = 1.0f;
			b.AltoCovSoftWidth    = 0.25f;
			b.AltoAbsorption      = 0.1f;
			b.AltoCloudSize       = 0.509f;
			b.AltoCloudAmount     = 0.0f;
			b.AltoCloudBrightness = 1.034f;
			b.AltoCloudColorR     = 1.0f;
			b.AltoCloudColorG     = 1.0f;
			b.AltoCloudColorB     = 1.0f;
			b.AltoCloudColorDarkR = 0.55f;
			b.AltoCloudColorDarkG = 0.55f;
			b.AltoCloudColorDarkB = 0.65f;
			b.AltoFbmLacunarity   = 4.0f;
			b.AltoFbmGain         = 0.687f;
			b.AltoThickness       = 1480.0f;
			b.AltoBottomSoftness  = 1.0f;
			b.AltoZenithBias      = 0.0f;
			b.AltoHeightBlendPower = 1.0f;

			_presets[def.Type] = def;
		}

		// ----- RainSnowOvercast -----
		// Heavy uniform overcast: large slow-rolling blanketing clouds.
		// Uses CloudB only (AltocumulusMid).
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::RainSnowOvercast;
			def.Name = "RainSnowOvercast";
			def.DefaultTransitionDuration  = 60.0f;
			def.NextPresetDwellDurationMin = 6.0f;
			def.NextPresetDwellDurationMax = 9.0f;
			def.NextPresetBCandidates = {
				{ "Altocumulus", 1.0f, -1.0f, 50.0f }
			};

			auto& b = def.TargetState.CloudB;
			b.Enabled             = true;
			b.Category            = CloudCategory::AltocumulusMid;
			b.Coverage            = 1.0f;
			b.BottomHeight        = 4061.0f;
			b.Thickness           = 1000.0f;
			b.EvolutionSpeed      = 5.0f;
			b.HorizonFade         = 1.0f;
			b.DistanceFade        = 0.0f;
			b.AltoBillowStrength  = 0.0f;
			b.AltoCovSoftWidth    = 0.25f;
			b.AltoAbsorption      = 0.1f;
			b.AltoCloudSize       = 0.317f;
			b.AltoCloudAmount     = 0.799f;
			b.AltoCloudBrightness = 0.932f;
			b.AltoCloudColorR     = 1.0f;
			b.AltoCloudColorG     = 1.0f;
			b.AltoCloudColorB     = 1.0f;
			b.AltoCloudColorDarkR = 0.55f;
			b.AltoCloudColorDarkG = 0.55f;
			b.AltoCloudColorDarkB = 0.65f;
			b.AltoFbmLacunarity   = 4.0f;
			b.AltoFbmGain         = 0.401f;
			b.AltoThickness       = 5000.0f;
			b.AltoBottomSoftness  = 1.0f;
			b.AltoZenithBias      = 0.0f;
			b.AltoHeightBlendPower = 1.0f;
			b.BlendThresholdHigh  = 1.0f;
			b.BlendThresholdLow   = 0.004f;

			_presets[def.Type] = def;
		}

		// ----- CloudsTransformation -----
		// General cloud morph transform preset. When transitioning TO a preset that
		// has transformPreset = "CloudsTransformation", the morph mechanism dissolves
		// old clouds and forms new clouds matching the target preset.
		// The TargetState here is clear sky (both layers disabled) — used as fallback
		// for SetPresetImmediate or when CloudsTransformation is itself the target.
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::CloudsTransformation;
			def.Name = "CloudsTransformation";
			def.Transform = TransformType::CloudMorph;
			def.DefaultTransitionDuration = 45.0f;

			// TargetState is clear sky (fallback for direct transitions).
			def.TargetState.CloudA.Enabled = false;
			def.TargetState.CloudB.Enabled = false;

			_presets[def.Type] = def;
		}

		// ----- Thunderstorm -----
		// Dark threatening overcast with lightning; large dense cloud masses.
		// CloudA disabled (high-altitude decor), CloudB active (heavy storm deck).
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Thunderstorm;
			def.Name = "Thunderstorm";
			def.DefaultTransitionDuration  = 60.0f;
			def.NextPresetDwellDurationMin = 6.0f;
			def.NextPresetDwellDurationMax = 9.0f;
			def.NextPresetBCandidates = {
				{ "Altocumulus",      0.7f, -1.0f, 60.0f },
				{ "RainSnowOvercast", 0.3f, -1.0f, 90.0f }
			};

			auto& a = def.TargetState.CloudA;
			a.Enabled             = false;
			a.Category            = CloudCategory::AltocumulusMid;
			a.Coverage            = 0.0f;
			a.BottomHeight        = 23136.0f;
			a.Thickness           = 100.0f;
			a.EvolutionSpeed      = 5.0f;
			a.HorizonFade         = 0.0f;
			a.DistanceFade        = 0.637f;
			a.AltoBillowStrength  = 0.26f;
			a.AltoCovSoftWidth    = 0.25f;
			a.AltoAbsorption      = 0.142f;
			a.AltoCloudSize       = 0.2f;
			a.AltoCloudAmount     = 0.506f;
			a.AltoCloudBrightness = 1.629f;
			a.AltoCloudColorR     = 1.0f;
			a.AltoCloudColorG     = 1.0f;
			a.AltoCloudColorB     = 1.0f;
			a.AltoCloudColorDarkR = 0.55f;
			a.AltoCloudColorDarkG = 0.55f;
			a.AltoCloudColorDarkB = 0.65f;
			a.AltoFbmLacunarity   = 4.0f;
			a.AltoFbmGain         = 0.315f;
			a.AltoThickness       = 5000.0f;
			a.AltoBottomSoftness  = 1.0f;
			a.AltoZenithBias      = -1.0f;
			a.AltoHeightBlendPower = 0.25f;

			auto& b = def.TargetState.CloudB;
			b.Enabled             = true;
			b.Category            = CloudCategory::AltocumulusMid;
			b.Coverage            = 1.0f;
			b.BottomHeight        = 2663.0f;
			b.Thickness           = 100.0f;
			b.EvolutionSpeed      = 5.0f;
			b.HorizonFade         = 0.724f;
			b.DistanceFade        = 0.0f;
			b.AltoBillowStrength  = 0.0f;
			b.AltoCovSoftWidth    = 0.25f;
			b.AltoAbsorption      = 0.1f;
			b.AltoCloudSize       = 0.536f;
			b.AltoCloudAmount     = 0.594f;
			b.AltoCloudBrightness = 0.401f;
			b.AltoCloudColorR     = 1.0f;
			b.AltoCloudColorG     = 1.0f;
			b.AltoCloudColorB     = 1.0f;
			b.AltoCloudColorDarkR = 0.13f;
			b.AltoCloudColorDarkG = 0.13f;
			b.AltoCloudColorDarkB = 0.172f;
			b.AltoFbmLacunarity   = 4.0f;
			b.AltoFbmGain         = 0.591f;
			b.AltoThickness       = 5000.0f;
			b.AltoBottomSoftness  = 0.745f;
			b.AltoZenithBias      = 0.0f;
			b.AltoHeightBlendPower = 4.0f;
			b.LightningEnabled          = true;
			b.LightningStrikeFreq       = 1.0f;
			b.LightningInternalFreq     = 0.423f;
			b.LightningSpeed            = 1.557f;
			b.LightningInternalSpeed    = 3.116f;
			b.LightningGlowIntensity    = 5.0f;
			b.LightningFlashIntensity   = 5.0f;
			b.LightningBoltColorR       = 0.363f;
			b.LightningBoltColorG       = 0.303f;
			b.LightningBoltColorB       = 0.98f;
			b.LightningAmbientContrib   = 0.322f;
			b.LightningBoltLengthScale    = 1.63f;
			b.LightningBoltThicknessScale = 1.732f;
			b.BlendThresholdHigh  = 1.0f;
			b.BlendThresholdLow   = 0.004f;

			_presets[def.Type] = def;
		}

		// ----- Overcast -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Overcast;
			def.Name = "Overcast";
			def.DefaultTransitionDuration = 60.0f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::AltocumulusMid;
			a.Coverage      = 0.85f;
			a.BottomHeight  = 1800.0f;
			a.Thickness     = 2200.0f;
			a.EvolutionSpeed = 0.1f;

			_presets[def.Type] = def;
		}

		// ----- Altocumulus -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Altocumulus;
			def.Name = "Altocumulus";
			def.DefaultTransitionDuration  = 40.0f;
			def.NextPresetDwellDurationMin = 6.0f;
			def.NextPresetDwellDurationMax = 9.0f;
			def.NextPresetBCandidates = {
				{ "ClearSky", 1.0f, -1.0f, 50.0f }
			};

			auto& b = def.TargetState.CloudB;
			b.Enabled             = true;
			b.Category            = CloudCategory::AltocumulusMid;
			b.Coverage            = 1.0f;
			b.BottomHeight        = 2127.0f;
			b.Thickness           = 3252.0f;
			b.EvolutionSpeed      = 5.0f;
			b.HorizonFade         = 1.0f;
			b.DistanceFade        = 0.0f;
			b.AltoBillowStrength  = 1.0f;
			b.AltoCovSoftWidth    = 0.25f;
			b.AltoAbsorption      = 0.1f;
			b.AltoCloudSize       = 0.509f;
			b.AltoCloudAmount     = 0.640f;
			b.AltoCloudBrightness = 1.034f;
			b.AltoCloudColorR     = 1.0f;
			b.AltoCloudColorG     = 1.0f;
			b.AltoCloudColorB     = 1.0f;
			b.AltoCloudColorDarkR = 0.55f;
			b.AltoCloudColorDarkG = 0.55f;
			b.AltoCloudColorDarkB = 0.65f;
			b.AltoFbmLacunarity   = 4.0f;
			b.AltoFbmGain         = 0.5f;
			b.AltoThickness       = 1480.0f;
			b.AltoBottomSoftness  = 1.0f;
			b.AltoZenithBias      = 0.0f;
			b.AltoHeightBlendPower = 1.0f;
			b.BlendThresholdHigh  = 1.0f;
			b.BlendThresholdLow   = 0.004f;

			_presets[def.Type] = def;
		}

		// ----- Aurora -----
		{
			WeatherPresetDefinition def;
			def.Type           = WeatherPresetType::Aurora;
			def.Name           = "Aurora";
			def.IsLayerAPreset = true;
			def.DefaultTransitionDuration = 50.0f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::Aurora;
			a.Coverage      = 0.65f;
			a.BottomHeight  = 1200.0f;
			a.Thickness     = 2000.0f;
			a.EvolutionSpeed = 0.1f;

			_presets[def.Type] = def;
		}

		// ----- StormBuildUp -----
		// Thickening overcast with dark undertones.
		// Cloud A: high-altitude Altocumulus overcast; Cloud B: towering storm cells.
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::StormBuildUp;
			def.Name = "StormBuildUp";
			def.DefaultTransitionDuration  = 90.0f;
			def.HighLayerLeadFraction      = 0.2f;
			def.NextPresetDwellDurationMin = 6.0f;
			def.NextPresetDwellDurationMax = 9.0f;
			def.NextPresetBCandidates = {
				{ "ClearSky",     0.3f, -1.0f, 60.0f },
				{ "Thunderstorm", 0.7f, -1.0f, 90.0f }
			};

			auto& a = def.TargetState.CloudA;
			a.Enabled             = true;
			a.Category            = CloudCategory::AltocumulusMid;
			a.Coverage            = 0.45f;
			a.BottomHeight        = 2500.0f;
			a.EvolutionSpeed      = 4.532f;
			a.HorizonFade         = 1.0f;
			a.DistanceFade        = 0.0f;
			a.AltoBillowStrength  = 0.458f;
			a.AltoCovSoftWidth    = 0.165f;
			a.AltoAbsorption      = 0.1f;
			a.AltoCloudSize       = 0.412f;
			a.AltoCloudAmount     = 0.451f;
			a.AltoCloudBrightness = 0.769f;
			a.AltoCloudColorR     = 1.0f;
			a.AltoCloudColorG     = 1.0f;
			a.AltoCloudColorB     = 1.0f;
			a.AltoCloudColorDarkR = 0.693f;
			a.AltoCloudColorDarkG = 0.693f;
			a.AltoCloudColorDarkB = 0.873f;
			a.AltoFbmLacunarity   = 4.0f;
			a.AltoFbmGain         = 0.853f;
			a.AltoThickness       = 5000.0f;
			a.AltoBottomSoftness  = 0.153f;
			a.AltoZenithBias      = 0.0f;
			a.AltoHeightBlendPower = 1.0f;

			auto& b = def.TargetState.CloudB;
			b.Enabled             = true;
			b.Category            = CloudCategory::AltocumulusMid;
			b.Coverage            = 1.0f;
			b.BottomHeight        = 2663.0f;
			b.EvolutionSpeed      = 4.034f;
			b.HorizonFade         = 0.787f;
			b.DistanceFade        = 0.205f;
			b.AltoBillowStrength  = 0.0f;
			b.AltoCovSoftWidth    = 0.2281f;
			b.AltoAbsorption      = 0.528f;
			b.AltoCloudSize       = 0.225f;
			b.AltoCloudAmount     = 0.0f;
			b.AltoCloudBrightness = 1.585f;
			b.AltoCloudColorR     = 1.0f;
			b.AltoCloudColorG     = 1.0f;
			b.AltoCloudColorB     = 1.0f;
			b.AltoCloudColorDarkR = 0.585f;
			b.AltoCloudColorDarkG = 0.636f;
			b.AltoCloudColorDarkB = 0.76f;
			b.AltoFbmLacunarity   = 4.0f;
			b.AltoFbmGain         = 0.506f;
			b.AltoThickness       = 5000.0f;
			b.AltoBottomSoftness  = 0.769f;
			b.AltoZenithBias      = 1.0f;
			b.AltoHeightBlendPower = 2.637f;
			b.BlendThresholdHigh  = 0.010f;
			b.BlendThresholdLow   = 0.0f;

			_presets[def.Type] = def;
		}

		// ----- Cirrustratus -----
		// Partially cloudy sky (Cloud A) with an optional high disabled layer (Cloud B).
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Cirrustratus;
			def.Name = "Cirrustratus";
			def.DefaultTransitionDuration  = 30.0f;
			def.NextPresetDwellDurationMin = 6.0f;
			def.NextPresetDwellDurationMax = 9.0f;
			auto& a = def.TargetState.CloudA;
			a.Enabled             = true;
			a.Category            = CloudCategory::AltocumulusMid;
			a.Coverage            = 0.45f;
			a.BottomHeight        = 2500.0f;
			a.EvolutionSpeed      = 4.532f;
			a.HorizonFade         = 1.0f;
			a.DistanceFade        = 0.0f;
			a.AltoBillowStrength  = 0.458f;
			a.AltoCovSoftWidth    = 0.165f;
			a.AltoAbsorption      = 0.1f;
			a.AltoCloudSize       = 0.412f;
			a.AltoCloudAmount     = 0.451f;
			a.AltoCloudBrightness = 0.769f;
			a.AltoCloudColorR     = 1.0f;
			a.AltoCloudColorG     = 1.0f;
			a.AltoCloudColorB     = 1.0f;
			a.AltoCloudColorDarkR = 0.693f;
			a.AltoCloudColorDarkG = 0.693f;
			a.AltoCloudColorDarkB = 0.873f;
			a.AltoFbmLacunarity   = 4.0f;
			a.AltoFbmGain         = 0.853f;
			a.AltoThickness       = 5000.0f;
			a.AltoBottomSoftness  = 0.153f;
			a.AltoZenithBias      = 0.0f;
			a.AltoHeightBlendPower = 1.0f;
			a.BlendThresholdHigh  = 0.8f;
			a.BlendThresholdLow   = 0.0f;

			auto& b = def.TargetState.CloudB;
			b.Enabled             = false;
			b.Category            = CloudCategory::AltocumulusMid;
			b.Coverage            = 1.0f;
			b.BottomHeight        = 11028.0f;
			b.AltoHorizonWidth    = 0.184f;
			b.EvolutionSpeed      = 5.0f;
			b.HorizonFade         = 0.836f;
			b.DistanceFade        = 0.278f;
			b.AltoBillowStrength  = 1.0f;
			b.AltoCovSoftWidth    = 0.159f;
			b.AltoAbsorption      = 0.1f;
			b.AltoCloudSize       = 0.225f;
			b.AltoCloudAmount     = 0.0f;
			b.AltoCloudBrightness = 1.585f;
			b.AltoCloudColorR     = 1.0f;
			b.AltoCloudColorG     = 1.0f;
			b.AltoCloudColorB     = 1.0f;
			b.AltoCloudColorDarkR = 0.585f;
			b.AltoCloudColorDarkG = 0.636f;
			b.AltoCloudColorDarkB = 0.76f;
			b.AltoFbmLacunarity   = 4.0f;
			b.AltoFbmGain         = 0.506f;
			b.AltoThickness       = 5000.0f;
			b.AltoBottomSoftness  = 0.769f;
			b.AltoZenithBias      = 1.0f;
			b.AltoHeightBlendPower = 1.924f;

			_presets[def.Type] = def;
		}

		// ----- CirrocumulusLots -----
		// Dense rippled mackerel-sky pattern at high altitude.
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::CirrocumulusLots;
			def.Name = "CirrocumulusLots";
			def.DefaultTransitionDuration  = 45.0f;
			def.NextPresetDwellDurationMin = 6.0f;
			def.NextPresetDwellDurationMax = 9.0f;
			auto& b = def.TargetState.CloudB;
			b.Enabled             = true;
			b.Category            = CloudCategory::AltocumulusMid;
			b.Coverage            = 1.0f;
			b.BottomHeight        = 1536.0f;
			b.AltoHorizonWidth    = 0.031f;
			b.EvolutionSpeed      = 3.043f;
			b.HorizonFade         = 1.0f;
			b.DistanceFade        = 0.0f;
			b.AltoBillowStrength  = 0.0f;
			b.AltoCovSoftWidth    = 0.25f;
			b.AltoAbsorption      = 0.1f;
			b.AltoCloudSize       = 1.637f;
			b.AltoCloudAmount     = 0.36f;
			b.AltoCloudBrightness = 1.0f;
			b.AltoCloudColorR     = 1.0f;
			b.AltoCloudColorG     = 1.0f;
			b.AltoCloudColorB     = 1.0f;
			b.AltoCloudColorDarkR = 0.693f;
			b.AltoCloudColorDarkG = 0.693f;
			b.AltoCloudColorDarkB = 0.873f;
			b.AltoFbmLacunarity   = 4.0f;
			b.AltoFbmGain         = 0.486f;
			b.AltoThickness       = 344.0f;
			b.AltoBottomSoftness  = 0.427f;
			b.AltoZenithBias      = 0.0f;
			b.AltoHeightBlendPower = 1.0f;
			b.BlendThresholdHigh  = 0.053f;
			b.BlendThresholdLow   = 0.0f;

			_presets[def.Type] = def;
		}

		// ----- CirrocumulusFew -----
		// Sparse patches of rippled cirrocumulus in an otherwise open sky.
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::CirrocumulusFew;
			def.Name = "CirrocumulusFew";
			def.DefaultTransitionDuration  = 30.0f;
			def.NextPresetDwellDurationMin = 6.0f;
			def.NextPresetDwellDurationMax = 9.0f;
			auto& b = def.TargetState.CloudB;
			b.Enabled             = true;
			b.Category            = CloudCategory::AltocumulusMid;
			b.Coverage            = 1.0f;
			b.BottomHeight        = 1536.0f;
			b.AltoHorizonWidth    = 0.0052f;
			b.EvolutionSpeed      = 5.0f;
			b.HorizonFade         = 1.0f;
			b.DistanceFade        = 0.0f;
			b.AltoBillowStrength  = 0.0f;
			b.AltoCovSoftWidth    = 0.25f;
			b.AltoAbsorption      = 0.1f;
			b.AltoCloudSize       = 1.318f;
			b.AltoCloudAmount     = 0.460f;
			b.AltoCloudBrightness = 1.352f;
			b.AltoCloudColorR     = 0.696f;
			b.AltoCloudColorG     = 0.696f;
			b.AltoCloudColorB     = 0.696f;
			b.AltoCloudColorDarkR = 0.348f;
			b.AltoCloudColorDarkG = 0.348f;
			b.AltoCloudColorDarkB = 0.348f;
			b.AltoFbmLacunarity   = 4.0f;
			b.AltoFbmGain         = 0.322f;
			b.AltoThickness       = 1164.0f;
			b.AltoBottomSoftness  = 0.465f;
			b.AltoZenithBias      = 0.269f;
			b.AltoHeightBlendPower = 0.749f;
			b.BlendThresholdHigh      = 0.020f;
			b.BlendThresholdHighWidth = 0.005f;
			b.BlendThresholdLow       = 0.0f;

			_presets[def.Type] = def;
		}

		// ----- Nothing -----
		// Layer A placeholder: cloud A is disabled. No aurora, no water surface.
		{
			WeatherPresetDefinition def;
			def.Type           = WeatherPresetType::Nothing;
			def.Name           = "Nothing";
			def.IsLayerAPreset = true;
			def.DefaultTransitionDuration = 30.0f;

			def.TargetState.CloudA.Enabled  = false;
			def.TargetState.CloudA.Category = CloudCategory::None;

			_presets[def.Type] = def;
		}

		// ----- WaterSurface -----
		// Layer A preset: underwater water-surface effect with caustic waves and
		// god-ray light shafts. Renderer-side parameters live in UnderwaterSkySettings.
		{
			WeatherPresetDefinition def;
			def.Type           = WeatherPresetType::WaterSurface;
			def.Name           = "WaterSurface";
			def.IsLayerAPreset = true;
			def.DefaultTransitionDuration = 30.0f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::UnderwaterSky;
			a.Coverage      = 1.0f;
			a.BottomHeight  = 1200.0f;
			a.Thickness     = 2000.0f;
			a.EvolutionSpeed = 0.1f;

			_presets[def.Type] = def;
		}
	}

	// ====================================================================
	// Per-frame update
	// ====================================================================

	void SkyCloudSystem::Update(float deltaTime)
	{
		if (_transition.Active)
			UpdateTransition(deltaTime);
		else
			UpdatePresetDwell(deltaTime);

		// Per-layer independent transitions � run last so they take priority over
		// any CloudA/B values written by the full-preset transition above.
		if (!_manualOverrideCloudA && _layerTransitionA.Active)
			UpdateLayerTransition(deltaTime, _layerTransitionA, _currentState.CloudA);

		if (!_manualOverrideCloudB && _layerTransitionB.Active)
		{
			if (UpdateLayerTransition(deltaTime, _layerTransitionB, _currentState.CloudB))
				StartLayerDwell(_layerBPreset, _layerDwellB);
		}
		else
		{
			UpdateLayerDwell(deltaTime, _layerDwellB, _layerBPreset);
		}

		// Drift-out: wind-directional dissolution when dwell expired with no NextPreset.
		if (!_manualOverrideCloudA && _driftOutA.Active)
			UpdateDriftOut(deltaTime, _driftOutA, _currentState.CloudA);
		if (!_manualOverrideCloudB && _driftOutB.Active)
			UpdateDriftOut(deltaTime, _driftOutB, _currentState.CloudB);
	}

	// ====================================================================
	// Transition system
	// ====================================================================

	void SkyCloudSystem::UpdateTransition(float deltaTime)
	{
		auto& tr = _transition;
		tr.Elapsed += deltaTime;

		// Look up transform type once — used by completion and update logic.
		auto targetIt = _presets.find(tr.Target);
		bool isMorph  = targetIt != _presets.end() &&
		                targetIt->second.Transform == TransformType::CloudMorph;

		if (tr.Elapsed >= tr.Duration)
		{
			// Transition complete.
			tr.Progress = 1.0f;
			_currentState  = tr.TargetSnapshot;

			if (isMorph)
			{
				// Morph complete: target snapshot already holds the real destination
				// preset state. Clear transform phases so clouds render normally.
				_currentState.CloudA.DissolvePhase  = 0.0f;
				_currentState.CloudB.DissolvePhase  = 0.0f;
				_currentState.CloudA.FormationPhase = 0.0f;
				_currentState.CloudB.FormationPhase = 0.0f;
				_currentState.CloudA.MorphActive    = 0.0f;
				_currentState.CloudB.MorphActive    = 0.0f;
			}

			_currentPreset = tr.Target;
			tr.Active = false;

				// Auto-chain or drift-out: check dwell / NextPreset for the new active preset.
			auto it = _presets.find(_currentPreset);
			if (it != _presets.end())
				StartNextPresetDwell(it->second);
			return;
		}

		float rawT = tr.Elapsed / tr.Duration;
		float easedT = ApplyEasing(rawT, tr.Curve);
		tr.Progress = easedT;

		// Staged transition: per-layer durations or legacy lead-fraction fallback.
		float highT, lowT;
		if (tr.DurationA != tr.DurationB)
		{
			// Explicit per-layer durations — each layer has its own independent timeline.
			highT = ApplyEasing(std::min(tr.Elapsed / tr.DurationA, 1.0f), tr.Curve);
			lowT  = ApplyEasing(std::min(tr.Elapsed / tr.DurationB, 1.0f), tr.Curve);
		}
		else if (tr.HighLayerLeadFraction > 0.0f)
		{
			// Legacy lead-fraction staging (equal DurationA/B).
			float highRaw = std::clamp(rawT / (1.0f - tr.HighLayerLeadFraction), 0.0f, 1.0f);
			highT = ApplyEasing(highRaw, tr.Curve);
			float lowRaw = std::clamp((rawT - tr.HighLayerLeadFraction) /
				(1.0f - tr.HighLayerLeadFraction), 0.0f, 1.0f);
			lowT = ApplyEasing(lowRaw, tr.Curve);
		}
		else
		{
			highT = easedT;
			lowT  = easedT;
		}

		// Interpolate each component separately for staged transitions.
		SkyCloudSnapshot blended;
		blended.Layer1 = LegacySkyLayerSnapshot::Lerp(tr.SourceSnapshot.Layer1, tr.TargetSnapshot.Layer1, easedT);
		blended.Layer2 = LegacySkyLayerSnapshot::Lerp(tr.SourceSnapshot.Layer2, tr.TargetSnapshot.Layer2, easedT);
		blended.CloudA = VolumetricCloudLayerSnapshot::Lerp(tr.SourceSnapshot.CloudA, tr.TargetSnapshot.CloudA, highT);
		blended.CloudB = VolumetricCloudLayerSnapshot::Lerp(tr.SourceSnapshot.CloudB, tr.TargetSnapshot.CloudB, lowT);

		// --- CloudMorph override ---
		// Shape params are NOT interpolated. Instead they are frozen at the source
		// formation during the dissolve half, then snapped to the target formation
		// during the formation half. Only appearance fields (color, lightning, blend
		// thresholds) use the Lerp'd values computed above.
		if (isMorph)
		{
			if (!_manualOverrideCloudA && !_layerTransitionA.Active)
				ApplyMorphToLayer(blended.CloudA, tr.SourceSnapshot.CloudA,
				                  tr.TargetSnapshot.CloudA, blended.CloudA, highT);
			if (!_manualOverrideCloudB && !_layerTransitionB.Active)
				ApplyMorphToLayer(blended.CloudB, tr.SourceSnapshot.CloudB,
				                  tr.TargetSnapshot.CloudB, blended.CloudB, lowT);
		}

		// Apply manual overrides if set.
		if (!_manualOverrideLayer1) _currentState.Layer1 = blended.Layer1;
		if (!_manualOverrideLayer2) _currentState.Layer2 = blended.Layer2;
		// Skip per-layer if an independent layer transition is running (it takes priority).
		if (!_manualOverrideCloudA && !_layerTransitionA.Active) _currentState.CloudA = blended.CloudA;
		if (!_manualOverrideCloudB && !_layerTransitionB.Active) _currentState.CloudB = blended.CloudB;
	}

	void SkyCloudSystem::SetPresetImmediate(WeatherPresetType preset)
	{
		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		_nextPresetDwellTarget  = -1.0f;
		_nextPresetDwellElapsed = 0.0f;
		_layerDwellB = {};
		_transition.Active      = false;
		_driftOutA.Active       = false;
		_driftOutB.Active       = false;
		_currentPreset = preset;
		_layerAPreset  = preset;
		_layerBPreset  = preset;

		if (it->second.Transform == TransformType::CloudMorph)
		{
			// CloudMorph preset set immediately: clouds already match target.
			// Nothing special needed — just use TargetState.
			_currentState = it->second.TargetState;
			_currentState.CloudA.DissolvePhase  = 0.0f;
			_currentState.CloudB.DissolvePhase  = 0.0f;
			_currentState.CloudA.FormationPhase = 0.0f;
			_currentState.CloudB.FormationPhase = 0.0f;
			_currentState.CloudA.MorphActive    = 0.0f;
			_currentState.CloudB.MorphActive    = 0.0f;
		}
		else
			_currentState = it->second.TargetState;

		// Auto-chain or drift-out: check dwell / NextPreset for the new active preset.
		const auto& def = it->second;
		StartNextPresetDwell(def);
	}

	// Helper: copy all fields that should smoothly interpolate during a CloudMorph transition.
	// This includes appearance (color, brightness, coverage, blend thresholds, lightning) AND all
	// geometry/positional params that are global CB variables in the shader and thus affect
	// BOTH source and target density evaluations simultaneously.
	//
	// NOT copied here (kept as target values in the main CB):
	//   AltoCloudSize, AltoCloudAmount, AltoBillowStrength, AltoCovSoftWidth,
	//   AltoFbmLacunarity, AltoFbmGain, AltoBottomSoftness, AltoZenithBias,
	//   AltoHorizonWidth (cpu shape field), EvolutionSpeed — these are the per-preset
	//   density-shaping params. The source copies live in MorphSrc* CB slots.
	static void CopyMorphAppearance(VolumetricCloudLayerSnapshot& dst,
	                                const VolumetricCloudLayerSnapshot& src)
	{
		// Global layer opacity: smoothly lerp so presets with different Coverage values
		// don't snap on the first frame of the morph.
		dst.Coverage            = src.Coverage;
		// Cloud color and brightness
		dst.AltoCloudColorR     = src.AltoCloudColorR;
		dst.AltoCloudColorG     = src.AltoCloudColorG;
		dst.AltoCloudColorB     = src.AltoCloudColorB;
		dst.AltoCloudColorDarkR = src.AltoCloudColorDarkR;
		dst.AltoCloudColorDarkG = src.AltoCloudColorDarkG;
		dst.AltoCloudColorDarkB = src.AltoCloudColorDarkB;
		dst.AltoCloudBrightness = src.AltoCloudBrightness;
		dst.AltoAbsorption      = src.AltoAbsorption;
		// Blend thresholds
		dst.BlendThresholdHigh      = src.BlendThresholdHigh;
		dst.BlendThresholdHighWidth = src.BlendThresholdHighWidth;
		dst.BlendThresholdLow       = src.BlendThresholdLow;
		// Geometry and positional params — these are global CB variables used by the shader
		// for both source and target density lookups. Snapping them to target values at t=0
		// causes visible position/shape jumps in the source clouds:
		//   AltoThickness   — determines heightFrac for every sample (CRITICAL: 344 vs 1164).
		//   HorizonFade/DistanceFade — global thresholds inside EvalAltoDensityCore.
		//   AltoHeightBlendPower — modifies skyH passed to EvalAltoDensityCore.
		//   AltoHorizonWidth — global zenith-cap applied to the combined morph density.
		dst.BottomHeight        = src.BottomHeight;
		dst.Thickness           = src.Thickness;
		dst.HorizonFade         = src.HorizonFade;
		dst.DistanceFade        = src.DistanceFade;
		dst.HorizonMeshBleed    = src.HorizonMeshBleed;
		dst.AltoThickness       = src.AltoThickness;
		dst.AltoHeightBlendPower = src.AltoHeightBlendPower;
		dst.AltoHorizonWidth    = src.AltoHorizonWidth;
		dst.AltoBleedDepth      = src.AltoBleedDepth;
		dst.AltoHorizonGradientFade = src.AltoHorizonGradientFade;
		// CurlWarpStrength scales curl displacement of p.x/p.z in EvalAltoDensityCore —
		// a global CB variable shared by both source and target evaluations. Snapping to
		// target at t=0 deforms the source clouds with the wrong amplitude → position jump.
		dst.CurlWarpStrength    = src.CurlWarpStrength;
		// AltoFbmScale is assigned to AltoDensityParams.FbmScale for BOTH evaluations
		// (srcParams.FbmScale = AltoFbmScale; line in shader). If presets differ here,
		// the source cloud frequency jumps immediately at transition start.
		dst.AltoFbmScale        = src.AltoFbmScale;
		// NOTE: EvolutionSpeed is intentionally NOT copied here.  evoOfs is now driven
		// by the pre-integrated EvoAccumOffset (accumulated += EvolutionSpeed * dt * 0.05)
		// which never decreases, so snapping EvolutionSpeed in the CB does not cause any
		// backwards positional drift.  Letting EvolutionSpeed snap to the target value from
		// frame 1 is correct: if the target has EvolutionSpeed=0 (e.g. Altocumulus), its
		// clouds should be temporally static (flowTime=0) from the very first frame.
		// Lightning
		dst.LightningEnabled           = src.LightningEnabled;
		dst.LightningStrikeFreq        = src.LightningStrikeFreq;
		dst.LightningInternalFreq      = src.LightningInternalFreq;
		dst.LightningSpeed             = src.LightningSpeed;
		dst.LightningInternalSpeed     = src.LightningInternalSpeed;
		dst.LightningGlowIntensity     = src.LightningGlowIntensity;
		dst.LightningBoltColorR        = src.LightningBoltColorR;
		dst.LightningBoltColorG        = src.LightningBoltColorG;
		dst.LightningBoltColorB        = src.LightningBoltColorB;
		dst.LightningFlashIntensity    = src.LightningFlashIntensity;
		dst.LightningAmbientContrib    = src.LightningAmbientContrib;
		dst.LightningBoltLengthScale   = src.LightningBoltLengthScale;
		dst.LightningBoltThicknessScale = src.LightningBoltThicknessScale;
	}

	// Apply CloudMorph transition to a single cloud layer.
	//
	// Simultaneous dual-density approach:
	//   The shader's main CB params carry the TARGET preset shape.
	//   MorphSrc* CB fields carry the SOURCE preset shape.
	//   When MorphActive=1, the shader evaluates density with BOTH param sets,
	//   applies DissolvePhase to the source density and FormationPhase to the
	//   target density, then combines them. Where source had cloud but target
	//   doesn't → dissolve. Where target has cloud but source doesn't → form.
	//   Where both overlap → natural crossfade.
	//
	// DissolvePhase and FormationPhase both run 0→1 over the full duration.
	// Appearance (color, lightning, blend) always from lerpAppearance.
	static void ApplyMorphToLayer(
		VolumetricCloudLayerSnapshot& out,
		const VolumetricCloudLayerSnapshot& source,
		const VolumetricCloudLayerSnapshot& target,
		const VolumetricCloudLayerSnapshot& lerpAppearance,
		float layerT)
	{
		// IMPORTANT: capture lerpAppearance as a value copy before touching `out`.
		// The caller may alias lerpAppearance with out (e.g. ApplyMorphToLayer(x,...,x,t)).
		// After `out = target` below, a reference alias would silently become target values,
		// turning CopyMorphAppearance into a no-op and snapping colors/thresholds to target.
		const VolumetricCloudLayerSnapshot appearance = lerpAppearance;

		constexpr float kFormEps = 0.002f;

		bool srcOn = source.Enabled;
		bool tgtOn = target.Enabled;

		if (!srcOn && !tgtOn)
		{
			out = source;
			return;
		}

		if (!srcOn && tgtOn)
		{
			// Formation only: grow target clouds over full duration.
			out = target;
			out.Enabled        = true;
			out.MorphActive    = 0.0f;
			out.DissolvePhase  = 0.0f;
			out.FormationPhase = std::max(kFormEps, layerT);
			CopyMorphAppearance(out, appearance);
			return;
		}

		if (srcOn && !tgtOn)
		{
			// Dissolve only: shrink source clouds over full duration.
			out = source;
			out.Enabled        = (layerT < 0.95f);
			out.MorphActive    = 0.0f;
			out.DissolvePhase  = layerT;
			out.FormationPhase = 0.0f;
			return;
		}

		// ---- Both enabled: simultaneous dual-density morph ----
		// Main (target) shape params drive the normal CB fields.
		out = target;
		// Preserve source category so the shader's cloud path matches the dissolving source
		// clouds. Without this, if the target has Category=None (e.g. a "clear" preset with
		// an enabled-but-empty cloud layer), CloudType=0 bypasses the Alto density evaluation
		// entirely and the source clouds vanish on the first frame instead of dissolving.
		out.Category = source.Category;
		out.Enabled = true;

		// Overlay Lerp'd appearance (color, brightness, absorption, lightning, blend thresholds).
		CopyMorphAppearance(out, appearance);

		// Both phases run 0→1 simultaneously over the full duration.
		out.DissolvePhase  = layerT;
		out.FormationPhase = std::max(kFormEps, layerT);

		// Activate dual-density evaluation and supply source preset density params.
		out.MorphActive          = 1.0f;
		out.MorphSrcCloudSize    = source.AltoCloudSize;
		out.MorphSrcCloudAmount  = source.AltoCloudAmount;
		out.MorphSrcBillowStr    = source.AltoBillowStrength;
		out.MorphSrcCovSoftWidth = source.AltoCovSoftWidth;
		out.MorphSrcFbmLac       = source.AltoFbmLacunarity;
		out.MorphSrcFbmGain      = source.AltoFbmGain;
		out.MorphSrcBottomSoft   = source.AltoBottomSoftness;
		out.MorphSrcZenithBias   = source.AltoZenithBias;
		out.MorphSrcEvolutionSpd = source.EvolutionSpeed;
		out.MorphSrcHorizonWidth = source.AltoHorizonWidth;
	}

	void SkyCloudSystem::TransitionToPreset(WeatherPresetType preset, float durationSeconds,
	                                         EasingCurve curve)
	{
		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		// Cancel any pending dwell / drift-out and per-layer transitions before starting a new
		// full-preset transition. Per-layer transitions would otherwise take priority (by design)
		// and silently block the morph from writing to CloudA/CloudB.
		_nextPresetDwellTarget  = -1.0f;
		_nextPresetDwellElapsed = 0.0f;
		_layerDwellB = {};
		_driftOutA.Active = false;
		_driftOutB.Active = false;
		_layerTransitionA.Active = false;
		_layerTransitionB.Active = false;

		const auto& def = it->second;
		float effA = (def.TransitionDurationA >= 0.0f) ? def.TransitionDurationA : durationSeconds;
		float effB = (def.TransitionDurationB >= 0.0f) ? def.TransitionDurationB : durationSeconds;

		// When no per-layer override, apply legacy HighLayerLeadFraction to derive A duration.
		if (def.TransitionDurationA < 0.0f && def.TransitionDurationB < 0.0f && def.HighLayerLeadFraction > 0.0f)
			effA = durationSeconds * (1.0f - def.HighLayerLeadFraction);

		auto& tr = _transition;
		tr.Active           = true;
		tr.Source            = _currentPreset;
		tr.Target            = preset;
		tr.SourceSnapshot    = _currentState;

		// Transform presets override the target snapshot construction.
		if (def.Transform == TransformType::CloudMorph)
		{
			// CloudMorph: target is the actual destination preset. The morph
			// logic in UpdateTransition freezes shape params and drives
			// dissolve/formation phases on the GPU.
			tr.TargetSnapshot = def.TargetState;

			// Apply per-preset transform duration override if set.
			if (def.TransformDuration >= 0.0f)
				durationSeconds = def.TransformDuration;

			// Stagger layers so they don't dissolve simultaneously:
			// higher clouds (A) dissolve faster, lower clouds (B) use full duration.
			bool bothActive = _currentState.CloudA.Enabled && _currentState.CloudB.Enabled;
			effA = bothActive ? (durationSeconds * 0.6f) : durationSeconds;
			effB = durationSeconds;
		}
		else
		{
			tr.TargetSnapshot = def.TargetState;
		}

		tr.DurationA         = std::max(effA, 0.1f);
		tr.DurationB         = std::max(effB, 0.1f);
		tr.Duration          = std::max(tr.DurationA, tr.DurationB);
		tr.Elapsed           = 0.0f;
		tr.Progress          = 0.0f;
		tr.Curve             = curve;
		tr.HighLayerLeadFraction = def.HighLayerLeadFraction;
	}

	void SkyCloudSystem::TransitionToPreset(WeatherPresetType preset, float durationASeconds, float durationBSeconds,
	                                         EasingCurve curve)
	{
		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		// Cancel any pending dwell, drift-out, and per-layer transitions before starting a new
		// full-preset transition. Per-layer transitions would otherwise take priority (by design)
		// and silently block the morph from writing to CloudA/CloudB.
		_nextPresetDwellTarget  = -1.0f;
		_nextPresetDwellElapsed = 0.0f;
		_layerDwellB = {};
		_driftOutA.Active = false;
		_driftOutB.Active = false;
		_layerTransitionA.Active = false;
		_layerTransitionB.Active = false;

		auto& tr = _transition;
		tr.Active           = true;
		tr.Source            = _currentPreset;
		tr.Target            = preset;
		tr.SourceSnapshot    = _currentState;

		// Transform presets override the target snapshot construction.
		if (it->second.Transform == TransformType::CloudMorph)
		{
			tr.TargetSnapshot = it->second.TargetState;

			// Apply per-preset transform duration override if set.
			if (it->second.TransformDuration >= 0.0f)
			{
				durationASeconds = it->second.TransformDuration;
				durationBSeconds = it->second.TransformDuration;
			}

			// Stagger layers: higher clouds dissolve faster.
			bool bothActive = _currentState.CloudA.Enabled && _currentState.CloudB.Enabled;
			float baseDur   = std::max(durationASeconds, durationBSeconds);
			durationASeconds = bothActive ? (baseDur * 0.6f) : baseDur;
			durationBSeconds = baseDur;
		}
		else
		{
			tr.TargetSnapshot = it->second.TargetState;
		}

		tr.DurationA         = std::max(durationASeconds, 0.1f);
		tr.DurationB         = std::max(durationBSeconds, 0.1f);
		tr.Duration          = std::max(tr.DurationA, tr.DurationB);
		tr.Elapsed           = 0.0f;
		tr.Progress          = 0.0f;
		tr.Curve             = curve;
		tr.HighLayerLeadFraction = it->second.HighLayerLeadFraction;
	}

	void SkyCloudSystem::InterruptTransition()
	{
		// Freeze the current blended state and stop transitioning.
		_transition.Active = false;
		// Clear shader dissolve/formation/morph phases — no residual effect.
		_currentState.CloudA.DissolvePhase  = 0.0f;
		_currentState.CloudB.DissolvePhase  = 0.0f;
		_currentState.CloudA.FormationPhase = 0.0f;
		_currentState.CloudB.FormationPhase = 0.0f;
		_currentState.CloudA.MorphActive    = 0.0f;
		_currentState.CloudB.MorphActive    = 0.0f;
		// Also cancel any pending dwell / drift-out that might fire from the interrupted target.
		_nextPresetDwellTarget  = -1.0f;
		_nextPresetDwellElapsed = 0.0f;
		_layerDwellB = {};
		_driftOutA.Active = false;
		_driftOutB.Active = false;
	}

	void SkyCloudSystem::StopAllTransitions()
	{
		// Full-preset transition.
		_transition.Active      = false;
		// Per-layer independent transitions.
		_layerTransitionA.Active = false;
		_layerTransitionB.Active = false;
		// Clear shader dissolve/formation/morph phases — no residual effect.
		_currentState.CloudA.DissolvePhase  = 0.0f;
		_currentState.CloudB.DissolvePhase  = 0.0f;
		_currentState.CloudA.FormationPhase = 0.0f;
		_currentState.CloudB.FormationPhase = 0.0f;
		_currentState.CloudA.MorphActive    = 0.0f;
		_currentState.CloudB.MorphActive    = 0.0f;
		// Preset dwell timer.
		_nextPresetDwellTarget  = -1.0f;
		_nextPresetDwellElapsed = 0.0f;
		_layerDwellB = {};
		// Drift-out dissolve.
		_driftOutA.Active = false;
		_driftOutB.Active = false;
		// Manual overrides — release so the next preset/transition can take over.
		ClearManualOverrides();
	}

	// ====================================================================
	// Independent per-layer preset transitions
	// ====================================================================

	bool SkyCloudSystem::UpdateLayerTransition(float deltaTime, LayerTransitionState& layerTr,
	                                           VolumetricCloudLayerSnapshot& current)
	{
		if (!layerTr.Active)
			return false;

		layerTr.Elapsed += deltaTime;

		auto targetIt = _presets.find(layerTr.TargetPreset);
		bool isMorph  = targetIt != _presets.end() &&
		                targetIt->second.Transform == TransformType::CloudMorph;

		if (layerTr.Elapsed >= layerTr.Duration)
		{
			layerTr.Progress = 1.0f;
			current          = layerTr.Target;

			if (isMorph)
			{
				// Morph complete: clear transform phases.
				current.DissolvePhase  = 0.0f;
				current.FormationPhase = 0.0f;
				current.MorphActive    = 0.0f;
			}

			layerTr.Active   = false;
			return true;
		}

		float rawT   = layerTr.Elapsed / layerTr.Duration;
		float easedT = ApplyEasing(rawT, layerTr.Curve);
		layerTr.Progress = easedT;
		current = VolumetricCloudLayerSnapshot::Lerp(layerTr.Source, layerTr.Target, easedT);

		if (isMorph)
			ApplyMorphToLayer(current, layerTr.Source, layerTr.Target, current, easedT);

		return false;
	}

	void SkyCloudSystem::TransitionLayerAToPreset(WeatherPresetType preset, float durationSeconds,
	                                               EasingCurve curve)
	{
		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		// Release the manual override so UpdateLayerTransition can drive this layer.
		_manualOverrideCloudA = false;

		_layerAPreset = preset;
		auto& tr  = _layerTransitionA;
		tr.Active       = true;
		tr.TargetPreset = preset;
		tr.Source   = _currentState.CloudA;

		// CloudMorph: use the actual destination preset's CloudA as morph target.
		// Regular transitions also use the same target.
		tr.Target = it->second.TargetState.CloudA;

		// Apply per-preset transform duration override if set.
		if (it->second.Transform == TransformType::CloudMorph && it->second.TransformDuration >= 0.0f)
			durationSeconds = it->second.TransformDuration;

		tr.Duration = std::max(durationSeconds, 0.1f);
		tr.Elapsed  = 0.0f;
		tr.Progress = 0.0f;
		tr.Curve    = curve;
	}

	void SkyCloudSystem::TransitionLayerBToPreset(WeatherPresetType preset, float durationSeconds,
	                                               EasingCurve curve)
	{
		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		// Release the manual override so UpdateLayerTransition can drive this layer.
		_manualOverrideCloudB = false;

		_layerBPreset = preset;
		_layerDwellB  = {};
		auto& tr  = _layerTransitionB;
		tr.Active       = true;
		tr.TargetPreset = preset;
		tr.Source   = _currentState.CloudB;

		// CloudMorph: use the actual destination preset's CloudB as morph target.
		tr.Target = it->second.TargetState.CloudB;

		// Apply per-preset transform duration override if set.
		if (it->second.Transform == TransformType::CloudMorph && it->second.TransformDuration >= 0.0f)
			durationSeconds = it->second.TransformDuration;

		tr.Duration = std::max(durationSeconds, 0.1f);
		tr.Elapsed  = 0.0f;
		tr.Progress = 0.0f;
		tr.Curve    = curve;
	}

	void SkyCloudSystem::SetLayerAPresetImmediate(WeatherPresetType preset)
	{
		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		_manualOverrideCloudA    = false;
		_layerTransitionA.Active = false;
		_transition.Active       = false;
		_layerAPreset            = preset;

		if (it->second.Transform == TransformType::CloudMorph)
		{
			_currentState.CloudA = it->second.TargetState.CloudA;
			_currentState.CloudA.DissolvePhase  = 0.0f;
			_currentState.CloudA.FormationPhase = 0.0f;
		}
		else
		{
			_currentState.CloudA = it->second.TargetState.CloudA;
		}

	}

	void SkyCloudSystem::SetLayerBPresetImmediate(WeatherPresetType preset)
	{
		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		_manualOverrideCloudB    = false;
		_layerTransitionB.Active = false;
		_transition.Active       = false;
		_layerBPreset            = preset;

		if (it->second.Transform == TransformType::CloudMorph)
		{
			_currentState.CloudB = it->second.TargetState.CloudB;
			_currentState.CloudB.DissolvePhase  = 0.0f;
			_currentState.CloudB.FormationPhase = 0.0f;
		}
		else
		{
			_currentState.CloudB = it->second.TargetState.CloudB;
		}

		StartLayerDwell(preset, _layerDwellB);
	}

	void SkyCloudSystem::InterruptLayerATransition()
	{
		_layerTransitionA.Active = false;
	}

	void SkyCloudSystem::InterruptLayerBTransition()
	{
		_layerTransitionB.Active = false;
		_layerDwellB = {};
	}

	void SkyCloudSystem::PauseLayerBDwell()
	{
		if (_layerDwellB.Target >= 0.0f)
			_layerDwellB.Paused = true;
	}

	void SkyCloudSystem::ResumeLayerBDwell()
	{
		_layerDwellB.Paused = false;
	}

	bool SkyCloudSystem::IsLayerBDwellPaused() const { return _layerDwellB.Paused; }

	bool SkyCloudSystem::IsLayerATransitioning() const { return _layerTransitionA.Active; }
	bool SkyCloudSystem::IsLayerBTransitioning() const { return _layerTransitionB.Active; }

	float SkyCloudSystem::GetLayerATransitionProgress() const { return _layerTransitionA.Progress; }
	float SkyCloudSystem::GetLayerBTransitionProgress() const { return _layerTransitionB.Progress; }

	// ====================================================================
	// Preset dwell (wait before chaining to NextPreset)
	// ====================================================================

	float SkyCloudSystem::ResolveNextPresetDwell(const WeatherPresetDefinition& def)
	{
		// If a random range is fully specified, roll within it.
		if (def.NextPresetDwellDurationMin >= 0.0f &&
			def.NextPresetDwellDurationMax >= def.NextPresetDwellDurationMin)
		{
			std::uniform_real_distribution<float> dist(
				def.NextPresetDwellDurationMin,
				def.NextPresetDwellDurationMax);
			return dist(_dwellRNG);
		}
		// Fixed value (< 0 means "chain immediately").
		return def.NextPresetDwellDuration;
	}

	void SkyCloudSystem::SetNightBlend(float blend)
	{
		_nightBlend = std::clamp(blend, 0.0f, 1.0f);
	}

	float SkyCloudSystem::GetNightBlend() const
	{
		return _nightBlend;
	}

	// Weighted-random pick from a NextPresetCandidate list.
	// Interpolates between Weight (day) and WeightNight (night) using _nightBlend.
	// Returns nullptr if the list is empty.
	const NextPresetCandidate* SkyCloudSystem::PickNextPresetCandidate(
		const std::vector<NextPresetCandidate>& candidates)
	{
		if (candidates.empty())
			return nullptr;

		auto effectiveWeight = [&](const NextPresetCandidate& c) -> float
		{
			if (c.WeightNight >= 0.0f)
				return c.Weight + (c.WeightNight - c.Weight) * _nightBlend;
			return c.Weight;
		};

		float totalWeight = 0.0f;
		for (const auto& c : candidates)
			totalWeight += std::max(effectiveWeight(c), 0.0f);

		if (totalWeight <= 0.0f)
			return &candidates[0];

		std::uniform_real_distribution<float> dist(0.0f, totalWeight);
		float roll = dist(_dwellRNG);

		float cumulative = 0.0f;
		for (const auto& c : candidates)
		{
			cumulative += std::max(effectiveWeight(c), 0.0f);
			if (roll <= cumulative)
				return &c;
		}

		return &candidates.back();
	}

	// Helper: resolve AB transition durations from a candidate entry.
	static void ResolveABDuration(const NextPresetCandidate& c, float& outA, float& outB)
	{
		outA = (c.TransitionDurationA >= 0.0f) ? c.TransitionDurationA : c.TransitionDuration;
		outB = (c.TransitionDurationB >= 0.0f) ? c.TransitionDurationB : c.TransitionDuration;
	}

	// Fire all applicable next-preset chains (AB, A-only, B-only) for a given preset definition.
	// Used by both StartNextPresetDwell (immediate path) and UpdatePresetDwell (dwell-expired path).
	void SkyCloudSystem::FireNextPresetChains(const WeatherPresetDefinition& def)
	{
		bool anyChainFired = false;

		// --- AB chain (full preset transition) ---
		if (const auto* cab = PickNextPresetCandidate(def.NextPresetCandidates))
		{
			float durA, durB;
			ResolveABDuration(*cab, durA, durB);
			TransitionToPreset(StringToPresetType(cab->Name), durA, durB);
			anyChainFired = true;
		}
		else if (!def.NextPreset.empty())
		{
			float durA = (def.NextPresetTransitionDurationA >= 0.0f)
				? def.NextPresetTransitionDurationA : def.NextPresetTransitionDuration;
			float durB = (def.NextPresetTransitionDurationB >= 0.0f)
				? def.NextPresetTransitionDurationB : def.NextPresetTransitionDuration;
			TransitionToPreset(StringToPresetType(def.NextPreset), durA, durB);
			anyChainFired = true;
		}

		// --- B-only chain (independent CloudB layer transition) ---
		if (const auto* cb = PickNextPresetCandidate(def.NextPresetBCandidates))
		{
			TransitionLayerBToPreset(StringToPresetType(cb->Name), cb->TransitionDuration);
			anyChainFired = true;
		}
		else if (!def.NextPresetB.empty())
		{
			TransitionLayerBToPreset(StringToPresetType(def.NextPresetB), def.NextPresetBDuration);
			anyChainFired = true;
		}

		// No chain fired ? preset stays active indefinitely. No drift-out.
	}

	void SkyCloudSystem::StartNextPresetDwell(const WeatherPresetDefinition& def)
	{
		float dwell = ResolveNextPresetDwell(def);

		// duration < 0 (omitted / -1) ? stay at this preset forever, no chaining.
		if (dwell < 0.0f)
			return;

		// duration == 0 ? fire all chains immediately.
		if (dwell == 0.0f)
		{
			FireNextPresetChains(def);
			return;
		}

		// duration > 0 ? start dwell timer; chains fire when it expires.
		_nextPresetDwellTarget  = dwell;
		_nextPresetDwellElapsed = 0.0f;
	}

	void SkyCloudSystem::StartLayerDwell(WeatherPresetType preset, LayerDwellState& dwellState)
	{
		dwellState = {};

		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		const auto& def = it->second;
		float dwell = ResolveNextPresetDwell(def);
		if (dwell < 0.0f)
			return;

		if (dwell == 0.0f)
		{
			if (const auto* candidate = PickNextPresetCandidate(def.NextPresetBCandidates))
				TransitionLayerBToPreset(StringToPresetType(candidate->Name), candidate->TransitionDuration);
			else if (!def.NextPresetB.empty())
				TransitionLayerBToPreset(StringToPresetType(def.NextPresetB), def.NextPresetBDuration);
			return;
		}

		dwellState.Target = dwell;
	}

	void SkyCloudSystem::UpdateLayerDwell(float deltaTime, LayerDwellState& dwellState, WeatherPresetType preset)
	{
		if (dwellState.Target < 0.0f)
			return;
		if (dwellState.Paused)
			return;

		dwellState.Elapsed += deltaTime;
		if (dwellState.Elapsed < dwellState.Target)
			return;

		dwellState = {};

		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		const auto& def = it->second;
		if (const auto* candidate = PickNextPresetCandidate(def.NextPresetBCandidates))
			TransitionLayerBToPreset(StringToPresetType(candidate->Name), candidate->TransitionDuration);
		else if (!def.NextPresetB.empty())
			TransitionLayerBToPreset(StringToPresetType(def.NextPresetB), def.NextPresetBDuration);
	}

	void SkyCloudSystem::UpdatePresetDwell(float deltaTime)
	{
		if (_nextPresetDwellTarget < 0.0f)
			return; // No dwell pending.

		_nextPresetDwellElapsed += deltaTime;
		if (_nextPresetDwellElapsed < _nextPresetDwellTarget)
			return;

		// Dwell expired � clear timer and fire all chains.
		_nextPresetDwellTarget  = -1.0f;
		_nextPresetDwellElapsed = 0.0f;

		auto it = _presets.find(_currentPreset);
		if (it == _presets.end())
			return;

		FireNextPresetChains(it->second);
	}

	// ====================================================================
	// Drift-out: wind-directional dissolution
	// ====================================================================

	void SkyCloudSystem::StartDriftOut(DriftOutState& state,
	                                   const VolumetricCloudLayerSnapshot& current)
	{
		state.Active        = true;
		state.Duration      = 60.0f; // Seconds for clouds to fully dissolve.
		state.Elapsed       = 0.0f;
		state.Progress      = 0.0f;
		state.StartSnapshot = current;
	}

	void SkyCloudSystem::UpdateDriftOut(float deltaTime, DriftOutState& state,
	                                    VolumetricCloudLayerSnapshot& current)
	{
		if (!state.Active)
			return;

		state.Elapsed += deltaTime;
		float t = std::clamp(state.Elapsed / state.Duration, 0.0f, 1.0f);
		state.Progress = t * t * (3.0f - 2.0f * t); // smoothstep

		// Reduce density-contributing parameters toward zero.
		float fade = 1.0f - state.Progress;
		current.Coverage        = state.StartSnapshot.Coverage        * fade;
		current.AltoCloudAmount = state.StartSnapshot.AltoCloudAmount * fade;

		// Slow down evolution so no new micro-formation appears.
		current.EvolutionSpeed  = state.StartSnapshot.EvolutionSpeed  * fade;

		// Wind stays unchanged � clouds keep drifting.

		if (state.Elapsed >= state.Duration)
		{
			// Drift-out complete � layer fully dissolved.
			state.Active    = false;
			current.Enabled = false;
			current.Coverage        = 0.0f;
			current.AltoCloudAmount = 0.0f;
		}
	}

	bool  SkyCloudSystem::IsCloudADriftingOut()        const { return _driftOutA.Active; }
	bool  SkyCloudSystem::IsCloudBDriftingOut()        const { return _driftOutB.Active; }
	float SkyCloudSystem::GetCloudADriftOutProgress()  const { return _driftOutA.Active ? _driftOutA.Progress : 0.0f; }
	float SkyCloudSystem::GetCloudBDriftOutProgress()  const { return _driftOutB.Active ? _driftOutB.Progress : 0.0f; }

	// ====================================================================
	// Manual layer overrides
	// ====================================================================

	void SkyCloudSystem::SetVolumetricLayerA(const VolumetricCloudLayerSnapshot& snapshot)
	{
		_currentState.CloudA = snapshot;
		_manualOverrideCloudA = true;
	}

	void SkyCloudSystem::SetVolumetricLayerB(const VolumetricCloudLayerSnapshot& snapshot)
	{
		_currentState.CloudB = snapshot;
		_manualOverrideCloudB = true;
	}

	void SkyCloudSystem::SetLegacyLayer1(const LegacySkyLayerSnapshot& snapshot)
	{
		_currentState.Layer1 = snapshot;
		_manualOverrideLayer1 = true;
	}

	void SkyCloudSystem::SetLegacyLayer2(const LegacySkyLayerSnapshot& snapshot)
	{
		_currentState.Layer2 = snapshot;
		_manualOverrideLayer2 = true;
	}

	void SkyCloudSystem::ClearManualOverrides()
	{
		_manualOverrideCloudA = false;
		_manualOverrideCloudB = false;
		_manualOverrideLayer1 = false;
		_manualOverrideLayer2 = false;
	}

	// ====================================================================
	// Queries
	// ====================================================================

	WeatherPresetType SkyCloudSystem::GetCurrentPreset() const
	{
		return _currentPreset;
	}

	WeatherPresetType SkyCloudSystem::GetTargetPreset() const
	{
		return _transition.Active ? _transition.Target : _currentPreset;
	}

	float SkyCloudSystem::GetTransitionProgress() const
	{
		return _transition.Active ? _transition.Progress : 1.0f;
	}

	bool SkyCloudSystem::IsTransitioning() const
	{
		return _transition.Active;
	}

	const SkyCloudSnapshot& SkyCloudSystem::GetCurrentState() const
	{
		return _currentState;
	}

	SkyCloudSnapshot& SkyCloudSystem::GetMutableCurrentState()
	{
		return _currentState;
	}

	void SkyCloudSystem::SetGlobalWind(float dirX, float dirY, float speed)
	{
		// Backwards compatibility: the legacy Lua API took a normalized direction
		// and a cloud speed in the range [0..8]. Translate that into the engine-
		// wide base wind vector so direction is shared across all wind-affected
		// systems. The cloud wind speed stored here overrides the auto-derived
		// magnitude so clouds move at exactly the requested speed.
		constexpr float LUA_SPEED_MAX = 8.0f;
		float magnitude = (LUA_SPEED_MAX > 0.0f)
			? std::clamp(speed / LUA_SPEED_MAX, 0.0f, 1.0f) *
			  TEN::Effects::Environment::EnvironmentController::MAX_BASE_WIND_STRENGTH
			: 0.0f;

		// Normalize the supplied direction in case the script passed a
		// non-unit vector.
		float dirLen = std::sqrt(dirX * dirX + dirY * dirY);
		if (dirLen > 0.0001f)
		{
			dirX /= dirLen;
			dirY /= dirLen;
		}
		else
		{
			dirX = 0.0f;
			dirY = 0.0f;
		}

		TEN::Effects::Environment::Weather.SetBaseWind(dirX * magnitude, dirY * magnitude);

		// Store the explicit cloud wind speed so it is not overridden by the
		// base wind magnitude in ApplyGlobalWindToRenderSettings.
		_cloudWindSpeed = std::clamp(speed, 0.0f, LUA_SPEED_MAX);
	}

	CloudQualityPreset SkyCloudSystem::GetGlobalQuality() const
	{
		return _globalQuality;
	}

	void SkyCloudSystem::SetGlobalQuality(CloudQualityPreset preset)
	{
		_globalQuality = preset;
	}

	CloudRenderSettings SkyCloudSystem::GetCloudARenderSettings() const
	{
		auto s = _currentState.CloudA.ToRenderSettings();
		s.DriftOutProgress = _driftOutA.Active ? _driftOutA.Progress : 0.0f;
		ApplyGlobalWindToRenderSettings(s);
		return s;
	}

	CloudRenderSettings SkyCloudSystem::GetCloudBRenderSettings() const
	{
		auto s = _currentState.CloudB.ToRenderSettings();
		s.DriftOutProgress = _driftOutB.Active ? _driftOutB.Progress : 0.0f;
		ApplyGlobalWindToRenderSettings(s);
		return s;
	}

	void SkyCloudSystem::ApplyGlobalWindToRenderSettings(CloudRenderSettings& s) const
	{
		// Direction always comes from the engine-wide base wind (steady, no fluctuation).
		// Speed is either the independently configured _cloudWindSpeed or is
		// derived from the base wind magnitude when no override is set.
		auto baseWind = TEN::Effects::Environment::Weather.BaseWind();
		float magnitude = std::sqrt(baseWind.x * baseWind.x + baseWind.z * baseWind.z);

		const float MAX_WIND = TEN::Effects::Environment::EnvironmentController::MAX_BASE_WIND_STRENGTH;
		constexpr float CLOUD_SPEED_MAX = 8.0f;

		// Determine cloud speed: use fixed override when set, otherwise map base wind magnitude.
		float speed;
		if (_cloudWindSpeed >= 0.0f)
		{
			speed = _cloudWindSpeed;
		}
		else
		{
			speed = (MAX_WIND > 0.0f) ? (magnitude / MAX_WIND) * CLOUD_SPEED_MAX : 0.0f;
		}

		if (magnitude > 0.0001f)
		{
			// Negate direction: in the engine +Z points South, so the widget's
			// convention (up = North = +Z) must be flipped for the cloud shader.
			s.WindDirection = Vector2(-baseWind.x / magnitude, -baseWind.z / magnitude);
			s.WindSpeed     = speed;
		}
		else
		{
			s.WindSpeed = 0.0f;
		}
	}

	bool SkyCloudSystem::IsCloudAActive() const
	{
		// UnderwaterSky occupies Layer A as an overlay effect, not a volumetric cloud.
		if (_currentState.CloudA.Category == CloudCategory::UnderwaterSky)
			return false;

		return _currentState.CloudA.Enabled && _currentState.CloudA.Coverage > 0.001f;
	}

	bool SkyCloudSystem::IsCloudBActive() const
	{
		return _currentState.CloudB.Enabled && _currentState.CloudB.Coverage > 0.001f;
	}

	bool SkyCloudSystem::IsAuroraPresetActive() const
	{
		// Forced on by level.dynamicSky.Aurora.enabled — stays visible regardless
		// of cloud layer category (so Layer B clouds don't suppress night aurora).
		if (_dynamicSkyAuroraForced)
			return true;

		// Aurora is active if any volumetric cloud layer has the Aurora category and is enabled.
		if (_currentState.CloudA.Enabled && _currentState.CloudA.Category == CloudCategory::Aurora)
			return true;
		if (_currentState.CloudB.Enabled && _currentState.CloudB.Category == CloudCategory::Aurora)
			return true;
		return false;
	}

	void SkyCloudSystem::SetDynamicSkyAuroraForced(bool forced)
	{
		_dynamicSkyAuroraForced = forced;
	}

	bool SkyCloudSystem::GetDynamicSkyAuroraForced() const
	{
		return _dynamicSkyAuroraForced;
	}

	bool SkyCloudSystem::IsUnderwaterSkyPresetActive() const
	{
		// Underwater sky is active if any cloud layer has the UnderwaterSky category and is enabled.
		// Layer A is the canonical owner (mutually exclusive with Aurora by design), but the check
		// also tolerates Layer B for forward compatibility.
		if (_currentState.CloudA.Enabled && _currentState.CloudA.Category == CloudCategory::UnderwaterSky)
			return true;
		if (_currentState.CloudB.Enabled && _currentState.CloudB.Category == CloudCategory::UnderwaterSky)
			return true;
		return false;
	}

	bool SkyCloudSystem::IsLegacyLayer1Active() const
	{
		return _currentState.Layer1.Enabled;
	}

	bool SkyCloudSystem::IsLegacyLayer2Active() const
	{
		return _currentState.Layer2.Enabled;
	}

	// ====================================================================
	// Lens flare occlusion
	// ====================================================================

	float SkyCloudSystem::GetCombinedCloudTransmittance() const
	{
		// When neither dual layer is actively rendering, the stored transmittances may be stale
		// from a previous cloud state (e.g. after a transition from Thunderstorm to ClearSky,
		// UpdateDualCloudLensFlareOcclusion is never called while both layers are inactive,
		// so _cloudATransmittance/_cloudBTransmittance keep their last cloud-state value near 0).
		// Return full transmittance (no occlusion) whenever no dual layer is rendering.
		if (!IsCloudAActive() && !IsCloudBActive())
			return 1.0f;

		// Beer-Lambert-style composition: multiply transmittances.
		return _cloudATransmittance * _cloudBTransmittance;
	}

	void SkyCloudSystem::SetLayerTransmittance(int layerIndex, float transmittance)
	{
		transmittance = std::clamp(transmittance, 0.0f, 1.0f);
		if (layerIndex == 0)
			_cloudATransmittance = transmittance;
		else if (layerIndex == 1)
			_cloudBTransmittance = transmittance;
	}

	// ====================================================================
	// Preset registry access
	// ====================================================================

	const WeatherPresetDefinition* SkyCloudSystem::GetPresetDefinition(WeatherPresetType type) const
	{
		auto it = _presets.find(type);
		return (it != _presets.end()) ? &it->second : nullptr;
	}

	WeatherPresetDefinition* SkyCloudSystem::GetMutablePresetDefinition(WeatherPresetType type)
	{
		auto it = _presets.find(type);
		return (it != _presets.end()) ? &it->second : nullptr;
	}

	std::vector<WeatherPresetType> SkyCloudSystem::GetAllPresetTypes() const
	{
		std::vector<WeatherPresetType> types;
		types.reserve(_presets.size());
		for (const auto& [type, def] : _presets)
			types.push_back(type);
		std::sort(types.begin(), types.end());
		return types;
	}

	std::vector<WeatherPresetType> SkyCloudSystem::GetLayerAPresetTypes() const
	{
		std::vector<WeatherPresetType> types;
		for (const auto& [type, def] : _presets)
		{
			if (def.IsLayerAPreset)
				types.push_back(type);
		}
		std::sort(types.begin(), types.end());
		return types;
	}

	std::vector<WeatherPresetType> SkyCloudSystem::GetLayerBPresetTypes() const
	{
		std::vector<WeatherPresetType> types;
		for (const auto& [type, def] : _presets)
		{
			if (!def.IsLayerAPreset)
				types.push_back(type);
		}
		std::sort(types.begin(), types.end());
		return types;
	}

	void SkyCloudSystem::OverridePreset(WeatherPresetType type, const WeatherPresetDefinition& def)
	{
		_presets[type] = def;
	}

	CloudCategory SkyCloudSystem::CategoryFromString(const std::string& name)
	{
		if (name == "AltocumulusMid")      return CloudCategory::AltocumulusMid;
		if (name == "Aurora")              return CloudCategory::Aurora;
		if (name == "UnderwaterSky")       return CloudCategory::UnderwaterSky;
		return CloudCategory::None;
	}

	const char* SkyCloudSystem::PresetTypeToString(WeatherPresetType type)
	{
		switch (type)
		{
		case WeatherPresetType::ClearSky:              return "ClearSky";
		case WeatherPresetType::CirrocumulusLots:      return "CirrocumulusLots";
		case WeatherPresetType::CirrocumulusFew:       return "CirrocumulusFew";
		case WeatherPresetType::Cirrustratus:          return "Cirrustratus";
		case WeatherPresetType::CloudsTransformation:  return "CloudsTransformation";
		case WeatherPresetType::Overcast:              return "Overcast";
		case WeatherPresetType::Altocumulus:           return "Altocumulus";
		case WeatherPresetType::RainSnowOvercast:      return "RainSnowOvercast";
		case WeatherPresetType::StormBuildUp:          return "StormBuildUp";
		case WeatherPresetType::Thunderstorm:          return "Thunderstorm";
		case WeatherPresetType::Nothing:               return "Nothing";
		case WeatherPresetType::Aurora:                return "Aurora";
		case WeatherPresetType::WaterSurface:  return "WaterSurface";
		default:                                       return "Unknown";
		}
	}

	WeatherPresetType SkyCloudSystem::StringToPresetType(const std::string& name)
	{
		static const std::unordered_map<std::string, WeatherPresetType> map = {
			{ "ClearSky",              WeatherPresetType::ClearSky },
			{ "CirrocumulusLots",      WeatherPresetType::CirrocumulusLots },
			{ "CirrocumulusFew",       WeatherPresetType::CirrocumulusFew },
			{ "Cirrustratus",          WeatherPresetType::Cirrustratus },
			{ "CloudsTransformation",  WeatherPresetType::CloudsTransformation },
			{ "Overcast",              WeatherPresetType::Overcast },
			{ "Altocumulus",           WeatherPresetType::Altocumulus },
			{ "RainSnowOvercast",      WeatherPresetType::RainSnowOvercast },
			{ "StormBuildUp",          WeatherPresetType::StormBuildUp },
			{ "Thunderstorm",          WeatherPresetType::Thunderstorm },
			{ "Nothing",               WeatherPresetType::Nothing },
			{ "Aurora",                WeatherPresetType::Aurora },
			{ "WaterSurface",          WeatherPresetType::WaterSurface },
		};

		auto it = map.find(name);
		return (it != map.end()) ? it->second : WeatherPresetType::ClearSky;
	}

	// ====================================================================
	// Debug
	// ====================================================================

	SkyCloudSystem::DebugInfo SkyCloudSystem::GetDebugInfo() const
	{
		DebugInfo info;
		info.CurrentPreset        = _currentPreset;
		info.TargetPreset         = GetTargetPreset();
		info.TransitionProgress   = GetTransitionProgress();
		// Show pending auto-chain preset if one is configured for the current preset.
		{
			auto it = _presets.find(_currentPreset);
			if (it != _presets.end())
				info.NextPreset = it->second.NextPreset;
		}
		info.Layer1Enabled        = _currentState.Layer1.Enabled;
		info.Layer2Enabled        = _currentState.Layer2.Enabled;
		info.CloudAEnabled        = IsCloudAActive();
		info.CloudBEnabled        = IsCloudBActive();
		info.CloudATransmittance  = _cloudATransmittance;
		info.CloudBTransmittance  = _cloudBTransmittance;
		info.CombinedTransmittance = GetCombinedCloudTransmittance();
		info.CloudACategory       = _currentState.CloudA.Category;
		info.CloudBCategory       = _currentState.CloudB.Category;
		info.LayerATransitioning      = _layerTransitionA.Active;
		info.LayerBTransitioning      = _layerTransitionB.Active;
		info.LayerATransitionProgress = _layerTransitionA.Progress;
		info.LayerBTransitionProgress = _layerTransitionB.Progress;
		info.LayerAPreset             = _layerAPreset;
		info.LayerBPreset             = _layerBPreset;
		info.LayerATargetPreset       = _layerTransitionA.Active ? _layerTransitionA.TargetPreset : _layerAPreset;
		info.LayerBTargetPreset       = _layerTransitionB.Active ? _layerTransitionB.TargetPreset : _layerBPreset;
		info.DwellElapsed             = _nextPresetDwellElapsed;
		info.DwellTarget              = _nextPresetDwellTarget;
		info.LayerBDwellElapsed       = _layerDwellB.Elapsed;
		info.LayerBDwellTarget        = _layerDwellB.Target;
		info.LayerBDwellPaused        = _layerDwellB.Paused;
		return info;
	}
}
