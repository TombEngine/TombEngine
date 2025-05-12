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
#include "Specific/trutils.h"
#include "Specific/winmain.h"

using namespace TEN::Gui;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

// Big TODO: Make an Input class and handle everything inside it.

namespace TEN::Input
{
	constexpr auto AXIS_SCALE			 = 1.5f;
	constexpr auto AXIS_DEADZONE		 = 8000;
	constexpr auto AXIS_OFFSET			 = 0.2f;
	constexpr auto MOUSE_AXIS_CONSTRAINT = 100.0f;

	// Globals

	RumbleData									   RumbleInfo = {};
	std::unordered_map<int, float>				   KeyMap;			// Key = key ID, value = key value.
	std::unordered_map<InputAxisID, Vector2>	   AxisMap;			// Key = axis ID, value = axis.
	std::unordered_map<ActionID, Action>		   ActionMap;		// Key = action ID, value = action.
	std::unordered_map<ActionID, ActionQueueState> ActionQueueMap;	// Key = action ID, value = action queue state.

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

	void InitializeInput(HWND handle)
	{
		TENLog("Initializing input system...", LogLevel::Info);

		RumbleInfo = {};

		// Initialize key map.
		for (int i = 0; i < KEY_COUNT; i++)
			KeyMap[i] = 0.0f;

		// Initialize axis map.
		for (int i = 0; i < (int)InputAxisID::Count; i++)
		{
			auto inputAxis = (InputAxisID)i;
			AxisMap[inputAxis] = Vector2::Zero;
		}

		// Initialize action and action queue maps.
		for (int i = 0; i < (int)ActionID::Count; i++)
		{
			auto actionID = (ActionID)i;
			ActionMap[actionID] = Action(actionID);
			ActionQueueMap[actionID] = ActionQueueState::None;
		}

		try
		{
			// Use OIS::ParamList since default behaviour blocks WIN key and steals mouse.
			auto paramList = OIS::ParamList{};
			auto wnd = std::ostringstream{};
			wnd << (size_t)handle;
			paramList.insert(std::make_pair(std::string("WINDOW"), wnd.str()));
			paramList.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_FOREGROUND")));
			paramList.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_NONEXCLUSIVE")));
			paramList.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_FOREGROUND")));
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
				if (g_Bindings.GetBoundKeyID(profileID, actionID) != OIS::KC_UNASSIGNED)
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
				AxisMap[InputAxisID::Move].y = 1.0f;
			}
			else if (g_Bindings.GetBoundKeyID(profileID, In::Back) == keyID)
			{
				AxisMap[InputAxisID::Move].y = -1.0f;
			}
			else if (g_Bindings.GetBoundKeyID(profileID, In::Left) == keyID)
			{
				AxisMap[InputAxisID::Move].x = -1.0f;
			}
			else if (g_Bindings.GetBoundKeyID(profileID, In::Right) == keyID)
			{
				AxisMap[InputAxisID::Move].x = 1.0f;
			}
		}
	}

	static void ReadKeyboard()
	{
		if (OisKeyboard == nullptr)
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
		if (OisMouse == nullptr)
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
			AxisMap[InputAxisID::Mouse] = normAxes;
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll mouse input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}
	
	static void ReadGameController()
	{
		if (OisGamepad == nullptr)
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
				// NOTE: Anything above 6 existing XBOX/PS controller axes not supported (2 sticks plus 2 triggers).
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
				// NOTE: abs() operations are needed to avoid issues with inverted axes on different controllers.

				if (g_Bindings.GetBoundKeyID(BindingProfileID::Custom, In::Forward) == usedKeyID)
				{
					AxisMap[InputAxisID::Move].y = abs(scaledValue);
				}
				else if (g_Bindings.GetBoundKeyID(BindingProfileID::Custom, In::Back) == usedKeyID)
				{
					AxisMap[InputAxisID::Move].y = -abs(scaledValue);
				}
				else if (g_Bindings.GetBoundKeyID(BindingProfileID::Custom, In::Left)  == usedKeyID)
				{
					AxisMap[InputAxisID::Move].x = -abs(scaledValue);
				}
				else if (g_Bindings.GetBoundKeyID(BindingProfileID::Custom, In::Right) == usedKeyID)
				{
					AxisMap[InputAxisID::Move].x = abs(scaledValue);
				}
				else if (!TestBoundKey(usedKeyID))
				{
					if ((axis % 2) == 0)
					{
						AxisMap[InputAxisID::Camera].y = normalizedValue;
					}
					else
					{
						AxisMap[InputAxisID::Camera].x = normalizedValue;
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

	static float Key(ActionID actionID)
	{
		int keyID = OIS::KC_UNASSIGNED;
		for (int i = (int)BindingProfileID::Count - 1; i >= 0; i--)
		{
			auto profileID = (BindingProfileID)i;
			if (profileID == BindingProfileID::Default && g_Bindings.TestConflict(actionID))
				continue;

			int newKeyID = g_Bindings.GetBoundKeyID(profileID, actionID);
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
			ReadGameController();
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
		g_Bindings.SetBindingProfile(BindingProfileID::Custom, set);
	}

	void ApplyDefaultBindings()
	{
		ApplyBindings(DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE);
		ApplyDefaultXInputBindings();
	}

	bool ApplyDefaultXInputBindings()
	{
		if (!OisGamepad)
			return false;

		for (int i = 0; i < (int)ActionID::Count; i++)
		{
			auto actionID = (ActionID)i;

			int defaultKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Default, actionID);
			int userKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Custom, actionID);

			if (userKeyID != OIS::KC_UNASSIGNED &&
				userKeyID != defaultKeyID)
			{
				return false;
			}
		}

		auto vendor = ToLower(OisGamepad->vendor());
		if (vendor.find("xbox") != std::string::npos || vendor.find("xinput") != std::string::npos)
		{
			ApplyBindings(DEFAULT_GAMEPAD_BINDING_PROFILE);
			g_Configuration.Bindings = g_Bindings.GetBindingProfile(BindingProfileID::Custom);

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

	// Time in game frames.
	unsigned int GetActionTimeActive(ActionID actionID)
	{
		return ActionMap[actionID].GetTimeActive();
	}

	// Time in game frames.
	unsigned int GetActionTimeInactive(ActionID actionID)
	{
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
		return AxisMap[InputAxisID::Move];
	}

	const Vector2& GetCameraAxis()
	{
		return AxisMap[InputAxisID::Camera];
	}

	const Vector2& GetMouseAxis()
	{
		return AxisMap[InputAxisID::Mouse];
	}
}
