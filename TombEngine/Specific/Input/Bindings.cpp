#include "framework.h"
#include "Specific/Input/Bindings.h"

#include "Specific/Input/InputAction.h"
#include "Specific/Input/Keys.h"

namespace TEN::Input
{
	BindingManager g_Bindings;

	const BindingProfile DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE =
	{
		{ In::Forward,        SDL_SCANCODE_UP },
		{ In::Back,           SDL_SCANCODE_DOWN },
		{ In::Left,           SDL_SCANCODE_LEFT },
		{ In::Right,          SDL_SCANCODE_RIGHT },
		{ In::StepLeft,       SDL_SCANCODE_DELETE },
		{ In::StepRight,      SDL_SCANCODE_PAGEDOWN },
		{ In::Action,         SDL_SCANCODE_RCTRL },
		{ In::Walk,           SDL_SCANCODE_RSHIFT },
		{ In::Sprint,         SDL_SCANCODE_SLASH },
		{ In::Crouch,         SDL_SCANCODE_PERIOD },
		{ In::Jump,           SDL_SCANCODE_RALT },
		{ In::Roll,           SDL_SCANCODE_END },
		{ In::Draw,           SDL_SCANCODE_SPACE },
		{ In::Look,           SDL_SCANCODE_KP_0 },

		{ In::Accelerate,     SDL_SCANCODE_RCTRL },
		{ In::Reverse,        SDL_SCANCODE_DOWN },
		{ In::Faster,         SDL_SCANCODE_SLASH },
		{ In::Slower,         SDL_SCANCODE_RSHIFT },
		{ In::Brake,          SDL_SCANCODE_RALT },
		{ In::Fire,           SDL_SCANCODE_SPACE },

		{ In::Flare,          SDL_SCANCODE_COMMA },
		{ In::SmallMedipack,  SDL_SCANCODE_MINUS },
		{ In::LargeMedipack,  SDL_SCANCODE_EQUALS },
		{ In::PreviousWeapon, SDL_SCANCODE_LEFTBRACKET },
		{ In::NextWeapon,     SDL_SCANCODE_RIGHTBRACKET },
		{ In::Weapon1,        SDL_SCANCODE_1 },
		{ In::Weapon2,        SDL_SCANCODE_2 },
		{ In::Weapon3,        SDL_SCANCODE_3 },
		{ In::Weapon4,        SDL_SCANCODE_4 },
		{ In::Weapon5,        SDL_SCANCODE_5 },
		{ In::Weapon6,        SDL_SCANCODE_6 },
		{ In::Weapon7,        SDL_SCANCODE_7 },
		{ In::Weapon8,        SDL_SCANCODE_8 },
		{ In::Weapon9,        SDL_SCANCODE_9 },
		{ In::Weapon10,       SDL_SCANCODE_0 },

		{ In::Select,         SDL_SCANCODE_RETURN },
		{ In::Deselect,       SDL_SCANCODE_ESCAPE },
		{ In::Pause,          SDL_SCANCODE_P },
		{ In::Inventory,      SDL_SCANCODE_ESCAPE },
		{ In::Save,           SDL_SCANCODE_F5 },
		{ In::Load,           SDL_SCANCODE_F6 }
	};

