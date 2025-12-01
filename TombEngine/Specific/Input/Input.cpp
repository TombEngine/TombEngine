#include "framework.h"
#include "Specific/Input/Input.h"

#include "Game/camera.h"
#include "Game/Gui.h"
#include "Game/items.h"
#include "Game/savegame.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/EngineMain.h"
#include "Specific/Input/Event.h"
#include "Specific/Parallel.h"
#include "Specific/trutils.h"

using namespace TEN::Gui;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

namespace TEN::Input
{
	constexpr auto AXIS_SCALE  = 1.5f;
	constexpr auto AXIS_OFFSET = 0.2f;

	// Globals

	std::unordered_map<int, float>				   KeyMap;			// Key = key ID, value = key value.
	std::unordered_map<ActionId, Action>		   ActionMap;		// Key = action ID, value = action.
	std::unordered_map<ActionId, ActionQueueState> ActionQueueMap;	// Key = action ID, value = action queue state.

	bool InputLocked = false; // Disables control polling when application is defocused.

	void SetInputLockState(bool locked)
	{
		InputLocked = locked;
	}

	static bool TestBoundKey(int keyID)
	{
		for (int i = 1; i >= 0; i--)
		{
			auto profileId = (BindingProfileId)i;
			for (int j = 0; j < (int)ActionId::Count; j++)
			{
				auto actionId = (ActionId)j;
				if (g_Bindings.GetBoundEventIds(profileId, actionId) == keyID)
					return true;
			}
		}

		return false;
	}

	// TODO
	static void SetDiscreteAxisValues(unsigned int keyID)
	{
		for (int i = 0; i < (int)BindingProfileId::Count; i++)
		{
			auto profileId = (BindingProfileId)i;
			if (g_Bindings.GetBoundEventIds(profileId, In::Forward) == keyID)
			{
				AxisMap[AnalogAxisId::Move].y = 1.0f;
			}
			else if (g_Bindings.GetBoundEventIds(profileId, In::Back) == keyID)
			{
				AxisMap[AnalogAxisId::Move].y = -1.0f;
			}
			else if (g_Bindings.GetBoundEventIds(profileId, In::Left) == keyID)
			{
				AxisMap[AnalogAxisId::Move].x = -1.0f;
			}
			else if (g_Bindings.GetBoundEventIds(profileId, In::Right) == keyID)
			{
				AxisMap[AnalogAxisId::Move].x = 1.0f;
			}
		}
	}

