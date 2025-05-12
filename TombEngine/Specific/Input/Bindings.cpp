#include "framework.h"
#include "Specific/Input/Bindings.h"

#include "Specific/Input/InputAction.h"
#include "Specific/Input/Keys.h"

namespace TEN::Input
{
	BindingManager g_Bindings;

	const BindingProfile DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE =
	{
		{ In::Forward,		  OIS::KC_UP },
		{ In::Back,			  OIS::KC_DOWN },
		{ In::Left,			  OIS::KC_LEFT },
		{ In::Right,		  OIS::KC_RIGHT },
		{ In::StepLeft,		  OIS::KC_DELETE },
		{ In::StepRight,	  OIS::KC_PGDOWN },
		{ In::Action,		  OIS::KC_RCONTROL },
		{ In::Walk,			  OIS::KC_RSHIFT },
		{ In::Sprint,		  OIS::KC_SLASH },
		{ In::Crouch,		  OIS::KC_PERIOD },
		{ In::Jump,			  OIS::KC_RMENU },
		{ In::Roll,			  OIS::KC_END },
		{ In::Draw,			  OIS::KC_SPACE },
		{ In::Look,			  OIS::KC_NUMPAD0 },

		{ In::Accelerate,	  OIS::KC_RCONTROL },
		{ In::Reverse,		  OIS::KC_DOWN },
		{ In::Faster,		  OIS::KC_SLASH },
		{ In::Slower,		  OIS::KC_RSHIFT },
		{ In::Brake,		  OIS::KC_RMENU },
		{ In::Fire,			  OIS::KC_SPACE },

		{ In::Flare,		  OIS::KC_COMMA },
		{ In::SmallMedipack,  OIS::KC_MINUS },
		{ In::LargeMedipack,  OIS::KC_EQUALS },
		{ In::PreviousWeapon, OIS::KC_LBRACKET },
		{ In::NextWeapon,	  OIS::KC_RBRACKET },
		{ In::Weapon1,		  OIS::KC_1 },
		{ In::Weapon2,		  OIS::KC_2 },
		{ In::Weapon3,		  OIS::KC_3 },
		{ In::Weapon4,		  OIS::KC_4 },
		{ In::Weapon5,		  OIS::KC_5 },
		{ In::Weapon6,		  OIS::KC_6 },
		{ In::Weapon7,		  OIS::KC_7 },
		{ In::Weapon8,		  OIS::KC_8 },
		{ In::Weapon9,		  OIS::KC_9 },
		{ In::Weapon10,		  OIS::KC_0 },

		{ In::Select,		  OIS::KC_RETURN },
		{ In::Deselect,		  OIS::KC_ESCAPE },
		{ In::Pause,		  OIS::KC_P },
		{ In::Inventory,	  OIS::KC_ESCAPE },
		{ In::Save,			  OIS::KC_F5 },
		{ In::Load,			  OIS::KC_F6 }
	};

	const BindingProfile DEFAULT_GAMEPAD_BINDING_PROFILE =
	{
		{ In::Forward,		  XK_AXIS_X_NEG },
		{ In::Back,			  XK_AXIS_X_POS },
		{ In::Left,			  XK_AXIS_Y_NEG },
		{ In::Right,		  XK_AXIS_Y_POS },
		{ In::StepLeft,		  XK_L_STICK },
		{ In::StepRight,	  XK_R_STICK },
		{ In::Action,		  XK_A },
		{ In::Walk,			  XK_R_SHIFT },
		{ In::Sprint,		  XK_AXIS_R_TRIGGER_NEG },
		{ In::Crouch,		  XK_AXIS_L_TRIGGER_NEG },
		{ In::Jump,			  XK_X },
		{ In::Roll,			  XK_B },
		{ In::Draw,			  XK_Y },
		{ In::Look,			  XK_L_SHIFT },

		{ In::Accelerate,	  XK_A },
		{ In::Reverse,		  XK_AXIS_X_POS },
		{ In::Faster,		  XK_AXIS_R_TRIGGER_NEG },
		{ In::Slower,		  XK_R_SHIFT },
		{ In::Brake,		  XK_X },
		{ In::Fire,			  XK_AXIS_L_TRIGGER_NEG },

		{ In::Flare,		  XK_DPAD_DOWN },
		{ In::SmallMedipack,  OIS::KC_MINUS },
		{ In::LargeMedipack,  OIS::KC_EQUALS },
		{ In::PreviousWeapon, OIS::KC_LBRACKET },
		{ In::NextWeapon,	  OIS::KC_RBRACKET },
		{ In::Weapon1,		  OIS::KC_1 },
		{ In::Weapon2,		  OIS::KC_2 },
		{ In::Weapon3,		  OIS::KC_3 },
		{ In::Weapon4,		  OIS::KC_4 },
		{ In::Weapon5,		  OIS::KC_5 },
		{ In::Weapon6,		  OIS::KC_6 },
		{ In::Weapon7,		  OIS::KC_7 },
		{ In::Weapon8,		  OIS::KC_8 },
		{ In::Weapon9,		  OIS::KC_9 },
		{ In::Weapon10,		  OIS::KC_0 },

		{ In::Select,		  OIS::KC_RETURN },
		{ In::Deselect,		  XK_SELECT },
		{ In::Pause,		  XK_START },
		{ In::Inventory,	  XK_SELECT },
		{ In::Save,			  OIS::KC_F5 },
		{ In::Load,			  OIS::KC_F6 }
	};

