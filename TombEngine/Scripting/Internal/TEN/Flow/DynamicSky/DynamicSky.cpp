#include "framework.h"
#include "Scripting/Internal/TEN/Flow/DynamicSky/DynamicSky.h"

#include <algorithm>
#include <sol/sol.hpp>

/***
Per-level dynamic sky container.
Groups atmospheric sky dome, aurora and volumetric cloud settings under
a single Lua object.

Use it from Gameflow.lua via:

    level.dynamicSky.realisticskydome  = true
    level.dynamicSky.blackvoidcolor    = Color(0, 0, 0)
    level.dynamicSky.horizonbottomfade = 0.0

    level.dynamicSky.Aurora.enabled = true
    level.dynamicSky.Aurora.color   = "GreenClassic"

    level.dynamicSky.Clouds.enabled           = true
    level.dynamicSky.Clouds.startPreset       = "Thunderstorm"
    level.dynamicSky.Clouds.windSpeed         = 0.27
    level.dynamicSky.Clouds.windDirectionX    = 1.0
    level.dynamicSky.Clouds.windDirectionZ    = 0.0
    level.dynamicSky.Clouds.transformDuration = 35
    level.dynamicSky.Clouds.changePresets     = {
        ClearSky    = { duration = 30, percent = 50 },
        Altocumulus = { duration = 60, percent = 50 }
    }

@tenprimitive Flow.DynamicSky
@pragma nostrip
*/

namespace TEN::Scripting
{
	// ====================================================================
	// Helpers
	// ====================================================================

	// Parse a single change-preset entry (one of several supported shapes).
	// Returns false if the entry is malformed and should be skipped.
	static bool ParseChangePresetEntry(
		const sol::object& key, const sol::object& val,
		DynamicSkyCloudChangeEntry& out)
	{
		// Form 1: name = { duration = .., percent = .. }
		// Form 2: name = { duration, percent }
		// Form 3: { name, duration, percent }   (key is integer index, val is table)
		// Form 4: { name = "...", duration = .., percent = .. }
		if (key.is<std::string>() && val.is<sol::table>())
		{
			out.Name = key.as<std::string>();
			sol::table sub = val.as<sol::table>();
			float fallbackDur = (float)sub.get_or(1, 30.0f);
			float fallbackPct = (float)sub.get_or(2, 100.0f);
			out.Duration = (float)sub.get_or("duration", fallbackDur);
			out.Percent  = (float)sub.get_or("percent",  fallbackPct);
			return !out.Name.empty();
		}

		if (val.is<sol::table>())
		{
			sol::table sub = val.as<sol::table>();
			std::string name = sub.get_or("name", std::string(""));
			if (name.empty())
				name = sub.get_or<std::string>(1, "");
			if (name.empty())
				return false;

			float fallbackDur = (float)sub.get_or(2, 30.0f);
			float fallbackPct = (float)sub.get_or(3, 100.0f);
			out.Name     = name;
			out.Duration = (float)sub.get_or("duration", fallbackDur);
			out.Percent  = (float)sub.get_or("percent",  fallbackPct);
			return true;
		}

		return false;
	}

	// ====================================================================
	// DynamicSkyClouds
	// ====================================================================

	void DynamicSkyClouds::SetChangePresetsLua(sol::object obj)
	{
		ChangePresets.clear();
		if (!obj.is<sol::table>())
			return;

		sol::table tbl = obj.as<sol::table>();
		tbl.for_each([this](const sol::object& key, const sol::object& val)
		{
			DynamicSkyCloudChangeEntry entry;
			if (ParseChangePresetEntry(key, val, entry))
				ChangePresets.push_back(std::move(entry));
		});
	}

