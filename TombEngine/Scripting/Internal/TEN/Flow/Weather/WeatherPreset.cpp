// ============================================================================
// WeatherPreset.cpp — Lua-facing weather preset control implementation
// ============================================================================

#include "framework.h"
#include "Scripting/Internal/TEN/Flow/Weather/WeatherPreset.h"

#include <sol/sol.hpp>
#include "Game/Sky/SkyCloudSystem.h"
#include "Renderer/Renderer.h"
#include "Scripting/Internal/ScriptAssert.h"

using namespace TEN::Sky;

namespace TEN::Scripting
{
	// ====================================================================
	// Helper: parse a Lua table into a VolumetricCloudLayerSnapshot
	// ====================================================================

	static VolumetricCloudLayerSnapshot ParseCloudLayerTable(sol::table table)
	{
		VolumetricCloudLayerSnapshot snap;

		// sol2 get_or with a double literal is unambiguous (Lua numbers are doubles).
		// Cast to float afterwards so std::clamp/std::max receive uniform types.
		auto tf = [&](const char* key, double def) -> float
		{
			return static_cast<float>(table.get_or(key, def));
		};

		snap.Enabled         = table.get_or("enabled", true);
		snap.Coverage        = std::clamp(tf("coverage",        0.55 ), 0.0f, 1.0f);
		snap.Density         = std::max(  tf("density",          0.8  ), 0.0f);
		snap.BottomHeight    =            tf("bottomHeight",   1536.0  );
		// "horizonWidth" controls the cloud slab's vertical extent (CloudThickness),
		// which determines how far toward the horizon clouds are visible.
		// "thickness" accepted as legacy alias.
		{
			double hw = table.get_or("horizonWidth",
				table.get_or("thickness", 2500.0));
			snap.Thickness = std::max(static_cast<float>(hw), 1.0f);
		}
		snap.WindDirectionX  =            tf("windDirectionX",    1.0  );
		snap.WindDirectionY  =            tf("windDirectionY",    0.0  );
		snap.WindSpeed       = std::max(  tf("windSpeed",       0.003  ), 0.0f);
		snap.EvolutionSpeed  = std::max(  tf("evolutionSpeed",  0.15   ), 0.0f);
		snap.ShapeScale      = std::max(  tf("shapeScale",    0.00008  ), 0.000001f);
		snap.DetailScale     = std::max(  tf("detailScale",    0.0008  ), 0.000001f);
		snap.DetailStrength  = std::clamp(tf("detailStrength",  0.35   ), 0.0f, 1.0f);
		snap.Absorption      = std::max(  tf("absorption",       1.1   ), 0.0f);
		snap.AmbientContrib  = std::clamp(tf("ambient",          0.35  ), 0.0f, 1.0f);
		snap.SilverliningStr = std::clamp(tf("silverlining",     0.4   ), 0.0f, 1.0f);
		snap.HorizonFade     = std::clamp(tf("horizonFade",      1.0   ), 0.0f, 1.0f);
		snap.DistanceFade    = std::clamp(tf("distanceFade",     1.0   ), 0.0f, 1.0f);

		// Altocumulus-specific fields (only meaningful for category == "AltocumulusMid").
		snap.AltoBillowStrength  = std::clamp(tf("altoBillowStrength",  0.75  ), 0.0f, 1.0f);
		snap.AltoCovSoftWidth    = std::clamp(tf("altoCovSoftWidth",    0.08  ), 0.0f, 0.5f);
		snap.AltoAbsorption      = std::max(  tf("altoAbsorption",      1.0   ), 0.0f);
		snap.AltoCloudSize       = std::max(  tf("altoCloudSize",       1.0   ), 0.01f);
		snap.AltoCloudAmount     = std::clamp(tf("altoCloudAmount",     0.6875), 0.0f, 1.0f);
		snap.AltoCloudBrightness = std::max(  tf("altoCloudBrightness", 1.0   ), 0.0f);
		snap.AltoCloudColorR     = std::clamp(tf("altoCloudColorR",     1.0   ), 0.0f, 1.0f);
		snap.AltoCloudColorG     = std::clamp(tf("altoCloudColorG",     1.0   ), 0.0f, 1.0f);
		snap.AltoCloudColorB     = std::clamp(tf("altoCloudColorB",     1.0   ), 0.0f, 1.0f);
		snap.AltoCloudColorDarkR = std::clamp(tf("altoCloudColorDarkR", 0.55  ), 0.0f, 1.0f);
		snap.AltoCloudColorDarkG = std::clamp(tf("altoCloudColorDarkG", 0.55  ), 0.0f, 1.0f);
		snap.AltoCloudColorDarkB = std::clamp(tf("altoCloudColorDarkB", 0.65  ), 0.0f, 1.0f);
		snap.AltoFbmLacunarity   = std::clamp(tf("altoFbmLacunarity",   2.6434), 1.0f, 6.0f);
		snap.AltoFbmGain         = std::clamp(tf("altoFbmGain",         0.5   ), 0.0f, 1.0f);
		snap.AltoThickness       = std::max(  tf("altoThickness",       1800.0), 1.0f);
		snap.AltoBottomSoftness  = std::clamp(tf("altoBottomSoftness",  0.35  ), 0.0f, 1.0f);
		snap.AltoZenithBias      = std::clamp(tf("altoZenithBias",       0.0  ), -1.0f, 1.0f);
		snap.AltoHeightBlendPower= std::clamp(tf("altoHeightBlendPower", 1.0  ), 0.25f, 4.0f);

		// Lightning parameters (AltocumulusMid only).
		snap.LightningEnabled        = table.get_or("lightningEnabled",        false);
		snap.LightningStrikeFreq     = std::clamp(tf("lightningStrikeFreq",     0.1  ), 0.0f,  1.0f);
		snap.LightningInternalFreq   = std::clamp(tf("lightningInternalFreq",   0.5  ), 0.0f,  1.0f);
		snap.LightningSpeed          = std::clamp(tf("lightningSpeed",          2.5  ), 0.5f, 10.0f);
		snap.LightningInternalSpeed  = std::clamp(tf("lightningInternalSpeed",  5.0  ), 1.0f, 20.0f);
		snap.LightningGlowIntensity  = std::clamp(tf("lightningGlowIntensity",  3.0  ), 0.5f, 10.0f);
		snap.LightningBoltColorR     = std::clamp(tf("lightningBoltColorR",     0.3  ), 0.0f,  1.0f);
		snap.LightningBoltColorG     = std::clamp(tf("lightningBoltColorG",     0.6  ), 0.0f,  1.0f);
		snap.LightningBoltColorB     = std::clamp(tf("lightningBoltColorB",     1.0  ), 0.0f,  1.0f);
		snap.LightningFlashIntensity = std::clamp(tf("lightningFlashIntensity", 4.0  ), 0.5f, 15.0f);
		snap.LightningAmbientContrib = std::clamp(tf("lightningAmbientContrib", 0.15 ), 0.0f,  1.0f);

		// Quality string -> enum.
		std::string qualStr = table.get_or("quality", std::string("Medium"));
		if (qualStr == "Low")
			snap.Quality = CloudQualityPreset::Low;
		else if (qualStr == "High")
			snap.Quality = CloudQualityPreset::High;
		else
			snap.Quality = CloudQualityPreset::Medium;

		// Cloud category string -> enum.
		std::string catStr = table.get_or("category", std::string("None"));
		snap.Category = SkyCloudSystem::CategoryFromString(catStr);

		return snap;
	}

