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
	constexpr auto AXIS_SCALE			 = 1.5f;
	constexpr auto AXIS_DEADZONE		 = 8000;
	constexpr auto AXIS_OFFSET			 = 0.2f;
	constexpr auto MOUSE_AXIS_CONSTRAINT = 100.0f;

	// Globals

	RumbleData									   RumbleInfo = {};
	std::unordered_map<int, float>				   KeyMap;			// Key = key ID, value = key value.
	std::unordered_map<ActionId, Action>		   ActionMap;		// Key = action ID, value = action.
	std::unordered_map<ActionId, ActionQueueState> ActionQueueMap;	// Key = action ID, value = action queue state.
	std::unordered_map<AnalogAxisId, Vector2>      AxisMap;			// Key = axis ID, value = axis.

	bool InputLocked = false; // Disables control polling in case application is defocused.

	// OIS interfaces

	static OIS::InputManager*  OisInputManager = nullptr;
	static OIS::Keyboard*	   OisKeyboard	   = nullptr;
	static OIS::Mouse*		   OisMouse		   = nullptr;
	static OIS::JoyStick*	   OisGamepad	   = nullptr;
	static OIS::ForceFeedback* OisRumble	   = nullptr;
	static OIS::Effect*		   OisEffect	   = nullptr;

	void InitializeEffect()
	{
		OisEffect = new OIS::Effect(OIS::Effect::ConstantForce, OIS::Effect::Constant);
		OisEffect->direction = OIS::Effect::North;
		OisEffect->trigger_button = 0;
		OisEffect->trigger_interval = 0;
		OisEffect->replay_length = OIS::Effect::OIS_INFINITE;
		OisEffect->replay_delay = 0;
		OisEffect->setNumAxes(1);

		auto& pConstForce = *dynamic_cast<OIS::ConstantEffect*>(OisEffect->getForceEffect());
		pConstForce.level = 0;
		pConstForce.envelope.attackLength = 0;
		pConstForce.envelope.attackLevel = 0;
		pConstForce.envelope.fadeLength = 0;
		pConstForce.envelope.fadeLevel = 0;
	}

	void InitializeInput()
	{
		TENLog("Initializing input system...", LogLevel::Info);

		RumbleInfo = {};

		// Initialize key map.
		for (int i = 0; i < KEY_COUNT; i++)
			KeyMap[i] = 0.0f;

		// Initialize action and action queue maps.
		for (int i = 0; i < (int)ActionId::Count; i++)
		{
			auto actionId = (ActionId)i;
			ActionMap[actionId] = Action(actionId);
			ActionQueueMap[actionId] = ActionQueueState::None;
		}

		// Initialize axis map.
		for (int i = 0; i < (int)AnalogAxisId::Count; i++)
		{
			auto axisID = (AnalogAxisId)i;
			AxisMap[axisID] = Vector2::Zero;
		}

		SDL_PropertiesID props = SDL_GetWindowProperties(g_Platform->GetSDL3Window());
		HWND handle = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

		try
		{
			// Use OIS::ParamList since default behaviour blocks WIN key and steals mouse.
			auto paramList = OIS::ParamList{};
			auto wnd = std::ostringstream{};
			wnd << (size_t)handle;
			paramList.insert(std::make_pair(std::string("WINDOW"), wnd.str()));
			paramList.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_BACKGROUND")));
			paramList.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_NONEXCLUSIVE")));
			paramList.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_BACKGROUND")));
			paramList.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_NONEXCLUSIVE")));

			OisInputManager = OIS::InputManager::createInputSystem(paramList);
			OisInputManager->enableAddOnFactory(OIS::InputManager::AddOn_All);

			if (OisInputManager->getNumberOfDevices(OIS::OISKeyboard) == 0)
			{
				TENLog("Keyboard not found.", LogLevel::Warning);
			}
			else
			{
				OisKeyboard = (OIS::Keyboard*)OisInputManager->createInputObject(OIS::OISKeyboard, true);
			}

			if (OisInputManager->getNumberOfDevices(OIS::OISMouse) == 0)
			{
				TENLog("Mouse not found.", LogLevel::Warning);
			}
			else
			{
				OisMouse = (OIS::Mouse*)OisInputManager->createInputObject(OIS::OISMouse, true);
			}
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Exception occured during input system initialization: " + std::string(ex.eText), LogLevel::Error);
		}

		int deviceCount = OisInputManager->getNumberOfDevices(OIS::OISJoyStick);
		if (deviceCount > 0)
		{
			TENLog("Found " + std::to_string(deviceCount) + " connected game controller" + ((deviceCount > 1) ? "s." : "."), LogLevel::Info);

			try
			{
				OisGamepad = (OIS::JoyStick*)OisInputManager->createInputObject(OIS::OISJoyStick, true);
				TENLog("Using '" + OisGamepad->vendor() + "' device for input.", LogLevel::Info);

				// Try to initialize vibration interface.
				OisRumble = (OIS::ForceFeedback*)OisGamepad->queryInterface(OIS::Interface::ForceFeedback);
				if (OisRumble != nullptr)
				{
					TENLog("Controller supports vibration.", LogLevel::Info);
					InitializeEffect();
				}

				// If controller is XInput and default bindings were successfully assigned, save configuration.
				if (ApplyDefaultXInputBindings())
				{
					g_Configuration.EnableRumble = (OisRumble != nullptr);
					g_Configuration.EnableThumbstickCamera = true;
					SaveConfiguration();
				}
			}
			catch (OIS::Exception& ex)
			{
				TENLog("Exception occured during game controller initialization: " + std::string(ex.eText), LogLevel::Error);
			}
		}
	}

	void DeinitializeInput()
	{
		TENLog("Shutting down OIS...", LogLevel::Info);

		if (OisKeyboard != nullptr)
			OisInputManager->destroyInputObject(OisKeyboard);

		if (OisMouse != nullptr)
			OisInputManager->destroyInputObject(OisMouse);

		if (OisGamepad != nullptr)
			OisInputManager->destroyInputObject(OisGamepad);

		if (OisEffect != nullptr)
		{
			delete OisEffect;
			OisEffect = nullptr;
		}

		OIS::InputManager::destroyInputSystem(OisInputManager);
	}

	void SetInputLockState(bool locked)
	{
		InputLocked = locked;
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
		for (int i = 0; i < (int)ActionId::Count; i++)
		{
			auto actionId = (ActionId)i;
			switch (ActionQueueMap[actionId])
			{
			default:
			case ActionQueueState::None:
				break;

			case ActionQueueState::Update:
				ActionMap[actionId].Update(true);
				break;

			case ActionQueueState::Clear:
				ActionMap[actionId].Clear();
				break;
			}
		}

		for (auto& [actionId, queue] : ActionQueueMap)
			queue = ActionQueueState::None;
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

	// Merge right and left Ctrl, Shift, and Alt keys.
	static int WrapSimilarKeys(int source)
	{
		switch (source)
		{
		case OIS::KC_LCONTROL:
			return OIS::KC_RCONTROL;

		case OIS::KC_LSHIFT:
			return OIS::KC_RSHIFT;

		case OIS::KC_LMENU:
			return OIS::KC_RMENU;
		}

		return source;
	}

	void DefaultConflict()
	{
		for (const auto& actionIdGroup : ACTION_ID_GROUPS)
		{
			for (auto actionId : actionIdGroup)
			{
				g_Bindings.SetConflict(actionId, false);

				int key = g_Bindings.GetBoundEventIds(BindingProfileId::Default, actionId);
				for (auto conflictActionID : actionIdGroup)
				{
					if (key != g_Bindings.GetBoundEventIds(BindingProfileId::Custom, conflictActionID))
						continue;

					g_Bindings.SetConflict(actionId, true);
					break;
				}
			}
		}
	}

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

	static void ReadKeyboard()
	{
		if (InputLocked || OisKeyboard == nullptr)
			return;

		try
		{
			OisKeyboard->capture();

			// Poll keyboard keys.
			for (int i = 0; i < KEYBOARD_KEY_COUNT; i++)
			{
				if (!OisKeyboard->isKeyDown((OIS::KeyCode)i))
					continue;

				int key = WrapSimilarKeys(i);
				KeyMap[key] = 1.0f;

				// Interpret discrete directional keypresses as analog axis values.
				SetDiscreteAxisValues(key);
			}
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll keyboard input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}

	static void ReadMouse()
	{
		if (InputLocked || OisMouse == nullptr)
			return;

		try
		{
			OisMouse->capture();
			auto& state = OisMouse->getMouseState();

			// Update active area resolution.
			auto screenRes = g_Renderer.GetScreenResolution();
			state.width = screenRes.x;
			state.height = screenRes.y;

			// Poll mouse buttons.
			for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
				KeyMap[KEY_OFFSET_MOUSE + i] = state.buttonDown((OIS::MouseButtonID)i) ? 1.0f : 0.0f;

			// Register multiple directional keypresses mapped to mouse axes.
			int baseIndex = KEY_OFFSET_MOUSE + MOUSE_BUTTON_COUNT;
			for (int pass = 0; pass < (MOUSE_AXIS_COUNT * 2); pass++)
			{
				switch (pass)
				{
				// Mouse X-
				case 0:
					if (state.X.rel >= 0)
						continue;
					break;

				// Mouse X+
				case 1:
					if (state.X.rel <= 0)
						continue;
					break;

				// Mouse Y-
				case 2:
					if (state.Y.rel >= 0)
						continue;
					break;

				// Mouse Y+
				case 3:
					if (state.Y.rel <= 0)
						continue;
					break;

				// Mouse Z-
				case 4:
					if (state.Z.rel >= 0)
						continue;
					break;

				// Mouse Z+
				case 5:
					if (state.Z.rel <= 0)
						continue;
					break;
				}

				KeyMap[baseIndex + pass] = 1.0f;

				// Interpret discrete directional keypresses as mouse axis values.
				SetDiscreteAxisValues(baseIndex + pass);
			}

			// Normalize raw mouse axis values to range [-1.0f, 1.0f].
			auto rawAxes = Vector2(state.X.rel, state.Y.rel);
			auto normAxes = Vector2(
				(((rawAxes.x - -DISPLAY_SPACE_RES.x) * 2.0f) / float(DISPLAY_SPACE_RES.x - -DISPLAY_SPACE_RES.x)) - 1.0f,
				(((rawAxes.y - -DISPLAY_SPACE_RES.y) * 2.0f) / float(DISPLAY_SPACE_RES.y - -DISPLAY_SPACE_RES.y)) - 1.0f);

			// Apply sensitivity.
			float sensitivity = (g_Configuration.MouseSensitivity * 0.1f) + 0.4f;
			normAxes *= sensitivity;

			// Set mouse axis values.
			AxisMap[AnalogAxisId::Mouse] = normAxes;
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll mouse input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}
	
	static void ReadGamepad()
	{
		if (InputLocked || OisGamepad == nullptr)
			return;

		try
		{
			OisGamepad->capture();
			const auto& state = OisGamepad->getJoyStickState();

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

	static void HandleHotkeyActions()
	{
		// Save screenshot.
		static bool dbScreenshot = true;
		if ((KeyMap[OIS::KC_SYSRQ] || KeyMap[OIS::KC_F12]) && dbScreenshot)
			g_Renderer.SaveScreenshot();
		dbScreenshot = !(KeyMap[OIS::KC_SYSRQ] || KeyMap[OIS::KC_F12]);

		// Toggle fullscreen.
		static bool dbFullscreen = true;
		if ((KeyMap[OIS::KC_LMENU] || KeyMap[OIS::KC_RMENU]) && KeyMap[OIS::KC_RETURN] && dbFullscreen)
		{
			g_Configuration.EnableWindowedMode = !g_Configuration.EnableWindowedMode;
			SaveConfiguration();
			g_Renderer.ToggleFullScreen();
		}
		dbFullscreen = !((KeyMap[OIS::KC_LMENU] || KeyMap[OIS::KC_RMENU]) && KeyMap[OIS::KC_RETURN]);

		if (!DebugMode)
			return;

		// Switch debug page.
		static bool dbDebugPage = true;
		if ((KeyMap[OIS::KC_F10] || KeyMap[OIS::KC_F11]) && dbDebugPage)
			g_Renderer.SwitchDebugPage(KeyMap[OIS::KC_F10]);
		dbDebugPage = !(KeyMap[OIS::KC_F10] || KeyMap[OIS::KC_F11]);

		// Reload shaders.
		static bool dbReloadShaders = true;
		if (KeyMap[OIS::KC_F9] && dbReloadShaders)
			g_Renderer.ReloadShaders();
		dbReloadShaders = !KeyMap[OIS::KC_F9];
	}

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
		for (auto& [actionId, action] : ActionMap)
			action.Update(Key(action.GetID()));

		if (applyQueue)
			ApplyActionQueue();

		// Additional handling.
		HandleHotkeyActions();
		SolveActionCollisions();
	}

	void ClearAllActions()
	{
		for (auto& [actionId, action] : ActionMap)
			action.Clear();

		for (auto& [actionId, queue] : ActionQueueMap)
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
	}

	void StopRumble()
	{
		if (!OisRumble || !OisEffect)
			return;

		try
		{
			OisRumble->remove(OisEffect);
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Error when stopping vibration effect: " + std::string(ex.eText), LogLevel::Error);
		}

		RumbleInfo = {};
	}

	static void ApplyBindings(const BindingProfile& set)
	{
		g_Bindings.SetBindingProfile(BindingProfileId::Custom, set);
	}

	void ApplyDefaultBindings()
	{
		ApplyBindings(DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE);
		ApplyDefaultXInputBindings();
	}

	bool ApplyDefaultXInputBindings()
	{
		if (!OisGamepad)
			return false;

		for (int i = 0; i < (int)ActionId::Count; i++)
		{
			auto actionId = (ActionId)i;

			int defaultKeyID = g_Bindings.GetBoundEventIds(BindingProfileId::Default, actionId);
			int userKeyID = g_Bindings.GetBoundEventIds(BindingProfileId::Custom, actionId);

			if (userKeyID != OIS::KC_UNASSIGNED &&
				userKeyID != defaultKeyID)
			{
				return false;
			}
		}

		auto vendor = ToLower(OisGamepad->vendor());
		if (vendor.find("xbox") != std::string::npos || vendor.find("xinput") != std::string::npos)
		{
			ApplyBindings(DEFAULT_USER_GAMEPAD_BINDING_PROFILE);
			g_Configuration.Bindings = g_Bindings.GetBindingProfile(BindingProfileId::Custom);

			// Additionally enable rumble and thumbstick camera.
			g_Configuration.EnableRumble = true;
			g_Configuration.EnableThumbstickCamera = true;

			return true;
		}
		else
		{
			return false;
		}
	}

	Vector2 GetMouse2DPosition()
	{
		const auto& state = OisMouse->getMouseState();

		auto areaRes = Vector2(state.width, state.height);
		auto areaPos = Vector2(state.X.abs, state.Y.abs);
		return (DISPLAY_SPACE_RES * (areaPos / areaRes));
	}

	void ClearAction(ActionId actionId)
	{
		ActionMap[actionId].Clear();
	}

	bool NoAction()
	{
		for (const auto& [actionId, action] : ActionMap)
		{
			if (action.IsHeld())
				return false;
		}

		return true;
	}

	bool IsClicked(ActionId actionId)
	{
		return ActionMap[actionId].IsClicked();
	}

	bool IsHeld(ActionId actionId, float delaySec)
	{
		return ActionMap[actionId].IsHeld(delaySec);
	}

	bool IsPulsed(ActionId actionId, float delaySec, float initialDelaySec)
	{
		return ActionMap[actionId].IsPulsed(delaySec, initialDelaySec);
	}

	bool IsReleased(ActionId actionId, float maxDelaySec)
	{
		return ActionMap[actionId].IsReleased(maxDelaySec);
	}

	float GetActionValue(ActionId actionId)
	{
		return ActionMap[actionId].GetValue();
	}

	// Time in game frames.
	unsigned int GetActionTimeActive(ActionId actionId)
	{
		return ActionMap[actionId].GetTimeActive();
	}

	// Time in game frames.
	unsigned int GetActionTimeInactive(ActionId actionId)
	{
		return ActionMap[actionId].GetTimeInactive();
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
		return AxisMap[AnalogAxisId::Move];
	}

	const Vector2& GetCameraAxis()
	{
		return AxisMap[AnalogAxisId::Camera];
	}

	const Vector2& GetMouseAxis()
	{
		return AxisMap[AnalogAxisId::Mouse];
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

		// TODO
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
		// TODO
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
	}

	void InputManager::ReadMouse(SDL_Window& window, const Vector2& wheelAxis)
	{
		constexpr int AXIS_COUNT = 2;

		int eventIdx = (int)START_MOUSE_EVENT_ID;

		// Compute cursor position.
		auto pos = Vector2::Zero;
		auto butState = SDL_GetMouseState(&pos.x, &pos.y); // NOTE: Not a thread-safe call, but still works correctly.
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

		// TODO: Must investigate. Unclear how SDL3 mouse wheel values work.
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

				// TODO: Adapt TEN-specific axis scaling.
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
