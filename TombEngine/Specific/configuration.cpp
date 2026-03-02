#include "framework.h"
#include "Specific/configuration.h"

#include "Renderer/Renderer.h"
#include "resource.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/LanguageScript.h"
#include "Specific/EngineMain.h"
#include "Specific/Input/Input.h"
#include "Specific/trutils.h"
#include "Sound/sound.h"

using namespace TEN::Input;
using namespace TEN::Renderer;
using namespace TEN::Utils;

GameConfiguration g_Configuration;

static std::string GetConfigFilePath()
{
	char* base = SDL_GetPrefPath("TEN", "TombEngine");
	if (base == nullptr)
		return "ten.conf";

	auto path = std::string(base) + "ten.conf";
	SDL_free(base);
	return path;
}

static bool WriteAllText(const std::string& path, const std::string& text)
{
	auto stream = SDL_IOFromFile(path.c_str(), "wb");
	if (!stream) 
		return false;

	const size_t size = text.size();
	size_t width = SDL_WriteIO(stream, text.data(), size);

	SDL_CloseIO(stream);

	return (width == size);
}

static bool ReadAllText(const std::string& path, std::string& out)
{
	auto stream = SDL_IOFromFile(path.c_str(), "rb");
	if (!stream) 
		return false;

	auto size = SDL_GetIOSize(stream);
	if (size < 0)
	{ 
		SDL_CloseIO(stream);
		return false;
	}

	out.resize(static_cast<size_t>(size));

	size_t read = SDL_ReadIO(stream, out.data(), out.size());

	SDL_CloseIO(stream);

	return (read == out.size());
}

void SaveAudioConfig()
{
	SetVolumeTracks(g_Configuration.MusicVolume);
	SetVolumeFX(g_Configuration.SfxVolume);
}

void InitDefaultConfiguration()
{
	// Include default device in list.
	BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, true);

	auto currentScreenResolution = GetScreenResolution();

	g_Configuration.ScreenWidth = currentScreenResolution.x;
	g_Configuration.ScreenHeight = currentScreenResolution.y;
	g_Configuration.ShadowType = ShadowMode::Player;
	g_Configuration.ShadowMapSize = GameConfiguration::DEFAULT_SHADOW_MAP_SIZE;
	g_Configuration.ShadowBlobsMax = GameConfiguration::DEFAULT_SHADOW_BLOBS_MAX;
	g_Configuration.EnableCaustics = true;
	g_Configuration.EnableDecals = true;
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
	g_Configuration.EnableInteractionHighlighter = true;
	g_Configuration.EnableRumble = true;
	g_Configuration.EnableThumbstickCamera = false;

	g_Configuration.MouseSensitivity = GameConfiguration::DEFAULT_MOUSE_SENSITIVITY;
	g_Configuration.MenuOptionLoopingMode = MenuOptionLoopingMode::SaveLoadOnly;

	g_Configuration.SupportedScreenResolutions = GetAllSupportedScreenResolutions();
	g_Configuration.AdapterName = {};

	g_Configuration.SupportedSoundDevices = Sound_ListDevices();
}

