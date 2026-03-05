#include "framework.h"
#include "Specific/Input/Input.h"

#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/Gui.h"
#include "Game/items.h"
#include "Game/savegame.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererEnums.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/EngineMain.h"
#include "Specific/Input/Bindings.h"
#include "Specific/Input/Event.h"
#include "Specific/Parallel.h"
#include "Specific/trutils.h"

using namespace TEN::Gui;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

namespace TEN::Input
{
	InputManager   g_Input    = InputManager();
	BindingManager g_Bindings = BindingManager();

	const Action& InputManager::GetAction(ActionId actionId) const
	{
		return _actions[(int)actionId];
	}

	const Vector2& InputManager::GetAnalogAxis(AnalogAxisId axisId) const
	{
		return _analogAxes[(int)axisId];
	}

	const Vector2& InputManager::GetCursorPosition() const
	{
		return _deviceStates.CursorPosition;
	}

	float InputManager::GetRawEventState(EventId eventId) const
	{
		return  _deviceStates.Events[(int)eventId];
	}

	GamepadVendorId InputManager::GetGamepadVendorId() const
	{
		return _gamepad.VendorId;
	}

	void InputManager::SetActiveBindingProfileId(BindingProfileId profileId)
	{
		_activeBindingProfileId = profileId;
	}

	void InputManager::SetActionQueue(ActionId actionId, ActionQueueState queueState)
	{
		_actionQueues[(int)actionId] = queueState;
	}

	void InputManager::SetRumble(RumbleMode mode, float intensityFrom, float intensityTo, float durationSec)
	{
		_rumble.Mode = mode;
		_rumble.IntensityFrom = std::clamp(intensityFrom, 0.0f,  1.0f);
		_rumble.IntensityTo = std::clamp(intensityTo, 0.0f, 1.0f);
		_rumble.DurationGameFrames =
		_rumble.GameFrames = SecToGameFrames(durationSec);
	}

	bool InputManager::IsGamepadConnected() const
	{
		return _gamepad.Id != NO_VALUE && _gamepad.Device != nullptr;
	}

	bool InputManager::IsUsingGamepad() const
	{
		return _deviceStates.IsUsingGamepad;
	}

	void InputManager::Initialize()
	{
		if (!SDL_Init(SDL_INIT_GAMEPAD))
		{
			TENLog(fmt::format("Failed to initialize gamepad subsystem: {}", SDL_GetError()), Debug::LogLevel::Error);
		}

		// Initialize device states.
		_deviceStates.Events.resize((int)EventId::Count, 0.0f);

		// Initialize analog axes.
		_analogAxes.resize((int)AnalogAxisId::Count, Vector2::Zero);

		// Initialize actions.
		_actions.resize((int)ActionId::Count);
		_actionQueues.resize((int)ActionId::Count, ActionQueueState::None);
		for (int i = 0; i < (int)ActionId::Count; i++)
			_actions[i] = Action((ActionId)i);

		// Initialize bindings.
		_bindings.Initialize(g_Configuration.KeyboardMouseBindings, g_Configuration.GamepadBindings);

		// TODO: Connect it first.
		if (IsUsingGamepad())
		{
			g_Configuration.EnableRumble           =
			g_Configuration.EnableThumbstickCamera = true;
			SaveConfiguration();
		}
	}

	void InputManager::Deinitialize()
	{
		DisconnectGamepad(_gamepad.Id);
	}

