#pragma once

// TODO: Refactor this entire system. Valid input "events" should be collected and
// boilerplated to make hardware input to game input translation library-agnostic.
// I already did this for another project using SDL, will be easy to adapt. -- Sezz 2025.05.03

namespace TEN::Input
{
	constexpr auto KEYBOARD_KEY_COUNT	  = 256;
	constexpr auto MOUSE_BUTTON_COUNT	  = 8;
	constexpr auto MOUSE_AXIS_COUNT		  = 3;
	constexpr auto GAMEPAD_BUTTON_COUNT	  = 16;
	constexpr auto GAMEPAD_AXIS_COUNT	  = 6;
	constexpr auto GAMEPAD_POV_AXIS_COUNT = 4;
	
	constexpr auto KEY_COUNT = KEYBOARD_KEY_COUNT +
							   MOUSE_BUTTON_COUNT + (MOUSE_AXIS_COUNT * 2) +
							   GAMEPAD_BUTTON_COUNT + (GAMEPAD_AXIS_COUNT * 2) + GAMEPAD_POV_AXIS_COUNT;

	constexpr auto KEY_OFFSET_MOUSE	  = KEYBOARD_KEY_COUNT;
	constexpr auto KEY_OFFSET_GAMEPAD = KEY_OFFSET_MOUSE + (MOUSE_BUTTON_COUNT + (MOUSE_AXIS_COUNT * 2));

	// Mouse
	// 8 buttons + (3 * 2) axes.
	enum MouseKey
	{
		// Buttons
		MK_LCLICK = KEY_OFFSET_MOUSE,
		MK_RCLICK,
		MK_MCLICK,
		MK_BUTTON_4,
		MK_BUTTON_5,
		MK_BUTTON_6,
		MK_BUTTON_7,
		MK_BUTTON_8,

		// Axes
		MK_AXIS_X_NEG,
		MK_AXIS_X_POS,
		MK_AXIS_Y_NEG,
		MK_AXIS_Y_POS,
		MK_AXIS_Z_NEG,
		MK_AXIS_Z_POS
	};

	// TODO: Don't need this one at all. -- Sezz 2025.03.05
	// Gamepad
	// 16 buttons + (6 * 2) axes + 4 POV axes.
	enum GamepadKey
	{
		// Buttons
		GK_BUTTON_1 = KEY_OFFSET_GAMEPAD,
		GK_BUTTON_2,
		GK_BUTTON_3,
		GK_BUTTON_4,
		GK_BUTTON_5,
		GK_BUTTON_6,
		GK_BUTTON_7,
		GK_BUTTON_8,
		GK_BUTTON_9,
		GK_BUTTON_10,
		GK_BUTTON_11,
		GK_BUTTON_12,
		GK_BUTTON_13,
		GK_BUTTON_14,
		GK_BUTTON_15,
		GK_BUTTON_16,
		
		// Axes
		GK_AXIS_X_NEG,
		GK_AXIS_X_POS,
		GK_AXIS_Y_NEG,
		GK_AXIS_Y_POS,
		GK_AXIS_Z_NEG,
		GK_AXIS_Z_POS,
		GK_AXIS_W_NEG,
		GK_AXIS_W_POS,
		GK_AXIS_L_TRIGGER_1,
		GK_AXIS_L_TRIGGER_2,
		GK_AXIS_R_TRIGGER_1,
		GK_AXIS_R_TRIGGER_2,

		// POV axes
		GK_DPAD_UP,
		GK_DPAD_DOWN,
		GK_DPAD_LEFT,
		GK_DPAD_RIGHT
	};

	// XBox Controller (GamepadKey mirror).
	// 11 buttons + (6 * 2) axes + 4 POV axes.
	enum XInputKey
	{
		// Buttons
		XK_START = GK_BUTTON_1,
		XK_SELECT,
		XK_L_STICK,
		XK_R_STICK,
		XK_L_SHIFT,
		XK_R_SHIFT,
		XK_UNUSED_1,
		XK_UNUSED_2,
		XK_A,
		XK_B,
		XK_X,
		XK_Y,
		XK_LOGO,

		// Axes
		XK_AXIS_X_POS = GK_AXIS_X_NEG,
		XK_AXIS_X_NEG,
		XK_AXIS_Y_POS,
		XK_AXIS_Y_NEG,
		XK_AXIS_Z_POS,
		XK_AXIS_Z_NEG,
		XK_AXIS_W_POS,
		XK_AXIS_W_NEG,
		XK_AXIS_L_TRIGGER_NEG,
		XK_AXIS_L_TRIGGER_POS,
		XK_AXIS_R_TRIGGER_NEG,
		XK_AXIS_R_TRIGGER_POS,

		// POV axes
		XK_DPAD_UP,
		XK_DPAD_DOWN,
		XK_DPAD_LEFT,
		XK_DPAD_RIGHT
	};

	const std::string& GetKeyName(int keyID);
}
