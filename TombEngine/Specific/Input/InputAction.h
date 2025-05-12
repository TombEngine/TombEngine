#pragma once

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

		// TODO: Can add raw gamepad actions too, however, we MUST ditch OIS in favour of SDL for it.
		// It's a FAR better library which can also be used for window management. -- Sezz 2025.05.03

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
		//Gamepad
	};

	extern const std::vector<std::vector<ActionID>> ACTION_ID_GROUPS;

	class Action
	{
	private:
		// Fields

		ActionID	 _id 			 = In::Forward;
		float		 _value			 = 0.0f;
		float		 _prevValue		 = 0.0f;
		unsigned int _timeActive	 = 0;			// Time in game frames.
		unsigned int _prevTimeActive = 0;			// Time in game frames.
		unsigned int _timeInactive	 = 0;			// Time in game frames.

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
		bool IsReleased(float delaySecMax = INFINITY) const;

		// Utilities

		void Update(bool value);
		void Update(float value);
		void Clear();

		void DrawDebug() const;
	};
}
