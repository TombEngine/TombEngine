#pragma once

#include "Specific/Input/Input.h"

using namespace TEN::Input;

namespace TEN::Scripting::Input
{
	/// Constants for analog axis IDs.
	// To be used with @{Input.GetAnalogAxisValue}.
	// @enum Input.AxisID
	// @pragma nostrip

	static const auto AXIS_IDS = std::unordered_map<std::string, AxisID>
	{
		/// Analog axis configured for player's movement.
		// @mem MOVE
		{ "MOVE", AxisID::Move },

		/// Analog axis configured for camera movement.
		// @mem CAMERA
		{ "CAMERA", AxisID::Camera },

		/// Raw mouse input analog axis.
		// @mem MOUSE
		{ "MOUSE", AxisID::Mouse }
	};
}
