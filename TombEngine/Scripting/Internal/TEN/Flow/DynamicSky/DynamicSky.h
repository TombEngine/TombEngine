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

		static void Register(sol::table& parent);
	};

	// Volumetric clouds sub-section of level.dynamicSky.
	struct DynamicSkyClouds
	{
		bool        Enabled           = false;
		std::string StartPreset       = "";    // Initial weather preset for the level.
		float       TransformDuration = -1.0f; // < 0 = per-preset default.
		std::string Quality           = "";    // "Low" / "Medium" / "High"; empty = keep current.

		std::vector<DynamicSkyCloudChangeEntry> ChangePresets;

		// Lua property: ChangePresets is exposed via a write-only table setter.
		void SetChangePresetsLua(sol::object obj);

		static void Register(sol::table& parent);
	};

	// Top-level dynamic sky container assigned to level.dynamicSky.
	struct DynamicSky
	{
		bool        RealisticSkyDome  = false;
		ScriptColor BlackVoidColor    = ScriptColor(0, 0, 0);
		float       HorizonBottomFade = 0.0f;

		DynamicSkyAurora Aurora = {};
		DynamicSkyClouds Clouds = {};

		static void Register(sol::table& parent);
	};
}