bool LoadConfiguration()
{
	auto path = GetConfigFilePath();

	auto text = std::string();
	if (!ReadAllText(path, text))
		return false; 

	auto in = std::istringstream(text);
	auto line = std::string();
	auto section = std::string();

	InitDefaultConfiguration();

	bool foundInput = false;

	while (std::getline(in, line))
	{
		line = Trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;

		if (line.front() == '[' && line.back() == ']')
		{
			section = line.substr(1, line.size() - 2);
			continue;
		}

		auto eq = line.find('=');
		if (eq == std::string::npos)
			continue;

		auto key = Trim(line.substr(0, eq));
		auto val = Trim(line.substr(eq + 1));

		if (section == "Graphics")
		{
			if (key == "RendererAPI")
			{
				g_Configuration.RendererAPI = (GraphicsAPI)ToInt(val, (int)g_Configuration.RendererAPI);
			}
			else if (key == "ScreenWidth")
			{
				g_Configuration.ScreenWidth = ToInt(val, g_Configuration.ScreenWidth);
			}
			else if (key == "ScreenHeight")
			{
				g_Configuration.ScreenHeight = ToInt(val, g_Configuration.ScreenHeight);
			}
			else if (key == "EnableWindowedMode")
			{
				g_Configuration.EnableWindowedMode = ToBool(val, g_Configuration.EnableWindowedMode);
			}
			else if (key == "ShadowsMode")
			{
				g_Configuration.ShadowType = (ShadowMode)ToInt(val, (int)g_Configuration.ShadowType);
			}
			else if (key == "ShadowMapSize")
			{
				g_Configuration.ShadowMapSize = ToInt(val, g_Configuration.ShadowMapSize);
			}
			else if (key == "ShadowBlobsMax")
			{
				g_Configuration.ShadowBlobsMax = ToInt(val, g_Configuration.ShadowBlobsMax);
			}
			else if (key == "EnableCaustics")
			{
				g_Configuration.EnableCaustics = ToBool(val, g_Configuration.EnableCaustics);
			}
			else if (key == "EnableDecals")
			{
				g_Configuration.EnableDecals = ToBool(val, g_Configuration.EnableDecals);
			}
			else if (key == "AntialiasingMode")
			{
				g_Configuration.AntialiasingMode = (AntialiasingMode)ToInt(val, (int)g_Configuration.AntialiasingMode);
			}
			else if (key == "AmbientOcclusion")
			{
				g_Configuration.EnableAmbientOcclusion = ToBool(val, g_Configuration.EnableAmbientOcclusion);
			}
			else if (key == "EnableHighFramerate")
			{
				g_Configuration.EnableHighFramerate = ToBool(val, g_Configuration.EnableHighFramerate);
			}
			else if (key == "AdapterName")
			{
				g_Configuration.AdapterName = val;
			}
		}
		else if (section == "Sound")
		{
			if (key == "SoundDevice")      
			{
				g_Configuration.SoundDevice = ToInt(val, g_Configuration.SoundDevice);
			}
			else if (key == "EnableReverb")
			{
				g_Configuration.EnableReverb = ToBool(val, g_Configuration.EnableReverb);
			}
			else if (key == "MusicVolume")
			{
				g_Configuration.MusicVolume = ToInt(val, g_Configuration.MusicVolume);
			}
			else if (key == "SfxVolume")
			{
				g_Configuration.SfxVolume = ToInt(val, g_Configuration.SfxVolume);
			}
		}
		else if (section == "Gameplay")
		{
			if (key == "EnableSubtitles")
			{
				g_Configuration.EnableSubtitles = ToBool(val, g_Configuration.EnableSubtitles);
			}
			else if (key == "EnableAutoMonkeySwingJump")
			{
				g_Configuration.EnableAutoMonkeySwingJump = ToBool(val, g_Configuration.EnableAutoMonkeySwingJump);
			}
			else if (key == "EnableAutoTargeting")
			{
				g_Configuration.EnableAutoTargeting = ToBool(val, g_Configuration.EnableAutoTargeting);
			}
			else if (key == "EnableTargetHighlighter")
			{
				g_Configuration.EnableTargetHighlighter = ToBool(val, g_Configuration.EnableTargetHighlighter);
			}
			else if (key == "EnableInteractionHighlighter")
			{
				g_Configuration.EnableInteractionHighlighter = ToBool(val, g_Configuration.EnableInteractionHighlighter);
			}
			else if (key == "EnableRumble")
			{
				g_Configuration.EnableRumble = ToBool(val, g_Configuration.EnableRumble);
			}
			else if (key == "EnableThumbstickCamera")
			{
				g_Configuration.EnableThumbstickCamera = ToBool(val, g_Configuration.EnableThumbstickCamera);
			}
		}
		else if (section == "Input")
		{
			if (key == "MouseSensitivity")
			{
				g_Configuration.MouseSensitivity = ToInt(val, g_Configuration.MouseSensitivity);
			}
			else if (key == "MenuOptionLoopingMode")
			{
				g_Configuration.MenuOptionLoopingMode = (MenuOptionLoopingMode)ToInt(val, (int)g_Configuration.MenuOptionLoopingMode);
			}
			else if (StartsWith(key, "bind."))
			{
				foundInput = true;

				int actionId = ToInt(key.substr(5), NO_VALUE);
				int keyId = ToInt(val, NO_VALUE);
				if (actionId >= 0 && keyId >= 0)
				{
					g_Configuration.Bindings.insert({ (ActionID)actionId, keyId });
					g_Bindings.SetKeyBinding(BindingProfileID::Custom, (ActionID)actionId, keyId);
				}
			}
		}
	}

	if (!foundInput)
		g_Configuration.Bindings = g_Bindings.GetBindingProfile(BindingProfileID::Default);

	g_Configuration.EnableSound = g_Configuration.SoundDevice > 0;

	SetVolumeTracks(g_Configuration.MusicVolume);
	SetVolumeFX(g_Configuration.SfxVolume);

	DefaultConflict();

	return true;
}