	// TODO
	static void ReadGamepad()
	{
		if (InputLocked)
			return;

		try
		{
			// Poll buttons.
			for (int keyID = 0; keyID < state.mButtons.size(); keyID++)
				KeyMap[KEY_OFFSET_GAMEPAD + keyID] = state.mButtons[keyID] ? 1.0f : 0.0f;

			// Poll axes.
			for (int axis = 0; axis < state.mAxes.size(); axis++)
			{
				// NOTE: Anything above 6 existing XBOX/PS controller axes not supported (2 sticks + 2 triggers).
				if (axis >= GAMEPAD_AXIS_COUNT)
					break;

				// Filter out deadzone.
				if (abs(state.mAxes[axis].abs) < AXIS_DEADZONE)
					continue;

				// Calculate raw normalized analog value (for camera).
				float normalizedValue = float(state.mAxes[axis].abs + (state.mAxes[axis].abs > 0 ? -AXIS_DEADZONE : AXIS_DEADZONE)) /
					float(SHRT_MAX - AXIS_DEADZONE);

				// Calculate scaled analog value for movement.
				// NOTE: [0.2f, 1.7f] range gives most organic rates.
				float scaledValue = (abs(normalizedValue) * AXIS_SCALE) + AXIS_OFFSET;

				// Calculate and reset discrete input slots.
				int negKeyID = (KEY_OFFSET_GAMEPAD + GAMEPAD_BUTTON_COUNT) + (axis * 2);
				int posKeyID = (KEY_OFFSET_GAMEPAD + GAMEPAD_BUTTON_COUNT) + (axis * 2) + 1;
				KeyMap[negKeyID] = (normalizedValue > 0) ? abs(normalizedValue) : 0.0f;
				KeyMap[posKeyID] = (normalizedValue < 0) ? abs(normalizedValue) : 0.0f;

				// Determine discrete input registering based on analog value.
				int usedKeyID = (normalizedValue > 0) ? negKeyID : posKeyID;

				// Register analog input in certain direction.
				// If axis is bound as directional controls, register axis as directional input.
				// Otherwise, register as camera movement input (for future).
				// NOTE: `abs()` operations are needed to avoid issues with inverted axes on different controllers.

				if (g_Bindings.GetBoundEventIds(BindingProfileId::Custom, In::Forward) == usedKeyID)
				{
					AxisMap[AnalogAxisId::Move].y = abs(scaledValue);
				}
				else if (g_Bindings.GetBoundEventIds(BindingProfileId::Custom, In::Back) == usedKeyID)
				{
					AxisMap[AnalogAxisId::Move].y = -abs(scaledValue);
				}
				else if (g_Bindings.GetBoundEventIds(BindingProfileId::Custom, In::Left)  == usedKeyID)
				{
					AxisMap[AnalogAxisId::Move].x = -abs(scaledValue);
				}
				else if (g_Bindings.GetBoundEventIds(BindingProfileId::Custom, In::Right) == usedKeyID)
				{
					AxisMap[AnalogAxisId::Move].x = abs(scaledValue);
				}
				else if (!TestBoundKey(usedKeyID))
				{
					if ((axis % 2) == 0)
					{
						AxisMap[AnalogAxisId::Camera].y = normalizedValue;
					}
					else
					{
						AxisMap[AnalogAxisId::Camera].x = normalizedValue;
					}
				}
			}

			// Poll POVs.
			// NOTE: Controllers usually have one, but scan all just in case.
			for (int pov = 0; pov < GAMEPAD_POV_AXIS_COUNT; pov++)
			{
				if (state.mPOV[pov].direction == OIS::Pov::Centered)
					continue;

				// Register multiple directional keypresses mapped to analog axes.
				int baseKeyID = (KEY_OFFSET_GAMEPAD + GAMEPAD_BUTTON_COUNT) + (GAMEPAD_AXIS_COUNT * 2);
				for (int pass = 0; pass < GAMEPAD_POV_AXIS_COUNT; pass++)
				{
					int keyID = (KEY_OFFSET_GAMEPAD + GAMEPAD_BUTTON_COUNT) + (GAMEPAD_AXIS_COUNT * 2);

					switch (pass)
					{
					// D-Pad Up
					case 0:
						if ((state.mPOV[pov].direction & OIS::Pov::North) == 0)
							continue;
						break;

					// D-Pad Down
					case 1:
						if ((state.mPOV[pov].direction & OIS::Pov::South) == 0)
							continue;
						break;

					// D-Pad Left
					case 2:
						if ((state.mPOV[pov].direction & OIS::Pov::West) == 0)
							continue;
						break;

					// D-Pad Right
					case 3:
						if ((state.mPOV[pov].direction & OIS::Pov::East) == 0)
							continue;
						break;
					}

					keyID += pass;
					KeyMap[keyID] = 1.0f;
					SetDiscreteAxisValues(keyID);
				}
			}
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll game controller input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}

	static float Key(ActionId actionId)
	{
		int keyID = OIS::KC_UNASSIGNED;
		for (int i = (int)BindingProfileId::Count - 1; i >= 0; i--)
		{
			auto profileId = (BindingProfileId)i;
			if (profileId == BindingProfileId::Default && g_Bindings.TestConflict(actionId))
				continue;

			int newKeyID = g_Bindings.GetBoundEventIds(profileId, actionId);
			if (KeyMap[newKeyID] != 0.0f)
			{
				keyID = newKeyID;
				break;
			}
		}

		return KeyMap[keyID];
	}

	void SolveActionCollisions()
	{
		// Block simultaneous Left+Right actions.
		if (IsHeld(In::Left) && IsHeld(In::Right))
		{
			ClearAction(In::Left);
			ClearAction(In::Right);
		}
	}

	// TODO
	static void UpdateRumble()
	{
		if (!OisRumble || !OisEffect || !RumbleInfo.Power)
			return;

		RumbleInfo.Power -= RumbleInfo.FadeSpeed;

		// Don't update effect too frequently if its value hasn't changed much.
		if (RumbleInfo.Power >= 0.2f && (RumbleInfo.LastPower - RumbleInfo.Power) < 0.1f)
			return;

		if (RumbleInfo.Power <= 0.0f)
		{
			StopRumble();
			return;
		}

		try
		{
			auto& force = *dynamic_cast<OIS::ConstantEffect*>(OisEffect->getForceEffect());
			force.level = RumbleInfo.Power * 10000;

			switch (RumbleInfo.Mode)
			{
			case RumbleMode::Left:
				OisEffect->direction = OIS::Effect::EDirection::West;
				break;

			case RumbleMode::Right:
				OisEffect->direction = OIS::Effect::EDirection::East;
				break;

			case RumbleMode::Both:
				OisEffect->direction = OIS::Effect::EDirection::North;
				break;
			}

			OisRumble->upload(OisEffect);
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Error updating vibration effect: " + std::string(ex.eText), LogLevel::Error);
		}

		RumbleInfo.LastPower = RumbleInfo.Power;
	}

	// TODO
	void UpdateInputActions(bool allowAsyncUpdate, bool applyQueue)
	{
		// Don't update input data during frameskip.
		if (allowAsyncUpdate || !g_Synchronizer.Locked())
		{
			UpdateRumble();
			ReadKeyboard();
			ReadMouse();
			ReadGamepad();
		}

		DefaultConflict();

		// Update action map.
		for (auto& [actionId, action] : ActionMap)
			action.Update(Key(action.GetID()));

		if (applyQueue)
			ApplyActionQueue();

		// Additional handling.
		HandleHotkeyActions();
		SolveActionCollisions();
	}

	// TODO
	void ClearAllActions()
	{
		for (auto& [actionId, action] : ActionMap)
			action.Clear();

		for (auto& [actionId, queue] : ActionQueueMap)
			queue = ActionQueueState::None;
	}

	void Rumble(float power, float delaySec, RumbleMode mode)
	{
		// TODO
		/*if (!g_Configuration.EnableRumble)
			return;

		power = std::clamp(power, 0.0f, 1.0f);

		if (power == 0.0f || RumbleInfo.Power)
			return;

		RumbleInfo.FadeSpeed = power / (delaySec * FPS);
		RumbleInfo.Power = power + RumbleInfo.FadeSpeed;
		RumbleInfo.LastPower = RumbleInfo.Power;*/
	}

	void StopRumble()
	{
		g_Input.StopRumble();
	}

	// TODO
	void ApplyDefaultBindings()
	{
		g_Bindings.SetBindingProfile(BindingProfileId::Custom, DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE);
	}

	void ClearAction(ActionId actionId)
	{
		g_Input.ClearAction(actionId);
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

	bool IsReleased(ActionId actionId, float maxDelaySec)
	{
		return g_Input.GetAction(actionId).IsReleased(maxDelaySec);
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

	const Vector2& GetMoveAxis()
	{
		return g_Input.GetAnalogAxis(AnalogAxisId2::Move);
	}

	const Vector2& GetCameraAxis()
	{
		return g_Input.GetAnalogAxis(AnalogAxisId2::Camera);
	}

	const Vector2& GetMouseAxis()
	{
		return g_Input.GetAnalogAxis(AnalogAxisId2::Mouse);
	}

	Vector2 GetMouse2DPosition()
	{
		return g_Input.GetCursorPosition();
	}

	// ====================================================================================================================

	InputManager g_Input = InputManager();

	const Action& InputManager::GetAction(ActionId actionId) const
	{
		return _actions[(int)actionId];
	}

	const Vector2& InputManager::GetAnalogAxis(AnalogAxisId2 axisId) const
	{
		return _analogAxes[(int)axisId];
	}

	const Vector2& InputManager::GetCursorPosition() const
	{
		return _states.CursorPosition;
	}

	GamepadVendorId InputManager::GetGamepadVendorId() const
	{
		return _gamepad.VendorId;
	}

	void InputManager::SetRumble(RumbleMode2 mode, float intensityFrom, float intensityTo, float durationSec)
	{
		_rumble.Mode = mode;
		_rumble.IntensityFrom = intensityFrom;
		_rumble.IntensityTo = intensityTo;
		_rumble.DurationTicks =
		_rumble.GameFrames = SecToGameFrames(durationSec);
	}

	bool InputManager::IsGamepadConnected() const
	{
		return _gamepad.Id != NO_VALUE && _gamepad.Device != nullptr;
	}

	bool InputManager::IsUsingGamepad() const
	{
		return _states.IsUsingGamepad;
	}

	void InputManager::Initialize()
	{
		if (!SDL_Init(SDL_INIT_GAMEPAD))
		{
			TENLog(fmt::format("Failed to initialize gamepad subsystem: {}", SDL_GetError()), Debug::LogLevel::Error);
		}

		// Initialize actions.
		_actions.reserve((int)ActionId::Count);
		for (int i = 0; i < (int)ActionId::Count; i++)
		{
			auto actionId = (ActionId)i;
			_actions.push_back(Action(actionId));
			_actionQueues.push_back(ActionQueueState::None);
		}

		// Initialize bindings.
		_bindings.Initialize(options->KeyboardMouseBindings, options->GamepadBindings);

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

	void InputManager::Update(SDL_Window& window, const Vector2& mouseWheelAxis)
	{
		// Capture event states asynchronously.
		auto tasks = ParallelTasks
		{
			TASK(ReadKeyboard()),
			TASK(ReadMouse(window, mouseWheelAxis)),
			TASK(ReadGamepad())
		};
		g_Parallel.AddTasks(tasks).wait();

		// Update "using gamepad" state.
		if (_states.HasKeyboardInput || _states.HasMouseInput)
		{
			_states.IsUsingGamepad = false;
		}
		else if (_states.HasGamepadInput)
		{
			_states.IsUsingGamepad = true;
		}

		// Update components.
		UpdateRumble();
		UpdateActions();
		HandleHotkeyActions();

		// Clear data.
		_states.HasKeyboardInput = false;
		_states.HasMouseInput = false;
		_states.HasGamepadInput = false;
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

			SetRumble(RumbleMode2::Low, 0.0f, 1.0f, 0.1f);

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

		// Disconnect with toast.
		_gamepad = {};
		SDL_CloseGamepad(_gamepad.Device);

		TENLog("Gamepad disconnected.");
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

	void InputManager::UpdateActions()
	{
		// 1) Update user action states.
		auto updateUserActions = [&]()
		{
			// Get binding profiles.
			const auto& userProfile = _bindings.GetBindingProfile(IsGamepadConnected() ? BindingProfileId::CustomGamepad: BindingProfileId::CustomKeyboardMouse);
			const auto& defaultProfile = _bindings.GetBindingProfile(IsGamepadConnected() ? BindingProfileId::DefaultGamepad : BindingProfileId::DefaultKeyboardMouse);

			for (auto actionGroupId : USER_ACTION_GROUP_IDS)
			{
				const auto& actionIds = ACTION_ID_GROUPS[(int)actionGroupId];
				for (auto actionId : actionIds)
				{
					auto& action = _actions[(int)actionId];
					float state = 0.0f;

					// Apply user-defined bound event state to action.
					const auto& userEventIds = userProfile.at(actionId);
					for (const auto& eventId : userEventIds)
					{
						state = std::max(state, _states.Events[(int)eventId]);
					}

					// TODO: Handle conflicts between user and default.
					const auto& defaultEventIds = defaultProfile.at(actionId);

					action.Update(state);
				}
			}
		};

		// 2) Update raw action states.
		auto updateRawActions = [&]()
		{
			for (auto profileId : RAW_EVENT_BINDING_PROFILE_IDS)
			{
				const auto& profile = _bindings.GetBindingProfile(profileId);
				for (auto& [keyActionId, eventIds] : profile)
				{
					auto& action = _actions[(int)keyActionId];
					float state = 0.0f;

					for (auto eventId : eventIds)
					{
						state = std::max(state, _states.Events[(int)eventId]);
					}

					// Use max bound event state.
					action.Update(state);
				}
			}
		};

		// Apply action queues.
		for (int i = 0; i < (int)ActionId::Count; i++)
		{
			auto actionId = (ActionId)i;
			switch (ActionQueueMap[actionId])
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

		// Clear action queues.
		for (auto& queue : _actionQueues)
			queue = ActionQueueState::None;

		// Update action states asynchronously.
		auto tasks = ParallelTasks
		{
			TASK(updateUserActions()),
			TASK(updateRawActions())
		};
		g_Parallel.AddTasks(tasks).wait();
	}

	void InputManager::UpdateRumble()
	{
		if (_rumble.GameFrames == 0 || !IsGamepadConnected())
		{
			_rumble = {};
			return;
		}

		// Compute intensity.
		float alpha = (float)_rumble.GameFrames / (float)_rumble.DurationTicks;
		float intensity = Lerp(_rumble.IntensityFrom, _rumble.IntensityTo, alpha);

		// Compute frequencies.
		unsigned short freqLow = (_rumble.Mode == RumbleMode2::Low || _rumble.Mode == RumbleMode2::LowAndHigh) ? (unsigned short)(intensity * USHRT_MAX) : 0;
		unsigned short freqHigh = (_rumble.Mode == RumbleMode2::High || _rumble.Mode == RumbleMode2::LowAndHigh) ? (unsigned short)(intensity * USHRT_MAX) : 0;

		// Compute duration.
		unsigned int durationMs = (unsigned int)round(GameFramesToSec(_rumble.DurationTicks) * 1000);

		// Rumble gamepad.
		if (!SDL_RumbleGamepad(_gamepad.Device, freqLow, freqHigh, durationMs))
		{
			TENLog(fmt::format("Failed to rumble gamepad: {}", SDL_GetError()), Debug::LogLevel::Error);
		}

		_rumble.GameFrames--;
	}

	void InputManager::ReadKeyboard()
	{
		int eventIdx = (int)START_KEYBOARD_EVENT_ID;

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
					_states.HasKeyboardInput = true;
				}

				_states.Events[eventIdx] = state ? 1.0f : 0.0f;
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
				_states.HasKeyboardInput = true;
			}

			_states.Events[eventIdx] = state ? 1.0f : 0.0f;
			eventIdx++;
		}

		// TODO
		SetDiscreteAxisValues();
	}

	void InputManager::ReadMouse(SDL_Window& window, const Vector2& wheelAxis)
	{
		constexpr int AXIS_COUNT = 2;

		int eventIdx = (int)START_MOUSE_EVENT_ID;

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
				_states.HasMouseInput = true;
			}

			_states.Events[eventIdx] = state ? 1.0f : 0.0f;
			eventIdx++;
		}

		if (wheelAxis != Vector2::Zero)
		{
			_states.HasMouseInput = true;
		}

		// TODO: Investigate. Unclear how SDL3 mouse wheel values work.
		// Set mouse scroll event states.
		_states.Events[eventIdx] = (wheelAxis.x < 0.0f) ? std::clamp(abs(wheelAxis.x), 0.0f, 1.0f) : 0.0f;
		_states.Events[eventIdx + 1] = (wheelAxis.x > 0.0f) ? std::clamp(abs(wheelAxis.x), 0.0f, 1.0f) : 0.0f;
		_states.Events[eventIdx + 2] = (wheelAxis.y < 0.0f) ? std::clamp(abs(wheelAxis.y), 0.0f, 1.0f) : 0.0f;
		_states.Events[eventIdx + 3] = (wheelAxis.y > 0.0f) ? std::clamp(abs(wheelAxis.y), 0.0f, 1.0f) : 0.0f;
		eventIdx += SQUARE(AXIS_COUNT);

		// Set cursor position state.
		_states.PrevCursorPosition = _states.CursorPosition;
		_states.CursorPosition = pos;

		auto res = Vector2i::Zero;
		if (!SDL_GetWindowSize(&window, &res.x, &res.y))
		{
			TENLog(fmt::format("Failed to get window size: {}", SDL_GetError()), Debug::LogLevel::Error);
		}

		float sensitivity = (g_Configuration.MouseSensitivity * 0.1f) + 0.4f;
		auto moveAxis = (((_states.CursorPosition - _states.PrevCursorPosition) / DISPLAY_SPACE_RES) * (res.ToVector2() / DISPLAY_SPACE_RES)) * sensitivity;
		if (moveAxis != Vector2::Zero)
		{
			_states.HasMouseInput = true;
		}

		// Set mouse movement event states.
		_states.Events[eventIdx] = (moveAxis.x < 0.0f) ? abs(moveAxis.x) : 0.0f;
		_states.Events[eventIdx + 1] = (moveAxis.x > 0.0f) ? abs(moveAxis.x) : 0.0f;
		_states.Events[eventIdx + 2] = (moveAxis.y < 0.0f) ? abs(moveAxis.y) : 0.0f;
		_states.Events[eventIdx + 3] = (moveAxis.y > 0.0f) ? abs(moveAxis.y) : 0.0f;
		eventIdx += SQUARE(AXIS_COUNT);

		// Set camera axis. Right gamepad stick takes priority over mouse.
		_analogAxes[(int)AnalogAxisId::Camera] = moveAxis;

		// Set raw mouse axis.
		_analogAxes[(int)AnalogAxisId::Mouse] = moveAxis;
	}

	void InputManager::ReadGamepad()
	{
		constexpr int   AXIS_COUNT    = 2;
		constexpr float AXIS_DEADZONE = ((float)SHRT_MAX / 8.0f) / (float)SHRT_MAX;

		int eventIdx = (int)START_GAMEPAD_EVENT_ID;

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
				_states.HasGamepadInput = true;
			}

			_states.Events[eventIdx] = state ? 1.0f : 0.0f;
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

				// TODO: Adapt for TEN-specific axis scaling.
				// Remap axis to active range.
				if (axis.Length() >= AXIS_DEADZONE)
				{
					float remappedLength = Remap(axis.Length(), AXIS_DEADZONE, 1.0f, 0.0f, 1.0f);

					axis = axis;
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

		// Set gamepad stick axis event states and control axes.
		for (int i = 0; i < stickAxes.size(); i++)
		{
			const auto& axis = stickAxes[i];
			if (axis != Vector2::Zero)
			{
				_states.HasGamepadInput = true;
			}

			_states.Events[eventIdx + i] = (axis.x < 0.0f) ? abs(axis.x) : 0.0f;
			_states.Events[eventIdx + (i + 1)] = (axis.x > 0.0f) ? abs(axis.x) : 0.0f;
			_states.Events[eventIdx + (i + 2)] = (axis.y < 0.0f) ? abs(axis.y) : 0.0f;
			_states.Events[eventIdx + (i + 3)] = (axis.y > 0.0f) ? abs(axis.y) : 0.0f;
			_analogAxes[i] = axis;
			eventIdx += AXIS_COUNT * 2;
		}

		// Set camera axis. Right gamepad stick takes priority over mouse.
		if (stickAxes.back() != Vector2::Zero)
		{
			_analogAxes[(int)AnalogAxisId::Camera] = stickAxes.back();
		}

		// Set raw gamepad stick axes.
		_analogAxes[(int)AnalogAxisId2::StickLeft] = stickAxes.front();
		_analogAxes[(int)AnalogAxisId2::StickRight] = stickAxes.back();

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
				_states.HasGamepadInput = true;
			}

			_states.Events[eventIdx] = state;
			eventIdx++;
		}
	}

	void InputManager::HandleHotkeyActions()
	{
		// Save screenshot.
		static bool dbScreenshot = true;
		if ((_states.Events[(int)EventId::PrintScreen] || _states.Events[(int)EventId::F12]) && dbScreenshot)
			g_Renderer.SaveScreenshot();
		dbScreenshot = !(_states.Events[(int)EventId::PrintScreen] || _states.Events[(int)EventId::F12]);

		// Toggle fullscreen.
		static bool dbFullscreen = true;
		if ((_states.Events[(int)EventId::Alt] && _states.Events[(int)EventId::Return]) && dbFullscreen)
		{
			g_Configuration.EnableWindowedMode = !g_Configuration.EnableWindowedMode;
			SaveConfiguration();
			g_Renderer.ToggleFullScreen();
		}
		dbFullscreen = !(_states.Events[(int)EventId::Alt] && _states.Events[(int)EventId::Return]);

		if (!DebugMode)
			return;

		// Switch debug page.
		static bool dbDebugPage = true;
		if ((_states.Events[(int)EventId::F10] || KeyMap[OIS::KC_F11]) && dbDebugPage)
			g_Renderer.SwitchDebugPage(_states.Events[(int)EventId::F10]);
		dbDebugPage = !(_states.Events[(int)EventId::F10] || KeyMap[OIS::KC_F11]);

		// Reload shaders.
		static bool dbReloadShaders = true;
		if (_states.Events[(int)EventId::F9] && dbReloadShaders)
			g_Renderer.ReloadShaders();
		dbReloadShaders = !_states.Events[(int)EventId::F9];
	}
}
