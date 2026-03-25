#pragma once

// ============================================================================
// WeatherPreset.h — Lua-facing weather preset control
//
// Provides the Flow.WeatherPreset class and weather control functions
// for the TEN scripting API.
// ============================================================================

#include <string>
#include <sol/sol.hpp>

namespace TEN::Scripting
{
	/// Registers weather/sky/cloud API into the Flow table.
	///
	/// Lua API:
	///
	///   -- Immediate preset switch
	///   Flow.SetWeatherPreset("ClearSky")
	///
	///   -- Smooth transition over time (seconds)
	///   Flow.TransitionWeather("Thunderstorm", 120.0)
	///   Flow.TransitionWeather("Overcast", 60.0, "EaseInOut")
	///
	///   -- Query
	///   local current = Flow.GetCurrentWeatherPreset()
	///   local target  = Flow.GetTargetWeatherPreset()
	///   local progress = Flow.GetWeatherTransitionProgress()
	///
	///   -- Manual layer override (advanced)
	///   Flow.SetVolumetricCloudLayerA({ coverage = 0.5, density = 0.8, ... })
	///   Flow.SetVolumetricCloudLayerB({ coverage = 0.3, density = 0.5, ... })
	///   Flow.ClearWeatherOverrides()
	///
	///   -- Aurora fade duration (seconds for aurora to appear/disappear on preset change)
	///   Flow.SetAuroraFadeDuration(15.0)
	///   local dur = Flow.GetAuroraFadeDuration()
	///
	void RegisterWeatherAPI(sol::table& parent);
}
