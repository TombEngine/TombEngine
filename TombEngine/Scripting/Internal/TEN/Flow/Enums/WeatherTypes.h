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
		// @mem NONE
		{ "NONE", WeatherType::None },

		/// Rain weather.
		// @mem RAIN
		{ "RAIN", WeatherType::Rain },

		/// Snow weather.
		// @mem SNOW
		{ "SNOW", WeatherType::Snow },

		//COMPATIBILITY
		{ "None", WeatherType::None },
		{ "Rain", WeatherType::Rain },
		{ "Snow", WeatherType::Snow }
	};
}
