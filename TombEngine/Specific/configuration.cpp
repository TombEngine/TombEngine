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

GameConfiguration g_Configuration;

static const auto SAVABLE_ACTION_GROUP_IDS = std::vector<ActionGroupID>
{
	ActionGroupID::General,
	ActionGroupID::Vehicle,
	ActionGroupID::Quick,
	ActionGroupID::Menu
};

void LoadResolutionsInCombobox(HWND handle)
{
	HWND cbHandle = GetDlgItem(handle, IDC_RESOLUTION);

	SendMessageA(cbHandle, CB_RESETCONTENT, 0, 0);

	for (int i = 0; i < g_Configuration.SupportedScreenResolutions.size(); i++)
	{
		auto screenResolution = g_Configuration.SupportedScreenResolutions[i];

		char* str = (char*)malloc(255);
		ZeroMemory(str, 255);
		sprintf(str, "%d x %d", screenResolution.x, screenResolution.y);

		SendMessageA(cbHandle, CB_ADDSTRING, i, (LPARAM)(str));

		free(str);
	}

	SendMessageA(cbHandle, CB_SETCURSEL, 0, 0);
	SendMessageA(cbHandle, CB_SETMINVISIBLE, 20, 0);
}

void LoadSoundDevicesInCombobox(HWND handle)
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