	void InputManager::Update(SDL_Window& window, const Vector2& mouseWheelAxis, bool allowAsyncUpdate, bool applyQueues)
	{
		// Capture event states asynchronously if unlocked and not in a frameskip.
		if (!_isLocked && (allowAsyncUpdate || !g_Synchronizer.Locked()))
		{
			auto tasks = ParallelTasks
			{
				TASK(ReadKeyboard()),
				TASK(ReadMouse(window, mouseWheelAxis)),
				TASK(ReadGamepad())
			};
			g_Parallel.AddTasks(tasks).wait();
		}

		// Update "using gamepad" state.
		if (_deviceStates.HasKeyboardInput || _deviceStates.HasMouseInput)
		{
			_deviceStates.IsUsingGamepad = false;
		}
		else if (_deviceStates.HasGamepadInput)
		{
			_deviceStates.IsUsingGamepad = true;
		}

		// Update components.
		UpdateActions(applyQueues);
		UpdateAnalogAxes();
		UpdateRumble();
		HandleHotkeyActions();

		// Block simultaneous Left and Right actions.
		if (GetAction(In::Left).IsHeld() && GetAction(In::Right).IsHeld())
		{
			ClearAction(In::Left);
			ClearAction(In::Right);
		}

		// Clear data.
		_deviceStates.HasKeyboardInput = false;
		_deviceStates.HasMouseInput = false;
		_deviceStates.HasGamepadInput = false;
	}

	void InputManager::Lock()
	{
		_isLocked = true;
	}

	void InputManager::Unlock()
	{
		_isLocked = false;
	}

	void InputManager::ConnectGamepad(int deviceId)
	{
		constexpr int XBOX_VENDOR_CODE     = 0x045E;
		constexpr int NINTENDO_VENDOR_CODE = 0x057E;
		constexpr int SONY_VENDOR_CODE     = 0x054C;

		// Check if a gamepad is already connected.
		if (IsGamepadConnected())
		{
			return;
		}

		// Set connection.
		_gamepad.Device = SDL_OpenGamepad(deviceId);
		if (_gamepad.Device != nullptr)
		{
			_gamepad.Id = deviceId;

			switch (SDL_GetGamepadVendor(_gamepad.Device))
			{
				case XBOX_VENDOR_CODE:
					_gamepad.VendorId = GamepadVendorId::Xbox;
					break;

				case NINTENDO_VENDOR_CODE:
					_gamepad.VendorId = GamepadVendorId::Nintendo;
					break;

				case SONY_VENDOR_CODE:
					_gamepad.VendorId = GamepadVendorId::Sony;
					break;

				default:
					_gamepad.VendorId = GamepadVendorId::Generic;
					break;
			}

			SetRumble(RumbleMode::Low, 0.0f, 1.0f, 0.1f);

			TENLog(fmt::format("{} gamepad connected.", GetGamepadVendorName(_gamepad.VendorId)));
		}
	}

	void InputManager::DisconnectGamepad(int deviceId)
	{
		// Check if a gamepad is connected and device IDs match.
		if (!IsGamepadConnected() || _gamepad.Id != deviceId)
		{
			return;
		}

		_gamepad = {};
		SDL_CloseGamepad(_gamepad.Device);

		TENLog("Gamepad disconnected.");
	}

	void InputManager::ApplyActionQueues()
	{
		for (int i = 0; i < (int)ActionId::Count; i++)
		{
			auto actionId = (ActionId)i;
			switch (_actionQueues[(int)actionId])
			{
				default:
				case ActionQueueState::None:
					break;

				case ActionQueueState::Update:
					_actions[(int)actionId].Update(true);
					break;

				case ActionQueueState::Clear:
					_actions[(int)actionId].Clear();
					break;
			}
		}

		for (auto& queue : _actionQueues)
			queue = ActionQueueState::None;
	}

	void InputManager::ClearAction(ActionId actionId)
	{
		_actions[(int)actionId].Clear();
	}

	void InputManager::StopRumble()
	{
		_rumble = {};
	}

	std::string InputManager::GetGamepadVendorName(GamepadVendorId vendorId) const
	{
		constexpr char GENERIC_VENDOR_NAME[]  = "Generic";
		constexpr char XBOX_VENDOR_NAME[]     = "Xbox";
		constexpr char NINTENDO_VENDOR_NAME[] = "Nintendo";
		constexpr char SONY_VENDOR_NAME[]     = "Sony";

		switch (vendorId)
		{
			case GamepadVendorId::Generic:
				break;

			case GamepadVendorId::Xbox:
				return XBOX_VENDOR_NAME;

			case GamepadVendorId::Nintendo:
				return NINTENDO_VENDOR_NAME;

			case GamepadVendorId::Sony:
				return SONY_VENDOR_NAME;
		}

		return GENERIC_VENDOR_NAME;
	}

