#pragma once

#include "Math/Math.h"
#include "Specific/Input/Input.h"
#include "Renderer/RendererEnums.h"
#include "Sound/sound.h"

using namespace TEN::Input;
using namespace TEN::Math;

// Graphics keys

constexpr auto OPTION_SCREEN_WIDTH		   = "ScreenWidth";
constexpr auto OPTION_SCREEN_HEIGHT		   = "ScreenHeight";
constexpr auto OPTION_ENABLE_WINDOWED_MODE = "EnableWindowedMode";
constexpr auto OPTION_GAMMA				   = "GammaCorrection";
constexpr auto OPTION_SHADOWS			   = "ShadowsMode";
constexpr auto OPTION_SHADOW_MAP_SIZE	   = "ShadowMapSize";
constexpr auto OPTION_SHADOW_BLOBS_MAX	   = "ShadowBlobsMax";
constexpr auto OPTION_ENABLE_CAUSTICS	   = "EnableCaustics";
constexpr auto OPTION_ENABLE_DECALS		   = "EnableDecals";
constexpr auto OPTION_ANTIALIASING_MODE	   = "AntialiasingMode";
constexpr auto OPTION_AMBIENT_OCCLUSION	   = "AmbientOcclusion";
constexpr auto OPTION_HIGH_FRAMERATE       = "EnableHighFramerate";
constexpr auto OPTION_ADAPTER_NAME         = "AdapterName";

// Sound keys

constexpr auto OPTION_SOUND_DEVICE	= "SoundDevice";
constexpr auto OPTION_ENABLE_SOUND	= "EnableSound";
constexpr auto OPTION_ENABLE_REVERB = "EnableReverb";
constexpr auto OPTION_MUSIC_VOLUME	= "MusicVolume";
constexpr auto OPTION_SFX_VOLUME	= "SfxVolume";

// Gameplay keys

constexpr auto OPTION_ENABLE_SUBTITLES					= "EnableSubtitles";
constexpr auto OPTION_ENABLE_AUTO_MONKEY_JUMP			= "EnableAutoMonkeySwingJump";
constexpr auto OPTION_ENABLE_AUTO_TARGETING				= "EnableAutoTargeting";
constexpr auto OPTION_ENABLE_TARGET_HIGHLIGHTER			= "EnableTargetHighlighter";
constexpr auto OPTION_ENABLE_INTERACTION_HIGHLIGHTER	= "EnableInteractionHighlighter";
constexpr auto OPTION_ENABLE_RUMBLE						= "EnableRumble";
constexpr auto OPTION_ENABLE_THUMBSTICK_CAMERA			= "EnableThumbstickCamera";

// Input keys

constexpr auto OPTION_MOUSE_SENSITIVITY		   = "MouseSensitivity";
constexpr auto OPTION_MENU_OPTION_LOOPING_MODE = "MenuOptionLoopingMode";
constexpr auto OPTION_BIND_PREFIX			   = "bind.";

enum class MenuOptionLoopingMode
{
	AllMenus,
	SaveLoadOnly,
	Disabled
};

struct GameConfiguration
{
	static constexpr auto DEFAULT_SHADOW_MAP_SIZE	= 1024;
	static constexpr auto DEFAULT_SHADOW_BLOBS_MAX	= 16;
	static constexpr auto DEFAULT_MOUSE_SENSITIVITY = 6;

	// Graphics

	int		   ScreenWidth		  = 0;
	int		   ScreenHeight		  = 0;
	float	   Gamma			  = 1.0f;
	bool	   EnableWindowedMode = false;
	ShadowMode ShadowType		  = ShadowMode::None;
	int		   ShadowMapSize	  = DEFAULT_SHADOW_MAP_SIZE;
	int		   ShadowBlobsMax	  = DEFAULT_SHADOW_BLOBS_MAX;
	bool	   EnableCaustics	  = false;
	bool	   EnableDecals		  = true;
	bool	   EnableAmbientOcclusion = false;
	bool	   EnableHighFramerate    = true;
	AntialiasingMode AntialiasingMode = AntialiasingMode::None;

	// Sound

	int	 SoundDevice  = 0;
	bool EnableSound  = false;
	bool EnableReverb = false;
	int	 MusicVolume  = 0;
	int	 SfxVolume	  = 0;

	// Gameplay

	bool EnableSubtitles				= false;
	bool EnableAutoMonkeySwingJump		= false;
	bool EnableAutoTargeting			= false;
	bool EnableTargetHighlighter		= false;
	bool EnableInteractionHighlighter	= false;
	bool EnableRumble					= false;
	bool EnableThumbstickCamera			= false;

	// Input
	int					  MouseSensitivity		= DEFAULT_MOUSE_SENSITIVITY;
	MenuOptionLoopingMode MenuOptionLoopingMode = MenuOptionLoopingMode::SaveLoadOnly;
	BindingProfile		  Bindings				= {};

	std::vector<Vector2i>	SupportedScreenResolutions	= {};
	std::string				AdapterName					= {};
	std::vector<BassDevice> SupportedSoundDevices				= {};
};

void InitDefaultConfiguration();
bool LoadConfiguration();
bool SaveConfiguration();
void SaveAudioConfig();

extern GameConfiguration g_Configuration;
