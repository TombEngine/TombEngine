#pragma once

#include "Game/Lara/lara_struct.h"

/// Constants for player water statuses.
// To be used with @{Objects.LaraObject.GetWaterStatus} function.
// @enum Objects.WaterStatus
// @pragma nostrip

namespace TEN::Scripting
{
	static const auto WATER_STATUSES = std::unordered_map<std::string, WaterStatus>
	{
		/// Player is in a dry room.
		// @mem DRY
		{ "DRY", WaterStatus::Dry },

		/// Player is wading the water.
		// @mem WADE
		{ "WADE", WaterStatus::Wade },

		/// Player is treading the water.
		// @mem TREADWATER
		{ "TREADWATER", WaterStatus::TreadWater },

		/// Player is underwater.
		// @mem UNDERWATER
		{ "UNDERWATER", WaterStatus::Underwater },

		/// Player is using flycheat.
		// @mem FLYCHEAT
		{ "FLYCHEAT", WaterStatus::FlyCheat }
	};
}
