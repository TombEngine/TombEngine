#pragma once

#include <string>
#include <vector>

#include "Scripting/Internal/TEN/Types/Color/Color.h"

namespace sol { class state; }

namespace TEN::Scripting
{
	using namespace TEN::Scripting::Types;

	// Single change-preset entry parsed from
	// level.dynamicSky.Clouds.changePresets in Gameflow.lua.
	struct DynamicSkyCloudChangeEntry
	{
		std::string Name     = "";    // Weather preset name (e.g. "ClearSky").
		float       Duration = 30.0f; // Seconds the preset stays before chaining.
		float       Percent  = 100.0f;// Relative weight when picking the next preset.
	};

	// Aurora sub-section of level.dynamicSky.
	struct DynamicSkyAurora
	{
		bool        Enabled = false;
		std::string Color   = "GreenClassic"; // AuroraColorPreset name.
		float       Speed   = -1.0f;          // < 0 = use engine default (0.679); 0.0 - 2.0 = override.

		// Custom color mode: when HasCustomColor is true the preset is ignored.
		// color = { Color(topR, topG, topB), Color(botR, botG, botB) } in Lua.
		bool        HasCustomColor    = false;
		ScriptColor CustomColorTop    = ScriptColor(13, 102, 26);   // ~0.05, 0.4, 0.1 * 255 (GreenClassic top)
		ScriptColor CustomColorBottom = ScriptColor(25, 204, 51);   // ~0.1, 0.8, 0.2 * 255  (GreenClassic bottom)

		// Lua property: accepts a preset name string OR a two-element Color table {top, bottom}.
		void        SetColorLua(sol::object obj);
		std::string GetColorLua() const { return Color; }

		static void Register(sol::table& parent);
	};

	// God rays sub-section of level.dynamicSky.
	struct DynamicSkyGodRays
	{
		bool Enabled = true;

		static void Register(sol::table& parent);
	};

	// Volumetric clouds sub-section of level.dynamicSky.
	struct DynamicSkyClouds
	{
		bool        Enabled           = false;
		std::string StartPreset       = "";    // Initial weather preset for the level.
		float       TransformDuration = -1.0f; // < 0 = per-preset default.
		std::string Quality           = "";    // "Low" / "Medium" / "High"; empty = keep current.
		float       WindSpeed         = -1.0f; // < 0 = derive from base wind; 0.0 - 8.0 = override.

		// Atmospheric-sky cloud-lighting overrides. Negative = keep engine default.
		float SunlightIntensity         = -1.0f; // [0, 5]
		float ForwardScatter            = -1.0f; // [0, 3]
		float SunsetUndersideIntensity  = -1.0f; // [0, 3]
		float SunsetUndersideSpread     = -1.0f; // [0.5, 4]
		float SunsetUndersideHeightFade = -1.0f; // [0.5, 4]

		// Per-level alto cloud color overrides. Applied to every preset (and any
		// transition between them) when set. Sentinels stay false when the Lua
		// script does not assign a value, in which case preset defaults are used.
		bool        HasColor     = false;
		ScriptColor Color        = ScriptColor(255, 255, 255);
		bool        HasDarkColor = false;
		ScriptColor DarkColor    = ScriptColor(140, 140, 165); // 0.55, 0.55, 0.65 * 255

		// Optional static sunset-underside color. When HasSunsetUndersideColor is
		// true the engine bypasses the procedural yellow->magenta gradient.
		bool        HasSunsetUndersideColor = false;
		ScriptColor SunsetUndersideColor    = ScriptColor(255, 140, 38); // ~1.0, 0.55, 0.15

		std::vector<DynamicSkyCloudChangeEntry> ChangePresets;

		// Lua property: ChangePresets is exposed via a write-only table setter.
		void SetChangePresetsLua(sol::object obj);

		// Lua color setter properties — track HasColor / HasDarkColor automatically.
		void        SetColorLua(sol::object obj);
		ScriptColor GetColorLua() const { return Color; }
		void        SetDarkColorLua(sol::object obj);
		ScriptColor GetDarkColorLua() const { return DarkColor; }
		void        SetSunsetUndersideColorLua(sol::object obj);
		ScriptColor GetSunsetUndersideColorLua() const { return SunsetUndersideColor; }

