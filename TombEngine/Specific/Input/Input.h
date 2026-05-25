#pragma once

#include "Math/Math.h"
#include "Specific/Input/Bindings.h"
#include "Specific/Input/InputAction.h"
#include "Specific/Input/Keys.h"

using namespace TEN::Math;

struct ItemInfo;

namespace TEN::Input
{
	enum class InputDevice
	{
		Keyboard,
		Mouse,
		Gamepad
	};

	enum class GamepadType
	{
		Xbox          = 0,
		PlayStation4  = 1,
		Switch        = 2, // Value must stay at 2 for existing config compatibility.
		PlayStation5  = 3,
		Count         = 4
	};

	enum class DefaultBindingType
	{
		KeyboardMouse,
		Gamepad
	};

	enum class AxisID
	{
		Move,
		Camera,

		Mouse,
		StickLeft,
		StickRight,

		Count
	};

	enum class ActionQueueState
	{
		None,
		Update,
		Clear
	};

	enum class RumbleMode
	{
		None,
		Left,
		Right,
		Both
	};

	struct RumbleData
	{
		float	   Power	 = 0.0f;
		RumbleMode Mode		 = RumbleMode::None;
		float	   LastPower = 0.0f;
		float	   FadeSpeed = 0.0f;
	};
	extern std::unordered_map<int, float>				  KeyMap;
	extern std::unordered_map<ActionID, Action>			  ActionMap;
	extern std::unordered_map<ActionID, ActionQueueState> ActionQueueMap;
	extern std::unordered_map<AxisID, Vector2>			  AxisMap;

	void InitializeInput();
	void DeinitializeInput();
	void HandleSDLEvent(const SDL_Event& event);
	void SetInputLockState(bool locked);
	void DefaultConflict();
	void UpdateInputActions(bool allowAsyncUpdate = false, bool applyQueue = false);
	void ApplyActionQueue();
	void ClearAllActions();
	void Rumble(float power, float delaySec = 0.3f, RumbleMode mode = RumbleMode::Both);
	void StopRumble();
	void SetGamepadLED(unsigned char r, unsigned char g, unsigned char b);
	void SetGamepadLEDOverride(unsigned char r, unsigned char g, unsigned char b);
	void ClearGamepadLEDOverride();
	bool RefreshGamepadLEDOverride();
	void ApplyDefaultBindings();
	bool RestoreDefaultBindings(DefaultBindingType bindingType);
	GamepadType GetGamepadType();
	InputDevice GetLastInputDevice();

	Vector2 GetMouse2DPosition();

	void		 ClearAction(ActionID actionID);
	bool		 NoAction();
	bool		 IsClicked(ActionID actionID);
	bool		 IsHeld(ActionID actionID, float delaySec = 0.0f);
	bool		 IsPulsed(ActionID actionID, float delaySec, float initialDelaySec = 0.0f);
	bool		 IsReleased(ActionID actionID, float maxDelaySec = FLT_MAX);
	float		 GetActionValue(ActionID actionID);
	unsigned int GetActionTimeActive(ActionID actionID);
	unsigned int GetActionTimeInactive(ActionID actionID);

	bool IsDirectionalActionHeld();
	bool IsWakeActionHeld();
	bool IsOpticActionHeld();

	const Vector2& GetMoveAxis();
	const Vector2& GetCameraAxis();
	const Vector2& GetMouseAxis();
}
