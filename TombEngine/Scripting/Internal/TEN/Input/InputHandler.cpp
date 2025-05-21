#include "framework.h"
#include "Scripting/Internal/TEN/Input/InputHandler.h"

#include "Renderer/RendererEnums.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Input/ActionIDs.h"
#include "Scripting/Internal/TEN/Input/AxisIDs.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Specific/Input/Input.h"

using namespace TEN::Input;

namespace TEN::Scripting::Input
{
	/// Functions for input management.
	// @tentable Input
	// @pragma nostrip

	static bool IsValidAction(int actionID)
	{
		if (actionID > (int)ActionID::Count)
		{
			ScriptAssertF(false, "Input action {} does not exist.", actionID);
			return false;
		}

		return true;
	}

	/// Get the analog value of an action key.
	// Returns either 0 or 1 for digital key mappings (e.g. keyboard or gamepad buttons),
	// but may return arbitrary values in the range 0 to 1 for analog key mappings
	// (e.g. gamepad sticks, gamepad triggers, or mouse axes).
	// @function GetAnalogKeyValue
	// @tparam Input.ActionID actionID Action ID to query.
	// @treturn float Analog value in the range [0, 1].
	static float GetAnalogKeyValue(int actionID)
	{
		if (!IsValidAction(actionID))
			return 0.0f;

		return GetActionValue((ActionID)actionID);
	}

	/// Get the analog value of an axis.
	// @function GetAnalogAxis
	// @tparam Input.AxisID axis Axis ID to fetch.
	// @treturn Vec2 Relative analog axis value with components in the range [-1, 1].
	static Vec2 GetAnalogAxisValue(AxisID axisID)
	{
		return Vec2(AxisMap[axisID]);
	}

	/// Get the display position of the cursor in percent.
	// @function GetMouseDisplayPosition
	// @treturn Vec2 Cursor display position in percent.
	static Vec2 GetMouseDisplayPosition()
	{
		// NOTE: Conversion from internal 800x600 to more intuitive 100x100 display space resolution is required.
		// In a future refactor, everything will use 100x100 natively. -- Sezz 2023.10.20

		auto cursorPos = GetMouse2DPosition();
		cursorPos = Vector2(cursorPos.x / DISPLAY_SPACE_RES.x, cursorPos.y / DISPLAY_SPACE_RES.y) * 100;
		return Vec2(cursorPos);
	}

	/// Check if an action key is being hit.
	// @function IsKeyHit
	// @tparam Input.ActionID actionID Action ID to check.
	static bool IsKeyHit(int actionID)
	{
		if (!IsValidAction(actionID))
			return false;

		return IsClicked((ActionID)actionID);
	}

	/// Check if an action key is being held.
	// @function IsKeyHeld
	// @tparam Input.ActionID actionID Action ID to check.
	// @tparam[opt=0] float delaySec Delay time in seconds before a hold can be registered.
	static bool IsKeyHeld(int actionID, TypeOrNil<float> delaySec)
	{
		if (!IsValidAction(actionID))
			return false;

		return IsHeld((ActionID)actionID, ValueOr<float>(delaySec, 0.0f));
	}

	/// Check if an action key is being pulsed.
	// Note that to avoid a stutter on the second pulse, `initialDelaySec` must be a multiple of `delaySec`.
	// @function IsKeyPulsed
	// @tparam Input.ActionID actionID Action ID to check.
	// @tparam float delaySec Delay time in seconds between pulses.
	// @tparam[opt=0] float initialDelaySec Initial delay time in seconds on the first pulse.
	static bool IsKeyPulsed(int actionID, float delaySec, TypeOrNil<float> initialDelaySec)
	{
		if (!IsValidAction(actionID))
			return false;

		return IsPulsed((ActionID)actionID, delaySec, ValueOr<float>(initialDelaySec, 0.0f));
	}

	/// Check if an action key is being released.
	// @function IsKeyReleased
	// @tparam Input.ActionID actionID Action ID to check.
	// @tparam[opt=infinity] float maxDelaySec Max delay time in seconds between hit and release within which a release can be registered.
	static bool IsKeyReleased(int actionID, TypeOrNil<float> maxDelaySec)
	{
		if (!IsValidAction(actionID))
			return false;

		return IsReleased((ActionID)actionID, ValueOr<float>(maxDelaySec, INFINITY));
	}

	/// Simulate an action key push.
	// @function PushKey
	// @tparam Input.ActionID actionID Action ID to push.
	static void PushKey(int actionID)
	{
		if (!IsValidAction(actionID))
			return;

		ActionQueueMap[(ActionID)actionID] = ActionQueueState::Update;
	}

	/// Clear an action key.
	// @function ClearKey
	// @tparam Input.ActionID actionID Action ID to clear.
	static void ClearKey(int actionID)
	{
		if (!IsValidAction(actionID))
			return;

		ActionQueueMap[(ActionID)actionID] = ActionQueueState::Clear;
	}

	/// Clear all action keys.
	// @function ClearAllKeys
	static void ClearAllKeys()
	{
		for (auto& [keyActionID, queue] : ActionQueueMap)
			queue = ActionQueueState::Clear;
	}

	/// Vibrate the game controller if the function is available and the setting is on.
	// @function Vibrate
	// @tparam float strength Vibration strength. 
	// @tparam[opt=0.3] float time Vibration time in seconds.
	static void Vibrate(float strength, sol::optional<float> time)
	{
		Rumble(strength, time.value_or(0.3f), RumbleMode::Both);
	}

	void Register(sol::state* state, sol::table& parent)
	{
		auto table = sol::table(state->lua_state(), sol::create);

		parent.set(ScriptReserved_Input, table);
		table.set_function(ScriptReserved_InputGetAnalogKeyValue, &GetAnalogKeyValue);
		table.set_function(ScriptReserved_InputGetAnalogAxisValue, &GetAnalogAxisValue);
		table.set_function(ScriptReserved_InputGetMouseDisplayPosition, &GetMouseDisplayPosition);
		table.set_function(ScriptReserved_InputIsKeyHit, &IsKeyHit);
		table.set_function(ScriptReserved_InputIsKeyHeld, &IsKeyHeld);
		table.set_function(ScriptReserved_InputIsKeyPulsed, &IsKeyPulsed);
		table.set_function(ScriptReserved_InputIsKeyReleased, &IsKeyReleased);
		table.set_function(ScriptReserved_InputPushKey, &PushKey);
		table.set_function(ScriptReserved_InputClearKey, &ClearKey);
		table.set_function(ScriptReserved_InputClearAllKeys, &ClearAllKeys);
		table.set_function(ScriptReserved_InputVibrate, &Vibrate);

		// COMPATIBILITY
		table.set_function("KeyIsHit", &IsKeyHit);
		table.set_function("KeyIsHeld", &IsKeyHeld);
		table.set_function("KeyPush", &PushKey);
		table.set_function("KeyClear", &ClearKey);
		table.set_function("KeyClearAll", &ClearAllKeys);
		table.set_function("GetCursorDisplayPosition", &GetMouseDisplayPosition);

		auto handler = LuaHandler(state);
		handler.MakeReadOnlyTable(table, ScriptReserved_InputActionID, ACTION_IDS);
		handler.MakeReadOnlyTable(table, ScriptReserved_InputAxisID, AXIS_IDS);
	}
}