	// ====================================================================
	// Helper: parse easing curve string
	// ====================================================================

	static EasingCurve ParseEasingString(const std::string& str)
	{
		if (str == "Linear")    return EasingCurve::Linear;
		if (str == "EaseIn")    return EasingCurve::EaseIn;
		if (str == "EaseOut")   return EasingCurve::EaseOut;
		if (str == "EaseInOut") return EasingCurve::EaseInOut;
		return EasingCurve::SmoothStep;
	}

	// ====================================================================
	// Registration
	// ====================================================================

	void RegisterWeatherAPI(sol::table& parent)
	{
		/// Set the weather preset immediately (no transition).
		///
		/// @function Flow.SetWeatherPreset
		/// @tparam string presetName Name of the weather preset.
		/// Valid values: "ClearSky", "FewClouds", "ScatteredClouds", "BrokenClouds",
		/// "Overcast", "Cirrus", "Altocumulus", "AuroraBorealis",
		/// "StormBuildUp", "Thunderstorm", "StormTransformation", "Random"
		parent.set_function("SetWeatherPreset",
			[](const std::string& presetName)
			{
				auto type = SkyCloudSystem::StringToPresetType(presetName);
				g_SkyCloudSystem.SetPresetImmediate(type);
			});

		/// Transition smoothly to a weather preset over the specified duration.
		///
		/// @function Flow.TransitionWeather
		/// @tparam string presetName Target weather preset name.
		/// @tparam float duration Transition duration in seconds.
		/// @tparam[opt="SmoothStep"] string easing Easing curve.
		/// Valid values: "Linear", "SmoothStep", "EaseIn", "EaseOut", "EaseInOut"
		parent.set_function("TransitionWeather",
			[](const std::string& presetName, float duration,
			   sol::optional<std::string> easingOpt)
			{
				auto type = SkyCloudSystem::StringToPresetType(presetName);
				EasingCurve curve = EasingCurve::SmoothStep;
				if (easingOpt.has_value())
					curve = ParseEasingString(easingOpt.value());

				g_SkyCloudSystem.TransitionToPreset(type, duration, curve);
			});

		/// Stop an in-progress weather transition, keeping current blended state.
		///
		/// @function Flow.InterruptWeatherTransition
		parent.set_function("InterruptWeatherTransition",
			[]()
			{
				g_SkyCloudSystem.InterruptTransition();
			});

		/// Start random weather mode.
		///
		/// @function Flow.StartRandomWeather
		/// @tparam[opt] table settings Optional settings table:
		///   - dwellTime (float): seconds before switching, default 120.
		///   - transitionTime (float): seconds per transition, default 60.
		///   - easing (string): easing curve name, default "SmoothStep".
		///   - seed (int): optional RNG seed for determinism.
		///   - exclude (table): list of preset name strings to exclude.
		parent.set_function("StartRandomWeather",
			[](sol::optional<sol::table> settingsOpt)
			{
				float dwell = 120.0f;
				float transition = 60.0f;
				EasingCurve curve = EasingCurve::SmoothStep;

				if (settingsOpt.has_value())
				{
					auto& tbl = settingsOpt.value();
					dwell = tbl.get_or("dwellTime", 120.0f);
					transition = tbl.get_or("transitionTime", 60.0f);

					std::string easingStr = tbl.get_or<std::string>("easing", "SmoothStep");
					curve = ParseEasingString(easingStr);

					sol::optional<uint32_t> seedOpt = tbl["seed"];
					if (seedOpt.has_value())
						g_SkyCloudSystem.SetRandomSeed(seedOpt.value());

					sol::optional<sol::table> excludeOpt = tbl["exclude"];
					if (excludeOpt.has_value())
					{
						std::vector<WeatherPresetType> exclusions;
						excludeOpt.value().for_each(
							[&](sol::object /*key*/, sol::object val)
							{
								if (val.is<std::string>())
								{
									exclusions.push_back(
										SkyCloudSystem::StringToPresetType(val.as<std::string>()));
								}
							});
						g_SkyCloudSystem.SetRandomExclusions(exclusions);
					}
				}

				g_SkyCloudSystem.StartRandomWeather(dwell, transition, curve);
			});

		/// Stop random weather mode.
		///
		/// @function Flow.StopRandomWeather
		parent.set_function("StopRandomWeather",
			[]()
			{
				g_SkyCloudSystem.StopRandomWeather();
			});

		/// Get the current active weather preset name.
		///
		/// @function Flow.GetCurrentWeatherPreset
		/// @treturn string Current preset name.
		parent.set_function("GetCurrentWeatherPreset",
			[]() -> std::string
			{
				return SkyCloudSystem::PresetTypeToString(
					g_SkyCloudSystem.GetCurrentPreset());
			});

		/// Get the target weather preset name (during transitions).
		///
		/// @function Flow.GetTargetWeatherPreset
		/// @treturn string Target preset name, same as current if not transitioning.
		parent.set_function("GetTargetWeatherPreset",
			[]() -> std::string
			{
				return SkyCloudSystem::PresetTypeToString(
					g_SkyCloudSystem.GetTargetPreset());
			});

		/// Get the current weather transition progress.
		///
		/// @function Flow.GetWeatherTransitionProgress
		/// @treturn float Progress [0, 1]. Returns 1.0 if not transitioning.
		parent.set_function("GetWeatherTransitionProgress",
			[]() -> float
			{
				return g_SkyCloudSystem.GetTransitionProgress();
			});

		/// Check if a weather transition is in progress.
		///
		/// @function Flow.IsWeatherTransitioning
		/// @treturn bool True if a transition is active.
		parent.set_function("IsWeatherTransitioning",
			[]() -> bool
			{
				return g_SkyCloudSystem.IsTransitioning();
			});

		/// Check if random weather mode is active.
		///
		/// @function Flow.IsRandomWeatherActive
		/// @treturn bool True if random weather mode is running.
		parent.set_function("IsRandomWeatherActive",
			[]() -> bool
			{
				return g_SkyCloudSystem.IsRandomWeatherActive();
			});

		/// Manually override volumetric cloud layer A settings.
		/// While active, this layer won't be affected by preset transitions.
		///
		/// @function Flow.SetVolumetricCloudLayerA
		/// @tparam table settings Cloud layer settings table.
		parent.set_function("SetVolumetricCloudLayerA",
			[](sol::table settings)
			{
				auto snap = ParseCloudLayerTable(settings);
				g_SkyCloudSystem.SetVolumetricLayerA(snap);
			});

		/// Manually override volumetric cloud layer B settings.
		/// While active, this layer won't be affected by preset transitions.
		///
		/// @function Flow.SetVolumetricCloudLayerB
		/// @tparam table settings Cloud layer settings table.
		parent.set_function("SetVolumetricCloudLayerB",
			[](sol::table settings)
			{
				auto snap = ParseCloudLayerTable(settings);
				g_SkyCloudSystem.SetVolumetricLayerB(snap);
			});

		/// Clear all manual weather/cloud overrides.
		/// Layers will resume following preset transitions.
		///
		/// @function Flow.ClearWeatherOverrides
		parent.set_function("ClearWeatherOverrides",
			[]()
			{
				g_SkyCloudSystem.ClearManualOverrides();
			});

		/// Define or override a weather preset.
		/// Any field not provided keeps the existing default.
		/// Changes take effect on the next transition or SetWeatherPreset call.
		///
		/// @function Flow.DefineWeatherPreset
		/// @tparam string presetName Name of the preset to override.
		/// @tparam table definition Table with:
		///   - transitionDuration (float): default transition seconds
		///   - randomWeight (float): likelihood in random mode
		///   - highLayerLeadFraction (float): how much high layer leads in transitions
		///   - cloudA (table): cloud layer A parameters (coverage, density, category, etc.)
		///   - cloudB (table): cloud layer B parameters
		/// Cloud layer tables support all fields from SetVolumetricCloudLayerA
		/// plus "category" ("None", "CirrusHigh", "AltocumulusMid",
		/// "StratocumulusLow", "CumulonimbusVertical", "Aurora") which selects the
		/// shader rendering path for that cloud type.
		parent.set_function("DefineWeatherPreset",
			[](const std::string& presetName, sol::table definition)
			{
				auto type = SkyCloudSystem::StringToPresetType(presetName);

				// Start from existing definition (keep defaults for omitted fields).
				const auto* existing = g_SkyCloudSystem.GetPresetDefinition(type);
				WeatherPresetDefinition def;
				if (existing)
					def = *existing;

				def.Type = type;
				def.Name = presetName;

				auto tf = [&](sol::table& tbl, const char* key, double fallback) -> float
				{
					return static_cast<float>(tbl.get_or(key, fallback));
				};

			def.DefaultTransitionDuration = tf(definition, "transitionDuration",
				(double)def.DefaultTransitionDuration);
			// Per-layer transition durations. -1 = not explicitly set; inherits durationSeconds at transition time.
			def.TransitionDurationA = (float)definition.get_or("transitionDurationA", -1.0);
			def.TransitionDurationB = (float)definition.get_or("transitionDurationB", -1.0);
			def.RandomWeight = std::max(tf(definition, "randomWeight",
					(double)def.RandomWeight), 0.0f);
			def.HighLayerLeadFraction = std::clamp(tf(definition, "highLayerLeadFraction",
					(double)def.HighLayerLeadFraction), 0.0f, 1.0f);

			// Auto-chain: next preset to transition to after this one becomes active.
			def.NextPreset = definition.get_or("nextPreset", std::string(""));
			def.NextPresetTransitionDuration  = std::max(tf(definition, "nextTransitionDuration",
				(double)def.NextPresetTransitionDuration), 0.1f);
			def.NextPresetTransitionDurationA = (float)definition.get_or("nextTransitionDurationA", -1.0);
			def.NextPresetTransitionDurationB = (float)definition.get_or("nextTransitionDurationB", -1.0);

				sol::optional<sol::table> cloudATbl = definition["cloudA"];
				if (cloudATbl.has_value())
					def.TargetState.CloudA = ParseCloudLayerTable(cloudATbl.value());

				sol::optional<sol::table> cloudBTbl = definition["cloudB"];
				if (cloudBTbl.has_value())
					def.TargetState.CloudB = ParseCloudLayerTable(cloudBTbl.value());

				g_SkyCloudSystem.OverridePreset(type, def);
			});

		/// Set atmospheric sky gradient curve parameters.
		/// Controls how quickly the sky shifts from white at zenith to the warm sun
		/// color near the horizon, and how strong that warm tint is.
		///
		/// @function Flow.SetAtmosphericSkySettings
		/// @tparam table settings Table with any subset of:
		///   sunElevationRampSpeed (float, default 1.1): how fast warm tint fades as
		///     the sun rises — higher = white zone starts sooner.
		///   sunWarmInfluence (float, default 0.3): max warmth blend at the horizon
		///     (0 = always white, 1 = full AtmoSunColor at the horizon).
		parent.set_function("SetAtmosphericSkySettings",
			[](sol::table settings)
			{
				auto& s = g_Renderer.GetAtmosphericSkySettings();
				if (auto v = settings.get<sol::optional<float>>("sunElevationRampSpeed"); v.has_value()) s.SunElevationRampSpeed = *v;
				if (auto v = settings.get<sol::optional<float>>("sunWarmInfluence");       v.has_value()) s.SunWarmInfluence       = *v;
			});
	}
}
