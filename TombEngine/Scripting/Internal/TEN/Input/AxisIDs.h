#pragma once

#include "Specific/Input/Input.h"

using namespace TEN::Input;

namespace TEN::Scripting::Input
{
	/// Constants for analog axis IDs.
	// @enum Input.AxisID
	// @pragma nostrip

	/// Table of Input.AxisID constants.
	// To be used with @{Input.GetAnalogAxisValue}.
	//
	// - `MOVE` - Analog axis configured for player's movement.
	// - `CAMERA` - Analog axis configured for camera movement.
	// - `MOUSE` - Raw mouse input analog axis.
	//
	//@table Input.AxisID

	static const auto AXIS_IDS = std::unordered_map<std::string, AxisID>
	{
		{ "MOVE", AxisID::Move },
		{ "CAMERA", AxisID::Camera },
		{ "MOUSE", AxisID::Mouse }
	};
}
