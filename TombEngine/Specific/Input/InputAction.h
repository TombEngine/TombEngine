#pragma once

#include "Game/control/control.h"

namespace TEN::Input
{
	typedef enum class ActionID
	{
		// General

		Forward,
		Back,
		Left,
		Right,
		StepLeft,
		StepRight,
		Walk,
		Sprint,
		Crouch,
		Jump,
		Roll,
		Action,
		Draw,
		Look,

		// Vehicle

		Accelerate,
		Reverse,
		Faster,
		Slower,
		Brake,
		Fire,

		// Quick

		Flare,
		SmallMedipack,
		LargeMedipack,
		PreviousWeapon,
		NextWeapon,
		Weapon1,
		Weapon2,
		Weapon3,
		Weapon4,
		Weapon5,
		Weapon6,
		Weapon7,
		Weapon8,
		Weapon9,
		Weapon10,

		// Menu

		Select,
		Deselect,
		Pause,
		Inventory,
		Save,
		Load,
		
		// Agnostic menu navigation

		MenuUp,
		MenuDown,
		MenuLeft,
		MenuRight,

		// Keyboard

		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,
		Return, Escape, Backspace, Tab, Space, Home, End, Delete,
		Minus, Equals, BracketLeft, BracketRight, Backslash, Semicolon, Apostrophe, Comma, Period, Slash,
		ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
		Ctrl, Shift, Alt,

		// Mouse

		MouseClickLeft,
		MouseClickMiddle,
		MouseClickRight,
		MouseScrollUp,
		MouseScrollDown,
		// TODO: Another time. There's some complexity involved. -- Sezz 2025.03.05
		/*MouseScrollLeft,
		MouseScrollRight,
		MouseUp,
		MouseDown,
		MouseLeft,
		MouseRight,*/

		// Gamepad

		GamepadSouth,
		GamepadEast,
		GamepadWest,
		GamepadNorth,
		GamepadBack,
		GamepadGuide,
		GamepadStart,
		GamepadLeftStick,
		GamepadRightStick,
		GamepadLeftShoulder,
		GamepadRightShoulder,
		GamepadDPadUp,
		GamepadDPadDown,
		GamepadDPadLeft,
		GamepadDPadRight,
		GamepadRightPaddle1,
		GamepadLeftPaddle1,
		GamepadRightPaddle2,
		GamepadLeftPaddle2,
		GamepadTouchpad,
		GamepadMisc1,
		GamepadMisc2,
		GamepadMisc3,
		GamepadMisc4,
		GamepadMisc5,
		GamepadMisc6,
		GamepadLeftTrigger,
		GamepadRightTrigger,

		Count
	} In;

	enum class ActionGroupID
	{
		General,
		Vehicle,
		Quick,
		Menu,

		Keyboard,
		Mouse,
		Gamepad
	};

	extern const std::vector<std::vector<ActionID>> ACTION_ID_GROUPS;
	extern const std::vector<ActionGroupID>			USER_ACTION_GROUP_IDS;
	extern const std::vector<ActionGroupID>			RAW_ACTION_GROUP_IDS;


	class Action
	{
	private:
		// Fields

		ActionID	 _id 			 = In::Forward;
		float		 _value			 = 0.0f;
		float		 _prevValue		 = 0.0f;
		FreezeMode	 _mode			 = FreezeMode::None;
		unsigned int _timeActive	 = 0;			// Time in game frames.
		unsigned int _prevTimeActive = 0;			// Time in game frames.
		unsigned int _timeInactive	 = 0;			// Time in game frames.

		bool IsClickedRaw() const;
		bool IsHeldRaw(float delaySec = 0.0f) const;
		bool IsReleasedRaw(float delaySecMax = FLT_MAX) const;

		FreezeMode GetCurrentMode() const;
		bool IsMatchingMode() const;

	public:
		// Constructors

		Action() = default;
		Action(ActionID actionID);

		// Getters

		ActionID	 GetID() const;
		float		 GetValue() const;
		unsigned int GetTimeActive() const;
		unsigned int GetTimeInactive() const;
		
		// Inquirers

		bool IsClicked() const;
		bool IsHeld(float delaySec = 0.0f) const;
		bool IsPulsed(float delaySec, float initialDelaySec = 0.0f) const;
		bool IsReleased(float delaySecMax = FLT_MAX) const;

		// Utilities

		void Update(bool value);
		void Update(float value);
		void Clear();

		void DrawDebug() const;
	};
}