	void InputManager::UpdateActions(bool applyQueues)
	{
		auto testDefaultConflict = [&](const std::vector<EventId>& defaultEventIds, const std::vector<ActionId>& actionIds, const BindingProfile& customProfile)
		{
			// Check for conflict between custom and default event binding.
			bool hasConflict = false;
			for (auto actionId : actionIds)
			{
				const auto& customEventIds = customProfile.at(actionId);
				for (auto defaultEventId : defaultEventIds)
				{
					if (Contains(customEventIds, defaultEventId))
					{
						hasConflict = true;
						break;
					}
				}

				if (hasConflict)
					break;
			}

			return hasConflict;
		};

		// 1) Update user action states.
		auto updateUserActions = [&]()
		{
			// Get binding profiles.
			const auto& customProfile = _bindings.GetProfile(_activeBindingProfileId);
			const auto& defaultProfile = (_activeBindingProfileId == BindingProfileId::CustomKeyboardMouse) ?
				DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE :
				DEFAULT_USER_GAMEPAD_BINDING_PROFILE;

			for (auto actionGroupId : USER_ACTION_GROUP_IDS)
			{
				const auto& actionIds = ACTION_ID_GROUPS[(int)actionGroupId];
				for (auto actionId : actionIds)
				{
					auto& action = _actions[(int)actionId];
					float state = 0.0f;

					// Apply custom-bound event state to action.
					const auto& customEventIds = customProfile.at(actionId);
					for (const auto& eventId : customEventIds)
						state = std::max(state, GetRawEventState(eventId));

					// Apply default-bound event state to action.
					if (state == 0.0f)
					{
						bool hasDefaultConflict = testDefaultConflict(defaultProfile.at(actionId), actionIds, customProfile);
						if (!hasDefaultConflict)
						{
							const auto& defaultEventIds = defaultProfile.at(actionId);
							for (const auto& eventId : defaultEventIds)
								state = std::max(state, GetRawEventState(eventId));
						}
					}

					action.Update(state);
				}
			}
		};

		// 2) Update raw action states.
		auto updateRawActions = [&]()
		{
			for (auto profileId : RAW_BINDING_PROFILE_IDS)
			{
				const auto& profile = _bindings.GetProfile(profileId);
				for (auto& [keyActionId, eventIds] : profile)
				{
					auto& action = _actions[(int)keyActionId];
					float state = 0.0f;

					for (auto eventId : eventIds)
						state = std::max(state, GetRawEventState(eventId));

					// Use max bound event state.
					action.Update(state);
				}
			}
		};

		// Update action states asynchronously.
		auto tasks = ParallelTasks
		{
			TASK(updateUserActions()),
			TASK(updateRawActions())
		};
		g_Parallel.AddTasks(tasks).wait();

		// Apply action queues.
		if (applyQueues)
			ApplyActionQueues();
	}

	void InputManager::UpdateAnalogAxes()
	{
		constexpr float AXIS_MIN = 0.2f;
		constexpr float AXIS_MAX = 1.7f;

		auto& moveAxis = _analogAxes[(int)AnalogAxisId::Move];
		auto& camAxis = _analogAxes[(int)AnalogAxisId::Camera];
		const auto& mouseAxis = GetAnalogAxis(AnalogAxisId::Mouse);
		const auto& stickLeftAxis = GetAnalogAxis(AnalogAxisId::StickLeft);
		const auto& stickRightAxis = GetAnalogAxis(AnalogAxisId::StickRight);

		// Set move axis.
		if (stickLeftAxis != Vector2::Zero)
		{
			auto remappedLength = Remap(stickLeftAxis.Length(), 0.0f, 1.0f, AXIS_MIN, AXIS_MAX);
			moveAxis = stickLeftAxis;
			moveAxis.Normalize();
			moveAxis *= remappedLength;
		}
		else
		{
			if (IsHeld(In::Forward))
			{
				moveAxis.y = 1.0f;
			}
			else if (IsHeld(In::Back))
			{
				moveAxis.y = -1.0f;
			}
			else
			{
				moveAxis.y = 0.0f;
			}

			if (IsHeld(In::Left))
			{
				moveAxis.x = -1.0f;
			}
			else if (IsHeld(In::Right))
			{
				moveAxis.x = 1.0f;
			}
			else
			{
				moveAxis.x = 0.0f;
			}
		}

		// Set camera axis.
		if (stickRightAxis != Vector2::Zero)
		{
			camAxis = stickRightAxis;
		}
		else
		{
			camAxis = mouseAxis;
		}
	}