BOOL CALLBACK DialogProc(HWND handle, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Vector2i mode;
	int selectedMode;

	switch (msg)
	{
	case WM_INITDIALOG:
		//DB_Log(6, "WM_INITDIALOG");

		SendMessageA(GetDlgItem(handle, IDC_GROUP_GFXADAPTER), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_DISPLAY_ADAPTER));
		SendMessageA(GetDlgItem(handle, IDC_GROUP_OUTPUT_SETTINGS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_OUTPUT_SETTINGS));
		SendMessageA(GetDlgItem(handle, IDOK), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_OK));
		SendMessageA(GetDlgItem(handle, IDCANCEL), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_CANCEL));
		SendMessageA(GetDlgItem(handle, IDC_GROUP_RESOLUTION), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_SCREEN_RESOLUTION));
		SendMessageA(GetDlgItem(handle, IDC_GROUP_SOUND), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_SOUND));
		SendMessageA(GetDlgItem(handle, IDC_ENABLE_SOUNDS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_ENABLE_SOUND));
		SendMessageA(GetDlgItem(handle, IDC_WINDOWED), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_WINDOWED));
		SendMessageA(GetDlgItem(handle, IDC_GROUP_RENDER_OPTIONS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_RENDER_OPTIONS));
		SendMessageA(GetDlgItem(handle, IDC_SHADOWS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_SHADOWS));
		SendMessageA(GetDlgItem(handle, IDC_CAUSTICS), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_CAUSTICS));
		SendMessageA(GetDlgItem(handle, IDC_ANTIALIASING), WM_SETTEXT, 0, (LPARAM)g_GameFlow->GetString(STRING_ANTIALIASING));

		LoadResolutionsInCombobox(handle);
		LoadSoundDevicesInCombobox(handle);

		// Set some default values.
		g_Configuration.EnableAutoTargeting = true;

		g_Configuration.AntialiasingMode = AntialiasingMode::Low;
		SendDlgItemMessage(handle, IDC_ANTIALIASING, BM_SETCHECK, 1, 0);

		g_Configuration.ShadowType = ShadowMode::Player;
		SendDlgItemMessage(handle, IDC_SHADOWS, BM_SETCHECK, 1, 0);

		g_Configuration.EnableCaustics = true;
		SendDlgItemMessage(handle, IDC_CAUSTICS, BM_SETCHECK, 1, 0);

		g_Configuration.EnableWindowedMode = true;
		SendDlgItemMessage(handle, IDC_WINDOWED, BM_SETCHECK, 1, 0);

		g_Configuration.EnableSound = true;
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
				g_Configuration.EnableWindowedMode = (SendDlgItemMessage(handle, IDC_WINDOWED, BM_GETCHECK, 0, 0));
				g_Configuration.ShadowType = (ShadowMode)(SendDlgItemMessage(handle, IDC_SHADOWS, BM_GETCHECK, 0, 0));
				g_Configuration.EnableCaustics = (SendDlgItemMessage(handle, IDC_CAUSTICS, BM_GETCHECK, 0, 0));
				g_Configuration.AntialiasingMode = (AntialiasingMode)(SendDlgItemMessage(handle, IDC_ANTIALIASING, BM_GETCHECK, 0, 0));
				g_Configuration.EnableSound = (SendDlgItemMessage(handle, IDC_ENABLE_SOUNDS, BM_GETCHECK, 0, 0));
				selectedMode = (SendDlgItemMessage(handle, IDC_RESOLUTION, CB_GETCURSEL, 0, 0));
				mode = g_Configuration.SupportedScreenResolutions[selectedMode];
				g_Configuration.ScreenWidth = mode.x;
				g_Configuration.ScreenHeight = mode.y;
				g_Configuration.SoundDevice = (SendDlgItemMessage(handle, IDC_SNDADAPTER, CB_GETCURSEL, 0, 0)) + 1;

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

	// Open Graphics subkey.
	HKEY graphicsKey = NULL;
	if (RegCreateKeyExA(rootKey, REGKEY_GRAPHICS, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &graphicsKey, NULL) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		return false;
	}

	// Set Graphics keys.
	if (SetDWORDRegKey(graphicsKey, REGKEY_SCREEN_WIDTH, g_Configuration.ScreenWidth) != ERROR_SUCCESS ||
		SetDWORDRegKey(graphicsKey, REGKEY_SCREEN_HEIGHT, g_Configuration.ScreenHeight) != ERROR_SUCCESS ||
		SetBoolRegKey(graphicsKey, REGKEY_ENABLE_WINDOWED_MODE, g_Configuration.EnableWindowedMode) != ERROR_SUCCESS ||
		SetDWORDRegKey(graphicsKey, REGKEY_SHADOWS, DWORD(g_Configuration.ShadowType)) != ERROR_SUCCESS ||
		SetDWORDRegKey(graphicsKey, REGKEY_SHADOW_MAP_SIZE, g_Configuration.ShadowMapSize) != ERROR_SUCCESS ||
		SetDWORDRegKey(graphicsKey, REGKEY_SHADOW_BLOBS_MAX, g_Configuration.ShadowBlobsMax) != ERROR_SUCCESS ||
		SetBoolRegKey(graphicsKey, REGKEY_ENABLE_CAUSTICS, g_Configuration.EnableCaustics) != ERROR_SUCCESS ||
		SetDWORDRegKey(graphicsKey, REGKEY_ANTIALIASING_MODE, (DWORD)g_Configuration.AntialiasingMode) != ERROR_SUCCESS ||
		SetBoolRegKey(graphicsKey, REGKEY_AMBIENT_OCCLUSION, g_Configuration.EnableAmbientOcclusion) != ERROR_SUCCESS ||
		SetBoolRegKey(graphicsKey, REGKEY_HIGH_FRAMERATE, g_Configuration.EnableHighFramerate) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		return false;
	}

	// Open Sound subkey.
	HKEY soundKey = NULL;
	if (RegCreateKeyExA(rootKey, REGKEY_SOUND, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &soundKey, NULL) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		return false;
	}

	// Set Sound keys.
	if (SetDWORDRegKey(soundKey, REGKEY_SOUND_DEVICE, g_Configuration.SoundDevice) != ERROR_SUCCESS ||
		SetBoolRegKey(soundKey, REGKEY_ENABLE_SOUND, g_Configuration.EnableSound) != ERROR_SUCCESS ||
		SetBoolRegKey(soundKey, REGKEY_ENABLE_REVERB, g_Configuration.EnableReverb) != ERROR_SUCCESS ||
		SetDWORDRegKey(soundKey, REGKEY_MUSIC_VOLUME, g_Configuration.MusicVolume) != ERROR_SUCCESS ||
		SetDWORDRegKey(soundKey, REGKEY_SFX_VOLUME, g_Configuration.SfxVolume) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		return false;
	}

	// Open Gameplay subkey.
	HKEY gameplayKey = NULL;
	if (RegCreateKeyExA(rootKey, REGKEY_GAMEPLAY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &gameplayKey, NULL) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		RegCloseKey(gameplayKey);
		return false;
	}

	// Set Gameplay keys.
	if (SetBoolRegKey(gameplayKey, REGKEY_ENABLE_SUBTITLES, g_Configuration.EnableSubtitles) != ERROR_SUCCESS ||
		SetBoolRegKey(gameplayKey, REGKEY_ENABLE_AUTO_MONKEY_JUMP, g_Configuration.EnableAutoMonkeySwingJump) != ERROR_SUCCESS ||
		SetBoolRegKey(gameplayKey, REGKEY_ENABLE_AUTO_TARGETING, g_Configuration.EnableAutoTargeting) != ERROR_SUCCESS ||
		SetBoolRegKey(gameplayKey, REGKEY_ENABLE_TARGET_HIGHLIGHTER, g_Configuration.EnableTargetHighlighter) != ERROR_SUCCESS ||
		SetBoolRegKey(gameplayKey, REGKEY_ENABLE_RUMBLE, g_Configuration.EnableRumble) != ERROR_SUCCESS ||
		SetBoolRegKey(gameplayKey, REGKEY_ENABLE_THUMBSTICK_CAMERA, g_Configuration.EnableThumbstickCamera) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		RegCloseKey(gameplayKey);
		return false;
	}

	// Open Input subkey.
	HKEY inputKey = NULL;
	if (RegCreateKeyExA(rootKey, REGKEY_INPUT, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &inputKey, NULL) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		RegCloseKey(gameplayKey);
		RegCloseKey(inputKey);
		return false;
	}

	// Set Input keys.
	if (SetDWORDRegKey(inputKey, REGKEY_MOUSE_SENSITIVITY, g_Configuration.MouseSensitivity) != ERROR_SUCCESS ||
		SetDWORDRegKey(inputKey, REGKEY_MENU_OPTION_LOOPING_MODE, (int)g_Configuration.MenuOptionLoopingMode) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		RegCloseKey(gameplayKey);
		RegCloseKey(inputKey);
		return false;
	}

	if (g_Configuration.Bindings.empty())
		g_Configuration.Bindings = DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;

	// Set Input binding keys.
	for (auto actionGroupID : SAVABLE_ACTION_GROUP_IDS)
	{
		const auto& actionIDGroup = ACTION_ID_GROUPS[(int)actionGroupID];
		for (auto actionID : actionIDGroup)
		{
			char buffer[9];
			sprintf(buffer, "Action%d", (int)actionID);

			if (SetDWORDRegKey(inputKey, buffer, g_Configuration.Bindings.at(actionID)) != ERROR_SUCCESS)
			{
				RegCloseKey(rootKey);
				RegCloseKey(graphicsKey);
				RegCloseKey(soundKey);
				RegCloseKey(gameplayKey);
				RegCloseKey(inputKey);
				return false;
			}
		}
	}

	// Close registry keys.
	RegCloseKey(rootKey);
	RegCloseKey(graphicsKey);
	RegCloseKey(soundKey);
	RegCloseKey(gameplayKey);
	RegCloseKey(inputKey);
	return true;
}

