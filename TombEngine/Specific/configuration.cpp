#include "framework.h"
#include "Specific/configuration.h"

#include "Renderer/Renderer.h"
#include "resource.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/LanguageScript.h"
#include "Specific/EngineMain.h"
#include "Specific/Input/Input.h"
#include "Specific/Serialization/flatbuffers/ten_configuration_generated.h"
#include "Specific/trutils.h"
#include "Sound/sound.h"

using namespace flatbuffers;
using namespace TEN::Input;
using namespace TEN::Renderer;
using namespace TEN::Serialization::Config;
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

static bool ReadConfigFileData(const std::string& path, std::vector<unsigned char>& fileData)
{
	auto file = std::ifstream();

	try
	{
		file.open(std::filesystem::path{ path }, std::ios_base::binary);
		if (!file.is_open())
			return false;

		file.seekg(0, std::ios::end);
		auto length = (std::streamsize)file.tellg();
		file.seekg(0, std::ios::beg);

		if (length <= 0)
		{
			file.close();
			return false;
		}

		fileData.resize((size_t)length);
		if (!file.read(reinterpret_cast<char*>(fileData.data()), length))
		{
			file.close();
			return false;
		}
		file.close();

		return true;
	}
	catch (std::exception&)
	{
		if (file.is_open())
			file.close();

		return false;
	}
}

static bool ExtractConfigurationBuffer(const std::vector<unsigned char>& fileData, std::vector<unsigned char>& buffer)
{
	if (fileData.size() <= sizeof(int))
		return false;

	int size = 0;
	memcpy(&size, fileData.data(), sizeof(size));
	if (size <= 0 || (sizeof(int) + (size_t)size) > fileData.size())
		return false;

	buffer.assign(fileData.begin() + sizeof(size), fileData.begin() + sizeof(size) + size);
	return true;
}

static void LoadBinding(int actionId, int keyId)
{
	if (actionId < 0 || keyId < 0 || actionId >= (int)ActionID::Count || keyId == KEY_UNASSIGNED)
		return;

	auto action = (ActionID)actionId;
	g_Configuration.Bindings.insert({ action, keyId });
	g_Bindings.SetKeyBinding(BindingProfileID::Custom, action, keyId);
}

static bool LoadConfigurationBuffer(const std::vector<unsigned char>& fileData)
{
	auto buffer = std::vector<unsigned char>();
	if (!ExtractConfigurationBuffer(fileData, buffer))
		return false;

	TENLog(fmt::format("Loading configuration: {}", GetConfigFilePath()), LogLevel::Info);

	auto verifier = Verifier(buffer.data(), buffer.size());
	if (!VerifyConfigurationBuffer(verifier))
		return false;

	InitDefaultConfiguration();

	const auto* config = GetConfiguration(buffer.data());
	g_Configuration.ScreenWidth = config->screen_width();
	g_Configuration.ScreenHeight = config->screen_height();
	g_Configuration.Gamma = config->gamma();
	g_Configuration.EnableWindowedMode = config->enable_windowed_mode();
	g_Configuration.ShadowType = (ShadowMode)config->shadow_type();
	g_Configuration.ShadowMapSize = config->shadow_map_size();
	g_Configuration.ShadowBlobsMax = config->shadow_blobs_max();
	g_Configuration.EnableCaustics = config->enable_caustics();
	g_Configuration.EnableDecals = config->enable_decals();
	g_Configuration.AntialiasingMode = (AntialiasingMode)config->antialiasing_mode();
	g_Configuration.EnableAmbientOcclusion = config->enable_ambient_occlusion();
	g_Configuration.EnableHighFramerate = config->enable_high_framerate();

	if (config->adapter_name() != nullptr)
		g_Configuration.AdapterName = config->adapter_name()->str();

	g_Configuration.SoundDevice = config->sound_device();
	g_Configuration.EnableReverb = config->enable_reverb();
	g_Configuration.MusicVolume = config->music_volume();
	g_Configuration.SfxVolume = config->sfx_volume();

	g_Configuration.EnableSubtitles = config->enable_subtitles();
	g_Configuration.EnableAutoMonkeySwingJump = config->enable_auto_monkey_swing_jump();
	g_Configuration.EnableAutoTargeting = config->enable_auto_targeting();
	g_Configuration.EnableTargetHighlighter = config->enable_target_highlighter();
	g_Configuration.EnableInteractionHighlighter = config->enable_interaction_highlighter();
	g_Configuration.EnableRumble = config->enable_rumble();
	g_Configuration.EnableThumbstickCamera = config->enable_thumbstick_camera();

	g_Configuration.MouseSensitivity = config->mouse_sensitivity();
	g_Configuration.MenuOptionLoopingMode = (MenuOptionLoopingMode)config->menu_option_looping_mode();

	int gamepadType = std::clamp(config->last_gamepad_type(), 0, (int)GamepadType::Count - 1);
	g_Configuration.LastGamepadType = (GamepadType)gamepadType;

	if (config->bindings() != nullptr)
	{
		for (const auto* binding : *config->bindings())
			LoadBinding(binding->action_id(), binding->key_id());
	}

	if (g_Configuration.Bindings.empty())
		g_Configuration.Bindings = g_Bindings.GetBindingProfile(BindingProfileID::Default);

	return true;
}