	void InputManager::UpdateRumble()
	{
		if (_rumble.GameFrames == 0 || !IsGamepadConnected())
		{
			_rumble = {};
			return;
		}

		// Compute intensity.
		float alpha = (float)_rumble.GameFrames / (float)_rumble.DurationGameFrames;
		float intensity = Lerp(_rumble.IntensityFrom, _rumble.IntensityTo, alpha);

		// Compute frequencies.
		unsigned short freqLow = (_rumble.Mode == RumbleMode::Low || _rumble.Mode == RumbleMode::LowAndHigh) ? (unsigned short)(intensity * USHRT_MAX) : 0;
		unsigned short freqHigh = (_rumble.Mode == RumbleMode::High || _rumble.Mode == RumbleMode::LowAndHigh) ? (unsigned short)(intensity * USHRT_MAX) : 0;

		// Compute duration.
		unsigned int durationMs = (unsigned int)round(GameFramesToSec(_rumble.DurationGameFrames) * 1000);

		// Rumble gamepad.
		if (!SDL_RumbleGamepad(_gamepad.Device, freqLow, freqHigh, durationMs))
		{
			TENLog(fmt::format("Failed to rumble gamepad: {}", SDL_GetError()), Debug::LogLevel::Error);
		}

		_rumble.GameFrames--;
	}

	void InputManager::ReadKeyboard()
	{
		constexpr auto START_EVENT_ID = EventId::A;

		int eventIdx = (int)START_EVENT_ID;

		// Set keyboard key event states.
		int keyboardStateCount = 0;
		const bool* keyboardState = SDL_GetKeyboardState(&keyboardStateCount);
		for (auto scanCode : VALID_KEYBOARD_SCAN_CODES)
		{
			if (scanCode < keyboardStateCount)
			{
				bool state = keyboardState[scanCode];
				if (state)
				{
					_deviceStates.HasKeyboardInput = true;
				}

				_deviceStates.Events[eventIdx] = state ? 1.0f : 0.0f;
			}

			eventIdx++;
		}

		// Set keyboard modifier event states.
		auto modState = SDL_GetModState();
		for (int modCode : VALID_KEYBOARD_MODIFIER_CODES)
		{
			bool state = modState & modCode;
			if (state)
			{
				_deviceStates.HasKeyboardInput = true;
			}

			_deviceStates.Events[eventIdx] = state ? 1.0f : 0.0f;
			eventIdx++;
		}
	}