	const BindingProfile RAW_EVENT_BINDING_PROFILE
	{
		{ In::A,				OIS::KC_A },
		{ In::B,				OIS::KC_B },
		{ In::C,				OIS::KC_C },
		{ In::D,				OIS::KC_D },
		{ In::E,				OIS::KC_E },
		{ In::F,				OIS::KC_F },
		{ In::G,				OIS::KC_G },
		{ In::H,				OIS::KC_H },
		{ In::I,				OIS::KC_I },
		{ In::J,				OIS::KC_J },
		{ In::K,				OIS::KC_K },
		{ In::L,				OIS::KC_L },
		{ In::M,				OIS::KC_M },
		{ In::N,				OIS::KC_N },
		{ In::O,				OIS::KC_O },
		{ In::P,				OIS::KC_P },
		{ In::Q,				OIS::KC_Q },
		{ In::R,				OIS::KC_R },
		{ In::S,				OIS::KC_S },
		{ In::T,				OIS::KC_T },
		{ In::U,				OIS::KC_U },
		{ In::V,				OIS::KC_V },
		{ In::W,				OIS::KC_W },
		{ In::X,				OIS::KC_X },
		{ In::Y,				OIS::KC_Y },
		{ In::Z,				OIS::KC_Z },
		{ In::Num0,				OIS::KC_0 },
		{ In::Num1,				OIS::KC_1 },
		{ In::Num2,				OIS::KC_2 },
		{ In::Num3,				OIS::KC_3 },
		{ In::Num4,				OIS::KC_4 },
		{ In::Num5,				OIS::KC_5 },
		{ In::Num6,				OIS::KC_6 },
		{ In::Num7,				OIS::KC_7 },
		{ In::Num8,				OIS::KC_8 },
		{ In::Num9,				OIS::KC_9 },
		{ In::Minus,			OIS::KC_MINUS },
		{ In::Equals,			OIS::KC_EQUALS },
		{ In::Escape,			OIS::KC_ESCAPE },
		{ In::Tab, 				OIS::KC_TAB },
		{ In::Shift, 			OIS::KC_RSHIFT },
		{ In::Ctrl, 			OIS::KC_RCONTROL },
		{ In::Alt, 				OIS::KC_RMENU },
		{ In::Space, 			OIS::KC_SPACE },
		{ In::Home,				OIS::KC_HOME },
		{ In::End,				OIS::KC_END },
		{ In::Delete,			OIS::KC_DELETE },
		{ In::Return,			OIS::KC_RETURN },
		{ In::Backspace, 		OIS::KC_BACK },
		{ In::BracketLeft, 		OIS::KC_LBRACKET },
		{ In::BracketRight,		OIS::KC_RBRACKET },
		{ In::Backslash, 		OIS::KC_BACKSLASH },
		{ In::Semicolon, 		OIS::KC_SEMICOLON },
		{ In::Apostrophe, 		OIS::KC_APOSTROPHE },
		{ In::Comma, 			OIS::KC_COMMA },
		{ In::Period, 			OIS::KC_PERIOD },
		{ In::Slash, 			OIS::KC_SLASH },
		{ In::ArrowUp, 			OIS::KC_UP },
		{ In::ArrowDown, 		OIS::KC_DOWN },
		{ In::ArrowLeft, 		OIS::KC_LEFT },
		{ In::ArrowRight, 		OIS::KC_RIGHT },

		{ In::MouseClickLeft,	MK_LCLICK },
		{ In::MouseClickMiddle, MK_MCLICK },
		{ In::MouseClickRight,	MK_RCLICK },
		{ In::MouseScrollUp,	MK_AXIS_X_NEG },
		{ In::MouseScrollDown,	MK_AXIS_X_POS }
	};

	int BindingManager::GetBoundKeyID(BindingProfileID profileID, ActionID actionID) const
	{
		// Find binding profile.
		auto profileIt = _bindings.find(profileID);
		if (profileIt == _bindings.end())
			return OIS::KC_UNASSIGNED;

		// Get binding profile.
		const auto& [inputDeviceID, profile] = *profileIt;

		// Find key-action binding.
		auto keyIt = profile.find(actionID);
		if (keyIt == profile.end())
			return OIS::KC_UNASSIGNED;

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
		// Reset binding profile defaults.
		switch (profileID)
		{
			case BindingProfileID::Default:
				_bindings[profileID] = DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;
				break;

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
			{ BindingProfileID::Custom,	 DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE },
			{ BindingProfileID::Raw,	 RAW_EVENT_BINDING_PROFILE }
		};

		// Initialize conflicts.
		_conflicts.reserve((int)ActionID::Count);
		for (int i = 0; i < (int)ActionID::Count; i++)
		{
			auto actionID = (ActionID)i;
			_conflicts.insert({ actionID, false });
		}
	}
}
