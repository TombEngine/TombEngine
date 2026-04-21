#include "framework.h"
#include "Scripting/Internal/TEN/Flow/VolumetricClouds/VolumetricClouds.h"

/***
Per-level volumetric cloud wind settings.
To be used with the @{Flow.Level.volumetricClouds} property.

@tenprimitive Flow.VolumetricClouds
@pragma nostrip
*/

namespace TEN::Scripting
{
	void VolumetricClouds::Register(sol::table& parent)
	{
		parent.new_usertype<VolumetricClouds>(
			"VolumetricClouds",
			sol::constructors<VolumetricClouds()>(),
			sol::call_constructor, sol::constructors<VolumetricClouds()>(),

			/// (float) Cloud wind speed for this level.
			// Overrides the global wind speed set by Flow.SetCloudWind() in Settings.lua.
			// Omit or set to a negative value to use the global speed.
			// @mem speed
			"speed", &VolumetricClouds::Speed,

			/// (float) X component of the wind direction vector for this level.
			// Together with windDirectionZ this defines a normalised 2D direction.
			// Omit to use the global direction from Settings.lua.
			// @mem windDirectionX
			"windDirectionX", &VolumetricClouds::WindDirectionX,

			/// (float) Z component of the wind direction vector for this level.
			// Together with windDirectionX this defines a normalised 2D direction.
			// Omit to use the global direction from Settings.lua.
			// @mem windDirectionZ
			"windDirectionZ", &VolumetricClouds::WindDirectionZ,

			/// (float) CloudMorph transition duration in seconds for this level.
			// Overrides the per-preset transform duration for all presets.
			// Omit or set to a negative value to use per-preset defaults.
			// @mem transformDuration
			"transformDuration", &VolumetricClouds::TransformDuration
		);
	}
}
