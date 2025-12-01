#pragma once

#include "Math/Math.h"
#include "Specific/Input/Bindings.h"
#include "Specific/Input/InputAction.h"
#include "Specific/Input/Keys.h"

using namespace TEN::Math;

struct ItemInfo;

namespace TEN::Input
{
	enum class AnalogAxisId
	{
		Move,
		Camera,

		Mouse,
		// TODO: Add raw axes for analog gamepad sticks. -- Sezz 2025.5.9
		/*StickLeft,
		StickRight,*/

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
	extern std::unordered_map<ActionId, Action>			  ActionMap;
	extern std::unordered_map<ActionId, ActionQueueState> ActionQueueMap;

	void InitializeInput();
	void SetInputLockState(bool locked);
	void UpdateInputActions(bool allowAsyncUpdate = false, bool applyQueue = false);
	void ApplyActionQueue();
	void ClearAllActions();
	void Rumble(float power, float delaySec = 0.3f, RumbleMode mode = RumbleMode::Both);
	void StopRumble();
	void ApplyDefaultBindings();

	void		 ClearAction(ActionId actionId);
	bool		 NoAction();
	bool		 IsClicked(ActionId actionId);
	bool		 IsHeld(ActionId actionId, float delaySec = 0.0f);
	bool		 IsPulsed(ActionId actionId, float delaySec, float initialDelaySec = 0.0f);
	bool		 IsReleased(ActionId actionId, float maxDelaySec = FLT_MAX);
	float		 GetActionValue(ActionId actionId);
	unsigned int GetActionTimeActive(ActionId actionId);
	unsigned int GetActionTimeInactive(ActionId actionId);

	bool IsDirectionalActionHeld();
	bool IsWakeActionHeld();
	bool IsOpticActionHeld();

	const Vector2& GetMoveAxis();
	const Vector2& GetCameraAxis();
	const Vector2& GetMouseAxis();

	Vector2 GetMouse2DPosition();

	// ====================================================================================================================

	enum class GamepadVendorId
	{
		Generic,
		Xbox,
		Nintendo,
		Sony
	};

	enum class AnalogAxisId2
	{
		/** Gameplay axes */

		Move,
		Camera,

		/** Input device axes */

		Mouse,
		StickLeft,
		StickRight,

		Count
	};

	enum class RumbleMode2
	{
		Low,
		High,
		LowAndHigh
	};

	struct StateData
	{
		std::vector<float> Events             = {}; // Index = `EventId`, value = event state.
		Vector2            CursorPosition     = {};
		Vector2            PrevCursorPosition = {};

		bool IsUsingGamepad   = false;
		bool HasKeyboardInput = false;
		bool HasMouseInput    = false;
		bool HasGamepadInput  = false;
	};

	struct GamepadData
	{
		int             Id       = NO_VALUE;
		SDL_Gamepad*    Device   = nullptr;
		GamepadVendorId VendorId = GamepadVendorId::Generic;
	};

	struct RumbleData2
	{
		RumbleMode2  Mode          = RumbleMode2::Low;
		float        IntensityFrom = 0.0f;
		float        IntensityTo   = 0.0f;
		unsigned int DurationTicks = 0;
		unsigned int GameFrames    = 0;
	};

	class InputManager
	{
	private:
		// Fields

		GamepadData                   _gamepad      = {};
		BindingManager                _bindings     = BindingManager();
		StateData                     _states       = {};
		RumbleData2                   _rumble       = {};
		std::vector<Action>           _actions      = {}; // Index = `ActionId`.
		std::vector<ActionQueueState> _actionQueues = {}; // Index = `ActionId`.
		std::vector<Vector2>          _analogAxes   = {}; // Index = `AnalogAxisId2`.

	public:
		// Constructors

		InputManager() = default;

		// Getters

		const Action&   GetAction(ActionId actionId) const;
		const Vector2&  GetAnalogAxis(AnalogAxisId2 axisId) const;
		const Vector2&  GetCursorPosition() const;
		GamepadVendorId GetGamepadVendorId() const;

		// Setters

		void SetRumble(RumbleMode2 mode, float intensityFrom, float intensityTo, float durationSec);

		// Inquirers

		bool IsGamepadConnected() const;
		bool IsUsingGamepad() const;

		// Utilities

		void Initialize();
		void Deinitialize();
		void Update(SDL_Window& window, const Vector2& mouseWheelAxis);

		void ConnectGamepad(int deviceId);
		void DisconnectGamepad(int deviceId);
		void ClearAction(ActionId actionId);
		void StopRumble();

	private:
		// Helpers

		std::string GetGamepadVendorName(GamepadVendorId vendorId) const;

		void UpdateActions();
		void UpdateRumble();

		void ReadKeyboard();
		void ReadMouse(SDL_Window& window, const Vector2& wheelAxis);
		void ReadGamepad();

		void HandleHotkeyActions();
	};

	extern InputManager g_Input = InputManager();
}
