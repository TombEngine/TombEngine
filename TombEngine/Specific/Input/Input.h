#pragma once

#include "Math/Math.h"
#include "Specific/Input/Action.h"
#include "Specific/Input/Bindings.h"
#include "Specific/Input/Events.h"

using namespace TEN::Math;

struct ItemInfo;

namespace TEN::Input
{
	enum class AnalogAxisId
	{
		// Gameplay

		Move,
		Camera,

		// Raw

		Mouse,
		StickLeft,
		StickRight,

		Count
	};

	enum class GamepadVendorId
	{
		Generic,
		Xbox,
		Nintendo,
		Sony
	};

	enum class RumbleMode
	{
		Low,
		High,
		LowAndHigh
	};

	enum class ActionQueueState
	{
		None,
		Update,
		Clear
	};

	struct StateData
	{
		std::vector<float> Events             = {}; // Index = `EventId`.
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

	struct RumbleData
	{
		RumbleMode   Mode               = RumbleMode::Low;
		float        IntensityFrom      = 0.0f;
		float        IntensityTo        = 0.0f;
		unsigned int DurationGameFrames = 0;
		unsigned int GameFrames         = 0;
	};

	class InputManager
	{
	private:
		// Fields

		bool                          _isLocked     = false; // TODO: Not used yet.
		GamepadData                   _gamepad      = {};
		BindingManager                _bindings     = BindingManager();
		StateData                     _states       = {};
		RumbleData                    _rumble       = {};
		std::vector<Action>           _actions      = {}; // Index = `ActionId`.
		std::vector<ActionQueueState> _actionQueues = {}; // Index = `ActionId`.
		std::vector<Vector2>          _analogAxes   = {}; // Index = `AnalogAxisId`.

	public:
		// Constructors

		InputManager() = default;

		// Getters

		const Action&   GetAction(ActionId actionId) const;
		const Vector2&  GetAnalogAxis(AnalogAxisId axisId) const;
		const Vector2&  GetCursorPosition() const;
		GamepadVendorId GetGamepadVendorId() const;

		// Setters

		void SetActionQueue(ActionId actionId, ActionQueueState queueState);
		void SetRumble(RumbleMode mode, float intensityFrom, float intensityTo, float durationSec);

		// Inquirers

		bool IsGamepadConnected() const;
		bool IsUsingGamepad() const;

		// Utilities

		void Initialize();
		void Deinitialize();
		void Update(SDL_Window& window, const Vector2& mouseWheelAxis, bool allowAsyncUpdate = false, bool applyQueues = false);

		void Lock();
		void Unlock();
		void ConnectGamepad(int deviceId);
		void DisconnectGamepad(int deviceId);
		void StopRumble();
		void ApplyActionQueues();
		void ClearAction(ActionId actionId);

	private:
		// Helpers

		std::string GetGamepadVendorName(GamepadVendorId vendorId) const;

		void UpdateActions(bool applyQueues);
		void UpdateAnalogAxes();
		void UpdateRumble();

		void ReadKeyboard();
		void ReadMouse(SDL_Window& window, const Vector2& wheelAxis);
		void ReadGamepad();

		void HandleHotkeyActions();
	};

	extern InputManager g_Input = InputManager();

	// Getters

	float          GetActionValue(ActionId actionId);
	unsigned int   GetActionTimeActive(ActionId actionId);
	unsigned int   GetActionTimeInactive(ActionId actionId);
	const Vector2& GetMoveAxis();
	const Vector2& GetCameraAxis();
	const Vector2& GetMouseAxis();
	Vector2        GetMouse2DPosition();

	// Inquirers

	bool IsClicked(ActionId actionId);
	bool IsHeld(ActionId actionId, float delaySec = 0.0f);
	bool IsPulsed(ActionId actionId, float delaySec, float initialDelaySec = 0.0f);
	bool IsReleased(ActionId actionId, float delaySecMax = FLT_MAX);
	bool IsDirectionalActionHeld();
	bool IsWakeActionHeld();
	bool IsOpticActionHeld();
	bool NoAction();

	// Utilities

	void ApplyDefaultBindings();
	void Rumble(float power, float durationSec = 0.3f, RumbleMode mode = RumbleMode::LowAndHigh);
	void StopRumble();

	void ClearAllActions();
	void ClearAction(ActionId actionId);
}