	void InputManager::ReadMouse(SDL_Window& window, const Vector2& wheelAxis)
	{
		constexpr auto START_EVENT_ID = EventId::MouseClickLeft;
		constexpr int  AXIS_COUNT     = 2;

		int eventIdx = (int)START_EVENT_ID;

		// Compute cursor position.
		auto pos = Vector2::Zero;
		auto butState = SDL_GetMouseState(&pos.x, &pos.y);
		pos = (pos / g_Renderer.GetScreenResolution().ToVector2()) * DISPLAY_SPACE_RES;
		pos.y = DISPLAY_SPACE_RES.y - pos.y;

		// Set mouse button event states.
		for (int butCode : VALID_MOUSE_BUTTON_CODES)
		{
			bool state = butState & SDL_BUTTON_MASK(butCode);
			if (state)
			{
				_deviceStates.HasMouseInput = true;
			}

			_deviceStates.Events[eventIdx] = state ? 1.0f : 0.0f;
			eventIdx++;
		}

		if (wheelAxis != Vector2::Zero)
		{
			_deviceStates.HasMouseInput = true;
		}

		// TODO: Investigate. Unclear how SDL3 mouse wheel values work.
		// Set mouse scroll event states.
		_deviceStates.Events[eventIdx] = (wheelAxis.x < 0.0f) ? std::clamp(abs(wheelAxis.x), 0.0f, 1.0f) : 0.0f;
		_deviceStates.Events[eventIdx + 1] = (wheelAxis.x > 0.0f) ? std::clamp(abs(wheelAxis.x), 0.0f, 1.0f) : 0.0f;
		_deviceStates.Events[eventIdx + 2] = (wheelAxis.y < 0.0f) ? std::clamp(abs(wheelAxis.y), 0.0f, 1.0f) : 0.0f;
		_deviceStates.Events[eventIdx + 3] = (wheelAxis.y > 0.0f) ? std::clamp(abs(wheelAxis.y), 0.0f, 1.0f) : 0.0f;
		eventIdx += SQUARE(AXIS_COUNT);

		// Set cursor position state.
		auto prevCursorPos = _deviceStates.CursorPosition;
		_deviceStates.CursorPosition = pos;

		auto res = Vector2i::Zero;
		if (!SDL_GetWindowSize(&window, &res.x, &res.y))
		{
			TENLog(fmt::format("Failed to get window size: {}", SDL_GetError()), Debug::LogLevel::Error);
		}

		float sensitivity = (g_Configuration.MouseSensitivity * 0.1f) + 0.4f;
		auto moveAxis = (((_deviceStates.CursorPosition - prevCursorPos) / DISPLAY_SPACE_RES) * (res.ToVector2() / DISPLAY_SPACE_RES)) * sensitivity;
		if (moveAxis != Vector2::Zero)
		{
			_deviceStates.HasMouseInput = true;
		}

		// Set mouse movement event states.
		_deviceStates.Events[eventIdx] = (moveAxis.x < 0.0f) ? abs(moveAxis.x) : 0.0f;
		_deviceStates.Events[eventIdx + 1] = (moveAxis.x > 0.0f) ? abs(moveAxis.x) : 0.0f;
		_deviceStates.Events[eventIdx + 2] = (moveAxis.y < 0.0f) ? abs(moveAxis.y) : 0.0f;
		_deviceStates.Events[eventIdx + 3] = (moveAxis.y > 0.0f) ? abs(moveAxis.y) : 0.0f;
		eventIdx += SQUARE(AXIS_COUNT);

		// Set raw mouse axis.
		_analogAxes[(int)AnalogAxisId::Mouse] = moveAxis;
	}