void SaveAudioConfig()
{
	SetVolumeTracks(g_Configuration.MusicVolume);
	SetVolumeFX(g_Configuration.SfxVolume);
}

void InitDefaultConfiguration()
{
	// Include default device into the list
	BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, true);

	auto currentScreenResolution = GetScreenResolution();

	g_Configuration.ScreenWidth = currentScreenResolution.x;
	g_Configuration.ScreenHeight = currentScreenResolution.y;
	g_Configuration.ShadowType = ShadowMode::Player;
	g_Configuration.ShadowMapSize = GameConfiguration::DEFAULT_SHADOW_MAP_SIZE;
	g_Configuration.ShadowBlobsMax = GameConfiguration::DEFAULT_SHADOW_BLOBS_MAX;
	g_Configuration.EnableCaustics = true;
	g_Configuration.AntialiasingMode = AntialiasingMode::Medium;
	g_Configuration.EnableAmbientOcclusion = true;
	g_Configuration.EnableHighFramerate = true;

	g_Configuration.SoundDevice = 1;
	g_Configuration.EnableSound = true;
	g_Configuration.EnableReverb = true;
	g_Configuration.MusicVolume = 100;
	g_Configuration.SfxVolume = 100;

	g_Configuration.EnableSubtitles = true;
	g_Configuration.EnableAutoMonkeySwingJump = false;
	g_Configuration.EnableAutoTargeting = true;
	g_Configuration.EnableTargetHighlighter = true;
	g_Configuration.EnableRumble = true;
	g_Configuration.EnableThumbstickCamera = false;

	g_Configuration.MouseSensitivity = GameConfiguration::DEFAULT_MOUSE_SENSITIVITY;
	g_Configuration.MenuOptionLoopingMode = MenuOptionLoopingMode::SaveLoadOnly;

	g_Configuration.SupportedScreenResolutions = GetAllSupportedScreenResolutions();
	g_Configuration.AdapterName = g_Renderer.GetDefaultAdapterName();
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

	// Open Graphics subkey.
	HKEY graphicsKey = NULL;
	if (RegOpenKeyExA(rootKey, REGKEY_GRAPHICS, 0, KEY_READ, &graphicsKey) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		return false;
	}

	DWORD screenWidth = 0;
	DWORD screenHeight = 0;
	bool enableWindowedMode = false;
	DWORD shadowMode = 1;
	DWORD shadowMapSize = GameConfiguration::DEFAULT_SHADOW_MAP_SIZE;
	DWORD shadowBlobsMax = GameConfiguration::DEFAULT_SHADOW_BLOBS_MAX;
	bool enableCaustics = false;
	DWORD antialiasingMode = 1;
	bool enableAmbientOcclusion = false;
	bool enableHighFramerate = false;

	// Load Graphics keys.
	if (GetDWORDRegKey(graphicsKey, REGKEY_SCREEN_WIDTH, &screenWidth, 0) != ERROR_SUCCESS ||
		GetDWORDRegKey(graphicsKey, REGKEY_SCREEN_HEIGHT, &screenHeight, 0) != ERROR_SUCCESS ||
		GetBoolRegKey(graphicsKey, REGKEY_ENABLE_WINDOWED_MODE, &enableWindowedMode, false) != ERROR_SUCCESS ||
		GetDWORDRegKey(graphicsKey, REGKEY_SHADOWS, &shadowMode, 1) != ERROR_SUCCESS ||
		GetDWORDRegKey(graphicsKey, REGKEY_SHADOW_MAP_SIZE, &shadowMapSize, GameConfiguration::DEFAULT_SHADOW_MAP_SIZE) != ERROR_SUCCESS ||
		GetDWORDRegKey(graphicsKey, REGKEY_SHADOW_BLOBS_MAX, &shadowBlobsMax, GameConfiguration::DEFAULT_SHADOW_BLOBS_MAX) != ERROR_SUCCESS ||
		GetBoolRegKey(graphicsKey, REGKEY_ENABLE_CAUSTICS, &enableCaustics, true) != ERROR_SUCCESS ||
		GetDWORDRegKey(graphicsKey, REGKEY_ANTIALIASING_MODE, &antialiasingMode, true) != ERROR_SUCCESS ||
		GetBoolRegKey(graphicsKey, REGKEY_AMBIENT_OCCLUSION, &enableAmbientOcclusion, false) != ERROR_SUCCESS ||
		GetBoolRegKey(graphicsKey, REGKEY_HIGH_FRAMERATE, &enableHighFramerate, false) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		return false;
	}

	// Open Sound subkey.
	HKEY soundKey = NULL;
	if (RegOpenKeyExA(rootKey, REGKEY_SOUND, 0, KEY_READ, &soundKey) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
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
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		return false;
	}

	// Open Gameplay subkey.
	HKEY gameplayKey = NULL;
	if (RegOpenKeyExA(rootKey, REGKEY_GAMEPLAY, 0, KEY_READ, &gameplayKey) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		RegCloseKey(gameplayKey);
		return false;
	}

	bool enableAutoMonkeySwingJump = false;
	bool enableSubtitles = true;
	bool enableAutoTargeting = true;
	bool enableTargetHighlighter = true;
	bool enableRumble = true;
	bool enableThumbstickCamera = true;

	// Load Gameplay keys.
	if (GetBoolRegKey(gameplayKey, REGKEY_ENABLE_AUTO_MONKEY_JUMP, &enableAutoMonkeySwingJump, true) != ERROR_SUCCESS ||
		GetBoolRegKey(gameplayKey, REGKEY_ENABLE_SUBTITLES, &enableSubtitles, true) != ERROR_SUCCESS ||
		GetBoolRegKey(gameplayKey, REGKEY_ENABLE_AUTO_TARGETING, &enableAutoTargeting, true) != ERROR_SUCCESS ||
		GetBoolRegKey(gameplayKey, REGKEY_ENABLE_TARGET_HIGHLIGHTER, &enableTargetHighlighter, true) != ERROR_SUCCESS ||
		GetBoolRegKey(gameplayKey, REGKEY_ENABLE_RUMBLE, &enableRumble, true) != ERROR_SUCCESS ||
		GetBoolRegKey(gameplayKey, REGKEY_ENABLE_THUMBSTICK_CAMERA, &enableThumbstickCamera, true) != ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		RegCloseKey(graphicsKey);
		RegCloseKey(soundKey);
		RegCloseKey(gameplayKey);
		return false;
	}

	DWORD mouseSensitivity = GameConfiguration::DEFAULT_MOUSE_SENSITIVITY;
	DWORD menuOptionLoopingMode = (DWORD)MenuOptionLoopingMode::SaveLoadOnly;

	// Load Input keys.
	HKEY inputKey = NULL;
	if (RegOpenKeyExA(rootKey, REGKEY_INPUT, 0, KEY_READ, &inputKey) == ERROR_SUCCESS)
	{
		if (GetDWORDRegKey(inputKey, REGKEY_MOUSE_SENSITIVITY, &mouseSensitivity, GameConfiguration::DEFAULT_MOUSE_SENSITIVITY) != ERROR_SUCCESS ||
			GetDWORDRegKey(inputKey, REGKEY_MENU_OPTION_LOOPING_MODE, &menuOptionLoopingMode, (DWORD)MenuOptionLoopingMode::SaveLoadOnly) != ERROR_SUCCESS)
		{
			RegCloseKey(rootKey);
			RegCloseKey(graphicsKey);
			RegCloseKey(soundKey);
			RegCloseKey(gameplayKey);
			RegCloseKey(inputKey);
			return false;
		}

		for (auto actionGroupID : SAVABLE_ACTION_GROUP_IDS)
		{
			const auto& actionIDGroup = ACTION_ID_GROUPS[(int)actionGroupID];
			for (auto actionID : actionIDGroup)
			{
				DWORD tempKeyID = 0;
				char buffer[9];
				sprintf(buffer, "Action%d", (int)actionID);

				int boundKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Default, actionID);

				if (GetDWORDRegKey(inputKey, buffer, &tempKeyID, boundKeyID) != ERROR_SUCCESS)
				{
					RegCloseKey(rootKey);
					RegCloseKey(graphicsKey);
					RegCloseKey(soundKey);
					RegCloseKey(gameplayKey);
					RegCloseKey(inputKey);
					return false;
				}

				g_Configuration.Bindings.insert({ actionID, tempKeyID });
				g_Bindings.SetKeyBinding(BindingProfileID::Custom, actionID, tempKeyID);
			}
		}

		RegCloseKey(inputKey);
	}
	// Input key doesn't exist; use default bindings.
	else
	{
		g_Configuration.Bindings = g_Bindings.GetBindingProfile(BindingProfileID::Default);
	}

	RegCloseKey(rootKey);
	RegCloseKey(graphicsKey);
	RegCloseKey(soundKey);
	RegCloseKey(gameplayKey);

	// All configuration values found; apply configuration to engine.
	g_Configuration.ScreenWidth = screenWidth;
	g_Configuration.ScreenHeight = screenHeight;
	g_Configuration.EnableWindowedMode = enableWindowedMode;
	g_Configuration.ShadowType = (ShadowMode)shadowMode;
	g_Configuration.ShadowBlobsMax = shadowBlobsMax;
	g_Configuration.EnableCaustics = enableCaustics;
	g_Configuration.AntialiasingMode = (AntialiasingMode)antialiasingMode;
	g_Configuration.ShadowMapSize = shadowMapSize;
	g_Configuration.EnableAmbientOcclusion = enableAmbientOcclusion;
	g_Configuration.EnableHighFramerate = enableHighFramerate;

	g_Configuration.EnableSound = enableSound;
	g_Configuration.EnableReverb = enableReverb;
	g_Configuration.MusicVolume = musicVolume;
	g_Configuration.SfxVolume = sfxVolume;
	g_Configuration.SoundDevice = soundDevice;

	g_Configuration.EnableSubtitles = enableSubtitles;
	g_Configuration.EnableAutoMonkeySwingJump = enableAutoMonkeySwingJump;
	g_Configuration.EnableAutoTargeting = enableAutoTargeting;
	g_Configuration.EnableTargetHighlighter = enableTargetHighlighter;
	g_Configuration.EnableRumble = enableRumble;
	g_Configuration.EnableThumbstickCamera = enableThumbstickCamera;

	g_Configuration.MouseSensitivity = mouseSensitivity;
	g_Configuration.MenuOptionLoopingMode = (MenuOptionLoopingMode)menuOptionLoopingMode;

	// Set legacy variables.
	SetVolumeTracks(musicVolume);
	SetVolumeFX(sfxVolume);

	DefaultConflict();

	return true;
}

