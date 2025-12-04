#pragma once

#include "Math/Math.h"
#include "Specific/Input/Action.h"
#include "Specific/Input/Bindings.h"
#include "Specific/Input/Event.h"

using namespace TEN::Math;

struct ItemInfo;

namespace TEN::Input
{
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

	enum class ActionQueueState
	{
		None,
		Update,
		Clear
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

	struct DeviceStateData
	{
		std::vector<float> Events         = {}; // Index = `EventId`.
		Vector2            CursorPosition = {};

		bool IsUsingGamepad   = false;
		bool HasKeyboardInput = false;
		bool HasMouseInput    = false;
		bool HasGamepadInput  = false;
	};

	class InputManager
	{
	private:
		// Fields

		GamepadData     _gamepad      = {};
		RumbleData      _rumble       = {};
		DeviceStateData _deviceStates = {};

		BindingProfileId			  _activeBindingProfileId = BindingProfileId::CustomKeyboardMouse;
		BindingManager                _bindings               = BindingManager();
		std::vector<Action>           _actions                = {}; // Index = `ActionId`.
		std::vector<ActionQueueState> _actionQueues           = {}; // Index = `ActionId`.
		std::vector<Vector2>          _analogAxes             = {}; // Index = `AnalogAxisId`.

		bool _isLocked = false;

	public:
		// Constructors

		InputManager() = default;

		// Getters

		const Action&   GetAction(ActionId actionId) const;
		const Vector2&  GetAnalogAxis(AnalogAxisId axisId) const;
		const Vector2&  GetCursorPosition() const;
		float           GetRawEventState(EventId eventId) const;
		GamepadVendorId GetGamepadVendorId() const;

		// Setters

		void SetActiveBindingProfileId(BindingProfileId profileId);
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

	extern InputManager   g_Input;
	extern BindingManager g_Bindings;

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
