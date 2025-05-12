#include "framework.h"
#include "Specific/Input/InputAction.h"

#include "Specific/clock.h"

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
		// Keyboard
		{
			In::A, In::B, In::C, In::D, In::E, In::F, In::G, In::H, In::I, In::J, In::K, In::L, In::M, In::N, In::O, In::P, In::Q, In::R, In::S, In::T, In::U, In::V, In::W, In::X, In::Y, In::Z,
			In::Num1, In::Num2, In::Num3, In::Num4, In::Num5, In::Num6, In::Num7, In::Num8, In::Num9, In::Num0,
			In::Return, In::Escape, In::Backspace, In::Tab, In::Space, In::Home, In::End, In::Delete,
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
			In::MouseScrollDown
		}
	};

	Action::Action(ActionID actionID)
	{
		_id = actionID;
	}

	ActionID Action::GetID() const
	{
		return _id;
	}

	float Action::GetValue() const
	{
		return _value;
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

	bool Action::IsClicked() const
	{
		return (_value != 0.0f && _prevValue == 0.0f);
	}

	bool Action::IsHeld(float delaySec) const
	{
		unsigned int delayGameFrames = (delaySec == 0.0f) ? 0 : SecToGameFrames(delaySec);
		return (_value != 0.0f && _timeActive >= delayGameFrames);
	}

	// NOTE: To avoid stutter on second pulse, ensure `initialDelaySec` is multiple of `delaySec`.
	bool Action::IsPulsed(float delaySec, float initialDelaySec) const
	{
		if (IsClicked())
			return true;

		if (!IsHeld() || _prevTimeActive == 0 || _timeActive == _prevTimeActive)
			return false;

		float activeDelaySec = (_timeActive > SecToGameFrames(initialDelaySec)) ? delaySec : initialDelaySec;
		unsigned int activeDelayGameFrames = SecToGameFrames(activeDelaySec);

		unsigned int delayGameFrames = SecToGameFrames(_timeActive) * activeDelayGameFrames;
		unsigned int prevDelayGameFrames = SecToGameFrames(_prevTimeActive) * activeDelayGameFrames;
		return (delayGameFrames > prevDelayGameFrames);
	}

	bool Action::IsReleased(float delaySecMax) const
	{
		unsigned int delayGameFramesMax = (delaySecMax == INFINITY) ? UINT_MAX : SecToGameFrames(delaySecMax);
		return (_value == 0.0f && _prevValue != 0.0f && _timeActive <= delayGameFramesMax);
	}

	void Action::Update(bool value)
	{
		Update(value ? 1.0f : 0.0f);
	}

	void Action::Update(float value)
	{
		_prevValue = _value;
		_value	   = value;

		if (IsClicked())
		{
			_prevTimeActive = 0;
			_timeActive		= 0;
			_timeInactive++;
		}
		else if (IsReleased())
		{
			_prevTimeActive = _timeActive;
			_timeInactive	= 0;
			_timeActive++;
		}
		else if (IsHeld())
		{
			_prevTimeActive = _timeActive;
			_timeInactive	= 0;
			_timeActive++;
		}
		else
		{
			_prevTimeActive = 0;
			_timeActive		= 0;
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
	}
}