void SetAudioConfiguration(const GameConfiguration& config)
{
	SetVolumeTracks(config.MusicVolume);
	SetVolumeFX(config.SfxVolume);
}

void InitDefaultConfiguration()
{
	// Include default device in list.
	BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, true);

	auto currentScreenResolution = GetScreenResolution();

	g_Configuration.ScreenWidth = currentScreenResolution.x;
	g_Configuration.ScreenHeight = currentScreenResolution.y;
	g_Configuration.EnableWindowedMode = false;
	g_Configuration.ShadowType = ShadowMode::Player;
	g_Configuration.ShadowMapSize = GameConfiguration::DEFAULT_SHADOW_MAP_SIZE;
	g_Configuration.ShadowBlobsMax = GameConfiguration::DEFAULT_SHADOW_BLOBS_MAX;
	g_Configuration.EnableCaustics = true;
	g_Configuration.EnableDecals = true;
	g_Configuration.AntialiasingMode = AntialiasingMode::Medium;
	g_Configuration.EnableAmbientOcclusion = true;
	g_Configuration.EnableHighFramerate = true;
	g_Configuration.Gamma = 1.0f;

	g_Configuration.SoundDevice = 1;
	g_Configuration.EnableSound = true;
	g_Configuration.EnableReverb = true;
	g_Configuration.MusicVolume = VOLUME_MAX;
	g_Configuration.SfxVolume = VOLUME_MAX;

	g_Configuration.EnableSubtitles = true;
	g_Configuration.EnableAutoMonkeySwingJump = false;
	g_Configuration.EnableAutoTargeting = true;
	g_Configuration.EnableTargetHighlighter = true;
	g_Configuration.EnableInteractionHighlighter = true;
	g_Configuration.EnableRumble = true;
	g_Configuration.EnableThumbstickCamera = false;

	g_Configuration.MouseSensitivity = GameConfiguration::DEFAULT_MOUSE_SENSITIVITY;
	g_Configuration.MenuOptionLoopingMode = MenuOptionLoopingMode::SaveLoadOnly;
	g_Configuration.LastGamepadType = GamepadType::Xbox;
	g_Configuration.Bindings = {};

	g_Configuration.SupportedScreenResolutions = GetAllSupportedScreenResolutions();
	g_Configuration.AdapterName = g_Renderer.GetDefaultAdapterName();

	g_Configuration.SupportedSoundDevices = Sound_ListDevices();
}

bool LoadConfiguration()
{
	auto path = GetConfigFilePath();
	auto fileData = std::vector<unsigned char>();
	if (!ReadConfigFileData(path, fileData))
		return false;

	if (!LoadConfigurationBuffer(fileData))
	{
		TENLog("Configuration data is incorrect and was not loaded! Incorrect flatbuffer format or memory corruption?", LogLevel::Error);
		return false;
	}

	g_Configuration.EnableSound = g_Configuration.SoundDevice > 0;
	SetAudioConfiguration(g_Configuration);
	DefaultConflict();
	SaveConfiguration();

	return true;
}

