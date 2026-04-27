#include "framework.h"
#include "Specific/Input/Keys.h"

namespace TEN::Input
{
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

		// Gamepad (Xbox-style nomenclature; SDL_Gamepad maps Nintendo/PlayStation pads to the same layout).
		{ GK_SOUTH,        "A" },
		{ GK_EAST,         "B" },
		{ GK_WEST,         "X" },
		{ GK_NORTH,        "Y" },
		{ GK_BACK,         "Back" },
		{ GK_GUIDE,        "Guide" },
		{ GK_START,        "Start" },
		{ GK_LSTICK,       "L Stick" },
		{ GK_RSTICK,       "R Stick" },
		{ GK_LSHOULDER,    "LB" },
		{ GK_RSHOULDER,    "RB" },
		{ GK_DPAD_UP,      "D-Pad Up" },
		{ GK_DPAD_DOWN,    "D-Pad Down" },
		{ GK_DPAD_LEFT,    "D-Pad Left" },
		{ GK_DPAD_RIGHT,   "D-Pad Right" },

		{ GK_LSTICK_X_NEG, "L Stick X-" },
		{ GK_LSTICK_X_POS, "L Stick X+" },
		{ GK_LSTICK_Y_NEG, "L Stick Y-" },
		{ GK_LSTICK_Y_POS, "L Stick Y+" },
		{ GK_RSTICK_X_NEG, "R Stick X-" },
		{ GK_RSTICK_X_POS, "R Stick X+" },
		{ GK_RSTICK_Y_NEG, "R Stick Y-" },
		{ GK_RSTICK_Y_POS, "R Stick Y+" },
		{ GK_LTRIGGER_NEG, "LT" },
		{ GK_LTRIGGER_POS, "LT" },
		{ GK_RTRIGGER_NEG, "RT" },
		{ GK_RTRIGGER_POS, "RT" }
	};

	const std::string& GetKeyName(int keyID)
	{
		auto it = KEY_NAME_MAP.find(keyID);
		if (it != KEY_NAME_MAP.end())
			return it->second;

		return KEY_NAME_MAP.at(KEY_UNASSIGNED);
	}

	// Legacy keyID translation table.
	//
	// Pre-SDL3 ten.conf files store integer keyIDs derived from OIS::KeyCode (DIK_*-style)
	// for keyboard, MK_* with a 256 base for mouse, and XK_*/GK_* with a 270 base for gamepad.
	// SDL3 scancodes use a different numbering, the mouse offset moved to SDL_SCANCODE_COUNT,
	// and the gamepad button order follows SDL_GamepadButton (south/east/west/north/...).
	//
	// This table maps every keyID that could legitimately appear in a v1 config to the
	// equivalent v2 keyID. Unknown values fall through to KEY_UNASSIGNED, which causes
	// the loader to skip the binding and let defaults fill in.
	int TranslateLegacyOisKeyId(int legacyKeyId)
	{
		// Legacy mouse range: [256, 270). Layout was identical (Left/Right/Middle/...),
		// so just shift to the new offset.
		constexpr int LEGACY_KEY_OFFSET_MOUSE   = 256;
		constexpr int LEGACY_KEY_OFFSET_GAMEPAD = 270;
		constexpr int LEGACY_MOUSE_SLOT_COUNT   = MOUSE_BUTTON_COUNT + (MOUSE_AXIS_COUNT * 2);

		if (legacyKeyId >= LEGACY_KEY_OFFSET_MOUSE && legacyKeyId < LEGACY_KEY_OFFSET_MOUSE + LEGACY_MOUSE_SLOT_COUNT)
			return KEY_OFFSET_MOUSE + (legacyKeyId - LEGACY_KEY_OFFSET_MOUSE);

		// Legacy gamepad range: [270, 302). XInput-style layout differs from SDL_GamepadButton order,
		// so an explicit table is needed. Buttons unused in the legacy XK_ enum return KEY_UNASSIGNED.
		if (legacyKeyId >= LEGACY_KEY_OFFSET_GAMEPAD)
		{
			static const auto LEGACY_GAMEPAD_TABLE = std::unordered_map<int, int>
			{
				{ 270, GK_START },        // XK_START
				{ 271, GK_BACK },         // XK_SELECT
				{ 272, GK_LSTICK },       // XK_L_STICK
				{ 273, GK_RSTICK },       // XK_R_STICK
				{ 274, GK_LSHOULDER },    // XK_L_SHIFT
				{ 275, GK_RSHOULDER },    // XK_R_SHIFT
				{ 278, GK_SOUTH },        // XK_A
				{ 279, GK_EAST },         // XK_B
				{ 280, GK_WEST },         // XK_X
				{ 281, GK_NORTH },        // XK_Y
				{ 282, GK_GUIDE },        // XK_LOGO
				{ 286, GK_LSTICK_X_POS }, // XK_AXIS_X_POS
				{ 287, GK_LSTICK_X_NEG }, // XK_AXIS_X_NEG
				{ 288, GK_LSTICK_Y_POS }, // XK_AXIS_Y_POS
				{ 289, GK_LSTICK_Y_NEG }, // XK_AXIS_Y_NEG
				{ 290, GK_RSTICK_X_POS }, // XK_AXIS_Z_POS
				{ 291, GK_RSTICK_X_NEG }, // XK_AXIS_Z_NEG
				{ 292, GK_RSTICK_Y_POS }, // XK_AXIS_W_POS
				{ 293, GK_RSTICK_Y_NEG }, // XK_AXIS_W_NEG
				{ 294, GK_LTRIGGER_NEG }, // XK_AXIS_L_TRIGGER_NEG
				{ 295, GK_LTRIGGER_POS }, // XK_AXIS_L_TRIGGER_POS
				{ 296, GK_RTRIGGER_NEG }, // XK_AXIS_R_TRIGGER_NEG
				{ 297, GK_RTRIGGER_POS }, // XK_AXIS_R_TRIGGER_POS
				{ 298, GK_DPAD_UP },      // XK_DPAD_UP
				{ 299, GK_DPAD_DOWN },    // XK_DPAD_DOWN
				{ 300, GK_DPAD_LEFT },    // XK_DPAD_LEFT
				{ 301, GK_DPAD_RIGHT }    // XK_DPAD_RIGHT
			};

			auto it = LEGACY_GAMEPAD_TABLE.find(legacyKeyId);
			return (it != LEGACY_GAMEPAD_TABLE.end()) ? it->second : KEY_UNASSIGNED;
		}

		// Legacy keyboard range: [0, 256), values are OIS::KeyCode (DIK_*).
		// Each entry uses the OIS::KC_* hex value for clarity; comments name the OIS macro.
		static const auto LEGACY_KEYBOARD_TABLE = std::unordered_map<int, int>
		{
			{ 0x01, SDL_SCANCODE_ESCAPE },        // KC_ESCAPE
			{ 0x02, SDL_SCANCODE_1 },             // KC_1
			{ 0x03, SDL_SCANCODE_2 },             // KC_2
			{ 0x04, SDL_SCANCODE_3 },             // KC_3
			{ 0x05, SDL_SCANCODE_4 },             // KC_4
			{ 0x06, SDL_SCANCODE_5 },             // KC_5
			{ 0x07, SDL_SCANCODE_6 },             // KC_6
			{ 0x08, SDL_SCANCODE_7 },             // KC_7
			{ 0x09, SDL_SCANCODE_8 },             // KC_8
			{ 0x0A, SDL_SCANCODE_9 },             // KC_9
			{ 0x0B, SDL_SCANCODE_0 },             // KC_0
			{ 0x0C, SDL_SCANCODE_MINUS },         // KC_MINUS
			{ 0x0D, SDL_SCANCODE_EQUALS },        // KC_EQUALS
			{ 0x0E, SDL_SCANCODE_BACKSPACE },     // KC_BACK
			{ 0x0F, SDL_SCANCODE_TAB },           // KC_TAB
			{ 0x10, SDL_SCANCODE_Q },             // KC_Q
			{ 0x11, SDL_SCANCODE_W },             // KC_W
			{ 0x12, SDL_SCANCODE_E },             // KC_E
			{ 0x13, SDL_SCANCODE_R },             // KC_R
			{ 0x14, SDL_SCANCODE_T },             // KC_T
			{ 0x15, SDL_SCANCODE_Y },             // KC_Y
			{ 0x16, SDL_SCANCODE_U },             // KC_U
			{ 0x17, SDL_SCANCODE_I },             // KC_I
			{ 0x18, SDL_SCANCODE_O },             // KC_O
			{ 0x19, SDL_SCANCODE_P },             // KC_P
			{ 0x1A, SDL_SCANCODE_LEFTBRACKET },   // KC_LBRACKET
			{ 0x1B, SDL_SCANCODE_RIGHTBRACKET },  // KC_RBRACKET
			{ 0x1C, SDL_SCANCODE_RETURN },        // KC_RETURN
			{ 0x1D, SDL_SCANCODE_LCTRL },         // KC_LCONTROL
			{ 0x1E, SDL_SCANCODE_A },             // KC_A
			{ 0x1F, SDL_SCANCODE_S },             // KC_S
			{ 0x20, SDL_SCANCODE_D },             // KC_D
			{ 0x21, SDL_SCANCODE_F },             // KC_F
			{ 0x22, SDL_SCANCODE_G },             // KC_G
			{ 0x23, SDL_SCANCODE_H },             // KC_H
			{ 0x24, SDL_SCANCODE_J },             // KC_J
			{ 0x25, SDL_SCANCODE_K },             // KC_K
			{ 0x26, SDL_SCANCODE_L },             // KC_L
			{ 0x27, SDL_SCANCODE_SEMICOLON },     // KC_SEMICOLON
			{ 0x28, SDL_SCANCODE_APOSTROPHE },    // KC_APOSTROPHE
			{ 0x29, SDL_SCANCODE_GRAVE },         // KC_GRAVE
			{ 0x2A, SDL_SCANCODE_LSHIFT },        // KC_LSHIFT
			{ 0x2B, SDL_SCANCODE_BACKSLASH },     // KC_BACKSLASH
			{ 0x2C, SDL_SCANCODE_Z },             // KC_Z
			{ 0x2D, SDL_SCANCODE_X },             // KC_X
			{ 0x2E, SDL_SCANCODE_C },             // KC_C
			{ 0x2F, SDL_SCANCODE_V },             // KC_V
			{ 0x30, SDL_SCANCODE_B },             // KC_B
			{ 0x31, SDL_SCANCODE_N },             // KC_N
			{ 0x32, SDL_SCANCODE_M },             // KC_M
			{ 0x33, SDL_SCANCODE_COMMA },         // KC_COMMA
			{ 0x34, SDL_SCANCODE_PERIOD },        // KC_PERIOD
			{ 0x35, SDL_SCANCODE_SLASH },         // KC_SLASH
			{ 0x36, SDL_SCANCODE_RSHIFT },        // KC_RSHIFT
			{ 0x37, SDL_SCANCODE_KP_MULTIPLY },   // KC_MULTIPLY
			{ 0x38, SDL_SCANCODE_LALT },          // KC_LMENU (left alt)
			{ 0x39, SDL_SCANCODE_SPACE },         // KC_SPACE
			{ 0x3A, SDL_SCANCODE_CAPSLOCK },      // KC_CAPITAL (caps lock)
			{ 0x3B, SDL_SCANCODE_F1 },            // KC_F1
			{ 0x3C, SDL_SCANCODE_F2 },            // KC_F2
			{ 0x3D, SDL_SCANCODE_F3 },            // KC_F3
			{ 0x3E, SDL_SCANCODE_F4 },            // KC_F4
			{ 0x3F, SDL_SCANCODE_F5 },            // KC_F5
			{ 0x40, SDL_SCANCODE_F6 },            // KC_F6
			{ 0x41, SDL_SCANCODE_F7 },            // KC_F7
			{ 0x42, SDL_SCANCODE_F8 },            // KC_F8
			{ 0x43, SDL_SCANCODE_F9 },            // KC_F9
			{ 0x44, SDL_SCANCODE_F10 },           // KC_F10
			{ 0x45, SDL_SCANCODE_NUMLOCKCLEAR },  // KC_NUMLOCK
			{ 0x46, SDL_SCANCODE_SCROLLLOCK },    // KC_SCROLL
			{ 0x47, SDL_SCANCODE_KP_7 },          // KC_NUMPAD7
			{ 0x48, SDL_SCANCODE_KP_8 },          // KC_NUMPAD8
			{ 0x49, SDL_SCANCODE_KP_9 },          // KC_NUMPAD9
			{ 0x4A, SDL_SCANCODE_KP_MINUS },      // KC_SUBTRACT
			{ 0x4B, SDL_SCANCODE_KP_4 },          // KC_NUMPAD4
			{ 0x4C, SDL_SCANCODE_KP_5 },          // KC_NUMPAD5
			{ 0x4D, SDL_SCANCODE_KP_6 },          // KC_NUMPAD6
			{ 0x4E, SDL_SCANCODE_KP_PLUS },       // KC_ADD
			{ 0x4F, SDL_SCANCODE_KP_1 },          // KC_NUMPAD1
			{ 0x50, SDL_SCANCODE_KP_2 },          // KC_NUMPAD2
			{ 0x51, SDL_SCANCODE_KP_3 },          // KC_NUMPAD3
			{ 0x52, SDL_SCANCODE_KP_0 },          // KC_NUMPAD0
			{ 0x53, SDL_SCANCODE_KP_PERIOD },     // KC_DECIMAL
			{ 0x56, SDL_SCANCODE_NONUSBACKSLASH },// KC_OEM_102
			{ 0x57, SDL_SCANCODE_F11 },           // KC_F11
			{ 0x58, SDL_SCANCODE_F12 },           // KC_F12
			{ 0x9C, SDL_SCANCODE_KP_ENTER },      // KC_NUMPADENTER
			{ 0x9D, SDL_SCANCODE_RCTRL },         // KC_RCONTROL
			{ 0xB5, SDL_SCANCODE_KP_DIVIDE },     // KC_DIVIDE
			{ 0xB7, SDL_SCANCODE_PRINTSCREEN },   // KC_SYSRQ
			{ 0xB8, SDL_SCANCODE_RALT },          // KC_RMENU
			{ 0xC7, SDL_SCANCODE_HOME },          // KC_HOME
			{ 0xC8, SDL_SCANCODE_UP },            // KC_UP
			{ 0xC9, SDL_SCANCODE_PAGEUP },        // KC_PGUP
			{ 0xCB, SDL_SCANCODE_LEFT },          // KC_LEFT
			{ 0xCD, SDL_SCANCODE_RIGHT },         // KC_RIGHT
			{ 0xCF, SDL_SCANCODE_END },           // KC_END
			{ 0xD0, SDL_SCANCODE_DOWN },          // KC_DOWN
			{ 0xD1, SDL_SCANCODE_PAGEDOWN },      // KC_PGDOWN
			{ 0xD2, SDL_SCANCODE_INSERT },        // KC_INSERT
			{ 0xD3, SDL_SCANCODE_DELETE },        // KC_DELETE
			{ 0xDB, SDL_SCANCODE_LGUI },          // KC_LWIN
			{ 0xDC, SDL_SCANCODE_RGUI }           // KC_RWIN
		};

		auto it = LEGACY_KEYBOARD_TABLE.find(legacyKeyId);
		return (it != LEGACY_KEYBOARD_TABLE.end()) ? it->second : KEY_UNASSIGNED;
	}
}
