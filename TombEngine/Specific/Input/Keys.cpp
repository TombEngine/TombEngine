#include "framework.h"
#include "Specific/Input/Input.h"
#include "Specific/Input/Keys.h"

namespace TEN::Input
{
	using GamepadKeyNameSet = std::array<std::string, (int)GamepadType::Count>;

	static GamepadKeyNameSet GamepadKeyNames(const char* xboxName, const char* playStationName = nullptr, const char* nintendoName = nullptr)
	{
		auto psName  = (playStationName != nullptr) ? playStationName : xboxName;
		auto ninName = (nintendoName    != nullptr) ? nintendoName    : xboxName;

		return { xboxName, psName, psName, ninName };
	}

	static const auto KEY_NAME_MAP = std::unordered_map<int, std::string>
	{
		{ KEY_UNASSIGNED, "<None>" },

		// Keyboard
		{ SDL_SCANCODE_ESCAPE,        "Esc" },
		{ SDL_SCANCODE_1,             "1" },
		{ SDL_SCANCODE_2,             "2" },
		{ SDL_SCANCODE_3,             "3" },
		{ SDL_SCANCODE_4,             "4" },
		{ SDL_SCANCODE_5,             "5" },
		{ SDL_SCANCODE_6,             "6" },
		{ SDL_SCANCODE_7,             "7" },
		{ SDL_SCANCODE_8,             "8" },
		{ SDL_SCANCODE_9,             "9" },
		{ SDL_SCANCODE_0,             "0" },
		{ SDL_SCANCODE_MINUS,         "-" },
		{ SDL_SCANCODE_EQUALS,        "+" },
		{ SDL_SCANCODE_BACKSPACE,     "Back" },
		{ SDL_SCANCODE_TAB,           "Tab" },
		{ SDL_SCANCODE_Q,             "Q" },
		{ SDL_SCANCODE_W,             "W" },
		{ SDL_SCANCODE_E,             "E" },
		{ SDL_SCANCODE_R,             "R" },
		{ SDL_SCANCODE_T,             "T" },
		{ SDL_SCANCODE_Y,             "Y" },
		{ SDL_SCANCODE_U,             "U" },
		{ SDL_SCANCODE_I,             "I" },
		{ SDL_SCANCODE_O,             "O" },
		{ SDL_SCANCODE_P,             "P" },
		{ SDL_SCANCODE_LEFTBRACKET,   "[" },
		{ SDL_SCANCODE_RIGHTBRACKET,  "]" },
		{ SDL_SCANCODE_RETURN,        "Enter" },
		{ SDL_SCANCODE_LCTRL,         "Ctrl" },
		{ SDL_SCANCODE_A,             "A" },
		{ SDL_SCANCODE_S,             "S" },
		{ SDL_SCANCODE_D,             "D" },
		{ SDL_SCANCODE_F,             "F" },
		{ SDL_SCANCODE_G,             "G" },
		{ SDL_SCANCODE_H,             "H" },
		{ SDL_SCANCODE_J,             "J" },
		{ SDL_SCANCODE_K,             "K" },
		{ SDL_SCANCODE_L,             "L" },
		{ SDL_SCANCODE_SEMICOLON,     ";" },
		{ SDL_SCANCODE_APOSTROPHE,    "'" },
		{ SDL_SCANCODE_GRAVE,         "`" },
		{ SDL_SCANCODE_LSHIFT,        "Shift" },
		{ SDL_SCANCODE_BACKSLASH,     "Back Slash" },
		{ SDL_SCANCODE_Z,             "Z" },
		{ SDL_SCANCODE_X,             "X" },
		{ SDL_SCANCODE_C,             "C" },
		{ SDL_SCANCODE_V,             "V" },
		{ SDL_SCANCODE_B,             "B" },
		{ SDL_SCANCODE_N,             "N" },
		{ SDL_SCANCODE_M,             "M" },
		{ SDL_SCANCODE_COMMA,         "," },
		{ SDL_SCANCODE_PERIOD,        "." },
		{ SDL_SCANCODE_SLASH,         "/" },
		{ SDL_SCANCODE_RSHIFT,        "Shift" },
		{ SDL_SCANCODE_KP_MULTIPLY,   "Pad X" },
		{ SDL_SCANCODE_LALT,          "Alt" },
		{ SDL_SCANCODE_SPACE,         "Space" },
		{ SDL_SCANCODE_CAPSLOCK,      "Caps Lock" },
		{ SDL_SCANCODE_F1,            "F1" },
		{ SDL_SCANCODE_F2,            "F2" },
		{ SDL_SCANCODE_F3,            "F3" },
		{ SDL_SCANCODE_F4,            "F4" },
		{ SDL_SCANCODE_F5,            "F5" },
		{ SDL_SCANCODE_F6,            "F6" },
		{ SDL_SCANCODE_F7,            "F7" },
		{ SDL_SCANCODE_F8,            "F8" },
		{ SDL_SCANCODE_F9,            "F9" },
		{ SDL_SCANCODE_F10,           "F10" },
		{ SDL_SCANCODE_NUMLOCKCLEAR,  "Num Lock" },
		{ SDL_SCANCODE_SCROLLLOCK,    "Scroll Lock" },
		{ SDL_SCANCODE_KP_7,          "Pad 7" },
		{ SDL_SCANCODE_KP_8,          "Pad 8" },
		{ SDL_SCANCODE_KP_9,          "Pad 9" },
		{ SDL_SCANCODE_KP_MINUS,      "Pad -" },
		{ SDL_SCANCODE_KP_4,          "Pad 4" },
		{ SDL_SCANCODE_KP_5,          "Pad 5" },
		{ SDL_SCANCODE_KP_6,          "Pad 6" },
		{ SDL_SCANCODE_KP_PLUS,       "Pad +" },
		{ SDL_SCANCODE_KP_1,          "Pad 1" },
		{ SDL_SCANCODE_KP_2,          "Pad 2" },
		{ SDL_SCANCODE_KP_3,          "Pad 3" },
		{ SDL_SCANCODE_KP_0,          "Pad 0" },
		{ SDL_SCANCODE_KP_PERIOD,     "Pad ." },
		{ SDL_SCANCODE_NONUSBACKSLASH,"\\" },
		{ SDL_SCANCODE_F11,           "F11" },
		{ SDL_SCANCODE_F12,           "F12" },
		{ SDL_SCANCODE_KP_ENTER,      "Pad Enter" },
		{ SDL_SCANCODE_RCTRL,         "Ctrl" },
		{ SDL_SCANCODE_KP_DIVIDE,     "Pad /" },
		{ SDL_SCANCODE_PRINTSCREEN,   "Print Screen" },
		{ SDL_SCANCODE_RALT,          "Alt" },
		{ SDL_SCANCODE_HOME,          "Home" },
		{ SDL_SCANCODE_UP,            "Up" },
		{ SDL_SCANCODE_PAGEUP,        "Page Up" },
		{ SDL_SCANCODE_LEFT,          "Left" },
		{ SDL_SCANCODE_RIGHT,         "Right" },
		{ SDL_SCANCODE_END,           "End" },
		{ SDL_SCANCODE_DOWN,          "Down" },
		{ SDL_SCANCODE_PAGEDOWN,      "Page Down" },
		{ SDL_SCANCODE_INSERT,        "Insert" },
		{ SDL_SCANCODE_DELETE,        "Del" },

		// Mouse
		{ MK_LCLICK,     "Left-Click" },
		{ MK_RCLICK,     "Right-Click" },
		{ MK_MCLICK,     "Middle-Click" },
		{ MK_BUTTON_4,   "Mouse 4" },
		{ MK_BUTTON_5,   "Mouse 5" },
		{ MK_BUTTON_6,   "Mouse 6" },
		{ MK_BUTTON_7,   "Mouse 7" },
		{ MK_BUTTON_8,   "Mouse 8" },
		{ MK_AXIS_X_NEG, "Mouse X-" },
		{ MK_AXIS_X_POS, "Mouse X+" },
		{ MK_AXIS_Y_NEG, "Mouse Y-" },
		{ MK_AXIS_Y_POS, "Mouse Y+" },
		{ MK_AXIS_Z_NEG, "Wheel Up" },
		{ MK_AXIS_Z_POS, "Wheel Down" },
	};