	const BindingProfile DEFAULT_GAMEPAD_BINDING_PROFILE =
	{
		{ In::Forward,        GK_LSTICK_Y_NEG },
		{ In::Back,           GK_LSTICK_Y_POS },
		{ In::Left,           GK_LSTICK_X_NEG },
		{ In::Right,          GK_LSTICK_X_POS },
		{ In::StepLeft,       GK_LSTICK },
		{ In::StepRight,      GK_RSTICK },
		{ In::Action,         GK_SOUTH },
		{ In::Walk,           GK_RSHOULDER },
		{ In::Sprint,         GK_RTRIGGER_POS },
		{ In::Crouch,         GK_LTRIGGER_POS },
		{ In::Jump,           GK_WEST },
		{ In::Roll,           GK_EAST },
		{ In::Draw,           GK_NORTH },
		{ In::Look,           GK_LSHOULDER },

		{ In::Accelerate,     GK_SOUTH },
		{ In::Reverse,        GK_LSTICK_Y_POS },
		{ In::Faster,         GK_RTRIGGER_POS },
		{ In::Slower,         GK_RSHOULDER },
		{ In::Brake,          GK_WEST },
		{ In::Fire,           GK_LTRIGGER_POS },

		{ In::Flare,          GK_DPAD_DOWN },
		{ In::SmallMedipack,  SDL_SCANCODE_MINUS },
		{ In::LargeMedipack,  SDL_SCANCODE_EQUALS },
		{ In::PreviousWeapon, SDL_SCANCODE_LEFTBRACKET },
		{ In::NextWeapon,     SDL_SCANCODE_RIGHTBRACKET },
		{ In::Weapon1,        SDL_SCANCODE_1 },
		{ In::Weapon2,        SDL_SCANCODE_2 },
		{ In::Weapon3,        SDL_SCANCODE_3 },
		{ In::Weapon4,        SDL_SCANCODE_4 },
		{ In::Weapon5,        SDL_SCANCODE_5 },
		{ In::Weapon6,        SDL_SCANCODE_6 },
		{ In::Weapon7,        SDL_SCANCODE_7 },
		{ In::Weapon8,        SDL_SCANCODE_8 },
		{ In::Weapon9,        SDL_SCANCODE_9 },
		{ In::Weapon10,       SDL_SCANCODE_0 },

		{ In::Select,         GK_SOUTH },
		{ In::Deselect,       GK_EAST },
		{ In::Pause,          GK_START },
		{ In::Inventory,      GK_BACK },
		{ In::Save,           SDL_SCANCODE_F5 },
		{ In::Load,           SDL_SCANCODE_F6 }
	};

