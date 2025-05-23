#include "framework.h"
#include "Specific/configuration.h"

#include <CommCtrl.h>

#include "Renderer/Renderer.h"
#include "resource.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/LanguageScript.h"
#include "Specific/Input/Input.h"
#include "Specific/winmain.h"
#include "Sound/sound.h"

using namespace TEN::Input;
using namespace TEN::Renderer;

namespace TEN::Config
{
	GameConfiguration g_Config;

	bool GameConfiguration::IsUsingClassicControls() const
	{
		return (ControlMode == ControlMode::Classic);
	}

	bool GameConfiguration::IsUsingEnhancedControls() const
	{
		return (ControlMode == ControlMode::Enhanced);
	}

	bool GameConfiguration::IsUsingModernControls() const
	{
		return (ControlMode == ControlMode::Modern);
	}

	bool GameConfiguration::IsUsingOmnidirectionalSwimControls() const
	{
		return (SwimControlMode == SwimControlMode::Omnidirectional);
	}

	bool GameConfiguration::IsUsingPlanarSwimControls() const
	{
		return (SwimControlMode == SwimControlMode::Planar);
	}

	static void LoadResolutionsInCombobox(HWND handle)
	{
		HWND cbHandle = GetDlgItem(handle, IDC_RESOLUTION);

		SendMessageA(cbHandle, CB_RESETCONTENT, 0, 0);

		for (int i = 0; i < g_Config.SupportedScreenResolutions.size(); i++)
		{
			auto screenRes = g_Config.SupportedScreenResolutions[i];

			char* str = (char*)malloc(255);
			ZeroMemory(str, 255);
			sprintf(str, "%d x %d", screenRes.x, screenRes.y);

			SendMessageA(cbHandle, CB_ADDSTRING, i, (LPARAM)(str));

			free(str);
		}

		SendMessageA(cbHandle, CB_SETCURSEL, 0, 0);
		SendMessageA(cbHandle, CB_SETMINVISIBLE, 20, 0);
	}

	static void LoadSoundDevicesInCombobox(HWND handle)
	{
		HWND cbHandle = GetDlgItem(handle, IDC_SNDADAPTER);

		SendMessageA(cbHandle, CB_RESETCONTENT, 0, 0);

		// Get all audio devices, including the default one
		BASS_DEVICEINFO info;
		int i = 1;
		while (BASS_GetDeviceInfo(i, &info))
		{
			SendMessageA(cbHandle, CB_ADDSTRING, 0, (LPARAM)info.name);
			i++;
		}

		SendMessageA(cbHandle, CB_SETCURSEL, 0, 0);
	}

