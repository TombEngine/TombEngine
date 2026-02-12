#pragma once

#include "Game/Lara/lara_struct.h"

/// Constants for player water statuses.
// @enum Objects.WaterStatus
// @pragma nostrip

/// Table of Objects.WaterStatus constants.
// To be used with @{Objects.LaraObject.GetWaterStatus} function.
//
// - `DRY` Player is in a dry room.
// - `WADE` Player is wading the water.
// - `TREADWATER` Player is treading the water.
// - `UNDERWATER` Player is underwater.
// - `FLYCHEAT` Player is using flycheat.
//
// @table Objects.WaterStatus

namespace TEN::Scripting
{
	static const auto WATER_STATUSES = std::unordered_map<std::string, WaterStatus>
	{
		{ "DRY", WaterStatus::Dry },
		{ "WADE", WaterStatus::Wade },
		{ "TREADWATER", WaterStatus::TreadWater },
		{ "UNDERWATER", WaterStatus::Underwater },
		{ "FLYCHEAT", WaterStatus::FlyCheat }
	};
}
