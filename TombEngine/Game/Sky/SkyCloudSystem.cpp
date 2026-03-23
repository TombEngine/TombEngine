// ============================================================================
// SkyCloudSystem.cpp — Layered Sky & Cloud Weather System Implementation
// ============================================================================

#include "framework.h"
#include "Game/Sky/SkyCloudSystem.h"

#include <algorithm>
#include "Game/control/control.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include <cmath>
#include <numeric>

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
			return t * t * (3.0f - 2.0f * t);

		case EasingCurve::EaseInOut:
		{
			if (t < 0.5f)
				return 4.0f * t * t * t;
			float f = 2.0f * t - 2.0f;
			return 0.5f * f * f * f + 1.0f;
		}

		case EasingCurve::EaseIn:
			return t * t;

		case EasingCurve::EaseOut:
			return t * (2.0f - t);

		default:
			return t;
		}
	}

	// ====================================================================
	// VolumetricCloudLayerSnapshot
	// ====================================================================

	CloudRenderSettings VolumetricCloudLayerSnapshot::ToRenderSettings() const
	{
		CloudRenderSettings s;
		s.Enabled         = Enabled;
		s.Mode            = Enabled ? CloudLayerMode::Volumetric : CloudLayerMode::LegacyBitmap;
		s.Coverage        = Coverage;
		s.Density         = Density;
		s.CloudBottomHeight = BottomHeight;
		s.CloudThickness  = Thickness;
		s.WindDirection   = Vector2(WindDirectionX, WindDirectionY);
		s.WindSpeed       = WindSpeed;
		s.EvolutionSpeed  = EvolutionSpeed;
		s.Noise.ShapeScale    = ShapeScale;
		s.Noise.DetailScale   = DetailScale;
		s.Noise.DetailStrength = DetailStrength;
		s.Absorption      = Absorption;
		s.AmbientContrib  = AmbientContrib;
		s.SilverliningStr = SilverliningStr;
		s.HorizonFade     = HorizonFade;
		s.DistanceFade    = DistanceFade;
		s.Quality         = Quality;
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

		return s;
	}

	VolumetricCloudLayerSnapshot VolumetricCloudLayerSnapshot::FromRenderSettings(
		const CloudRenderSettings& src)
	{
		VolumetricCloudLayerSnapshot snap;
		snap.Enabled       = src.Enabled;
		snap.Category      = static_cast<CloudCategory>(src.CloudType);
		snap.Coverage      = src.Coverage;
		snap.Density       = src.Density;
		snap.BottomHeight  = src.CloudBottomHeight;
		snap.Thickness     = src.CloudThickness;
		snap.WindDirectionX = src.WindDirection.x;
		snap.WindDirectionY = src.WindDirection.y;
		snap.WindSpeed     = src.WindSpeed;
		snap.EvolutionSpeed = src.EvolutionSpeed;
		snap.ShapeScale    = src.Noise.ShapeScale;
		snap.DetailScale   = src.Noise.DetailScale;
		snap.DetailStrength = src.Noise.DetailStrength;
		snap.Absorption    = src.Absorption;
		snap.AmbientContrib = src.AmbientContrib;
		snap.SilverliningStr = src.SilverliningStr;
		snap.HorizonFade     = src.HorizonFade;
		snap.DistanceFade    = src.DistanceFade;
		snap.Quality         = src.Quality;
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

		return snap;
	}

	static float LerpFloat(float a, float b, float t)
	{
		return a + (b - a) * t;
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
			result.Coverage = LerpFloat(a.Coverage, 0.0f, t);
		}
		else if (!a.Enabled && b.Enabled)
		{
			// Fading in: enable early, ramp coverage from 0.
			result.Enabled = (t > 0.05f);
			result.Coverage = LerpFloat(0.0f, b.Coverage, t);
		}
		else
		{
			result.Coverage = LerpFloat(a.Coverage, b.Coverage, t);
		}

		// Category: snap at halfway.
		result.Category = (t < 0.5f) ? a.Category : b.Category;

		result.Density         = LerpFloat(a.Density, b.Density, t);
		result.BottomHeight    = LerpFloat(a.BottomHeight, b.BottomHeight, t);
		result.Thickness       = LerpFloat(a.Thickness, b.Thickness, t);
		result.WindDirectionX  = LerpFloat(a.WindDirectionX, b.WindDirectionX, t);
		result.WindDirectionY  = LerpFloat(a.WindDirectionY, b.WindDirectionY, t);
		result.WindSpeed       = LerpFloat(a.WindSpeed, b.WindSpeed, t);
		result.EvolutionSpeed  = LerpFloat(a.EvolutionSpeed, b.EvolutionSpeed, t);
		result.ShapeScale      = LerpFloat(a.ShapeScale, b.ShapeScale, t);
		result.DetailScale     = LerpFloat(a.DetailScale, b.DetailScale, t);
		result.DetailStrength  = LerpFloat(a.DetailStrength, b.DetailStrength, t);
		result.Absorption      = LerpFloat(a.Absorption, b.Absorption, t);
		result.AmbientContrib  = LerpFloat(a.AmbientContrib, b.AmbientContrib, t);
		result.SilverliningStr = LerpFloat(a.SilverliningStr, b.SilverliningStr, t);
		result.HorizonFade        = LerpFloat(a.HorizonFade,        b.HorizonFade,        t);
		result.DistanceFade       = LerpFloat(a.DistanceFade,       b.DistanceFade,       t);
		result.AltoBillowStrength = LerpFloat(a.AltoBillowStrength, b.AltoBillowStrength, t);
		result.AltoCovSoftWidth   = LerpFloat(a.AltoCovSoftWidth,   b.AltoCovSoftWidth,   t);
		result.AltoAbsorption      = LerpFloat(a.AltoAbsorption,      b.AltoAbsorption,      t);
		result.AltoCloudSize      = LerpFloat(a.AltoCloudSize,      b.AltoCloudSize,      t);
		result.AltoCloudAmount    = LerpFloat(a.AltoCloudAmount,    b.AltoCloudAmount,    t);
		result.AltoCloudBrightness = LerpFloat(a.AltoCloudBrightness, b.AltoCloudBrightness, t);
		result.AltoCloudColorR    = LerpFloat(a.AltoCloudColorR,    b.AltoCloudColorR,    t);
		result.AltoCloudColorG    = LerpFloat(a.AltoCloudColorG,    b.AltoCloudColorG,    t);
		result.AltoCloudColorB    = LerpFloat(a.AltoCloudColorB,    b.AltoCloudColorB,    t);
		result.AltoFbmLacunarity  = LerpFloat(a.AltoFbmLacunarity,  b.AltoFbmLacunarity,  t);
		result.AltoFbmGain        = LerpFloat(a.AltoFbmGain,        b.AltoFbmGain,        t);
		result.AltoThickness      = LerpFloat(a.AltoThickness,      b.AltoThickness,      t);
		result.AltoCloudColorDarkR = LerpFloat(a.AltoCloudColorDarkR, b.AltoCloudColorDarkR, t);
		result.AltoCloudColorDarkG = LerpFloat(a.AltoCloudColorDarkG, b.AltoCloudColorDarkG, t);
		result.AltoCloudColorDarkB = LerpFloat(a.AltoCloudColorDarkB, b.AltoCloudColorDarkB, t);
		result.AltoBottomSoftness  = LerpFloat(a.AltoBottomSoftness,  b.AltoBottomSoftness,  t);

		result.AltoZenithBias       = LerpFloat(a.AltoZenithBias,       b.AltoZenithBias,       t);
		result.AltoHeightBlendPower  = LerpFloat(a.AltoHeightBlendPower,  b.AltoHeightBlendPower,  t);

		// Lightning
		result.LightningEnabled      = (t < 0.5f) ? a.LightningEnabled : b.LightningEnabled;
		result.LightningStrikeFreq   = LerpFloat(a.LightningStrikeFreq,   b.LightningStrikeFreq,   t);
		result.LightningInternalFreq = LerpFloat(a.LightningInternalFreq, b.LightningInternalFreq, t);
		result.LightningSpeed        = LerpFloat(a.LightningSpeed,        b.LightningSpeed,        t);
		result.LightningInternalSpeed = LerpFloat(a.LightningInternalSpeed, b.LightningInternalSpeed, t);
		result.LightningGlowIntensity = LerpFloat(a.LightningGlowIntensity, b.LightningGlowIntensity, t);
		result.LightningBoltColorR   = LerpFloat(a.LightningBoltColorR,   b.LightningBoltColorR,   t);
		result.LightningBoltColorG   = LerpFloat(a.LightningBoltColorG,   b.LightningBoltColorG,   t);
		result.LightningBoltColorB   = LerpFloat(a.LightningBoltColorB,   b.LightningBoltColorB,   t);
		result.LightningFlashIntensity = LerpFloat(a.LightningFlashIntensity, b.LightningFlashIntensity, t);
		result.LightningAmbientContrib = LerpFloat(a.LightningAmbientContrib, b.LightningAmbientContrib, t);
		result.LightningBoltLengthScale    = LerpFloat(a.LightningBoltLengthScale,    b.LightningBoltLengthScale,    t);
		result.LightningBoltThicknessScale = LerpFloat(a.LightningBoltThicknessScale, b.LightningBoltThicknessScale, t);

		// Quality: snap at halfway.
		result.Quality = (t < 0.5f) ? a.Quality : b.Quality;

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
	// SkyCloudSystem — Construction & Initialization
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

	void SkyCloudSystem::Initialize()
	{
		_currentPreset = WeatherPresetType::ClearSky;
		_currentState  = _presets[WeatherPresetType::ClearSky].TargetState;
		_transition    = {};
		_randomWeather = {};
		_manualOverrideCloudA = false;
		_manualOverrideCloudB = false;
		_manualOverrideLayer1 = false;
		_manualOverrideLayer2 = false;
		_cloudATransmittance  = 1.0f;
		_cloudBTransmittance  = 1.0f;

		// Apply weather config from Gameflow.lua (level.weatherPreset / level.randomWeather).
		auto* level = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));
		if (level)
		{
			// randomWeather takes priority over weatherPreset.
			if (level->RandomWeather.has_value())
			{
				const auto& cfg = level->RandomWeather.value();
				// Convert exclude list from strings to enum values.
				std::vector<WeatherPresetType> exclusions;
				for (const auto& name : cfg.Exclude)
					exclusions.push_back(StringToPresetType(name));
				if (!exclusions.empty())
					SetRandomExclusions(exclusions);
				StartRandomWeather(cfg.DwellTime, cfg.TransitionTime, EasingFromString(cfg.Easing));
			}
			else if (level->WeatherPreset.has_value())
			{
				SetPresetImmediate(StringToPresetType(level->WeatherPreset.value()));
			}
		}
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
			def.RandomWeight = 2.0f;
			// All layers disabled — perfectly clear sky.
			_presets[def.Type] = def;
		}

		// ----- RainSnowOvercast -----
		// Heavy uniform overcast: large slow-rolling blanketing clouds.
		// Uses CloudB only (AltocumulusMid). Values set by level designer.
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::RainSnowOvercast;
			def.Name = "RainSnowOvercast";
			def.DefaultTransitionDuration = 60.0f;
			def.RandomWeight = 0.8f;

			auto& b = def.TargetState.CloudB;
			b.Enabled           = true;
			b.Category          = CloudCategory::AltocumulusMid;
			b.BottomHeight      = 4061.0f;
			b.Thickness         = 1000.0f;
			b.WindDirectionX    = 1.0f;
			b.WindDirectionY    = 0.0f;
			b.WindSpeed         = 0.6288f;
			b.EvolutionSpeed    = 5.0f;
			b.HorizonFade       = 0.0f;
			b.DistanceFade      = 0.549f;
			b.AltoBillowStrength = 0.0f;
			b.AltoCovSoftWidth   = 0.25f;
			b.AltoAbsorption     = 0.1f;
			b.AltoCloudSize      = 0.317f;
			b.AltoCloudAmount    = 0.799f;
			b.AltoCloudBrightness = 0.932f;
			b.AltoCloudColorR    = 1.0f;
			b.AltoCloudColorG    = 1.0f;
			b.AltoCloudColorB    = 1.0f;
			b.AltoCloudColorDarkR = 0.55f;
			b.AltoCloudColorDarkG = 0.55f;
			b.AltoCloudColorDarkB = 0.65f;
			b.AltoFbmLacunarity  = 4.0f;
			b.AltoFbmGain        = 0.401f;
			b.AltoThickness      = 5000.0f;
			b.AltoBottomSoftness = 1.0f;

			_presets[def.Type] = def;
		}

		// ----- BrokenClouds -----
		// Partial coverage: distinct cloud groups at varying heights.
		// Uses CloudB only (AltocumulusMid).
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::BrokenClouds;
			def.Name = "BrokenClouds";
			def.DefaultTransitionDuration = 45.0f;
			def.RandomWeight = 1.5f;

			auto& b = def.TargetState.CloudB;
			b.Enabled           = true;
			b.Category          = CloudCategory::AltocumulusMid;
			b.BottomHeight      = 3500.0f;
			b.Thickness         = 1400.0f;
			b.WindDirectionX    = 1.0f;
			b.WindDirectionY    = 0.0f;
			b.WindSpeed         = 0.35f;
			b.EvolutionSpeed    = 2.5f;
			b.HorizonFade       = 0.0f;
			b.DistanceFade      = 0.5f;
			b.AltoBillowStrength = 0.5f;
			b.AltoCovSoftWidth   = 0.15f;
			b.AltoAbsorption     = 0.8f;
			b.AltoCloudSize      = 0.45f;
			b.AltoCloudAmount    = 0.62f;
			b.AltoCloudBrightness = 1.0f;
			b.AltoCloudColorR    = 1.0f;
			b.AltoCloudColorG    = 1.0f;
			b.AltoCloudColorB    = 1.0f;
			b.AltoCloudColorDarkR = 0.55f;
			b.AltoCloudColorDarkG = 0.55f;
			b.AltoCloudColorDarkB = 0.65f;
			b.AltoFbmLacunarity  = 3.2f;
			b.AltoFbmGain        = 0.45f;
			b.AltoThickness      = 3000.0f;
			b.AltoBottomSoftness = 0.65f;

			_presets[def.Type] = def;
		}

		// ----- Thunderstorm -----
		// Dark threatening overcast with rapid churn; very large cloud masses.
		// Uses CloudB only (AltocumulusMid).
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Thunderstorm;
			def.Name = "Thunderstorm";
			def.DefaultTransitionDuration = 60.0f;
			def.RandomWeight = 0.3f;

			auto& b = def.TargetState.CloudB;
			b.Enabled           = true;
			b.Category          = CloudCategory::AltocumulusMid;
			b.BottomHeight      = 2500.0f;
			b.Thickness         = 1200.0f;
			b.WindDirectionX    = 1.0f;
			b.WindDirectionY    = 0.0f;
			b.WindSpeed         = 0.8f;
			b.EvolutionSpeed    = 6.0f;
			b.HorizonFade       = 0.0f;
			b.DistanceFade      = 0.4f;
			b.AltoBillowStrength = 0.0f;
			b.AltoCovSoftWidth   = 0.30f;
			b.AltoAbsorption     = 0.1f;
			b.AltoCloudSize      = 0.22f;
			b.AltoCloudAmount    = 0.90f;
			b.AltoCloudBrightness = 0.75f;
			b.AltoCloudColorR    = 0.90f;
			b.AltoCloudColorG    = 0.90f;
			b.AltoCloudColorB    = 0.95f;
			b.AltoCloudColorDarkR = 0.27f;
			b.AltoCloudColorDarkG = 0.27f;
			b.AltoCloudColorDarkB = 0.35f;
			b.AltoFbmLacunarity  = 4.0f;
			b.AltoFbmGain        = 0.40f;
			b.AltoThickness      = 5000.0f;
			b.AltoBottomSoftness = 1.0f;

			_presets[def.Type] = def;
		}
	
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::FewClouds;
			def.Name = "FewClouds";
			def.DefaultTransitionDuration = 45.0f;
			def.RandomWeight = 2.5f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::CirrusHigh;
			a.Coverage      = 0.15f;
			a.Density       = 0.3f;
			a.BottomHeight  = 4000.0f;
			a.Thickness     = 1500.0f;
			a.WindSpeed     = 0.002f;
			a.EvolutionSpeed = 0.08f;
			a.ShapeScale    = 0.00006f;
			a.DetailScale   = 0.0006f;
			a.DetailStrength = 0.2f;
			a.Absorption    = 0.6f;
			a.AmbientContrib = 0.5f;
			a.SilverliningStr = 0.6f;

			_presets[def.Type] = def;
		}

		// ----- ScatteredClouds -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::ScatteredClouds;
			def.Name = "ScatteredClouds";
			def.DefaultTransitionDuration = 40.0f;
			def.RandomWeight = 2.0f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::AltocumulusMid;
			a.Coverage      = 0.46f;
			a.Density       = 0.5f;
			a.BottomHeight  = 2500.0f;
			a.Thickness     = 1700.0f;
			a.WindSpeed     = 0.003f;
			a.EvolutionSpeed = 0.12f;
			a.ShapeScale    = 0.00011f;
			a.DetailScale   = 0.00085f;
			a.DetailStrength = 0.3f;
			a.Absorption    = 0.9f;
			a.AmbientContrib = 0.4f;
			a.SilverliningStr = 0.5f;

			_presets[def.Type] = def;
		}

		// ----- BrokenClouds -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::BrokenClouds;
			def.Name = "BrokenClouds";
			def.DefaultTransitionDuration = 45.0f;
			def.RandomWeight = 1.5f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::AltocumulusMid;
			a.Coverage      = 0.62f;
			a.Density       = 0.6f;
			a.BottomHeight  = 2000.0f;
			a.Thickness     = 2000.0f;
			a.WindSpeed     = 0.004f;
			a.EvolutionSpeed = 0.15f;
			a.ShapeScale    = 0.00011f;
			a.DetailScale   = 0.0008f;
			a.DetailStrength = 0.35f;
			a.Absorption    = 1.0f;
			a.AmbientContrib = 0.35f;
			a.SilverliningStr = 0.45f;

			auto& b = def.TargetState.CloudB;
			b.Enabled       = true;
			b.Category      = CloudCategory::StratocumulusLow;
			b.Coverage      = 0.2f;
			b.Density       = 0.4f;
			b.BottomHeight  = 1200.0f;
			b.Thickness     = 1200.0f;
			b.WindSpeed     = 0.005f;
			b.EvolutionSpeed = 0.1f;
			b.ShapeScale    = 0.0001f;
			b.DetailScale   = 0.001f;
			b.DetailStrength = 0.3f;
			b.Absorption    = 1.1f;
			b.AmbientContrib = 0.3f;
			b.SilverliningStr = 0.35f;

			def.HighLayerLeadFraction = 0.15f;
			_presets[def.Type] = def;
		}

		// ----- Overcast -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Overcast;
			def.Name = "Overcast";
			def.DefaultTransitionDuration = 60.0f;
			def.RandomWeight = 1.2f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::AltocumulusMid;
			a.Coverage      = 0.85f;
			a.Density       = 0.75f;
			a.BottomHeight  = 1800.0f;
			a.Thickness     = 2200.0f;
			a.WindSpeed     = 0.003f;
			a.EvolutionSpeed = 0.1f;
			a.ShapeScale    = 0.00012f;
			a.DetailScale   = 0.001f;
			a.DetailStrength = 0.25f;
			a.Absorption    = 1.3f;
			a.AmbientContrib = 0.25f;
			a.SilverliningStr = 0.2f;

			auto& b = def.TargetState.CloudB;
			b.Enabled       = true;
			b.Category      = CloudCategory::StratocumulusLow;
			b.Coverage      = 0.7f;
			b.Density       = 0.6f;
			b.BottomHeight  = 1000.0f;
			b.Thickness     = 1500.0f;
			b.WindSpeed     = 0.004f;
			b.EvolutionSpeed = 0.08f;
			b.ShapeScale    = 0.00012f;
			b.DetailScale   = 0.0012f;
			b.DetailStrength = 0.2f;
			b.Absorption    = 1.4f;
			b.AmbientContrib = 0.2f;
			b.SilverliningStr = 0.15f;

			_presets[def.Type] = def;
		}

		// ----- Cirrus -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Cirrus;
			def.Name = "Cirrus";
			def.DefaultTransitionDuration = 50.0f;
			def.RandomWeight = 1.5f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::CirrusHigh;
			a.Coverage      = 0.4f;
			a.Density       = 0.2f;
			a.BottomHeight  = 5000.0f;
			a.Thickness     = 1000.0f;
			a.WindSpeed     = 0.006f;
			a.EvolutionSpeed = 0.05f;
			a.ShapeScale    = 0.00005f;
			a.DetailScale   = 0.0005f;
			a.DetailStrength = 0.15f;
			a.Absorption    = 0.4f;
			a.AmbientContrib = 0.6f;
			a.SilverliningStr = 0.7f;

			_presets[def.Type] = def;
		}

		// ----- Altocumulus -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Altocumulus;
			def.Name = "Altocumulus";
			def.DefaultTransitionDuration = 40.0f;
			def.RandomWeight = 1.5f;

			auto& b = def.TargetState.CloudB;
			b.Enabled            = true;
			b.Category           = CloudCategory::AltocumulusMid;
			b.BottomHeight       = 2127.0f;
			b.Thickness          = 3252.0f;
			b.WindDirectionX     = 1.0f;
			b.WindDirectionY     = 0.0f;
			b.WindSpeed          = 0.2423f;
			b.EvolutionSpeed     = 5.0f;
			b.HorizonFade        = 1.0f;
			b.DistanceFade       = 0.0f;
			b.AltoBillowStrength = 1.0f;
			b.AltoCovSoftWidth   = 0.25f;
			b.AltoAbsorption     = 0.1f;
			b.AltoCloudSize      = 0.509f;
			b.AltoCloudAmount    = 0.640f;
			b.AltoCloudBrightness = 1.034f;
			b.AltoCloudColorR    = 1.0f;
			b.AltoCloudColorG    = 1.0f;
			b.AltoCloudColorB    = 1.0f;
			b.AltoCloudColorDarkR = 0.55f;
			b.AltoCloudColorDarkG = 0.55f;
			b.AltoCloudColorDarkB = 0.65f;
			b.AltoFbmLacunarity  = 4.0f;
			b.AltoFbmGain        = 0.5f;
			b.AltoThickness      = 1480.0f;
			b.AltoBottomSoftness = 1.0f;

			_presets[def.Type] = def;
		}

		// ----- Aurora Borealis -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::AuroraBorealis;
			def.Name = "AuroraBorealis";
			def.DefaultTransitionDuration = 50.0f;
			def.RandomWeight = 0.0f;
			def.AllowInRandom = false;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::Aurora;
			a.Coverage      = 0.65f;
			a.Density       = 0.7f;
			a.BottomHeight  = 1200.0f;
			a.Thickness     = 2000.0f;
			a.WindSpeed     = 0.004f;
			a.EvolutionSpeed = 0.1f;
			a.ShapeScale    = 0.0001f;
			a.DetailScale   = 0.001f;
			a.DetailStrength = 0.3f;
			a.Absorption    = 1.2f;
			a.AmbientContrib = 0.3f;
			a.SilverliningStr = 0.3f;

			_presets[def.Type] = def;
		}

		// ----- StormBuildUp -----
		// Distant towering cumulonimbus buildup near the horizon.
		// Cloud A: Altocumulus mid-level overcast thickening overhead.
		// Cloud B: CumulonimbusVerticalBuildUp — ring of distant tower formations.
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::StormBuildUp;
			def.Name = "StormBuildUp";
			def.DefaultTransitionDuration = 90.0f;
			def.RandomWeight = 0.6f;
			def.HighLayerLeadFraction = 0.2f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::AltocumulusMid;
			a.Coverage      = 0.7f;
			a.Density       = 0.65f;
			a.BottomHeight  = 2000.0f;
			a.Thickness     = 2400.0f;
			a.WindSpeed     = 0.005f;
			a.EvolutionSpeed = 0.2f;
			a.ShapeScale    = 0.00011f;
			a.DetailScale   = 0.0009f;
			a.DetailStrength = 0.4f;
			a.Absorption    = 1.2f;
			a.AmbientContrib = 0.25f;
			a.SilverliningStr = 0.3f;
			a.AltoCloudColorDarkR = 0.45f;
			a.AltoCloudColorDarkG = 0.45f;
			a.AltoCloudColorDarkB = 0.55f;

			auto& b = def.TargetState.CloudB;
			b.Enabled       = true;
			b.Category      = CloudCategory::CumulonimbusVerticalBuildUp;
			b.Coverage      = 0.55f;
			b.Density       = 0.85f;
			b.BottomHeight  = 800.0f;
			b.Thickness     = 4500.0f;
			b.WindSpeed     = 0.004f;
			b.EvolutionSpeed = 0.2f;
			b.ShapeScale    = 0.00012f;
			b.DetailScale   = 0.001f;
			b.DetailStrength = 0.4f;
			b.Absorption    = 1.5f;
			b.AmbientContrib = 0.2f;
			b.SilverliningStr = 0.2f;
			b.HorizonFade   = 0.3f;

			_presets[def.Type] = def;
		}

		// ----- Thunderstorm -----
		// Full active thunderstorm with lightning.
		// Cloud A: Dense altocumulus overcast (dark, oppressive sky).
		// Cloud B: CumulonimbusVertical with lightning flashes.
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::Thunderstorm;
			def.Name = "Thunderstorm";
			def.DefaultTransitionDuration = 60.0f;
			def.RandomWeight = 0.3f;
			def.HighLayerLeadFraction = 0.0f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::AltocumulusMid;
			a.Coverage      = 0.85f;
			a.Density       = 0.8f;
			a.BottomHeight  = 1500.0f;
			a.Thickness     = 2600.0f;
			a.WindSpeed     = 0.007f;
			a.EvolutionSpeed = 0.3f;
			a.ShapeScale    = 0.00011f;
			a.DetailScale   = 0.001f;
			a.DetailStrength = 0.4f;
			a.Absorption    = 1.6f;
			a.AmbientContrib = 0.15f;
			a.SilverliningStr = 0.15f;
			a.AltoCloudColorDarkR = 0.27f;
			a.AltoCloudColorDarkG = 0.27f;
			a.AltoCloudColorDarkB = 0.35f;

			auto& b = def.TargetState.CloudB;
			b.Enabled       = true;
			b.Category      = CloudCategory::CumulonimbusVertical;
			b.Coverage      = 0.8f;
			b.Density       = 1.0f;
			b.BottomHeight  = 500.0f;
			b.Thickness     = 5000.0f;
			b.WindSpeed     = 0.008f;
			b.EvolutionSpeed = 0.35f;
			b.ShapeScale    = 0.00014f;
			b.DetailScale   = 0.0012f;
			b.DetailStrength = 0.5f;
			b.Absorption    = 2.0f;
			b.AmbientContrib = 0.1f;
			b.SilverliningStr = 0.1f;
			// Lightning — active thunderstorm
			b.LightningEnabled      = true;
			b.LightningStrikeFreq   = 0.1f;
			b.LightningInternalFreq = 0.5f;
			b.LightningSpeed        = 2.5f;
			b.LightningInternalSpeed = 5.0f;
			b.LightningGlowIntensity = 3.0f;
			b.LightningBoltColorR   = 0.3f;
			b.LightningBoltColorG   = 0.6f;
			b.LightningBoltColorB   = 1.0f;
			b.LightningFlashIntensity = 4.0f;
			b.LightningAmbientContrib = 0.15f;
			b.LightningBoltLengthScale    = 1.0f;
			b.LightningBoltThicknessScale = 1.0f;

			_presets[def.Type] = def;
		}

		// ----- StormTransformation -----
		{
			WeatherPresetDefinition def;
			def.Type = WeatherPresetType::StormTransformation;
			def.Name = "StormTransformation";
			def.DefaultTransitionDuration = 45.0f;
			def.RandomWeight = 0.15f;

			auto& a = def.TargetState.CloudA;
			a.Enabled       = true;
			a.Category      = CloudCategory::CumulonimbusVertical;
			a.Coverage      = 0.95f;
			a.Density       = 1.0f;
			a.BottomHeight  = 1000.0f;
			a.Thickness     = 4000.0f;
			a.WindSpeed     = 0.01f;
			a.EvolutionSpeed = 0.4f;
			a.ShapeScale    = 0.00012f;
			a.DetailScale   = 0.0012f;
			a.DetailStrength = 0.5f;
			a.Absorption    = 2.0f;
			a.AmbientContrib = 0.08f;
			a.SilverliningStr = 0.05f;

			auto& b = def.TargetState.CloudB;
			b.Enabled       = true;
			b.Category      = CloudCategory::CumulonimbusVertical;
			b.Coverage      = 0.9f;
			b.Density       = 1.2f;
			b.BottomHeight  = 300.0f;
			b.Thickness     = 5500.0f;
			b.WindSpeed     = 0.012f;
			b.EvolutionSpeed = 0.45f;
			b.ShapeScale    = 0.00016f;
			b.DetailScale   = 0.0015f;
			b.DetailStrength = 0.55f;
			b.Absorption    = 2.5f;
			b.AmbientContrib = 0.05f;
			b.SilverliningStr = 0.05f;

			_presets[def.Type] = def;
		}
	}

	// ====================================================================
	// Per-frame update
	// ====================================================================

	void SkyCloudSystem::Update(float deltaTime)
	{
		if (_randomWeather.Active)
			UpdateRandomWeather(deltaTime);

		if (_transition.Active)
			UpdateTransition(deltaTime);
	}

	// ====================================================================
	// Transition system
	// ====================================================================

	void SkyCloudSystem::UpdateTransition(float deltaTime)
	{
		auto& tr = _transition;
		tr.Elapsed += deltaTime;

		if (tr.Elapsed >= tr.Duration)
		{
			// Transition complete.
			tr.Progress = 1.0f;
			_currentState  = tr.TargetSnapshot;
			_currentPreset = tr.Target;
			tr.Active = false;

			// Auto-chain: if this preset defines a NextPreset, start transitioning immediately.
			auto it = _presets.find(_currentPreset);
			if (it != _presets.end() && !it->second.NextPreset.empty())
			{
				const auto& chainDef = it->second;
				float durA = (chainDef.NextPresetTransitionDurationA >= 0.0f)
					? chainDef.NextPresetTransitionDurationA
					: chainDef.NextPresetTransitionDuration;
				float durB = (chainDef.NextPresetTransitionDurationB >= 0.0f)
					? chainDef.NextPresetTransitionDurationB
					: chainDef.NextPresetTransitionDuration;
				TransitionToPreset(StringToPresetType(chainDef.NextPreset), durA, durB);
			}
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

		// Apply manual overrides if set.
		if (!_manualOverrideLayer1) _currentState.Layer1 = blended.Layer1;
		if (!_manualOverrideLayer2) _currentState.Layer2 = blended.Layer2;
		if (!_manualOverrideCloudA) _currentState.CloudA = blended.CloudA;
		if (!_manualOverrideCloudB) _currentState.CloudB = blended.CloudB;
	}

	void SkyCloudSystem::SetPresetImmediate(WeatherPresetType preset)
	{
		if (preset == WeatherPresetType::Random)
		{
			StartRandomWeather(120.0f, 60.0f);
			return;
		}

		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		_transition.Active = false;
		_currentPreset = preset;
		_currentState = it->second.TargetState;

		// Auto-chain: if this preset defines a NextPreset, start transitioning immediately.
		const auto& def = it->second;
		if (!def.NextPreset.empty())
		{
			float durA = (def.NextPresetTransitionDurationA >= 0.0f)
				? def.NextPresetTransitionDurationA
				: def.NextPresetTransitionDuration;
			float durB = (def.NextPresetTransitionDurationB >= 0.0f)
				? def.NextPresetTransitionDurationB
				: def.NextPresetTransitionDuration;
			TransitionToPreset(StringToPresetType(def.NextPreset), durA, durB);
		}
	}

	void SkyCloudSystem::TransitionToPreset(WeatherPresetType preset, float durationSeconds,
	                                         EasingCurve curve)
	{
		if (preset == WeatherPresetType::Random)
		{
			StartRandomWeather(120.0f, durationSeconds, curve);
			return;
		}

		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		const auto& def = it->second;

		// Resolve per-layer durations: use stored defaults if set (>= 0), else use durationSeconds.
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
		tr.TargetSnapshot    = def.TargetState;
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
		if (preset == WeatherPresetType::Random)
		{
			StartRandomWeather(120.0f, std::max(durationASeconds, durationBSeconds), curve);
			return;
		}

		auto it = _presets.find(preset);
		if (it == _presets.end())
			return;

		auto& tr = _transition;
		tr.Active           = true;
		tr.Source            = _currentPreset;
		tr.Target            = preset;
		tr.SourceSnapshot    = _currentState;
		tr.TargetSnapshot    = it->second.TargetState;
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
	}

	// ====================================================================
	// Random weather
	// ====================================================================

	void SkyCloudSystem::StartRandomWeather(float dwellTime, float transitionTime,
	                                         EasingCurve curve)
	{
		_randomWeather.Active = true;
		_randomWeather.DwellTime = std::max(dwellTime, 5.0f);
		_randomWeather.DwellElapsed = 0.0f;
		_randomWeather.TransitionTime = std::max(transitionTime, 1.0f);
		_randomWeather.Curve = curve;

		if (!_randomWeather.Seeded)
		{
			std::random_device rd;
			_randomWeather.RNG.seed(rd());
		}
	}

	void SkyCloudSystem::StopRandomWeather()
	{
		_randomWeather.Active = false;
	}

	void SkyCloudSystem::SetRandomSeed(uint32_t seed)
	{
		_randomWeather.Seed = seed;
		_randomWeather.Seeded = true;
		_randomWeather.RNG.seed(seed);
	}

	void SkyCloudSystem::SetRandomExclusions(const std::vector<WeatherPresetType>& exclusions)
	{
		_randomWeather.ExcludedPresets = exclusions;
	}

	void SkyCloudSystem::UpdateRandomWeather(float deltaTime)
	{
		if (_transition.Active)
			return; // Wait for current transition to finish.

		_randomWeather.DwellElapsed += deltaTime;

		if (_randomWeather.DwellElapsed >= _randomWeather.DwellTime)
		{
			_randomWeather.DwellElapsed = 0.0f;

			WeatherPresetType next = PickRandomPreset();
			if (next != _currentPreset)
			{
				TransitionToPreset(next, _randomWeather.TransitionTime, _randomWeather.Curve);
			}
		}
	}

	WeatherPresetType SkyCloudSystem::PickRandomPreset()
	{
		// Build weighted list of eligible presets.
		std::vector<std::pair<WeatherPresetType, float>> candidates;

		for (auto& [type, def] : _presets)
		{
			if (!def.AllowInRandom)
				continue;

			if (type == WeatherPresetType::Random)
				continue;

			// Check exclusions.
			bool excluded = false;
			for (auto excl : _randomWeather.ExcludedPresets)
			{
				if (excl == type)
				{
					excluded = true;
					break;
				}
			}

			if (excluded)
				continue;

			// Reduce weight for the current preset to avoid picking the same one.
			float weight = def.RandomWeight;
			if (type == _currentPreset)
				weight *= 0.1f;

			if (weight > 0.0f)
				candidates.emplace_back(type, weight);
		}

		if (candidates.empty())
			return _currentPreset;

		float totalWeight = 0.0f;
		for (auto& [type, w] : candidates)
			totalWeight += w;

		std::uniform_real_distribution<float> dist(0.0f, totalWeight);
		float roll = dist(_randomWeather.RNG);

		float accumulated = 0.0f;
		for (auto& [type, w] : candidates)
		{
			accumulated += w;
			if (roll <= accumulated)
				return type;
		}

		return candidates.back().first;
	}

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

	bool SkyCloudSystem::IsRandomWeatherActive() const
	{
		return _randomWeather.Active;
	}

	const SkyCloudSnapshot& SkyCloudSystem::GetCurrentState() const
	{
		return _currentState;
	}

	SkyCloudSnapshot& SkyCloudSystem::GetMutableCurrentState()
	{
		return _currentState;
	}

	CloudRenderSettings SkyCloudSystem::GetCloudARenderSettings() const
	{
		return _currentState.CloudA.ToRenderSettings();
	}

	CloudRenderSettings SkyCloudSystem::GetCloudBRenderSettings() const
	{
		return _currentState.CloudB.ToRenderSettings();
	}

	bool SkyCloudSystem::IsCloudAActive() const
	{
		return _currentState.CloudA.Enabled && _currentState.CloudA.Coverage > 0.001f;
	}

	bool SkyCloudSystem::IsCloudBActive() const
	{
		return _currentState.CloudB.Enabled && _currentState.CloudB.Coverage > 0.001f;
	}

	bool SkyCloudSystem::IsAuroraPresetActive() const
	{
		// Aurora is active if any volumetric cloud layer has the Aurora category and is enabled.
		if (_currentState.CloudA.Enabled && _currentState.CloudA.Category == CloudCategory::Aurora)
			return true;
		if (_currentState.CloudB.Enabled && _currentState.CloudB.Category == CloudCategory::Aurora)
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

	void SkyCloudSystem::OverridePreset(WeatherPresetType type, const WeatherPresetDefinition& def)
	{
		_presets[type] = def;
	}

	CloudCategory SkyCloudSystem::CategoryFromString(const std::string& name)
	{
		if (name == "CirrusHigh")          return CloudCategory::CirrusHigh;
		if (name == "AltocumulusMid")      return CloudCategory::AltocumulusMid;
		if (name == "StratocumulusLow")    return CloudCategory::StratocumulusLow;
		if (name == "CumulonimbusVertical") return CloudCategory::CumulonimbusVertical;
		if (name == "CumulonimbusVerticalBuildUp") return CloudCategory::CumulonimbusVerticalBuildUp;
		if (name == "Aurora")              return CloudCategory::Aurora;
		return CloudCategory::None;
	}

	const char* SkyCloudSystem::PresetTypeToString(WeatherPresetType type)
	{
		switch (type)
		{
		case WeatherPresetType::ClearSky:        return "ClearSky";
		case WeatherPresetType::FewClouds:       return "FewClouds";
		case WeatherPresetType::ScatteredClouds: return "ScatteredClouds";
		case WeatherPresetType::BrokenClouds:    return "BrokenClouds";
		case WeatherPresetType::Overcast:        return "Overcast";
		case WeatherPresetType::Cirrus:          return "Cirrus";
		case WeatherPresetType::Altocumulus:     return "Altocumulus";
		case WeatherPresetType::AuroraBorealis:  return "AuroraBorealis";
		case WeatherPresetType::RainSnowOvercast:    return "RainSnowOvercast";
		case WeatherPresetType::StormBuildUp:    return "StormBuildUp";
		case WeatherPresetType::Thunderstorm:    return "Thunderstorm";
		case WeatherPresetType::StormTransformation:      return "StormTransformation";
		case WeatherPresetType::Random:          return "Random";
		default:                                 return "Unknown";
		}
	}

	WeatherPresetType SkyCloudSystem::StringToPresetType(const std::string& name)
	{
		static const std::unordered_map<std::string, WeatherPresetType> map = {
			{ "ClearSky",        WeatherPresetType::ClearSky },
			{ "FewClouds",       WeatherPresetType::FewClouds },
			{ "ScatteredClouds", WeatherPresetType::ScatteredClouds },
			{ "BrokenClouds",    WeatherPresetType::BrokenClouds },
			{ "Overcast",        WeatherPresetType::Overcast },
			{ "Cirrus",          WeatherPresetType::Cirrus },
			{ "Altocumulus",     WeatherPresetType::Altocumulus },
			{ "AuroraBorealis",  WeatherPresetType::AuroraBorealis },
			{ "RainSnowOvercast", WeatherPresetType::RainSnowOvercast },
			{ "StormBuildUp",    WeatherPresetType::StormBuildUp },
			{ "Thunderstorm",    WeatherPresetType::Thunderstorm },
			{ "StormTransformation",      WeatherPresetType::StormTransformation },
			{ "Random",          WeatherPresetType::Random },
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
		info.RandomModeActive     = _randomWeather.Active;
		info.RandomDwellRemaining = std::max(0.0f, _randomWeather.DwellTime - _randomWeather.DwellElapsed);
		info.Layer1Enabled        = _currentState.Layer1.Enabled;
		info.Layer2Enabled        = _currentState.Layer2.Enabled;
		info.CloudAEnabled        = IsCloudAActive();
		info.CloudBEnabled        = IsCloudBActive();
		info.CloudATransmittance  = _cloudATransmittance;
		info.CloudBTransmittance  = _cloudBTransmittance;
		info.CombinedTransmittance = GetCombinedCloudTransmittance();
		info.CloudACategory       = _currentState.CloudA.Category;
		info.CloudBCategory       = _currentState.CloudB.Category;
		return info;
	}
}
