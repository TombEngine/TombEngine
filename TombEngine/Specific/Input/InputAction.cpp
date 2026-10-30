#include "framework.h"
#include "Specific/Input/InputAction.h"

#include "Game/Gui.h"
#include "Specific/clock.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/trutils.h"

using namespace TEN::Utils;

namespace TEN::Input
{
	const std::vector<std::vector<ActionID>> ACTION_ID_GROUPS =
	{
		// General
		{
			In::Forward,
			In::Back,
			In::Left,
			In::Right,
			In::StepLeft,
			In::StepRight,
			In::Walk,
			In::Sprint,
			In::Crouch,
			In::Jump,
			In::Roll,
			In::Action,
			In::Draw,
			In::Look
		},
		// Vehicle
		{
			In::Accelerate,
			In::Reverse,
			In::Faster,
			In::Slower,
			In::Brake,
			In::Fire
		},
		// Quick
		{
			In::Flare,
			In::SmallMedipack,
			In::LargeMedipack,
			In::PreviousWeapon,
			In::NextWeapon,
			In::Weapon1,
			In::Weapon2,
			In::Weapon3,
			In::Weapon4,
			In::Weapon5,
			In::Weapon6,
			In::Weapon7,
			In::Weapon8,
			In::Weapon9,
			In::Weapon10
		},
		// Menu
		{
			In::Select,
			In::Deselect,
			In::Pause,
			In::Inventory,
			In::Save,
			In::Load
		},
		// Agnostic directional menu navigation
		{
			In::MenuUp,
			In::MenuDown,
			In::MenuLeft,
			In::MenuRight
		},
		// Keyboard
		{
			In::A, In::B, In::C, In::D, In::E, In::F, In::G, In::H, In::I, In::J, In::K, In::L, In::M,
			In::N, In::O, In::P, In::Q, In::R, In::S, In::T, In::U, In::V, In::W, In::X, In::Y, In::Z,
			In::Num1, In::Num2, In::Num3, In::Num4, In::Num5, In::Num6, In::Num7, In::Num8, In::Num9, In::Num0,
			In::F1, In::F2, In::F3, In::F4, In::F5, In::F6, In::F7, In::F8, In::F9, In::F10, In::F11, In::F12,
			In::Return, In::Escape, In::Backspace, In::Tab, In::Space,
			In::PageUp, In::PageDown, In::Insert, In::Home, In::End, In::Delete,
			In::PauseKey, In::PrintScreen, In::ScrollLock, In::CapsLock, In::NumLock,
			In::Minus, In::Equals, In::BracketLeft, In::BracketRight, In::Backslash, In::Semicolon, In::Apostrophe, In::Comma, In::Period, In::Slash,
			In::ArrowUp, In::ArrowDown, In::ArrowLeft, In::ArrowRight,
			In::Ctrl, In::Shift, In::Alt
		},
		// Mouse
		{
			In::MouseClickLeft,
			In::MouseClickMiddle,
			In::MouseClickRight,
			In::MouseScrollUp,
			In::MouseScrollDown,
			In::MouseUp,
			In::MouseDown,
			In::MouseLeft,
			In::MouseRight
		},
		// Gamepad
		{
			In::GamepadSouth,
			In::GamepadEast,
			In::GamepadWest,
			In::GamepadNorth,
			In::GamepadBack,
			In::GamepadGuide,
			In::GamepadStart,
			In::GamepadLeftStick,
			In::GamepadLeftStickUp,
			In::GamepadLeftStickDown,
			In::GamepadLeftStickLeft,
			In::GamepadLeftStickRight,
			In::GamepadRightStick,
			In::GamepadRightStickUp,
			In::GamepadRightStickDown,
			In::GamepadRightStickLeft,
			In::GamepadRightStickRight,
			In::GamepadLeftShoulder,
			In::GamepadRightShoulder,
			In::GamepadDPadUp,
			In::GamepadDPadDown,
			In::GamepadDPadLeft,
			In::GamepadDPadRight,
			In::GamepadMisc1,
			In::GamepadRightPaddle1,
			In::GamepadLeftPaddle1,
			In::GamepadRightPaddle2,
			In::GamepadLeftPaddle2,
			In::GamepadTouchpad,
			In::GamepadMisc2,
			In::GamepadMisc3,
			In::GamepadMisc4,
			In::GamepadMisc5,
			In::GamepadMisc6,
			In::GamepadLeftTrigger,
			In::GamepadRightTrigger
		}
	};

	const std::vector<ActionID> MODE_SWITCH_ACTION_IDS =
	{
		In::Jump,
		In::Roll,
		In::Action,
		In::Draw
	};

	const std::vector<ActionGroupID> USER_ACTION_GROUP_IDS =
	{
		ActionGroupID::General,
		ActionGroupID::Vehicle,
		ActionGroupID::Quick,
		ActionGroupID::Menu
	};

	const std::vector<ActionGroupID> RAW_ACTION_GROUP_IDS =
	{
		ActionGroupID::Keyboard,
		ActionGroupID::Mouse,
		ActionGroupID::Gamepad
	};

	Action::Action(ActionID actionID)
	{
		_id = actionID;
		_mode = FreezeMode::None;
	}

