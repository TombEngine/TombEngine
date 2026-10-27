#pragma once

#include "Specific/Input/Input.h"

using namespace TEN::Input;

namespace TEN::Scripting::Input
{
	/// Constants for input device IDs.
	// To be used with @{Input.GetLastInputDevice}.
	// @enum Input.InputDevice
	// @pragma nostrip

	static const auto INPUT_DEVICE_IDS = std::unordered_map<std::string, InputDevice>
	{
		/// Keyboard input device.
		// @mem KEYBOARD
		{ "KEYBOARD", InputDevice::Keyboard },

		/// Mouse input device.
		// @mem MOUSE
		{ "MOUSE", InputDevice::Mouse },

		/// Gamepad input device.
		// @mem GAMEPAD
		{ "GAMEPAD", InputDevice::Gamepad }
	};
}