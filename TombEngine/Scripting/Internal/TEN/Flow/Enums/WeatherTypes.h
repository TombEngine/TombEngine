#pragma once

#include "Scripting/Include/ScriptInterfaceLevel.h"

namespace TEN::Scripting
{
	/// Constants for weather types.
	// @enum Flow.WeatherType
	// @pragma nostrip

	static const std::unordered_map<std::string, WeatherType> WEATHER_TYPES
	{
		/// No weather.
		// @mem None
		{ "None", WeatherType::None },

		/// Rain weather.
		// @mem Rain
		{ "Rain", WeatherType::Rain },

		/// Snow weather.
		// @mem Snow
		{ "Snow", WeatherType::Snow }
	};
}
