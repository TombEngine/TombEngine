#pragma once

namespace sol { class state; }

namespace TEN::Scripting
{
	struct VolumetricClouds
	{
		float       Speed             = -1.0f;    // < 0 = use global (Settings.lua)
		float       WindDirectionX    = 1.0f;
		float       WindDirectionZ    = 0.0f;
		float       TransformDuration = -1.0f;    // < 0 = use per-preset default
		std::string Quality           = "Medium"; // "Low", "Medium", or "High"

		static void Register(sol::table& parent);
	};
}
