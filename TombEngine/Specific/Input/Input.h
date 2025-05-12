#pragma once

#include "Math/Math.h"
#include "Specific/Input/Bindings.h"
#include "Specific/Input/InputAction.h"
#include "Specific/Input/Keys.h"

using namespace TEN::Math;

struct ItemInfo;

namespace TEN::Input
{
	enum class InputAxisID
	{
		Move,
		Camera,
		Mouse,

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
	extern std::unordered_map<InputAxisID, Vector2>		  AxisMap;
	extern std::unordered_map<ActionID, Action>			  ActionMap;
	extern std::unordered_map<ActionID, ActionQueueState> ActionQueueMap;

	void InitializeInput(HWND handle);
	void DeinitializeInput();
	void DefaultConflict();
	void UpdateInputActions(bool allowAsyncUpdate = false, bool applyQueue = false);
	void ApplyActionQueue();
	void ClearAllActions();
	void Rumble(float power, float delaySec = 0.3f, RumbleMode mode = RumbleMode::Both);
	void StopRumble();
    void ApplyDefaultBindings();
    bool ApplyDefaultXInputBindings();

	Vector2 GetMouse2DPosition();

	void		 ClearAction(ActionID actionID);
	bool		 NoAction();
	bool		 IsClicked(ActionID actionID);
	bool		 IsHeld(ActionID actionID, float delaySec = 0.0f);
	bool		 IsPulsed(ActionID actionID, float delaySec, float initialDelaySec = 0.0f);
	bool		 IsReleased(ActionID actionID, float maxDelaySec = INFINITY);
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