	void InputManager::ReadGamepad()
	{
		constexpr auto  START_EVENT_ID = EventId::GamepadSouth;
		constexpr int   AXIS_COUNT     = 2;
		constexpr float AXIS_DEADZONE  = ((float)SHRT_MAX / 8.0f) / (float)SHRT_MAX;

		int eventIdx = (int)START_EVENT_ID;

		// Set gamepad button event states.
		for (auto butCode : VALID_GAMEPAD_BUTTON_CODES)
		{
			bool state = false;
			if (IsGamepadConnected())
			{
				state = SDL_GetGamepadButton(_gamepad.Device, butCode);
			}
			if (state)
			{
				_deviceStates.HasGamepadInput = true;
			}

			_deviceStates.Events[eventIdx] = state ? 1.0f : 0.0f;
			eventIdx++;
		}

		// Collect stick axes.
		auto stickAxes = std::vector<Vector2>(VALID_GAMEPAD_STICK_AXIS_CODES.size() / AXIS_COUNT);
		for (int i = 0, j = 0; i < VALID_GAMEPAD_STICK_AXIS_CODES.size(); i++)
		{
			if (!IsGamepadConnected())
			{
				break;
			}

			auto axisCode = VALID_GAMEPAD_STICK_AXIS_CODES[i];
			float state = (float)SDL_GetGamepadAxis(_gamepad.Device, axisCode) / (float)SHRT_MAX;

			auto& axis = stickAxes[j];
			if ((i % AXIS_COUNT) == 0)
			{
				axis.x = state;
			}
			else
			{
				axis.y = state;

				// Remap axis to active range.
				if (axis.Length() >= AXIS_DEADZONE)
				{
					float remappedLength = Remap(axis.Length(), AXIS_DEADZONE, 1.0f, 0.0f, 1.0f);
					axis.Normalize();
					axis *= remappedLength;
				}
				else
				{
					axis = Vector2::Zero;
				}

				j++;
			}
		}

		// Set gamepad stick axis event states.
		for (int i = 0; i < stickAxes.size(); i++)
		{
			const auto& axis = stickAxes[i];
			if (axis != Vector2::Zero)
			{
				_deviceStates.HasGamepadInput = true;
			}

			_deviceStates.Events[eventIdx + i] = (axis.x < 0.0f) ? abs(axis.x) : 0.0f;
			_deviceStates.Events[eventIdx + (i + 1)] = (axis.x > 0.0f) ? abs(axis.x) : 0.0f;
			_deviceStates.Events[eventIdx + (i + 2)] = (axis.y < 0.0f) ? abs(axis.y) : 0.0f;
			_deviceStates.Events[eventIdx + (i + 3)] = (axis.y > 0.0f) ? abs(axis.y) : 0.0f;
			eventIdx += AXIS_COUNT * 2;
		}

		// Set gamepad trigger axis event states.
		for (auto axisCode : VALID_GAMEPAD_TRIGGER_AXIS_CODES)
		{
			float state = 0.0f;
			if (IsGamepadConnected())
			{
				// Remap state to active range.
				state = (float)SDL_GetGamepadAxis(_gamepad.Device, axisCode) / (float)SHRT_MAX;
				if (state >= AXIS_DEADZONE)
				{
					state = Remap(state, AXIS_DEADZONE, 1.0f, 0.0f, 1.0f);
				}
			}
			if (state > 0.0f)
			{
				_deviceStates.HasGamepadInput = true;
			}

			_deviceStates.Events[eventIdx] = state;
			eventIdx++;
		}

		// Set raw stick axes.
		_analogAxes[(int)AnalogAxisId::StickLeft] = stickAxes.front();
		_analogAxes[(int)AnalogAxisId::StickRight] = stickAxes.back();
	}

	void InputManager::HandleHotkeyActions()
	{
		// Save screenshot.
		static bool dbScreenshot = true;
		if ((GetRawEventState(EventId::PrintScreen) || GetRawEventState(EventId::F12)) && dbScreenshot)
			g_Renderer.SaveScreenshot();
		dbScreenshot = !(GetRawEventState(EventId::PrintScreen) || GetRawEventState(EventId::F12));

		// Toggle fullscreen.
		static bool dbFullscreen = true;
		if ((GetRawEventState(EventId::Alt) && GetRawEventState(EventId::Return)) && dbFullscreen)
		{
			g_Configuration.EnableWindowedMode = !g_Configuration.EnableWindowedMode;
			SaveConfiguration();
			g_Renderer.ToggleFullScreen();
		}
		dbFullscreen = !(GetRawEventState(EventId::Alt) && GetRawEventState(EventId::Return));

		if (!DebugMode)
			return;

		// Switch debug page.
		static bool dbDebugPage = true;
		if ((GetRawEventState(EventId::F10) || GetRawEventState(EventId::F11)) && dbDebugPage)
			g_Renderer.SwitchDebugPage(GetRawEventState(EventId::F10));
		dbDebugPage = !(GetRawEventState(EventId::F10) || GetRawEventState(EventId::F11));

		// Cycle pathfinding display with TAB when on pathfinding debug page.
		static bool dbPathfindingCycle = true;
		if (GetRawEventState(EventId::Tab) && dbPathfindingCycle &&
			g_Renderer.GetDebugPage() == RendererDebugPage::PathfindingStats)
		{
			CyclePathfindingDisplay();
		}
		dbPathfindingCycle = !GetRawEventState(EventId::Tab);

		// Reload shaders.
		static bool dbReloadShaders = true;
		if (GetRawEventState(EventId::F9) && dbReloadShaders)
			g_Renderer.ReloadShaders();
		dbReloadShaders = !GetRawEventState(EventId::F9);
	}

