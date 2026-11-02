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

	void DynamicSkyClouds::SetColorLua(sol::object obj)
	{
		if (obj.is<ScriptColor>())
		{
			Color = obj.as<ScriptColor>();
			HasColor = true;
		}
	}

	void DynamicSkyClouds::SetDarkColorLua(sol::object obj)
	{
		if (obj.is<ScriptColor>())
		{
			DarkColor = obj.as<ScriptColor>();
			HasDarkColor = true;
		}
	}

	void DynamicSkyClouds::SetSunsetUndersideColorLua(sol::object obj)
	{
		if (obj.is<ScriptColor>())
		{
			SunsetUndersideColor = obj.as<ScriptColor>();
			HasSunsetUndersideColor = true;
		}
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

			/// (float) CloudMorph transition duration in seconds for this level.
			// Negative = per-preset default.
			// @mem transformDuration
			"transformDuration", &DynamicSkyClouds::TransformDuration,

			/// (string) Volumetric cloud rendering quality preset.
			// Allowed values: "Low", "Medium", "High". Empty = keep current global quality.
			// @mem quality
			"quality", &DynamicSkyClouds::Quality,

			/// (float) Independent cloud layer wind speed. Range 0.0 - 8.0.
			// Negative value (default) means speed is derived from the engine base wind.
			// The cloud wind direction always follows level.windDirectionX / windDirectionZ.
			// @mem windSpeed
			"windSpeed", &DynamicSkyClouds::WindSpeed,

			/// (Color) Per-level alto cloud bright (lit) color override.
			// When set, replaces altoCloudColor on every weather preset for this level so
			// every preset and any transition between them shares the same lit color.
			// @mem color
			"color", sol::property(&DynamicSkyClouds::GetColorLua, &DynamicSkyClouds::SetColorLua),

			/// (Color) Per-level alto cloud shadow (dark) color override.
			// When set, replaces altoCloudColorDark on every weather preset for this level.
			// @mem darkColor
			"darkColor", sol::property(&DynamicSkyClouds::GetDarkColorLua, &DynamicSkyClouds::SetDarkColorLua),

			/// (float) Direct sun light intensity on volumetric clouds. Range 0.0 - 5.0.
			// Negative value (default) keeps the engine default.
			// @mem sunlightIntensity
			"sunlightIntensity", &DynamicSkyClouds::SunlightIntensity,

			/// (float) Cloud forward-scatter (HG-like) strength. Range 0.0 - 3.0.
			// Negative value (default) keeps the engine default.
			// @mem forwardScatter
			"forwardScatter", &DynamicSkyClouds::ForwardScatter,

			/// (float) Sunset underside glow intensity. Range 0.0 - 3.0.
			// Negative value (default) keeps the engine default.
			// @mem sunsetUndersideIntensity
			"sunsetUndersideIntensity", &DynamicSkyClouds::SunsetUndersideIntensity,

			/// (float) Sunset underside glow angular spread. Range 0.5 - 4.0.
			// Negative value (default) keeps the engine default.
			// @mem sunsetUndersideSpread
			"sunsetUndersideSpread", &DynamicSkyClouds::SunsetUndersideSpread,

			/// (float) Sunset underside vertical fade exponent. Range 0.5 - 4.0.
			// Negative value (default) keeps the engine default.
			// @mem sunsetUndersideHeightFade
			"sunsetUndersideHeightFade", &DynamicSkyClouds::SunsetUndersideHeightFade,

			/// (Color) Optional static sunset underside color. When omitted the engine
			// uses its built-in procedural yellow->orange->red->magenta gradient.
			// @mem sunsetUndersideColor
			"sunsetUndersideColor", sol::property(&DynamicSkyClouds::GetSunsetUndersideColorLua, &DynamicSkyClouds::SetSunsetUndersideColorLua),

			/// (table) Random weather rotation table.
			// Each entry has a duration (seconds) and a percent (relative weight).
			// @mem changePresets
			"changePresets", sol::property(&DynamicSkyClouds::SetChangePresetsLua)
		);
	}

	// ====================================================================
	// DynamicSkyAurora
	// ====================================================================

	void DynamicSkyAurora::SetColorLua(sol::object obj)
	{
		if (obj.is<std::string>())
		{
			Color          = obj.as<std::string>();
			HasCustomColor = false;
		}
		else if (obj.is<sol::table>())
		{
			sol::table tbl = obj.as<sol::table>();
			sol::object first  = tbl[1];
			sol::object second = tbl[2];
			if (first.is<ScriptColor>() && second.is<ScriptColor>())
			{
				CustomColorTop    = first.as<ScriptColor>();
				CustomColorBottom = second.as<ScriptColor>();
				HasCustomColor    = true;
			}
		}
	}

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

			/// (string|table) Aurora color: either a preset name string or a two-element table
			// { Color(topR, topG, topB), Color(botR, botG, botB) } for a custom top/bottom gradient.
			// Valid preset names: "GreenClassic", "GreenPurple", "GreenRedTips", "BluePurple",
			// "StrongMulticolor", "TurquoiseBluePurple".
			// @mem color
			"color", sol::property(&DynamicSkyAurora::GetColorLua, &DynamicSkyAurora::SetColorLua),

			/// (float) Aurora animation drift speed. Range 0.0 - 2.0.
			// Negative value (default) keeps the engine default of 0.679.
			// @mem speed
			"speed", &DynamicSkyAurora::Speed
		);
	}

	// ====================================================================
	// DynamicSky
	// ====================================================================

	void DynamicSky::SetSkyColorLua(sol::object obj)
	{
		if (obj.is<ScriptColor>())
		{
			SkyColor    = obj.as<ScriptColor>();
			HasSkyColor = true;
		}
	}

	void DynamicSky::Register(sol::table& parent)
	{
		DynamicSkyAurora::Register(parent);
		DynamicSkyClouds::Register(parent);
		DynamicSkyGodRays::Register(parent);

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

			/// (Color) Base sky color (Rayleigh tint) override.
			// Omit to keep the engine default.
			// @mem skyColor
			"skyColor", sol::property(&DynamicSky::GetSkyColorLua, &DynamicSky::SetSkyColorLua),

			/// (float) Sun disk apparent half-angle in degrees. Range 0.10 - 10.00.
			// Negative value (default) keeps the engine default.
			// @mem sundiskSize
			"sundiskSize", &DynamicSky::SundiskSize,

			/// (float) Sun disk brightness before tone mapping. Range 1.0 - 200.0.
			// Negative value (default) keeps the engine default.
			// @mem sundiskIntensity
			"sundiskIntensity", &DynamicSky::SundiskIntensity,

			/// (float) Horizon darkening exponent. Range 0.1 - 5.0.
			// Negative value (default) keeps the engine default.
			// @mem horizonDarkening
			"horizonDarkening", &DynamicSky::HorizonDarkening,

			/// (float) Twilight onset offset (radians). Range 0.0 - 0.3.
			// Negative value (default) keeps the engine default.
			// @mem twilightOffset
			"twilightOffset", &DynamicSky::TwilightOffset,

			/// (float) Sky gradient ramp speed: how quickly the warm sun tint fades as the sun rises. Range 0.1 - 5.0.
			// Low = warm tint persists high in the sky. High = warm tint only near the horizon.
			// Negative value (default) keeps the engine default.
			// @mem skyGradient
			"skyGradient", &DynamicSky::SkyGradient,

			/// (float) Maximum warm-color blend weight toward the sun color at the horizon. Range 0.0 - 1.0.
			// 0 = always white sun rays, 1 = full sun color at horizon.
			// Negative value (default) keeps the engine default.
			// @mem warmInfluence
			"warmInfluence", &DynamicSky::WarmInfluence,

			/// (DynamicSkyAurora) Aurora borealis sub-settings.
			// @mem Aurora
			"Aurora", &DynamicSky::Aurora,

			/// (DynamicSkyClouds) Volumetric clouds sub-settings.
			// @mem Clouds
			"Clouds", &DynamicSky::Clouds,

			/// (DynamicSkyGodRays) Screen-space god rays sub-settings.
			// @mem godRays
			"godRays", &DynamicSky::GodRays
		);
	}

	// ====================================================================
	// DynamicSkyGodRays
	// ====================================================================

	void DynamicSkyGodRays::Register(sol::table& parent)
	{
		parent.new_usertype<DynamicSkyGodRays>(
			"DynamicSkyGodRays",
			sol::constructors<DynamicSkyGodRays()>(),
			sol::call_constructor, sol::constructors<DynamicSkyGodRays()>(),

			/// (bool) Enables the screen-space god ray pass for this level.
			// Default = true (god rays remain on if the line is omitted).
			// @mem enabled
			"enabled", &DynamicSkyGodRays::Enabled
		);
	}

	// ====================================================================
	// MoonLens
	// ====================================================================

	MoonLens::MoonLens(float pitch, float yaw)
	{
		_pitch     = pitch;
		_yaw       = yaw;
		_isEnabled = true;
	}

	void MoonLens::Register(sol::table& parent)
	{
		using ctors = sol::constructors<MoonLens(), MoonLens(float, float)>;

		parent.new_usertype<MoonLens>(
			"MoonLens",
			ctors(), sol::call_constructor, ctors(),

			/// (bool) Moon enabled state.
			// @mem enabled
			"enabled", sol::property(&MoonLens::GetEnabled, &MoonLens::SetEnabled),

			/// (float) Moon pitch (vertical) angle in degrees.
			// @mem pitch
			"pitch", sol::property(&MoonLens::GetPitch, &MoonLens::SetPitch),

			/// (float) Moon yaw (horizontal) angle in degrees.
			// @mem yaw
			"yaw", sol::property(&MoonLens::GetYaw, &MoonLens::SetYaw)
		);
	}

	// ====================================================================
	// LevelDustStorm
	// ====================================================================

	void LevelDustStorm::SetColorLua(sol::object obj)
	{
		if (obj.is<ScriptColor>())
		{
			Color = obj.as<ScriptColor>();
			HasColor = true;
		}
	}

	void LevelDustStorm::Register(sol::table& parent)
	{
		parent.new_usertype<LevelDustStorm>(
			"DustStorm",
			sol::constructors<LevelDustStorm()>(),
			sol::call_constructor, sol::constructors<LevelDustStorm()>(),

			/// (bool) Enables the volumetric dust storm pass for this level.
			// @mem enabled
			"enabled", &LevelDustStorm::Enabled,

			/// (float) Overall dust opacity multiplier. Range 0.0 - 2.0.
			// Negative value (default) keeps the engine default.
			// @mem density
			"density", &LevelDustStorm::Density,

			/// (float) Normalized base height (0 = ground). Range 0.0 - 1.0.
			// @mem minHeight
			"minHeight", &LevelDustStorm::MinHeight,

			/// (float) Normalized cap height (1 = top of weather column). Range 0.0 - 1.0.
			// @mem maxHeight
			"maxHeight", &LevelDustStorm::MaxHeight,

			/// (Color) Dust color. Defaults to the engine sand tint when not set.
			// @mem color
			"color", sol::property(&LevelDustStorm::GetColorLua, &LevelDustStorm::SetColorLua),

			/// (float) Wind coupling strength. Range 0.0 - 4.0.
			// @mem windCoupling
			"windCoupling", &LevelDustStorm::WindCoupling
		);
	}

	// ====================================================================
	// LevelUnderwaterSky
	// ====================================================================

	void LevelUnderwaterSky::SetEnabledLua(sol::object obj)
	{
		if (obj.is<bool>())
		{
			Enabled    = obj.as<bool>();
			HasEnabled = true;
		}
	}

	void LevelUnderwaterSky::SetColorLua(sol::object obj)
	{
		if (obj.is<ScriptColor>())
		{
			Color    = obj.as<ScriptColor>();
			HasColor = true;
		}
	}

	void LevelUnderwaterSky::Register(sol::table& parent)
	{
		parent.new_usertype<LevelUnderwaterSky>(
			"UnderwaterSky",
			sol::constructors<LevelUnderwaterSky()>(),
			sol::call_constructor, sol::constructors<LevelUnderwaterSky()>(),

			/// (bool) Enables the WaterSurface weather preset for this level.
			// @mem enabled
			"enabled", sol::property(&LevelUnderwaterSky::GetEnabledLua, &LevelUnderwaterSky::SetEnabledLua),

			/// (float) Wave animation drift speed override. Negative (default) keeps preset value.
			// @mem waveSpeed
			"waveSpeed", &LevelUnderwaterSky::WaveSpeed,

			/// (Color) Underwater base color override.
			// @mem color
			"color", sol::property(&LevelUnderwaterSky::GetColorLua, &LevelUnderwaterSky::SetColorLua)
		);
	}
}