	void DynamicSkyClouds::Register(sol::table& parent)
	{
		parent.new_usertype<DynamicSkyClouds>(
			"DynamicSkyClouds",
			sol::constructors<DynamicSkyClouds()>(),
			sol::call_constructor, sol::constructors<DynamicSkyClouds()>(),

			/// (bool) Enables the volumetric cloud layer for this level.
			// When true, the legacy bitmap sky layer 1 is suppressed.
			// @mem enabled
			"enabled", &DynamicSkyClouds::Enabled,

			/// (string) Initial weather preset name applied at level start.
			// Same names accepted by Flow.SetWeatherPreset / Flow.TransitionWeather.
			// @mem startPreset
			"startPreset", &DynamicSkyClouds::StartPreset,

			/// (float) Cloud wind speed for this level. Range 0.0 - 8.0. Negative = global.
			// @mem windSpeed
			"windSpeed", &DynamicSkyClouds::WindSpeed,

			/// (float) X component of the wind direction vector. Range -1.0 - 1.0.
			// @mem windDirectionX
			"windDirectionX", &DynamicSkyClouds::WindDirectionX,

			/// (float) Z component of the wind direction vector. Range -1.0 - 1.0.
			// @mem windDirectionZ
			"windDirectionZ", &DynamicSkyClouds::WindDirectionZ,

			/// (float) CloudMorph transition duration in seconds for this level.
			// Negative = per-preset default.
			// @mem transformDuration
			"transformDuration", &DynamicSkyClouds::TransformDuration,

			/// (string) Volumetric cloud rendering quality preset.
			// Allowed values: "Low", "Medium", "High". Empty = keep current global quality.
			// @mem quality
			"quality", &DynamicSkyClouds::Quality,

			/// (table) Random weather rotation table.
			// Each entry has a duration (seconds) and a percent (relative weight).
			// @mem changePresets
			"changePresets", sol::property(&DynamicSkyClouds::SetChangePresetsLua)
		);
	}

	// ====================================================================
	// DynamicSkyAurora
	// ====================================================================

	void DynamicSkyAurora::Register(sol::table& parent)
	{
		parent.new_usertype<DynamicSkyAurora>(
			"DynamicSkyAurora",
			sol::constructors<DynamicSkyAurora()>(),
			sol::call_constructor, sol::constructors<DynamicSkyAurora()>(),

			/// (bool) Enables the aurora borealis effect.
			// When true, the legacy bitmap sky layer 1 is suppressed.
			// @mem enabled
			"enabled", &DynamicSkyAurora::Enabled,

			/// (string) Aurora color preset name.
			// Valid: "GreenClassic", "GreenPurple", "GreenRedTips", "BluePurple",
			// "StrongMulticolor", "TurquoiseBluePurple".
			// @mem color
			"color", &DynamicSkyAurora::Color
		);
	}

	// ====================================================================
	// DynamicSky
	// ====================================================================

	void DynamicSky::Register(sol::table& parent)
	{
		DynamicSkyAurora::Register(parent);
		DynamicSkyClouds::Register(parent);

		parent.new_usertype<DynamicSky>(
			"DynamicSky",
			sol::constructors<DynamicSky()>(),
			sol::call_constructor, sol::constructors<DynamicSky()>(),

			/// (bool) Enables the atmospheric scattering sky dome.
			// @mem realisticskydome
			"realisticskydome", &DynamicSky::RealisticSkyDome,

			/// (Color) Color of the lower horizon band (replaces the default black void).
			// @mem blackvoidcolor
			"blackvoidcolor", &DynamicSky::BlackVoidColor,

			/// (float) Bottom-to-top alpha gradient on the horizon mesh. Range 0.0 - 1.0.
			// @mem horizonbottomfade
			"horizonbottomfade", &DynamicSky::HorizonBottomFade,

			/// (DynamicSkyAurora) Aurora borealis sub-settings.
			// @mem Aurora
			"Aurora", &DynamicSky::Aurora,

			/// (DynamicSkyClouds) Volumetric clouds sub-settings.
			// @mem Clouds
			"Clouds", &DynamicSky::Clouds
		);
	}
}
