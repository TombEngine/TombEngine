#pragma once

#include "Specific/Input/Input.h"

using namespace TEN::Input;

namespace TEN::Scripting::Input
{
	/// Constants for analog axis IDs.
	// @enum Input.AxisID
	// @pragma nostrip

	/// Table of Input.AxisID constants.
	// To be used with @{Input.GetAxis}.
	//
	//	MOVE
	//	CAMERA
	//	MOUSE
	//
	//@table Input.AxisID

	static const auto AXIS_IDS = std::unordered_map<std::string, AxisID>
	{

		{ "MOVE", AxisID::Move },
		{ "CAMERA", AxisID::Camera },
		{ "MOUSE", AxisID::Mouse }
	};
}
