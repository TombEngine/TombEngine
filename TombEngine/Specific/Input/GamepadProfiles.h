#pragma once

#include "Specific/Input/Bindings.h"
#include "Specific/Input/Input.h"

namespace TEN::Input
{
	// Default binding profiles for each controller family. All share the same
	// SDL3-normalized logical button layout (SOUTH / EAST / WEST / NORTH), so
	// the profiles start identical and can diverge as needed per family.
	extern const BindingProfile DEFAULT_XBOX_GAMEPAD_BINDING_PROFILE;
	extern const BindingProfile DEFAULT_PS4_GAMEPAD_BINDING_PROFILE;
	extern const BindingProfile DEFAULT_PS5_GAMEPAD_BINDING_PROFILE;
	extern const BindingProfile DEFAULT_SWITCH_PRO_GAMEPAD_BINDING_PROFILE;

	const BindingProfile& GetDefaultGamepadBindingProfile(GamepadType gamepadType);
}