		static void Register(sol::table& parent);
	};

	// Top-level dynamic sky container assigned to level.dynamicSky.
	struct DynamicSky
	{
		bool        RealisticSkyDome  = false;
		ScriptColor BlackVoidColor    = ScriptColor(0, 0, 0);
		float       HorizonBottomFade = 0.0f;

		// Atmospheric sky scattering overrides. Negative = keep engine default.
		bool        HasSkyColor     = false;
		ScriptColor SkyColor        = ScriptColor(16, 37, 107); // 0.065, 0.145, 0.422 * 255 approx
		float       SundiskSize      = -1.0f; // [0.10, 10.0]
		float       SundiskIntensity = -1.0f; // [1.0, 200.0]
		float       HorizonDarkening = -1.0f; // [0.1, 5.0]
		float       TwilightOffset   = -1.0f; // [0.0, 0.3]
		float       SkyGradient      = -1.0f; // [0.1, 5.0]  SunElevationRampSpeed override.
		float       WarmInfluence    = -1.0f; // [0.0, 1.0]  SunWarmInfluence override.

		DynamicSkyAurora  Aurora  = {};
		DynamicSkyClouds  Clouds  = {};
		DynamicSkyGodRays GodRays = {};

		void        SetSkyColorLua(sol::object obj);
		ScriptColor GetSkyColorLua() const { return SkyColor; }

		static void Register(sol::table& parent);
	};

	// Standalone Lua type assigned to level.moonLens. Mirrors the LensFlare
	// constructor convention: MoonLens(pitch, yaw) creates an enabled moon at
	// the given orientation. Pitch is elevation in degrees (0 = horizon,
	// 90 = zenith); yaw is the compass direction in degrees.
	class MoonLens
	{
	public:
		static void Register(sol::table& parent);

		MoonLens() = default;
		MoonLens(float pitch, float yaw);

		float GetPitch() const   { return _pitch; }
		float GetYaw() const     { return _yaw; }
		bool  GetEnabled() const { return _isEnabled; }

		void SetPitch(float pitch)  { _pitch = pitch; }
		void SetYaw(float yaw)      { _yaw   = yaw; }
		void SetEnabled(bool value) { _isEnabled = value; }

	private:
		float _pitch     = 45.0f;
		float _yaw       = 180.0f;
		bool  _isEnabled = false;
	};

	// Per-level dust storm container assigned to level.dustStorm.
	// All numeric fields use a -1 sentinel meaning "not provided by the Lua
	// script — keep the engine default value".
	struct LevelDustStorm
	{
		bool        Enabled      = false;
		float       Density      = -1.0f; // [0, 2]
		float       MinHeight    = -1.0f; // [0, 1]
		float       MaxHeight    = -1.0f; // [0, 1]
		float       WindCoupling = -1.0f; // [0, 4]

		bool        HasColor     = false;
		ScriptColor Color        = ScriptColor(217, 166, 128); // 0.85, 0.65, 0.50 * 255

		void        SetColorLua(sol::object obj);
		ScriptColor GetColorLua() const { return Color; }

		static void Register(sol::table& parent);
	};

	// Per-level underwater sky container assigned to level.underwaterSky.
	// Enabled toggles the WaterSurface Layer A weather preset; WaveSpeed and
	// Color override the corresponding UnderwaterSkySettings fields when set.
	struct LevelUnderwaterSky
	{
		bool        HasEnabled = false;
		bool        Enabled    = false;
		float       WaveSpeed  = -1.0f; // < 0 = keep preset/default.

		bool        HasColor   = false;
		ScriptColor Color      = ScriptColor(0, 120, 255);

		void        SetEnabledLua(sol::object obj);
		bool        GetEnabledLua() const { return Enabled; }
		void        SetColorLua(sol::object obj);
		ScriptColor GetColorLua() const { return Color; }

		static void Register(sol::table& parent);
	};
}