	ActionID Action::GetID() const
	{
		return _id;
	}

	float Action::GetValue() const
	{
		return IsMatchingMode() ? _value : 0.0f;
	}

	// Time in game frames.
	unsigned int Action::GetTimeActive() const
	{
		return _timeActive;
	}

	// Time in game frames.
	unsigned int Action::GetTimeInactive() const
	{
		return _timeInactive;
	}

	FreezeMode Action::GetCurrentMode() const
	{
		return (TEN::Gui::g_Gui.GetInventoryMode() != TEN::Gui::InventoryMode::None) ? FreezeMode::Full : g_GameFlow->CurrentFreezeMode;
	}

	bool Action::IsMatchingMode() const
	{
		// Only do mode comparison for specifically listed controls.
		if (!Contains(MODE_SWITCH_ACTION_IDS, _id))
			return true;

		return (_mode == FreezeMode::None || _mode == GetCurrentMode());
	}

	bool Action::IsClickedRaw() const
	{
		return (_value != 0.0f && _prevValue == 0.0f);
	}

	bool Action::IsHeldRaw(float delaySec) const
	{
		unsigned int delayGameFrames = (delaySec == 0.0f) ? 0 : SecToGameFrames(delaySec);
		return (_value != 0.0f && _timeActive >= delayGameFrames);
	}

	bool Action::IsReleasedRaw(float delaySecMax) const
	{
		unsigned int delayGameFramesMax = (delaySecMax == FLT_MAX) ? UINT_MAX : SecToGameFrames(delaySecMax);
		return (_value == 0.0f && _prevValue != 0.0f && _timeActive <= delayGameFramesMax);
	}

	bool Action::IsClicked() const
	{
		return (IsMatchingMode() && IsClickedRaw());
	}

	bool Action::IsHeld(float delaySec) const
	{
		// HACK: Always report action button as held to prevent ledge or rope drops while getting out of inventory.
		return (IsMatchingMode() || _id == In::Action) && IsHeldRaw(delaySec);
	}

	// NOTE: To avoid stutter on second pulse, ensure `initialDelaySec` is multiple of `delaySec`.
	bool Action::IsPulsed(float delaySec, float initialDelaySec) const
	{
		if (!IsMatchingMode())
			return false;

		if (IsClickedRaw())
			return true;

		if (!IsHeldRaw() || _prevTimeActive == 0 || _timeActive == _prevTimeActive)
			return false;

		float activeDelaySec = (_timeActive > SecToGameFrames(initialDelaySec)) ? delaySec : initialDelaySec;
		unsigned int activeDelayGameFrames = SecToGameFrames(activeDelaySec);

		unsigned int delayGameFrames = (unsigned int)floor(_timeActive / activeDelayGameFrames) * activeDelayGameFrames;
		unsigned int prevDelayGameFrames = (unsigned int)floor(_prevTimeActive / activeDelayGameFrames) * activeDelayGameFrames;
		return (delayGameFrames > prevDelayGameFrames);
	}

	bool Action::IsReleased(float delaySecMax) const
	{
		return (IsMatchingMode() && IsReleasedRaw(delaySecMax));
	}

	void Action::Update(bool value)
	{
		Update(value ? 1.0f : 0.0f);
	}

	void Action::Update(float value)
	{
		_prevValue = _value;
		_value	   = value;

		if (IsClickedRaw())
		{
			_prevTimeActive = 0;
			_timeActive		= 0;
			_mode = GetCurrentMode();
			_timeInactive++;
		}
		else if (IsReleasedRaw())
		{
			_prevTimeActive = _timeActive;
			_timeInactive	= 0;
			_timeActive++;
		}
		else if (IsHeldRaw())
		{
			_prevTimeActive = _timeActive;
			_timeInactive	= 0;
			_timeActive++;
		}
		else
		{
			_prevTimeActive = 0;
			_timeActive		= 0;
			_mode = FreezeMode::None;
			_timeInactive++;
		}
	}

	void Action::Clear()
	{
		_value			= 0.0f;
		_prevValue		= 0.0f;
		_timeActive		= 0;
		_prevTimeActive = 0;
		_timeInactive	= 0;
		_mode			= FreezeMode::None;
	}

	void Action::DrawDebug() const
	{
		PrintDebugMessage("INPUT ACTION DEBUG");
		PrintDebugMessage("ID: %d", (int)_id);
		PrintDebugMessage("IsClicked: %d", IsClicked());
		PrintDebugMessage("IsHeld: %d", IsHeld());
		PrintDebugMessage("IsPulsed (.2s, .6s): %d", IsPulsed(0.2f, 0.6f));
		PrintDebugMessage("IsReleased: %d", IsReleased());
		PrintDebugMessage("");
		PrintDebugMessage("Value: %.3f", _value);
		PrintDebugMessage("PrevValue: %.3f", _prevValue);
		PrintDebugMessage("TimeActive: %d", _timeActive);
		PrintDebugMessage("PrevTimeActive: %d", _prevTimeActive);
		PrintDebugMessage("TimeInactive: %d", _timeInactive);
		PrintDebugMessage("Mode: %d", (int)_mode);
	}
}
