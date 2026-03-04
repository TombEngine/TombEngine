#pragma once

#include "Specific/Input/Input.h"

using namespace TEN::Input;

namespace TEN::Scripting::Input
{
	/// Constants for analog axis IDs.
	// To be used with @{Input.GetAnalogAxisValue}.
	// @enum Input.AxisID
	// @pragma nostrip

	static const auto ANALOG_AXIS_IDS = std::unordered_map<std::string, AnalogAxisId>
	{
		/// Analog axis configured for player movement.
		// @mem MOVE
		{ "MOVE", AnalogAxisId::Move },

		/// Analog axis configured for camera movement.
		// @mem CAMERA
		{ "CAMERA", AnalogAxisId::Camera },

		/// Raw mouse movement analog axis.
		// @mem MOUSE
		{ "MOUSE", AnalogAxisId::Mouse },

		/// Raw left gamepad stick analog axis.
		// @mem STICK_LEFT
		{ "STICK_LEFT", AnalogAxisId::StickLeft },

		/// Raw right gamepad stick analog axis.
		// @mem STICK_RIGHT
		{ "STICK_RIGHT", AnalogAxisId::StickRight }
	};
}