static std::vector<unsigned char> BuildConfigurationBuffer()
{
	auto fbb = FlatBufferBuilder();
	auto adapterNameOffset = fbb.CreateString(g_Configuration.AdapterName);

	auto bindings = std::vector<Offset<Binding>>();
	bindings.reserve(g_Configuration.Bindings.size());

	for (const auto& [action, keyId] : g_Configuration.Bindings)
		bindings.push_back(CreateBinding(fbb, (int)action, keyId));

	auto bindingsOffset = fbb.CreateVector(bindings);

	ConfigurationBuilder builder{ fbb };
	builder.add_screen_width(g_Configuration.ScreenWidth);
	builder.add_screen_height(g_Configuration.ScreenHeight);
	builder.add_gamma(g_Configuration.Gamma);
	builder.add_enable_windowed_mode(g_Configuration.EnableWindowedMode);
	builder.add_shadow_type((int)g_Configuration.ShadowType);
	builder.add_shadow_map_size(g_Configuration.ShadowMapSize);
	builder.add_shadow_blobs_max(g_Configuration.ShadowBlobsMax);
	builder.add_enable_caustics(g_Configuration.EnableCaustics);
	builder.add_enable_decals(g_Configuration.EnableDecals);
	builder.add_antialiasing_mode((int)g_Configuration.AntialiasingMode);
	builder.add_enable_ambient_occlusion(g_Configuration.EnableAmbientOcclusion);
	builder.add_enable_high_framerate(g_Configuration.EnableHighFramerate);
	builder.add_adapter_name(adapterNameOffset);
	builder.add_sound_device(g_Configuration.SoundDevice);
	builder.add_enable_reverb(g_Configuration.EnableReverb);
	builder.add_music_volume(g_Configuration.MusicVolume);
	builder.add_sfx_volume(g_Configuration.SfxVolume);
	builder.add_enable_subtitles(g_Configuration.EnableSubtitles);
	builder.add_enable_auto_monkey_swing_jump(g_Configuration.EnableAutoMonkeySwingJump);
	builder.add_enable_auto_targeting(g_Configuration.EnableAutoTargeting);
	builder.add_enable_target_highlighter(g_Configuration.EnableTargetHighlighter);
	builder.add_enable_interaction_highlighter(g_Configuration.EnableInteractionHighlighter);
	builder.add_enable_rumble(g_Configuration.EnableRumble);
	builder.add_enable_thumbstick_camera(g_Configuration.EnableThumbstickCamera);
	builder.add_mouse_sensitivity(g_Configuration.MouseSensitivity);
	builder.add_menu_option_looping_mode((int)g_Configuration.MenuOptionLoopingMode);
	builder.add_last_gamepad_type((int)g_Configuration.LastGamepadType);
	builder.add_bindings(bindingsOffset);

	auto config = builder.Finish();
	FinishConfigurationBuffer(fbb, config);

	auto buffer = fbb.GetBufferPointer();
	auto size = fbb.GetSize();
	return std::vector<unsigned char>(buffer, buffer + size);
}

bool SaveConfiguration()
{
	if (g_Configuration.Bindings.empty())
		g_Configuration.Bindings = DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;

	auto buffer = BuildConfigurationBuffer();
	auto path = GetConfigFilePath();
	auto configPath = std::filesystem::path{ path };

	auto parentPath = configPath.parent_path();
	if (!parentPath.empty() && !std::filesystem::is_directory(parentPath))
	{
		auto errorCode = std::error_code();
		std::filesystem::create_directories(parentPath, errorCode);

		if (errorCode)
			return false;
	}

	TENLog(fmt::format("Saving configuration {}.", path), LogLevel::Info);

	auto fileOut = std::ofstream();
	try
	{
		fileOut.open(configPath, std::ios_base::binary | std::ios_base::out);
		if (!fileOut.is_open())
			return false;

		int size = (int)buffer.size();
		fileOut.write(reinterpret_cast<const char*>(&size), sizeof(size));
		fileOut.write(reinterpret_cast<const char*>(buffer.data()), size);
		fileOut.close();
		return true;
	}
	catch (std::exception&)
	{
		if (fileOut.is_open())
			fileOut.close();

		return false;
	}
}