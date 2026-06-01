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
#include "Specific/trutils.h"

using namespace TEN::Gui;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

namespace TEN::Input
{
	constexpr auto AXIS_SCALE    = 1.5f;
	constexpr auto AXIS_OFFSET   = 0.2f;
	constexpr auto AXIS_DEADZONE = 8000;

    // Globals.
    RumbleData                                     RumbleInfo = {};
    std::unordered_map<int, float>				   KeyMap;			// Key = key ID, value = key value.
    std::unordered_map<ActionID, Action>		   ActionMap;		// Key = action ID, value = action.
    std::unordered_map<ActionID, ActionQueueState> ActionQueueMap;	// Key = action ID, value = action queue state.
    std::unordered_map<AxisID, Vector2>			   AxisMap;			// Key = axis ID, value = axis.

	// Input state.
	bool InputLocked = false; // Disables control polling in case application is defocused.
	InputDevice	LastInputDevice = InputDevice::Keyboard;
	float MouseWheelAccumY = 0.0f; // Mouse wheel deltas are event-only in SDL3, so accumulate them between frames.

	// SDL3 gamepad state.
	SDL_Gamepad* ActiveGamepad   = nullptr;
	SDL_JoystickID ActiveGamepadId = 0;
	bool ActiveGamepadHasRumble = false;

	static bool HasMatchingBindings(const BindingProfile& bindingProfile)
	{
		for (const auto& [actionID, keyID] : bindingProfile)
		{
			int userKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Custom, actionID);
			if (userKeyID != KEY_UNASSIGNED && userKeyID != keyID)
				return false;
		}

