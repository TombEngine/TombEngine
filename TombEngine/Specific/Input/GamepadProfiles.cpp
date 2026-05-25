#include "framework.h"
#include "Specific/Input/GamepadProfiles.h"

#include "Specific/Input/InputAction.h"
#include "Specific/Input/Keys.h"

namespace TEN::Input
{
	static BindingProfile BuildDefaultGamepadBindingProfile()
	{
		return
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
	}

	// All profiles share the same SDL3-normalized logical button layout.
	// GK_SOUTH  = Cross (PS) / A (Xbox) / B (Switch)
	// GK_EAST   = Circle (PS) / B (Xbox) / A (Switch)
	// GK_WEST   = Square (PS) / X (Xbox) / Y (Switch)
	// GK_NORTH  = Triangle (PS) / Y (Xbox) / X (Switch)
	// GK_BACK   = Select/Share (PS) / Back/View (Xbox) / Minus (Switch)
	const BindingProfile DEFAULT_XBOX_GAMEPAD_BINDING_PROFILE       = BuildDefaultGamepadBindingProfile();
	const BindingProfile DEFAULT_PS4_GAMEPAD_BINDING_PROFILE        = BuildDefaultGamepadBindingProfile();
	const BindingProfile DEFAULT_PS5_GAMEPAD_BINDING_PROFILE        = BuildDefaultGamepadBindingProfile();
	const BindingProfile DEFAULT_SWITCH_PRO_GAMEPAD_BINDING_PROFILE = BuildDefaultGamepadBindingProfile();

	const BindingProfile& GetDefaultGamepadBindingProfile(GamepadType gamepadType)
	{
		switch (gamepadType)
		{
		case GamepadType::PlayStation4:
			return DEFAULT_PS4_GAMEPAD_BINDING_PROFILE;

		case GamepadType::PlayStation5:
			return DEFAULT_PS5_GAMEPAD_BINDING_PROFILE;

		case GamepadType::Switch:
			return DEFAULT_SWITCH_PRO_GAMEPAD_BINDING_PROFILE;

		case GamepadType::Xbox:
		default:
			return DEFAULT_XBOX_GAMEPAD_BINDING_PROFILE;
		}
	}
}