LONG SetDWORDRegKey(HKEY hKey, LPCSTR strValueName, DWORD nValue)
{
	return RegSetValueExA(hKey, strValueName, 0, REG_DWORD, reinterpret_cast<LPBYTE>(&nValue), sizeof(DWORD));
}

LONG SetBoolRegKey(HKEY hKey, LPCSTR strValueName, bool bValue)
{
	return SetDWORDRegKey(hKey, strValueName, (bValue ? 1 : 0));
}

LONG SetStringRegKey(HKEY hKey, LPCSTR strValueName, char* strValue)
{
	return 1; // RegSetValueExA(hKey, strValueName, 0, REG_DWORD, reinterpret_cast<LPBYTE>(&nValue), sizeof(DWORD));
}

LONG GetDWORDRegKey(HKEY hKey, LPCSTR strValueName, DWORD* nValue, DWORD nDefaultValue)
{
	*nValue = nDefaultValue;

	DWORD dwBufferSize(sizeof(DWORD));
	DWORD nResult(0);
	LONG nError = ::RegQueryValueEx(
		hKey,
		strValueName,
		0,
		NULL,
		reinterpret_cast<LPBYTE>(&nResult),
		&dwBufferSize);

	if (ERROR_SUCCESS == nError)
		*nValue = nResult;

	return nError;
}

LONG GetBoolRegKey(HKEY hKey, LPCSTR strValueName, bool* bValue, bool bDefaultValue)
{
	DWORD nDefValue((bDefaultValue) ? 1 : 0);
	DWORD nResult(nDefValue);
	LONG nError = GetDWORDRegKey(hKey, strValueName, &nResult, nDefValue);

	if (ERROR_SUCCESS == nError)
		*bValue = (nResult != 0);

	return nError;
}

LONG GetStringRegKey(HKEY hKey, LPCSTR strValueName, char** strValue, char* strDefaultValue)
{
	*strValue = strDefaultValue;
	char szBuffer[512];
	DWORD dwBufferSize = sizeof(szBuffer);
	ULONG nError;
	nError = RegQueryValueEx(hKey, strValueName, 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
	if (ERROR_SUCCESS == nError)
	{
		*strValue = szBuffer;
	}
	return nError;
}