		return true;
	}

	static bool RestoreDefaultBindings(DefaultBindingType bindingType, bool force)
	{
		const BindingProfile* previousBindings = nullptr;
		const BindingProfile* defaultBindings = nullptr;
		bool enableRumble = false;
		bool enableThumbstickCamera = false;

		switch (bindingType)
		{
		case DefaultBindingType::KeyboardMouse:
			if (!force && ActiveGamepad != nullptr)
				return false;

			previousBindings = &DEFAULT_GAMEPAD_BINDING_PROFILE;
			defaultBindings = &DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;
			break;

		case DefaultBindingType::Gamepad:
			if (ActiveGamepad == nullptr)
				return false;

			previousBindings = &DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;
			defaultBindings = &DEFAULT_GAMEPAD_BINDING_PROFILE;
			enableRumble = ActiveGamepadHasRumble;
			enableThumbstickCamera = true;
			break;
		}

		if (!force && !HasMatchingBindings(*previousBindings))
			return false;

		// Apply the selected default binding profile.
		g_Bindings.SetBindingProfile(BindingProfileID::Custom, *defaultBindings);
		g_Configuration.EnableRumble = enableRumble;
		g_Configuration.EnableThumbstickCamera = enableThumbstickCamera;

		g_Configuration.Bindings = g_Bindings.GetBindingProfile(BindingProfileID::Custom);
		return true;
	}

	static void OpenFirstGamepad()
	{
		if (ActiveGamepad != nullptr)
			return;

		int count = 0;
		auto* gamepads = SDL_GetGamepads(&count);
		if (gamepads == nullptr || count <= 0)
		{
			SDL_free(gamepads);
			return;
		}

		ActiveGamepadId = gamepads[0];
		ActiveGamepad = SDL_OpenGamepad(ActiveGamepadId);
		SDL_free(gamepads);

		if (ActiveGamepad == nullptr)
		{
			TENLog(std::string("Failed to open gamepad: ") + SDL_GetError(), LogLevel::Warning);
			ActiveGamepadId = 0;
			return;
		}

		auto* gamepadName = SDL_GetGamepadName(ActiveGamepad);
		TENLog(std::string("Using '") + (gamepadName ? gamepadName : "unknown") + "' gamepad for input.", LogLevel::Info);

		auto previousGamepadType = g_Configuration.LastGamepadType;
		g_Configuration.LastGamepadType = GetGamepadType();
		bool gamepadTypeChanged = (g_Configuration.LastGamepadType != previousGamepadType);

		// SDL3 reports rumble support via SDL_GetGamepadProperties.
		auto props = SDL_GetGamepadProperties(ActiveGamepad);
		ActiveGamepadHasRumble = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
		if (ActiveGamepadHasRumble)
			TENLog("Controller supports vibration.", LogLevel::Info);

		// If the user is on default keyboard/mouse bindings and this controller matches
		// a supported Xbox/PlayStation/Switch Pro style layout, swap to gamepad defaults.
		if (RestoreDefaultBindings(DefaultBindingType::Gamepad) || gamepadTypeChanged)
			SaveConfiguration();
	}

	static void CloseGamepadIfMatches(SDL_JoystickID id)
	{
		if (ActiveGamepad == nullptr || ActiveGamepadId != id)
			return;

		SDL_CloseGamepad(ActiveGamepad);
		ActiveGamepad = nullptr;
		ActiveGamepadId = 0;
		ActiveGamepadHasRumble = false;

		TENLog("Gamepad disconnected.", LogLevel::Info);
		OpenFirstGamepad();

		if (ActiveGamepad == nullptr)
		{
			if (RestoreDefaultBindings(DefaultBindingType::KeyboardMouse))
				SaveConfiguration();
		}
	}

	void InitializeInput()
	{
		TENLog("Initializing input system...", LogLevel::Info);

		RumbleInfo = {};
		LastInputDevice = InputDevice::Keyboard;
		MouseWheelAccumY = 0.0f;

		// Initialize key map.
		for (int i = 0; i < KEY_COUNT; i++)
			KeyMap[i] = 0.0f;

		// Initialize action and action queue maps.
		for (int i = 0; i < (int)ActionID::Count; i++)
		{
			auto actionID = (ActionID)i;
			ActionMap[actionID] = Action(actionID);
			ActionQueueMap[actionID] = ActionQueueState::None;
		}

		// Initialize axis map.
		for (int i = 0; i < (int)AxisID::Count; i++)
		{
			auto axisID = (AxisID)i;
			AxisMap[axisID] = Vector2::Zero;
		}

		// Make sure SDL gamepad subsystem is running. SDL_Init in EngineMain does not include it,
		// so initialize it lazily here. SDL_INIT_GAMEPAD pulls in SDL_INIT_JOYSTICK + SDL_INIT_EVENTS.
		if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
		{
			TENLog(std::string("Failed to initialize SDL gamepad subsystem: ") + SDL_GetError(), LogLevel::Warning);
			return;
		}

		// Open whatever gamepad is connected at startup. Subsequent connections arrive via
		// SDL_EVENT_GAMEPAD_ADDED through HandleSDLEvent.
		OpenFirstGamepad();
	}

	void DeinitializeInput()
	{
		TENLog("Shutting down input system...", LogLevel::Info);

		if (ActiveGamepad != nullptr)
		{
			SDL_CloseGamepad(ActiveGamepad);
			ActiveGamepad = nullptr;
		}

		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	}

	void HandleSDLEvent(const SDL_Event& event)
	{
		switch (event.type)
		{
		case SDL_EVENT_MOUSE_WHEEL:
			MouseWheelAccumY += event.wheel.y;
			break;

		case SDL_EVENT_GAMEPAD_ADDED:
			if (ActiveGamepad == nullptr)
				OpenFirstGamepad();
			break;

		case SDL_EVENT_GAMEPAD_REMOVED:
			CloseGamepadIfMatches(event.gdevice.which);
			break;
		}
	}

	void SetInputLockState(bool locked)
	{
		InputLocked = locked;
	}

	GamepadType GetGamepadType()
	{
		if (ActiveGamepad == nullptr)
			return g_Configuration.LastGamepadType;

		switch (SDL_GetGamepadType(ActiveGamepad))
		{
		case SDL_GAMEPAD_TYPE_PS3:
		case SDL_GAMEPAD_TYPE_PS4:
		case SDL_GAMEPAD_TYPE_PS5:
			return GamepadType::PlayStation;

		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
			return GamepadType::Switch;

		default:
			return GamepadType::Xbox;
		}
	}

	void ClearInputData()
	{
		for (auto& [keyID, value] : KeyMap)
			value = 0.0f;

		for (auto& [axisID, axis] : AxisMap)
			axis = Vector2::Zero;
	}

	void ApplyActionQueue()
	{
		for (int i = 0; i < (int)ActionID::Count; i++)
		{
			auto actionID = (ActionID)i;
			switch (ActionQueueMap[actionID])
			{
			default:
			case ActionQueueState::None:
				break;

			case ActionQueueState::Update:
				ActionMap[actionID].Update(true);
				break;

			case ActionQueueState::Clear:
				ActionMap[actionID].Clear();
				break;
			}
		}

		for (auto& [actionID, queue] : ActionQueueMap)
			queue = ActionQueueState::None;
	}

	static bool TestBoundKey(int keyID)
	{
		for (int i = 1; i >= 0; i--)
		{
			auto profileID = (BindingProfileID)i;
			for (int j = 0; j < (int)ActionID::Count; j++)
			{
				auto actionID = (ActionID)j;
				if (g_Bindings.GetBoundKeyID(profileID, actionID) == keyID)
					return true;
			}
		}

		return false;
	}

	// Merge left/right Ctrl, Shift, and Alt scancodes onto the right-hand variant
	// so default bindings (which use the right-hand keys) match either side.
	static int WrapSimilarKeys(int source)
	{
		switch (source)
		{
		case SDL_SCANCODE_LCTRL:  return SDL_SCANCODE_RCTRL;
		case SDL_SCANCODE_LSHIFT: return SDL_SCANCODE_RSHIFT;
		case SDL_SCANCODE_LALT:   return SDL_SCANCODE_RALT;
		}

		return source;
	}

	void DefaultConflict()
	{
		for (const auto& actionIDGroup : ACTION_ID_GROUPS)
		{
			for (auto actionID : actionIDGroup)
			{
				g_Bindings.SetConflict(actionID, false);

				int key = g_Bindings.GetBoundKeyID(BindingProfileID::Default, actionID);
				for (auto conflictActionID : actionIDGroup)
				{
					if (key != g_Bindings.GetBoundKeyID(BindingProfileID::Custom, conflictActionID))
						continue;

					g_Bindings.SetConflict(actionID, true);
					break;
				}
			}
		}
	}

	static void SetDiscreteAxisValues(unsigned int keyID)
	{
		for (int i = 0; i < (int)BindingProfileID::Count; i++)
		{
			auto profileID = (BindingProfileID)i;
			if (g_Bindings.GetBoundKeyID(profileID, In::Forward) == keyID)
			{
				AxisMap[AxisID::Move].y = 1.0f;
			}
			else if (g_Bindings.GetBoundKeyID(profileID, In::Back) == keyID)
			{
				AxisMap[AxisID::Move].y = -1.0f;
			}
			else if (g_Bindings.GetBoundKeyID(profileID, In::Left) == keyID)
			{
				AxisMap[AxisID::Move].x = -1.0f;
			}
			else if (g_Bindings.GetBoundKeyID(profileID, In::Right) == keyID)
			{
				AxisMap[AxisID::Move].x = 1.0f;
			}
		}
	}

	static void ReadKeyboard()
	{
		if (InputLocked)
			return;

		int numKeys = 0;
		const bool* state = SDL_GetKeyboardState(&numKeys);
		if (state == nullptr)
			return;

		// Poll keyboard keys.
		int limit = std::min(numKeys, KEYBOARD_KEY_COUNT);
		for (int sc = 0; sc < limit; sc++)
		{
			if (!state[sc])
				continue;

			int key = WrapSimilarKeys(sc);
			KeyMap[key] = 1.0f;

			// Interpret discrete directional keypresses as analog axis values.
			SetDiscreteAxisValues(key);
		}
	}

	static void ReadMouse()
	{
		if (InputLocked)
			return;

		// Buttons. SDL3 mouse mask uses SDL_BUTTON_LMASK/MMASK/RMASK/X1MASK/X2MASK.
		float absX = 0.0f, absY = 0.0f;
		auto mask = SDL_GetMouseState(&absX, &absY);

		KeyMap[MK_LCLICK]   = (mask & SDL_BUTTON_LMASK)  ? 1.0f : 0.0f;
		KeyMap[MK_RCLICK]   = (mask & SDL_BUTTON_RMASK)  ? 1.0f : 0.0f;
		KeyMap[MK_MCLICK]   = (mask & SDL_BUTTON_MMASK)  ? 1.0f : 0.0f;
		KeyMap[MK_BUTTON_4] = (mask & SDL_BUTTON_X1MASK) ? 1.0f : 0.0f;
		KeyMap[MK_BUTTON_5] = (mask & SDL_BUTTON_X2MASK) ? 1.0f : 0.0f;
		// Buttons 6-8 are not exposed by SDL3's basic state mask.

		// Relative motion since last call.
		float relX = 0.0f, relY = 0.0f;
		SDL_GetRelativeMouseState(&relX, &relY);

		// Axis-as-button slots.
		if (relX < 0) KeyMap[MK_AXIS_X_NEG] = 1.0f;
		if (relX > 0) KeyMap[MK_AXIS_X_POS] = 1.0f;
		if (relY < 0) KeyMap[MK_AXIS_Y_NEG] = 1.0f;
		if (relY > 0) KeyMap[MK_AXIS_Y_POS] = 1.0f;

		// Mouse wheel: pop accumulator filled by HandleSDLEvent.
		if (MouseWheelAccumY > 0)
			KeyMap[MK_AXIS_Z_NEG] = 1.0f;
		else if (MouseWheelAccumY < 0)
			KeyMap[MK_AXIS_Z_POS] = 1.0f;
		MouseWheelAccumY = 0.0f;

		for (int slot = MK_AXIS_X_NEG; slot <= MK_AXIS_Z_POS; slot++)
		{
			if (KeyMap[slot] != 0.0f)
				SetDiscreteAxisValues(slot);
		}

		// Normalized [-1, 1] mouse axis with sensitivity, used as analog input.
		auto rawAxes = Vector2(relX, relY);
		auto normAxes = Vector2(
			(((rawAxes.x - -DISPLAY_SPACE_RES.x) * 2.0f) / float(DISPLAY_SPACE_RES.x - -DISPLAY_SPACE_RES.x)) - 1.0f,
			(((rawAxes.y - -DISPLAY_SPACE_RES.y) * 2.0f) / float(DISPLAY_SPACE_RES.y - -DISPLAY_SPACE_RES.y)) - 1.0f);

		// Apply sensitivity.
		float sensitivity = (g_Configuration.MouseSensitivity * 0.1f) + 0.4f;
		normAxes *= sensitivity;

		// Set mouse axis values.
		AxisMap[AxisID::Mouse] = normAxes;
	}

	static void ReadGamepad()
	{
		if (InputLocked || ActiveGamepad == nullptr)
			return;

		// Buttons.
		for (int b = 0; b < GAMEPAD_BUTTON_COUNT; b++)
		{
			bool pressed = SDL_GetGamepadButton(ActiveGamepad, (SDL_GamepadButton)b);
			int keyID = KEY_OFFSET_GAMEPAD + b;
			KeyMap[keyID] = pressed ? 1.0f : 0.0f;

			if (pressed)
				SetDiscreteAxisValues(keyID);
		}

		// Axes.
		for (int axis = 0; axis < GAMEPAD_AXIS_COUNT; axis++)
		{
			auto raw = SDL_GetGamepadAxis(ActiveGamepad, (SDL_GamepadAxis)axis);

			float normalizedValue = 0.0f;
			if (std::abs((int)raw) >= AXIS_DEADZONE)
				normalizedValue = std::clamp((float)(raw + (raw > 0 ? -AXIS_DEADZONE : AXIS_DEADZONE)) / (float)(SHRT_MAX - AXIS_DEADZONE), -1.0f, 1.0f);

			float absNormalizedValue = std::abs(normalizedValue);
			if (absNormalizedValue <= EPSILON)
				continue;

			switch ((SDL_GamepadAxis)axis)
			{
			case SDL_GAMEPAD_AXIS_LEFTX:
				AxisMap[AxisID::StickLeft].x = normalizedValue;
				break;

			case SDL_GAMEPAD_AXIS_LEFTY:
				AxisMap[AxisID::StickLeft].y = normalizedValue;
				break;

			case SDL_GAMEPAD_AXIS_RIGHTX:
				AxisMap[AxisID::StickRight].x = normalizedValue;
				break;

			case SDL_GAMEPAD_AXIS_RIGHTY:
				AxisMap[AxisID::StickRight].y = normalizedValue;
				break;

			default:
				break;
			}

			// Register analog input in certain direction.
			// If axis is bound as directional controls, register axis as directional input.
			// Otherwise, register as camera movement input (for future).
			// NOTE: `abs()` operations are needed to avoid issues with inverted axes on different controllers.

			float scaledValue = (absNormalizedValue * AXIS_SCALE) + AXIS_OFFSET;

			int negKeyID = KEY_OFFSET_GAMEPAD_AXIS + (axis * 2);
			int posKeyID = negKeyID + 1;
			KeyMap[negKeyID] = (normalizedValue < 0) ? std::abs(normalizedValue) : 0.0f;
			KeyMap[posKeyID] = (normalizedValue > 0) ? std::abs(normalizedValue) : 0.0f;

			int usedKeyID = (normalizedValue < 0) ? negKeyID : posKeyID;

			if (g_Bindings.GetBoundKeyID(BindingProfileID::Custom, In::Forward) == usedKeyID)
			{
				AxisMap[AxisID::Move].y = std::abs(scaledValue);
			}
			else if (g_Bindings.GetBoundKeyID(BindingProfileID::Custom, In::Back) == usedKeyID)
			{
				AxisMap[AxisID::Move].y = -std::abs(scaledValue);
			}
			else if (g_Bindings.GetBoundKeyID(BindingProfileID::Custom, In::Left) == usedKeyID)
			{
				AxisMap[AxisID::Move].x = -std::abs(scaledValue);
			}
			else if (g_Bindings.GetBoundKeyID(BindingProfileID::Custom, In::Right) == usedKeyID)
			{
				AxisMap[AxisID::Move].x = std::abs(scaledValue);
			}
			else if (!TestBoundKey(usedKeyID))
			{
				// g_Camera fallback: even axes drive Y, odd axes drive X (matches develop behavior).
				if ((axis % 2) == 0)
					AxisMap[AxisID::Camera].x = normalizedValue;
				else
					AxisMap[AxisID::Camera].y = normalizedValue;
			}
		}
	}

	static void UpdateLastInputDevice(ActionID actionID, int keyID)
	{
		// Detect whether specified action ID corresponds to a particular raw input action ID, 
		// and if it does, record corresponding input device as the last used device.

		for (auto userActionGroupID : USER_ACTION_GROUP_IDS)
		{
			const auto& userActionGroup = ACTION_ID_GROUPS[(int)userActionGroupID];
			if (!Contains(userActionGroup, actionID))
				continue;

			for (auto rawActionGroupID : RAW_ACTION_GROUP_IDS)
			{
				const auto& rawActionGroup = ACTION_ID_GROUPS[(int)rawActionGroupID];
				for (auto rawActionID : rawActionGroup)
				{
					if (g_Bindings.GetBoundKeyID(BindingProfileID::Raw, rawActionID) != keyID)
						continue;

					switch (rawActionGroupID)
					{
					case ActionGroupID::Keyboard:
						LastInputDevice = InputDevice::Keyboard;
						return;

					case ActionGroupID::Mouse:
						LastInputDevice = InputDevice::Mouse;
						return;

					case ActionGroupID::Gamepad:
						LastInputDevice = InputDevice::Gamepad;
						return;

					default:
						break;
					}
				}
			}
		}
	}

	static float Key(ActionID actionID)
	{
		// Hard-wired directional inputs for menu navigation, unaffected by user bindings.
		switch (actionID)
		{
		case In::MenuUp:
			return std::max({ KeyMap[SDL_SCANCODE_UP], KeyMap[GK_DPAD_UP], KeyMap[GK_LSTICK_Y_NEG] });

		case In::MenuDown:
			return std::max({ KeyMap[SDL_SCANCODE_DOWN], KeyMap[GK_DPAD_DOWN], KeyMap[GK_LSTICK_Y_POS] });

		case In::MenuLeft:
			return std::max({ KeyMap[SDL_SCANCODE_LEFT], KeyMap[GK_DPAD_LEFT], KeyMap[GK_LSTICK_X_NEG] });

		case In::MenuRight:
			return std::max({ KeyMap[SDL_SCANCODE_RIGHT], KeyMap[GK_DPAD_RIGHT], KeyMap[GK_LSTICK_X_POS] });

		default:
			break;
		}

		int keyID = KEY_UNASSIGNED;
		for (int i = (int)BindingProfileID::Count - 1; i >= 0; i--)
		{
			auto profileID = (BindingProfileID)i;
			if (profileID == BindingProfileID::Default && g_Bindings.TestConflict(actionID))
				continue;

			int newKeyID = g_Bindings.GetBoundKeyID(profileID, actionID);
			if (newKeyID == KEY_UNASSIGNED)
				continue;

			if (KeyMap[newKeyID] != 0.0f)
			{
				keyID = newKeyID;
				break;
			}
		}

		float value = (keyID == KEY_UNASSIGNED) ? 0.0f : KeyMap[keyID];
		if (keyID != KEY_UNASSIGNED && ActionMap[actionID].IsClicked())
			UpdateLastInputDevice(actionID, keyID);

		return value;
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

	static void HandleHotkeyActions()
	{
		// Save screenshot.
		static bool dbScreenshot = true;
		bool screenshotKey = KeyMap[SDL_SCANCODE_PRINTSCREEN] || KeyMap[SDL_SCANCODE_F12];
		if (screenshotKey && dbScreenshot)
			g_Renderer.SaveScreenshot();
		dbScreenshot = !screenshotKey;

		// Toggle fullscreen.
		static bool dbFullscreen = true;
		bool altHeld = KeyMap[SDL_SCANCODE_LALT] || KeyMap[SDL_SCANCODE_RALT];
		bool fullscreenKey = altHeld && KeyMap[SDL_SCANCODE_RETURN];
		if (fullscreenKey && dbFullscreen)
		{
			g_Configuration.EnableWindowedMode = !g_Configuration.EnableWindowedMode;
			SaveConfiguration();
			g_Renderer.ToggleFullScreen();
		}
		dbFullscreen = !fullscreenKey;

		if (!DebugMode)
			return;

		// Switch debug page.
		static bool dbDebugPage = true;
		bool debugPageKey = KeyMap[SDL_SCANCODE_F10] || KeyMap[SDL_SCANCODE_F11];
		if (debugPageKey && dbDebugPage)
			g_Renderer.SwitchDebugPage(KeyMap[SDL_SCANCODE_F10] != 0.0f);
		dbDebugPage = !debugPageKey;

		// Cycle pathfinding display.
		static bool dbPathfindingCycle = true;
		if (KeyMap[SDL_SCANCODE_TAB] && dbPathfindingCycle &&
			g_Renderer.GetDebugPage() == RendererDebugPage::PathfindingStats)
		{
			CyclePathfindingDisplay();
		}
		dbPathfindingCycle = !KeyMap[SDL_SCANCODE_TAB];

		// Reload shaders.
		static bool dbReloadShaders = true;
		if (KeyMap[SDL_SCANCODE_F9] && dbReloadShaders)
			g_Renderer.ReloadShaders();
		dbReloadShaders = !KeyMap[SDL_SCANCODE_F9];
	}

	static void UpdateRumble()
	{
		if (ActiveGamepad == nullptr || !ActiveGamepadHasRumble || RumbleInfo.Power == 0.0f)
			return;

		RumbleInfo.Power -= RumbleInfo.FadeSpeed;

		// Don't update effect too frequently if its value hasn't changed much.
		if (RumbleInfo.Power >= 0.2f && (RumbleInfo.LastPower - RumbleInfo.Power) < 0.1f)
			return;

		if (RumbleInfo.Power <= EPSILON)
		{
			StopRumble();
			return;
		}

		// SDL_RumbleGamepad takes Uint16 amplitudes for low/high frequency motors and a duration in ms.
		Uint16 lowFreq = 0, highFreq = 0;
		Uint16 amplitude = (Uint16)std::clamp(RumbleInfo.Power * USHRT_MAX, 0.0f, (float)USHRT_MAX);

		switch (RumbleInfo.Mode)
		{
		case RumbleMode::Left:
			lowFreq = amplitude;
			break;

		case RumbleMode::Right:
			highFreq = amplitude;
			break;

		case RumbleMode::Both:
		default:
			lowFreq = amplitude;
			highFreq = amplitude;
			break;
		}

		if (!SDL_RumbleGamepad(ActiveGamepad, lowFreq, highFreq, 1000 / FPS))
			TENLog(std::string("Rumble update failed: ") + SDL_GetError(), LogLevel::Warning);

		RumbleInfo.LastPower = RumbleInfo.Power;
	}

	void UpdateInputActions(bool allowAsyncUpdate, bool applyQueue)
	{
		// Don't update input data during frameskip.
		if (allowAsyncUpdate || !g_Synchronizer.Locked())
		{
			ClearInputData();
			UpdateRumble();
			ReadKeyboard();
			ReadMouse();
			ReadGamepad();
		}

		DefaultConflict();

		// Update action map.
		for (auto& [actionID, action] : ActionMap)
			action.Update(Key(action.GetID()));

		if (applyQueue)
			ApplyActionQueue();

		// Additional handling.
		HandleHotkeyActions();
		SolveActionCollisions();
	}

	void ClearAllActions()
	{
		for (auto& [actionID, action] : ActionMap)
			action.Clear();

		for (auto& [actionID, queue] : ActionQueueMap)
			queue = ActionQueueState::None;
	}

	void Rumble(float power, float delaySec, RumbleMode mode)
	{
		if (!g_Configuration.EnableRumble)
			return;

		power = std::clamp(power, 0.0f, 1.0f);

		if (power == 0.0f || RumbleInfo.Power)
			return;

		RumbleInfo.FadeSpeed = power / (delaySec * FPS);
		RumbleInfo.Power = power + RumbleInfo.FadeSpeed;
		RumbleInfo.LastPower = RumbleInfo.Power;
		RumbleInfo.Mode = mode;
	}

	void StopRumble()
	{
		if (ActiveGamepad != nullptr && ActiveGamepadHasRumble)
			SDL_RumbleGamepad(ActiveGamepad, 0, 0, 0);

		RumbleInfo = {};
	}

	void ApplyDefaultBindings()
	{
		auto bindingType = (ActiveGamepad != nullptr) ? DefaultBindingType::Gamepad : DefaultBindingType::KeyboardMouse;
		RestoreDefaultBindings(bindingType, true);
	}

	bool RestoreDefaultBindings(DefaultBindingType bindingType)
	{
		return RestoreDefaultBindings(bindingType, false);
	}

	InputDevice GetLastInputDevice()
	{
		return LastInputDevice;
	}

	Vector2 GetMouse2DPosition()
	{
		float absX = 0.0f, absY = 0.0f;
		SDL_GetMouseState(&absX, &absY);

		auto screenRes = g_Renderer.GetScreenResolution();
		auto areaRes = Vector2((float)screenRes.x, (float)screenRes.y);
		auto areaPos = Vector2(absX, absY);
		return (DISPLAY_SPACE_RES * (areaPos / areaRes));
	}

	void ClearAction(ActionID actionID)
	{
		ActionMap[actionID].Clear();
	}

	bool NoAction()
	{
		for (const auto& [actionID, action] : ActionMap)
		{
			if (action.IsHeld())
				return false;
		}

		return true;
	}

	bool IsClicked(ActionID actionID)
	{
		return ActionMap[actionID].IsClicked();
	}

	bool IsHeld(ActionID actionID, float delaySec)
	{
		return ActionMap[actionID].IsHeld(delaySec);
	}

	bool IsPulsed(ActionID actionID, float delaySec, float initialDelaySec)
	{
		return ActionMap[actionID].IsPulsed(delaySec, initialDelaySec);
	}

	bool IsReleased(ActionID actionID, float maxDelaySec)
	{
		return ActionMap[actionID].IsReleased(maxDelaySec);
	}

	float GetActionValue(ActionID actionID)
	{
		return ActionMap[actionID].GetValue();
	}

	unsigned int GetActionTimeActive(ActionID actionID)
	{
		// Time in game frames.
		return ActionMap[actionID].GetTimeActive();
	}

	unsigned int GetActionTimeInactive(ActionID actionID)
	{
		// Time in game frames.
		return ActionMap[actionID].GetTimeInactive();
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
		return AxisMap[AxisID::Move];
	}

	const Vector2& GetCameraAxis()
	{
		return AxisMap[AxisID::Camera];
	}

	const Vector2& GetMouseAxis()
	{
		return AxisMap[AxisID::Mouse];
	}
}