	const BindingProfile RAW_EVENT_BINDING_PROFILE =
	{
		{ In::A,            SDL_SCANCODE_A },
		{ In::B,            SDL_SCANCODE_B },
		{ In::C,            SDL_SCANCODE_C },
		{ In::D,            SDL_SCANCODE_D },
		{ In::E,            SDL_SCANCODE_E },
		{ In::F,            SDL_SCANCODE_F },
		{ In::G,            SDL_SCANCODE_G },
		{ In::H,            SDL_SCANCODE_H },
		{ In::I,            SDL_SCANCODE_I },
		{ In::J,            SDL_SCANCODE_J },
		{ In::K,            SDL_SCANCODE_K },
		{ In::L,            SDL_SCANCODE_L },
		{ In::M,            SDL_SCANCODE_M },
		{ In::N,            SDL_SCANCODE_N },
		{ In::O,            SDL_SCANCODE_O },
		{ In::P,            SDL_SCANCODE_P },
		{ In::Q,            SDL_SCANCODE_Q },
		{ In::R,            SDL_SCANCODE_R },
		{ In::S,            SDL_SCANCODE_S },
		{ In::T,            SDL_SCANCODE_T },
		{ In::U,            SDL_SCANCODE_U },
		{ In::V,            SDL_SCANCODE_V },
		{ In::W,            SDL_SCANCODE_W },
		{ In::X,            SDL_SCANCODE_X },
		{ In::Y,            SDL_SCANCODE_Y },
		{ In::Z,            SDL_SCANCODE_Z },
		{ In::Num0,         SDL_SCANCODE_0 },
		{ In::Num1,         SDL_SCANCODE_1 },
		{ In::Num2,         SDL_SCANCODE_2 },
		{ In::Num3,         SDL_SCANCODE_3 },
		{ In::Num4,         SDL_SCANCODE_4 },
		{ In::Num5,         SDL_SCANCODE_5 },
		{ In::Num6,         SDL_SCANCODE_6 },
		{ In::Num7,         SDL_SCANCODE_7 },
		{ In::Num8,         SDL_SCANCODE_8 },
		{ In::Num9,         SDL_SCANCODE_9 },
		{ In::F1,           SDL_SCANCODE_F1 },
		{ In::F2,           SDL_SCANCODE_F2 },
		{ In::F3,           SDL_SCANCODE_F3 },
		{ In::F4,           SDL_SCANCODE_F4 },
		{ In::F5,           SDL_SCANCODE_F5 },
		{ In::F6,           SDL_SCANCODE_F6 },
		{ In::F7,           SDL_SCANCODE_F7 },
		{ In::F8,           SDL_SCANCODE_F8 },
		{ In::F9,           SDL_SCANCODE_F9 },
		{ In::F10,          SDL_SCANCODE_F10 },
		{ In::F11,          SDL_SCANCODE_F11 },
		{ In::F12,          SDL_SCANCODE_F12 },
		{ In::Minus,        SDL_SCANCODE_MINUS },
		{ In::Equals,       SDL_SCANCODE_EQUALS },
		{ In::Escape,       SDL_SCANCODE_ESCAPE },
		{ In::Tab,          SDL_SCANCODE_TAB },
		{ In::Shift,        SDL_SCANCODE_RSHIFT },
		{ In::Ctrl,         SDL_SCANCODE_RCTRL },
		{ In::Alt,          SDL_SCANCODE_RALT },
		{ In::Space,        SDL_SCANCODE_SPACE },
		{ In::PageUp,       SDL_SCANCODE_PAGEUP },
		{ In::PageDown,     SDL_SCANCODE_PAGEDOWN },
		{ In::Insert,       SDL_SCANCODE_INSERT },
		{ In::Home,         SDL_SCANCODE_HOME },
		{ In::End,          SDL_SCANCODE_END },
		{ In::Delete,       SDL_SCANCODE_DELETE },
		{ In::PauseKey,     SDL_SCANCODE_PAUSE },
		{ In::PrintScreen,  SDL_SCANCODE_PRINTSCREEN },
		{ In::ScrollLock,   SDL_SCANCODE_SCROLLLOCK },
		{ In::CapsLock,     SDL_SCANCODE_CAPSLOCK },
		{ In::NumLock,      SDL_SCANCODE_NUMLOCKCLEAR },
		{ In::Return,       SDL_SCANCODE_RETURN },
		{ In::Backspace,    SDL_SCANCODE_BACKSPACE },
		{ In::BracketLeft,  SDL_SCANCODE_LEFTBRACKET },
		{ In::BracketRight, SDL_SCANCODE_RIGHTBRACKET },
		{ In::Backslash,    SDL_SCANCODE_BACKSLASH },
		{ In::Semicolon,    SDL_SCANCODE_SEMICOLON },
		{ In::Apostrophe,   SDL_SCANCODE_APOSTROPHE },
		{ In::Comma,        SDL_SCANCODE_COMMA },
		{ In::Period,       SDL_SCANCODE_PERIOD },
		{ In::Slash,        SDL_SCANCODE_SLASH },
		{ In::ArrowUp,      SDL_SCANCODE_UP },
		{ In::ArrowDown,    SDL_SCANCODE_DOWN },
		{ In::ArrowLeft,    SDL_SCANCODE_LEFT },
		{ In::ArrowRight,   SDL_SCANCODE_RIGHT },

		{ In::MouseClickLeft,   MK_LCLICK },
		{ In::MouseClickMiddle, MK_MCLICK },
		{ In::MouseClickRight,  MK_RCLICK },
		{ In::MouseLeft,        MK_AXIS_X_NEG },
		{ In::MouseRight,       MK_AXIS_X_POS },
		{ In::MouseUp,          MK_AXIS_Y_NEG },
		{ In::MouseDown,        MK_AXIS_Y_POS },
		{ In::MouseScrollUp,    MK_AXIS_Z_NEG },
		{ In::MouseScrollDown,  MK_AXIS_Z_POS },

		{ In::GamepadSouth,           GK_SOUTH },
		{ In::GamepadEast,            GK_EAST },
		{ In::GamepadWest,            GK_WEST },
		{ In::GamepadNorth,           GK_NORTH },
		{ In::GamepadBack,            GK_BACK },
		{ In::GamepadGuide,           GK_GUIDE },
		{ In::GamepadStart,           GK_START },
		{ In::GamepadLeftStick,       GK_LSTICK },
		{ In::GamepadLeftStickLeft,   GK_LSTICK_X_NEG },
		{ In::GamepadLeftStickRight,  GK_LSTICK_X_POS },
		{ In::GamepadLeftStickUp,     GK_LSTICK_Y_NEG },
		{ In::GamepadLeftStickDown,   GK_LSTICK_Y_POS },
		{ In::GamepadRightStick,      GK_RSTICK },
		{ In::GamepadRightStickLeft,  GK_RSTICK_X_NEG },
		{ In::GamepadRightStickRight, GK_RSTICK_X_POS },
		{ In::GamepadRightStickUp,    GK_RSTICK_Y_NEG },
		{ In::GamepadRightStickDown,  GK_RSTICK_Y_POS },
		{ In::GamepadLeftShoulder,    GK_LSHOULDER },
		{ In::GamepadRightShoulder,   GK_RSHOULDER },
		{ In::GamepadDPadUp,          GK_DPAD_UP },
		{ In::GamepadDPadDown,        GK_DPAD_DOWN },
		{ In::GamepadDPadLeft,        GK_DPAD_LEFT },
		{ In::GamepadDPadRight,       GK_DPAD_RIGHT },
		{ In::GamepadMisc1,           GK_MISC1 },
		{ In::GamepadRightPaddle1,    GK_RPADDLE1 },
		{ In::GamepadLeftPaddle1,     GK_LPADDLE1 },
		{ In::GamepadRightPaddle2,    GK_RPADDLE2 },
		{ In::GamepadLeftPaddle2,     GK_LPADDLE2 },
		{ In::GamepadTouchpad,        GK_TOUCHPAD },
		{ In::GamepadMisc2,           GK_MISC2 },
		{ In::GamepadMisc3,           GK_MISC3 },
		{ In::GamepadMisc4,           GK_MISC4 },
		{ In::GamepadMisc5,           GK_MISC5 },
		{ In::GamepadMisc6,           GK_MISC6 },
		{ In::GamepadLeftTrigger,     GK_LTRIGGER_POS },
		{ In::GamepadRightTrigger,    GK_RTRIGGER_POS }
	};