	static const auto GAMEPAD_KEY_NAME_MAP = std::unordered_map<int, GamepadKeyNameSet>
	{
		{ GK_SOUTH,        GamepadKeyNames("A", "Cross", "B") },
		{ GK_EAST,         GamepadKeyNames("B", "Circle", "A") },
		{ GK_WEST,         GamepadKeyNames("X", "Square", "Y") },
		{ GK_NORTH,        GamepadKeyNames("Y", "Triangle", "X") },
		{ GK_BACK,         GamepadKeyNames("Back", "Select", "Minus") },
		{ GK_GUIDE,        GamepadKeyNames("Guide", "PS", "Home") },
		{ GK_START,        GamepadKeyNames("Start", "Start", "Plus") },
		{ GK_LSTICK,       GamepadKeyNames("L Stick") },
		{ GK_RSTICK,       GamepadKeyNames("R Stick") },
		{ GK_LSHOULDER,    GamepadKeyNames("LB", "L1", "L") },
		{ GK_RSHOULDER,    GamepadKeyNames("RB", "R1", "R") },
		{ GK_DPAD_UP,      GamepadKeyNames("D-Pad Up") },
		{ GK_DPAD_DOWN,    GamepadKeyNames("D-Pad Down") },
		{ GK_DPAD_LEFT,    GamepadKeyNames("D-Pad Left") },
		{ GK_DPAD_RIGHT,   GamepadKeyNames("D-Pad Right") },
		{ GK_MISC1,        GamepadKeyNames("Share", "Mic", "Capture") },
		{ GK_RPADDLE1,     GamepadKeyNames("P1") },
		{ GK_LPADDLE1,     GamepadKeyNames("P3") },
		{ GK_RPADDLE2,     GamepadKeyNames("P2") },
		{ GK_LPADDLE2,     GamepadKeyNames("P4") },
		{ GK_TOUCHPAD,     GamepadKeyNames("Touchpad") },
		{ GK_MISC2,        GamepadKeyNames("Misc 2") },
		{ GK_MISC3,        GamepadKeyNames("Misc 3") },
		{ GK_MISC4,        GamepadKeyNames("Misc 4") },
		{ GK_MISC5,        GamepadKeyNames("Misc 5") },
		{ GK_MISC6,        GamepadKeyNames("Misc 6") },

		{ GK_LSTICK_X_NEG, GamepadKeyNames("L Stick X-") },
		{ GK_LSTICK_X_POS, GamepadKeyNames("L Stick X+") },
		{ GK_LSTICK_Y_NEG, GamepadKeyNames("L Stick Y-") },
		{ GK_LSTICK_Y_POS, GamepadKeyNames("L Stick Y+") },
		{ GK_RSTICK_X_NEG, GamepadKeyNames("R Stick X-") },
		{ GK_RSTICK_X_POS, GamepadKeyNames("R Stick X+") },
		{ GK_RSTICK_Y_NEG, GamepadKeyNames("R Stick Y-") },
		{ GK_RSTICK_Y_POS, GamepadKeyNames("R Stick Y+") },
		{ GK_LTRIGGER_NEG, GamepadKeyNames("LT", "L2", "ZL") },
		{ GK_LTRIGGER_POS, GamepadKeyNames("LT", "L2", "ZL") },
		{ GK_RTRIGGER_NEG, GamepadKeyNames("RT", "R2", "ZR") },
		{ GK_RTRIGGER_POS, GamepadKeyNames("RT", "R2", "ZR") }
	};

	const std::string& GetKeyName(int keyID)
	{
		// Find and return key name.
		// For gamepads, return key name according to a gamepad vendor.
		auto gamepadIt = GAMEPAD_KEY_NAME_MAP.find(keyID);
		if (gamepadIt != GAMEPAD_KEY_NAME_MAP.end())
		{
			const auto& keyNames = gamepadIt->second;
			return keyNames[(int)GetGamepadType()];
		}

		auto it = KEY_NAME_MAP.find(keyID);
		if (it != KEY_NAME_MAP.end())
			return it->second;

		return KEY_NAME_MAP.at(KEY_UNASSIGNED);
	}
}
