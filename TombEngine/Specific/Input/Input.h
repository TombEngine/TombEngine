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

	extern std::unordered_map<int, float>					   KeyMap;
	extern std::unordered_map<InputAxisID, Vector2>			   AxisMap;
	extern std::unordered_map<InputActionID, InputAction>	   ActionMap;
	extern std::unordered_map<InputActionID, ActionQueueState> ActionQueueMap;

	void InitializeInput(HWND handle);
	void DeinitializeInput();
	void DefaultConflict();
	void UpdateInputActions(bool allowAsyncUpdate = false, bool applyQueue = false);
	void ApplyActionQueue();
	void ClearAllActions();
	void Rumble(float power, float delayInSec = 0.3f, RumbleMode mode = RumbleMode::Both);
	void StopRumble();
    void ApplyDefaultBindings();
    bool ApplyDefaultXInputBindings();

	Vector2 GetMouse2DPosition();

	void		 ClearAction(InputActionID actionID);
	bool		 NoAction();
	bool		 IsClicked(InputActionID actionID);
	bool		 IsHeld(InputActionID actionID, float delayInSec = 0.0f);
	bool		 IsPulsed(InputActionID actionID, float delayInSec, float initialDelayInSec = 0.0f);
	bool		 IsReleased(InputActionID actionID, float maxDelayInSec = INFINITY);
	float		 GetActionValue(InputActionID actionID);
	unsigned int GetActionTimeActive(InputActionID actionID);
	unsigned int GetActionTimeInactive(InputActionID actionID);

	bool IsDirectionalActionHeld();
	bool IsWakeActionHeld();
	bool IsOpticActionHeld();
}