bool SaveConfiguration()
{
	std::ostringstream ss;

	ss << "[Graphics]\n";
	ss << "RendererAPI=" << (int)g_Configuration.RendererAPI << "\n";
	ss << "ScreenWidth=" << g_Configuration.ScreenWidth << "\n";
	ss << "ScreenHeight=" << g_Configuration.ScreenHeight << "\n";
	ss << "EnableWindowedMode=" << (g_Configuration.EnableWindowedMode ? 1 : 0) << "\n";
	ss << "ShadowsMode=" << (int)g_Configuration.ShadowType << "\n";
	ss << "ShadowMapSize=" << g_Configuration.ShadowMapSize << "\n";
	ss << "ShadowBlobsMax=" << g_Configuration.ShadowBlobsMax << "\n";
	ss << "EnableCaustics=" << (g_Configuration.EnableCaustics ? 1 : 0) << "\n";
	ss << "EnableDecals=" << (g_Configuration.EnableDecals ? 1 : 0) << "\n";
	ss << "AntialiasingMode=" << (int)g_Configuration.AntialiasingMode << "\n";
	ss << "AmbientOcclusion=" << (g_Configuration.EnableAmbientOcclusion ? 1 : 0) << "\n";
	ss << "EnableHighFramerate=" << (g_Configuration.EnableHighFramerate ? 1 : 0) << "\n";
	ss << "AdapterName=" << g_Configuration.AdapterName << "\n\n";

	ss << "[Sound]\n";
	ss << "SoundDevice=" << g_Configuration.SoundDevice << "\n";
	ss << "EnableSound=" << (g_Configuration.EnableSound ? 1 : 0) << "\n";
	ss << "EnableReverb=" << (g_Configuration.EnableReverb ? 1 : 0) << "\n";
	ss << "MusicVolume=" << g_Configuration.MusicVolume << "\n";
	ss << "SfxVolume=" << g_Configuration.SfxVolume << "\n\n";

	ss << "[Gameplay]\n";
	ss << "EnableSubtitles=" << (g_Configuration.EnableSubtitles ? 1 : 0) << "\n";
	ss << "EnableAutoMonkeySwingJump=" << (g_Configuration.EnableAutoMonkeySwingJump ? 1 : 0) << "\n";
	ss << "EnableAutoTargeting=" << (g_Configuration.EnableAutoTargeting ? 1 : 0) << "\n";
	ss << "EnableTargetHighlighter=" << (g_Configuration.EnableTargetHighlighter ? 1 : 0) << "\n";
	ss << "EnableInteractionHighlighter=" << (g_Configuration.EnableInteractionHighlighter ? 1 : 0) << "\n";
	ss << "EnableRumble=" << (g_Configuration.EnableRumble ? 1 : 0) << "\n";
	ss << "EnableThumbstickCamera=" << (g_Configuration.EnableThumbstickCamera ? 1 : 0) << "\n\n";

	ss << "[Input]\n";
	ss << "MouseSensitivity=" << g_Configuration.MouseSensitivity << "\n";
	ss << "MenuOptionLoopingMode=" << (int)g_Configuration.MenuOptionLoopingMode << "\n";

	if (g_Configuration.Bindings.empty())
		g_Configuration.Bindings = DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;

	for (const auto& kv : g_Configuration.Bindings)
	{
		ss << "bind." << (int)kv.first << "=" << (int)kv.second << "\n";
	}
	ss << "\n";

	auto path = GetConfigFilePath();
	return WriteAllText(path, ss.str());
}