	int BindingManager::GetBoundKeyID(BindingProfileID profileID, ActionID actionID) const
	{
		// Find binding profile.
		auto profileIt = _bindings.find(profileID);
		if (profileIt == _bindings.end())
			return KEY_UNASSIGNED;

		// Get binding profile.
		const auto& [inputDeviceID, profile] = *profileIt;

		// Find key-action binding.
		auto keyIt = profile.find(actionID);
		if (keyIt == profile.end())
			return KEY_UNASSIGNED;

		// Return key binding.
		const auto& [keyActionID, keyID] = *keyIt;
		return keyID;
	}

	const BindingProfile& BindingManager::GetBindingProfile(BindingProfileID profileID) const
	{
		// Find binding profile.
		auto profileIt = _bindings.find(profileID);
		TENAssert(profileIt != _bindings.end(), "Attempted to get missing binding profile " + std::to_string((int)profileID) + ".");

		// Return binding profile.
		const auto& [keyProfileID, profile] = *profileIt;
		return profile;
	}

	const std::string& BindingManager::GetBoundKeyName(ActionID actionID)
	{
		auto device = GetLastInputDevice();
		const BindingProfile& profile = (device == InputDevice::Gamepad) ?
			DEFAULT_GAMEPAD_BINDING_PROFILE : DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;
		
		auto it = profile.find(actionID);

		if (it != profile.end())
			return GetKeyName(it->second);

		return GetKeyName(KEY_UNASSIGNED);
	}

	void BindingManager::SetKeyBinding(BindingProfileID profileID, ActionID actionID, int keyID)
	{
		// Overwrite or add key-action binding.
		_bindings[profileID][actionID] = keyID;
	}

	void BindingManager::SetBindingProfile(BindingProfileID profileID, const BindingProfile& bindingProfile)
	{
		// Overwrite or create binding profile.
		_bindings[profileID] = bindingProfile;
	}

	void BindingManager::SetDefaultBindingProfile(BindingProfileID profileID)
	{
		// Set binding profile defaults.
		switch (profileID)
		{
			case BindingProfileID::Default:
			case BindingProfileID::Custom:
				_bindings[profileID] = DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;
				break;

			default:
				TENLog("Failed to reset defaults for binding profile " + std::to_string((int)profileID) + ".", LogLevel::Warning);
				return;
		}
	}

	void BindingManager::SetConflict(ActionID actionID, bool value)
	{
		_conflicts[actionID] = value;
	}

	bool BindingManager::TestConflict(ActionID actionID)
	{
		return _conflicts.at(actionID);
	}

	void BindingManager::Initialize()
	{
		// Initialize bindings.
		_bindings =
		{
			{ BindingProfileID::Default, DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE },
			{ BindingProfileID::Custom,  DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE },
			{ BindingProfileID::Raw,     RAW_EVENT_BINDING_PROFILE }
		};

		_conflicts.reserve((int)ActionID::Count);
		for (int i = 0; i < (int)ActionID::Count; i++)
		{
			auto actionID = (ActionID)i;
			_conflicts.insert({ actionID, false });
		}
	}
}