	float GetActionValue(ActionId actionId)
	{
		return g_Input.GetAction(actionId).GetValue();
	}

	// Time in game frames.
	unsigned int GetActionTimeActive(ActionId actionId)
	{
		return g_Input.GetAction(actionId).GetTimeActive();
	}

	// Time in game frames.
	unsigned int GetActionTimeInactive(ActionId actionId)
	{
		return g_Input.GetAction(actionId).GetTimeInactive();
	}

	const Vector2& GetMoveAxis()
	{
		return g_Input.GetAnalogAxis(AnalogAxisId::Move);
	}

	const Vector2& GetCameraAxis()
	{
		return g_Input.GetAnalogAxis(AnalogAxisId::Camera);
	}

	const Vector2& GetMouseAxis()
	{
		return g_Input.GetAnalogAxis(AnalogAxisId::Mouse);
	}

	Vector2 GetMouse2DPosition()
	{
		return g_Input.GetCursorPosition();
	}

	bool IsClicked(ActionId actionId)
	{
		return g_Input.GetAction(actionId).IsClicked();
	}

	bool IsHeld(ActionId actionId, float delaySec)
	{
		return g_Input.GetAction(actionId).IsHeld(delaySec);
	}

	bool IsPulsed(ActionId actionId, float delaySec, float initialDelaySec)
	{
		return g_Input.GetAction(actionId).IsPulsed(delaySec, initialDelaySec);
	}

	bool IsReleased(ActionId actionId, float delaySecMax)
	{
		return g_Input.GetAction(actionId).IsReleased(delaySecMax);
	}

	bool IsDirectionalActionHeld()
	{
		return (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right));
	}

	bool IsWakeActionHeld()
	{
		if (IsDirectionalActionHeld() || IsHeld(In::StepLeft) || IsHeld(In::StepRight) ||
			IsHeld(In::Walk) || IsHeld(In::Jump) || IsHeld(In::Sprint) || IsHeld(In::Roll) || IsHeld(In::Crouch) ||
			IsHeld(In::Draw) || IsHeld(In::Flare) || IsHeld(In::Action))
		{
			return true;
		}

		return false;
	}

	bool IsOpticActionHeld()
	{
		return (IsDirectionalActionHeld() || IsHeld(In::Action) || IsHeld(In::Crouch) || IsHeld(In::Sprint));
	}

	bool NoAction()
	{
		for (auto actionGroupId : RAW_ACTION_GROUP_IDS)
		{
			const auto& actionGroup = ACTION_ID_GROUPS[(int)actionGroupId];
			for (auto actionId : actionGroup)
			{
				if (IsHeld(actionId))
					return false;
			}
		}

		return true;
	}

	void ApplyDefaultBindings()
	{
		// @inputme
		//_bindings.SetProfile(BindingProfileId::CustomKeyboardMouse, DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE);
		//_bindings.SetProfile(BindingProfileId::CustomGamepad, DEFAULT_USER_GAMEPAD_BINDING_PROFILE);
	}

	void Rumble(float power, float durationSec, RumbleMode mode)
	{
		g_Input.SetRumble(mode, power, power, durationSec);
	}

	void StopRumble()
	{
		g_Input.StopRumble();
	}

	void ClearAllActions()
	{
		for (auto actionIds : ACTION_ID_GROUPS)
		{
			for (auto actionId : actionIds)
			{
				g_Input.ClearAction(actionId);
				g_Input.SetActionQueue(actionId, ActionQueueState::None);
			}
		}
	}

	void ClearAction(ActionId actionId)
	{
		g_Input.ClearAction(actionId);
	}
}