	static BOOL CALLBACK DialogProc(HWND handle, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Vector2i mode;
		int selectedMode;

		switch (msg)
		{
		case WM_INITDIALOG:
			SendMessageA(GetDlgItem(handle, IDC_GROUP_GFXADAPTER), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_DISPLAY_ADAPTER).c_str());
			SendMessageA(GetDlgItem(handle, IDC_GROUP_OUTPUT_SETTINGS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_OUTPUT_SETTINGS).c_str());
			SendMessageA(GetDlgItem(handle, IDOK), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_OK).c_str());
			SendMessageA(GetDlgItem(handle, IDCANCEL), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_CANCEL).c_str());
			SendMessageA(GetDlgItem(handle, IDC_GROUP_RESOLUTION), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_SCREEN_RESOLUTION).c_str());
			SendMessageA(GetDlgItem(handle, IDC_GROUP_SOUND), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_SOUND).c_str());
			SendMessageA(GetDlgItem(handle, IDC_ENABLE_SOUNDS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_ENABLE_SOUND).c_str());
			SendMessageA(GetDlgItem(handle, IDC_WINDOW_MODE), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_WINDOW_MODE).c_str());
			SendMessageA(GetDlgItem(handle, IDC_GROUP_RENDER_OPTIONS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_RENDER_OPTIONS).c_str());
			SendMessageA(GetDlgItem(handle, IDC_SHADOWS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_SHADOWS).c_str());
			SendMessageA(GetDlgItem(handle, IDC_CAUSTICS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_CAUSTICS).c_str());
			SendMessageA(GetDlgItem(handle, IDC_ANTIALIASING), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_ANTIALIASING).c_str());

			LoadResolutionsInCombobox(handle);
			LoadSoundDevicesInCombobox(handle);

			// Set some default values.
			g_Config.EnableAutoTargeting = true;

			g_Config.AntialiasingMode = AntialiasingMode::Low;
			SendDlgItemMessage(handle, IDC_ANTIALIASING, BM_SETCHECK, 1, 0);

			g_Config.ShadowType = ShadowMode::Player;
			SendDlgItemMessage(handle, IDC_SHADOWS, BM_SETCHECK, 1, 0);

			g_Config.EnableCaustics = true;
			SendDlgItemMessage(handle, IDC_CAUSTICS, BM_SETCHECK, 1, 0);

			g_Config.WindowMode = WindowMode::Windowed;
			SendDlgItemMessage(handle, IDC_WINDOW_MODE, BM_SETCHECK, 1, 0);

			g_Config.EnableSound = true;
			SendDlgItemMessage(handle, IDC_ENABLE_SOUNDS, BM_SETCHECK, 1, 0);

			break;

		case WM_COMMAND:
			//DB_Log(6, "WM_COMMAND");

			// Checkboxes
			if (HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
				case IDOK:
					// Get values from dialog components.
					g_Config.WindowMode = WindowMode(SendDlgItemMessage(handle, IDC_WINDOW_MODE, BM_GETCHECK, 0, 0));
					g_Config.ShadowType = ShadowMode(SendDlgItemMessage(handle, IDC_SHADOWS, BM_GETCHECK, 0, 0));
					g_Config.EnableCaustics = SendDlgItemMessage(handle, IDC_CAUSTICS, BM_GETCHECK, 0, 0);
					g_Config.AntialiasingMode = AntialiasingMode(SendDlgItemMessage(handle, IDC_ANTIALIASING, BM_GETCHECK, 0, 0));
					g_Config.EnableSound = SendDlgItemMessage(handle, IDC_ENABLE_SOUNDS, BM_GETCHECK, 0, 0);
					selectedMode = SendDlgItemMessage(handle, IDC_RESOLUTION, CB_GETCURSEL, 0, 0);
					mode = g_Config.SupportedScreenResolutions[selectedMode];
					g_Config.ScreenWidth = mode.x;
					g_Config.ScreenHeight = mode.y;
					g_Config.SoundDevice = SendDlgItemMessage(handle, IDC_SNDADAPTER, CB_GETCURSEL, 0, 0) + 1;

					// Save configuration.
					SaveConfiguration();
					EndDialog(handle, wParam);
					return 1;

				case IDCANCEL:
					EndDialog(handle, wParam);
					return 1;
				}

				return 0;
			}

			break;

		default:
			return 0;
		}

		return 0;
	}

	int SetupDialog()
	{
		auto res = FindResource(nullptr, MAKEINTRESOURCE(IDD_SETUP), RT_DIALOG);

		ShowCursor(true);
		int result = DialogBoxParamA(nullptr, MAKEINTRESOURCE(IDD_SETUP), 0, (DLGPROC)DialogProc, 0);
		ShowCursor(false);

		return true;
	}

	static LONG SetDWORDRegKey(HKEY hKey, LPCSTR strValueName, DWORD nValue)
	{
		return RegSetValueExA(hKey, strValueName, 0, REG_DWORD, reinterpret_cast<LPBYTE>(&nValue), sizeof(DWORD));
	}

	static LONG SetBoolRegKey(HKEY hKey, LPCSTR strValueName, bool bValue)
	{
		return SetDWORDRegKey(hKey, strValueName, (bValue ? 1 : 0));
	}

	static LONG SetStringRegKey(HKEY hKey, LPCSTR strValueName, char* strValue)
	{
		// TODO: Fix this line.
		return 1; // RegSetValueExA(hKey, strValueName, 0, REG_DWORD, reinterpret_cast<LPBYTE>(&nValue), sizeof(DWORD));
	}

	void InitDefaultConfiguration()
	{
		// Include default device in list.
		BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, true);

		auto screenRes = GetScreenResolution();

		// Controls
		g_Config.EnableTankCameraControl = false;
		g_Config.InvertCameraXAxis = false;
		g_Config.InvertCameraYAxis = false;
		g_Config.EnableRumble = true;
		g_Config.MouseSensitivity = GameConfiguration::DEFAULT_MOUSE_SENSITIVITY;
		g_Config.MenuOptionLoopingMode = MenuOptionLoopingMode::SaveLoadOnly;

		// Gameplay
		g_Config.ControlMode = ControlMode::Enhanced;
		g_Config.SwimControlMode = SwimControlMode::Omnidirectional;
		g_Config.EnableWalkToggle = false;
		g_Config.EnableCrouchToggle = false;
		g_Config.EnableClimbToggle = false;
		g_Config.EnableAutoMonkeySwingJump = false;
		g_Config.EnableAutoTargeting = true;
		g_Config.EnableOppositeActionRoll = true;
		g_Config.EnableTargetHighlighter = true;

		// Graphics
		g_Config.ScreenWidth = screenRes.x;
		g_Config.ScreenHeight = screenRes.y;
		g_Config.WindowMode = WindowMode::Windowed;
		g_Config.FrameRateMode = FrameRateMode::Sixty;
		g_Config.ShadowType = ShadowMode::Player;
		g_Config.ShadowMapSize = GameConfiguration::DEFAULT_SHADOW_MAP_SIZE;
		g_Config.ShadowBlobCountMax = GameConfiguration::DEFAULT_SHADOW_BLOB_COUNT_MAX;
		g_Config.EnableAmbientOcclusion = true;
		g_Config.EnableCaustics = true;
		g_Config.AntialiasingMode = AntialiasingMode::Medium;
		g_Config.EnableSubtitles = true;

		// Sound
		g_Config.SoundDevice = 1;
		g_Config.EnableSound = true;
		g_Config.EnableReverb = true;
		g_Config.MusicVolume = GameConfiguration::SOUND_VOLUME_MAX;
		g_Config.SfxVolume = GameConfiguration::SOUND_VOLUME_MAX;

		g_Config.SupportedScreenResolutions = GetAllSupportedScreenResolutions();
		g_Config.AdapterName = g_Renderer.GetDefaultAdapterName();
	}

	bool SaveConfiguration()
	{
		// Open root key.
		HKEY rootKey = NULL;
		if (RegOpenKeyA(HKEY_CURRENT_USER, REGKEY_ROOT, &rootKey) != ERROR_SUCCESS)
		{
			// Create new key.
			if (RegCreateKeyA(HKEY_CURRENT_USER, REGKEY_ROOT, &rootKey) != ERROR_SUCCESS)
				return false;
		}

		// Open Controls subkey.
		HKEY controlsKey = NULL;
		if (RegCreateKeyExA(rootKey, REGKEY_CONTROLS, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &controlsKey, NULL) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			return false;
		}

		// Set Controls keys.
		if (SetBoolRegKey(controlsKey, REGKEY_ENABLE_TANK_CAMERA_CONTROL, g_Config.EnableTankCameraControl) != ERROR_SUCCESS ||
			SetBoolRegKey(controlsKey, REGKEY_INVERT_CAMERA_X_AXIS, g_Config.InvertCameraXAxis) != ERROR_SUCCESS ||
			SetBoolRegKey(controlsKey, REGKEY_INVERT_CAMERA_Y_AXIS, g_Config.InvertCameraYAxis) != ERROR_SUCCESS ||
			SetBoolRegKey(controlsKey, REGKEY_ENABLE_RUMBLE, g_Config.EnableRumble) != ERROR_SUCCESS ||
			SetDWORDRegKey(controlsKey, REGKEY_MOUSE_SENSITIVITY, g_Config.MouseSensitivity) != ERROR_SUCCESS ||
			SetDWORDRegKey(controlsKey, REGKEY_MENU_OPTION_LOOPING_MODE, (int)g_Config.MenuOptionLoopingMode) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			return false;
		}

		// Open Controls\\KeyBindings subkey.
		HKEY keyBindingsKey = NULL;
		if (RegCreateKeyExA(controlsKey, REGKEY_KEY_BINDINGS, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &keyBindingsKey, NULL) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			return false;
		}

		// Set Controls\\KeyBindings keys.
		//g_Config.KeyBindings.resize((int)In::Count);
		for (int i = 0; i < (int)In::Count; i++)
		{
			char buffer[9];
			sprintf(buffer, "Action%d", i);

			/*if (SetDWORDRegKey(keyBindingsKey, buffer, g_Config.KeyBindings[i]) != ERROR_SUCCESS)
			{
				RegCloseKey(rootKey);
				RegCloseKey(controlsKey);
				RegCloseKey(keyBindingsKey);
				return false;
			}*/
		}

		// Open Gameplay subkey.
		HKEY gameplayKey = NULL;
		if (RegCreateKeyExA(rootKey, REGKEY_GAMEPLAY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &gameplayKey, NULL) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			return false;
		}

		// Set Gameplay subkey.
		if (SetDWORDRegKey(gameplayKey, REGKEY_CONTROL_MODE, (DWORD)g_Config.ControlMode) != ERROR_SUCCESS ||
			SetDWORDRegKey(gameplayKey, REGKEY_SWIM_CONTROL_MODE, (DWORD)g_Config.SwimControlMode) != ERROR_SUCCESS ||
			SetBoolRegKey(gameplayKey, REGKEY_ENABLE_WALK_TOGGLE, g_Config.EnableWalkToggle) != ERROR_SUCCESS ||
			SetBoolRegKey(gameplayKey, REGKEY_ENABLE_CROUCH_TOGGLE, g_Config.EnableCrouchToggle) != ERROR_SUCCESS ||
			SetBoolRegKey(gameplayKey, REGKEY_ENABLE_CLIMB_TOGGLE, g_Config.EnableClimbToggle) != ERROR_SUCCESS ||
			SetBoolRegKey(gameplayKey, REGKEY_ENABLE_AUTO_MONKEY_SWING_JUMP, g_Config.EnableAutoMonkeySwingJump) != ERROR_SUCCESS ||
			SetBoolRegKey(gameplayKey, REGKEY_ENABLE_AUTO_TARGETING, g_Config.EnableAutoTargeting) != ERROR_SUCCESS ||
			SetBoolRegKey(gameplayKey, REGKEY_ENABLE_OPPOSITE_ACTION_ROLL, g_Config.EnableOppositeActionRoll) != ERROR_SUCCESS ||
			SetBoolRegKey(gameplayKey, REGKEY_ENABLE_TARGET_HIGHLIGHTER, g_Config.EnableTargetHighlighter) != ERROR_SUCCESS ||
			SetBoolRegKey(gameplayKey, REGKEY_ENABLE_SUBTITLES, g_Config.EnableSubtitles) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			return false;
		}

		// Open Graphics subkey.
		HKEY graphicsKey = NULL;
		if (RegCreateKeyExA(rootKey, REGKEY_GRAPHICS, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &graphicsKey, NULL) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(graphicsKey);
			return false;
		}

		// Set Graphics keys.
		if (SetDWORDRegKey(graphicsKey, REGKEY_SCREEN_WIDTH, g_Config.ScreenWidth) != ERROR_SUCCESS ||
			SetDWORDRegKey(graphicsKey, REGKEY_SCREEN_HEIGHT, g_Config.ScreenHeight) != ERROR_SUCCESS ||
			SetDWORDRegKey(graphicsKey, REGKEY_WINDOW_MODE, (DWORD)g_Config.WindowMode) != ERROR_SUCCESS ||
			SetDWORDRegKey(graphicsKey, REGKEY_FRAME_RATE_MODE, (DWORD)g_Config.FrameRateMode) != ERROR_SUCCESS ||
			SetDWORDRegKey(graphicsKey, REGKEY_SHADOWS, (DWORD)g_Config.ShadowType) != ERROR_SUCCESS ||
			SetDWORDRegKey(graphicsKey, REGKEY_SHADOW_MAP_SIZE, g_Config.ShadowMapSize) != ERROR_SUCCESS ||
			SetDWORDRegKey(graphicsKey, REGKEY_SHADOW_BLOB_COUNT_MAX, g_Config.ShadowBlobCountMax) != ERROR_SUCCESS ||
			SetBoolRegKey(graphicsKey, REGKEY_ENABLE_CAUSTICS, g_Config.EnableCaustics) != ERROR_SUCCESS ||
			SetBoolRegKey(graphicsKey, REGKEY_AMBIENT_OCCLUSION, g_Config.EnableAmbientOcclusion) != ERROR_SUCCESS ||
			SetDWORDRegKey(graphicsKey, REGKEY_ANTIALIASING_MODE, (DWORD)g_Config.AntialiasingMode) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(graphicsKey);
			return false;
		}

		// Open Sound subkey.
		HKEY soundKey = NULL;
		if (RegCreateKeyExA(rootKey, REGKEY_SOUND, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &soundKey, NULL) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(graphicsKey);
			RegCloseKey(soundKey);
			return false;
		}

		// Set Sound keys.
		if (SetDWORDRegKey(soundKey, REGKEY_SOUND_DEVICE, g_Config.SoundDevice) != ERROR_SUCCESS ||
			SetBoolRegKey(soundKey, REGKEY_ENABLE_SOUND, g_Config.EnableSound) != ERROR_SUCCESS ||
			SetBoolRegKey(soundKey, REGKEY_ENABLE_REVERB, g_Config.EnableReverb) != ERROR_SUCCESS ||
			SetDWORDRegKey(soundKey, REGKEY_MUSIC_VOLUME, g_Config.MusicVolume) != ERROR_SUCCESS ||
			SetDWORDRegKey(soundKey, REGKEY_SFX_VOLUME, g_Config.SfxVolume) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(graphicsKey);
			RegCloseKey(soundKey);
			return false;
		}

		// Close registry keys.
		RegCloseKey(rootKey);
		RegCloseKey(controlsKey);
		RegCloseKey(keyBindingsKey);
		RegCloseKey(gameplayKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		return true;
	}

	static LONG GetDWORDRegKey(HKEY hKey, LPCSTR strValueName, DWORD* nValue, DWORD nDefaultValue)
	{
		*nValue = nDefaultValue;

		DWORD dwBufferSize(sizeof(DWORD));
		DWORD nResult(0);

		LONG nError = RegQueryValueEx(hKey, strValueName, 0, NULL, reinterpret_cast<LPBYTE>(&nResult), &dwBufferSize);
		if (ERROR_SUCCESS == nError)
			*nValue = nResult;

		return nError;
	}

	static LONG GetBoolRegKey(HKEY hKey, LPCSTR strValueName, bool* bValue, bool bDefaultValue)
	{
		DWORD nDefValue((bDefaultValue) ? 1 : 0);
		DWORD nResult(nDefValue);

		LONG nError = GetDWORDRegKey(hKey, strValueName, &nResult, nDefValue);
		if (ERROR_SUCCESS == nError)
			*bValue = (nResult != 0);

		return nError;
	}

	static LONG GetStringRegKey(HKEY hKey, LPCSTR strValueName, char** strValue, char* strDefaultValue)
	{
		*strValue = strDefaultValue;
		char szBuffer[512];
		DWORD dwBufferSize = sizeof(szBuffer);

		ULONG nError = RegQueryValueEx(hKey, strValueName, 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
		if (ERROR_SUCCESS == nError)
			*strValue = szBuffer;

		return nError;
	}

	bool LoadConfiguration()
	{
		// Open root key.
		HKEY rootKey = NULL;
		if (RegOpenKeyA(HKEY_CURRENT_USER, REGKEY_ROOT, &rootKey) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			return false;
		}

		// Open Controls subkey.
		HKEY controlsKey = NULL;
		if (RegOpenKeyExA(rootKey, REGKEY_CONTROLS, 0, KEY_READ, &controlsKey) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			return false;
		}

		bool enableThumbstickCamera = true;
		bool invertCameraXAxis = false;
		bool invertCameraYAxis = false;
		bool enableRumble = true;
		DWORD mouseSensitivity = GameConfiguration::DEFAULT_MOUSE_SENSITIVITY;
		DWORD menuOptionLoopingMode = (DWORD)MenuOptionLoopingMode::SaveLoadOnly;

		// Load Controls keys.
		if (GetBoolRegKey(controlsKey, REGKEY_ENABLE_TANK_CAMERA_CONTROL, &enableThumbstickCamera, true) != ERROR_SUCCESS ||
			GetBoolRegKey(controlsKey, REGKEY_INVERT_CAMERA_X_AXIS, &invertCameraXAxis, false) != ERROR_SUCCESS ||
			GetBoolRegKey(controlsKey, REGKEY_INVERT_CAMERA_Y_AXIS, &invertCameraYAxis, false) != ERROR_SUCCESS ||
			GetBoolRegKey(controlsKey, REGKEY_ENABLE_RUMBLE, &enableRumble, true) != ERROR_SUCCESS ||
			GetDWORDRegKey(controlsKey, REGKEY_MOUSE_SENSITIVITY, &mouseSensitivity, GameConfiguration::DEFAULT_MOUSE_SENSITIVITY) != ERROR_SUCCESS ||
			GetDWORDRegKey(controlsKey, REGKEY_MENU_OPTION_LOOPING_MODE, &menuOptionLoopingMode, (DWORD)MenuOptionLoopingMode::SaveLoadOnly) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			return false;
		}

		// Load Controls\\KeyBindings keys.
		HKEY keyBindingsKey = NULL;
		if (RegOpenKeyExA(controlsKey, REGKEY_KEY_BINDINGS, 0, KEY_READ, &keyBindingsKey) == ERROR_SUCCESS)
		{
			for (int i = 0; i < (int)In::Count; i++)
			{
				DWORD tempAction = 0;
				char buffer[9];
				sprintf(buffer, "Action%d", i);

				/*if (GetDWORDRegKey(keyBindingsKey, buffer, &tempAction, Bindings[0][i]) != ERROR_SUCCESS)
				{
					RegCloseKey(rootKey);
					RegCloseKey(controlsKey);
					RegCloseKey(keyBindingsKey);
					return false;
				}*/

				//g_Config.KeyBindings.push_back(tempAction);
				//KeyBindings[1][i] = tempAction;
			}
		}
		else
		{
			// "KeyBindings" key doesn't exist; use default bindings.
			//g_Config.KeyBindings = Bindings[0];
		}

		// Open Gameplay subkey.
		HKEY gameplayKey = NULL;
		if (RegOpenKeyExA(rootKey, REGKEY_GAMEPLAY, 0, KEY_READ, &gameplayKey) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			return false;
		}

		DWORD controlMode = (DWORD)ControlMode::Enhanced;
		DWORD swimControlMode = (DWORD)SwimControlMode::Omnidirectional;
		bool enableWalkToggle = false;
		bool enableCrouchToggle = false;
		bool enableClimbToggle = false;
		bool enableAutoMonkeySwingJump = false;
		bool enableAutoTargeting = true;
		bool enableOppositeActionRoll = true;
		bool enableSubtitles = true;
		bool enableTargetHighlighter = true;

		// Load Gameplay subkey.
		if (GetDWORDRegKey(gameplayKey, REGKEY_CONTROL_MODE, &controlMode, (DWORD)ControlMode::Enhanced) != ERROR_SUCCESS ||
			GetDWORDRegKey(gameplayKey, REGKEY_SWIM_CONTROL_MODE, &swimControlMode, (DWORD)SwimControlMode::Omnidirectional) != ERROR_SUCCESS ||
			GetBoolRegKey(gameplayKey, REGKEY_ENABLE_WALK_TOGGLE, &enableCrouchToggle, false) != ERROR_SUCCESS ||
			GetBoolRegKey(gameplayKey, REGKEY_ENABLE_CROUCH_TOGGLE, &enableCrouchToggle, false) != ERROR_SUCCESS ||
			GetBoolRegKey(gameplayKey, REGKEY_ENABLE_CLIMB_TOGGLE, &enableClimbToggle, false) != ERROR_SUCCESS ||
			GetBoolRegKey(gameplayKey, REGKEY_ENABLE_AUTO_MONKEY_SWING_JUMP, &enableAutoMonkeySwingJump, false) != ERROR_SUCCESS ||
			GetBoolRegKey(gameplayKey, REGKEY_ENABLE_AUTO_TARGETING, &enableAutoTargeting, true) != ERROR_SUCCESS ||
			GetBoolRegKey(gameplayKey, REGKEY_ENABLE_OPPOSITE_ACTION_ROLL, &enableOppositeActionRoll, true) != ERROR_SUCCESS ||
			GetBoolRegKey(gameplayKey, REGKEY_ENABLE_TARGET_HIGHLIGHTER, &enableTargetHighlighter, true) != ERROR_SUCCESS ||
			GetBoolRegKey(gameplayKey, REGKEY_ENABLE_SUBTITLES, &enableSubtitles, true) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			return false;
		}

		// Open Graphics subkey.
		HKEY graphicsKey = NULL;
		if (RegOpenKeyExA(rootKey, REGKEY_GRAPHICS, 0, KEY_READ, &graphicsKey) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(graphicsKey);
			return false;
		}

		DWORD screenWidth = 0;
		DWORD screenHeight = 0;
		DWORD windowMode = 0;
		DWORD frameRateMode = (DWORD)FrameRateMode::Sixty;
		DWORD shadowMode = 1;
		DWORD shadowMapSize = GameConfiguration::DEFAULT_SHADOW_MAP_SIZE;
		DWORD shadowBlobsMax = GameConfiguration::DEFAULT_SHADOW_BLOB_COUNT_MAX;
		bool enableCaustics = false;
		DWORD antialiasingMode = (DWORD)AntialiasingMode::High;
		bool enableAmbientOcclusion = false;

		// Load Graphics keys.
		if (GetDWORDRegKey(graphicsKey, REGKEY_SCREEN_WIDTH, &screenWidth, 0) != ERROR_SUCCESS ||
			GetDWORDRegKey(graphicsKey, REGKEY_SCREEN_HEIGHT, &screenHeight, 0) != ERROR_SUCCESS ||
			GetDWORDRegKey(graphicsKey, REGKEY_WINDOW_MODE, &windowMode, 0) != ERROR_SUCCESS ||
			GetDWORDRegKey(graphicsKey, REGKEY_FRAME_RATE_MODE, &frameRateMode, 0) != ERROR_SUCCESS ||
			GetDWORDRegKey(graphicsKey, REGKEY_SHADOWS, &shadowMode, 1) != ERROR_SUCCESS ||
			GetDWORDRegKey(graphicsKey, REGKEY_SHADOW_MAP_SIZE, &shadowMapSize, GameConfiguration::DEFAULT_SHADOW_MAP_SIZE) != ERROR_SUCCESS ||
			GetDWORDRegKey(graphicsKey, REGKEY_SHADOW_BLOB_COUNT_MAX, &shadowBlobsMax, GameConfiguration::DEFAULT_SHADOW_BLOB_COUNT_MAX) != ERROR_SUCCESS ||
			GetBoolRegKey(graphicsKey, REGKEY_ENABLE_CAUSTICS, &enableCaustics, true) != ERROR_SUCCESS ||
			GetDWORDRegKey(graphicsKey, REGKEY_ANTIALIASING_MODE, &antialiasingMode, true) != ERROR_SUCCESS ||
			GetBoolRegKey(graphicsKey, REGKEY_AMBIENT_OCCLUSION, &enableAmbientOcclusion, false) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(graphicsKey);
			return false;
		}

		// Open Sound subkey.
		HKEY soundKey = NULL;
		if (RegOpenKeyExA(rootKey, REGKEY_SOUND, 0, KEY_READ, &soundKey) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(graphicsKey);
			RegCloseKey(soundKey);
			return false;
		}

		DWORD soundDevice = 0;
		bool enableSound = true;
		bool enableReverb = true;
		DWORD musicVolume = 100;
		DWORD sfxVolume = 100;

		// Load Sound keys.
		if (GetDWORDRegKey(soundKey, REGKEY_SOUND_DEVICE, &soundDevice, 1) != ERROR_SUCCESS ||
			GetBoolRegKey(soundKey, REGKEY_ENABLE_SOUND, &enableSound, true) != ERROR_SUCCESS ||
			GetBoolRegKey(soundKey, REGKEY_ENABLE_REVERB, &enableReverb, true) != ERROR_SUCCESS ||
			GetDWORDRegKey(soundKey, REGKEY_MUSIC_VOLUME, &musicVolume, 100) != ERROR_SUCCESS ||
			GetDWORDRegKey(soundKey, REGKEY_SFX_VOLUME, &sfxVolume, 100) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(controlsKey);
			RegCloseKey(keyBindingsKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(graphicsKey);
			RegCloseKey(soundKey);
			return false;
		}

		// Close registry keys.
		RegCloseKey(rootKey);
		RegCloseKey(controlsKey);
		RegCloseKey(keyBindingsKey);
		RegCloseKey(gameplayKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);

		// All configuration values found; apply configuration.
		g_Config.EnableTankCameraControl = enableThumbstickCamera;
		g_Config.InvertCameraXAxis = invertCameraXAxis;
		g_Config.InvertCameraYAxis = invertCameraYAxis;
		g_Config.EnableRumble = enableRumble;
		g_Config.MouseSensitivity = mouseSensitivity;
		g_Config.MenuOptionLoopingMode = (MenuOptionLoopingMode)menuOptionLoopingMode;

		g_Config.ControlMode = (ControlMode)controlMode;
		g_Config.SwimControlMode = (SwimControlMode)swimControlMode;
		g_Config.EnableWalkToggle = enableWalkToggle;
		g_Config.EnableCrouchToggle = enableCrouchToggle;
		g_Config.EnableClimbToggle = enableClimbToggle;
		g_Config.EnableAutoMonkeySwingJump = enableAutoMonkeySwingJump;
		g_Config.EnableAutoTargeting = enableAutoTargeting;
		g_Config.EnableOppositeActionRoll = enableOppositeActionRoll;

		g_Config.ScreenWidth = screenWidth;
		g_Config.ScreenHeight = screenHeight;
		g_Config.WindowMode = (WindowMode)windowMode;
		g_Config.FrameRateMode = (FrameRateMode)frameRateMode;
		g_Config.ShadowType = (ShadowMode)shadowMode;
		g_Config.ShadowBlobCountMax = shadowBlobsMax;
		g_Config.EnableCaustics = enableCaustics;
		g_Config.AntialiasingMode = (AntialiasingMode)antialiasingMode;
		g_Config.ShadowMapSize = shadowMapSize;
		g_Config.EnableAmbientOcclusion = enableAmbientOcclusion;
		g_Config.EnableTargetHighlighter = enableTargetHighlighter;
		g_Config.EnableSubtitles = enableSubtitles;

		g_Config.EnableSound = enableSound;
		g_Config.EnableReverb = enableReverb;
		g_Config.MusicVolume = musicVolume;
		g_Config.SfxVolume = sfxVolume;
		g_Config.SoundDevice = soundDevice;

		// Set legacy variables.
		SetVolumeTracks(musicVolume);
		SetVolumeFX(sfxVolume);

		DefaultConflict();

		return true;
	}
}
