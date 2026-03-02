#pragma once

#include "Math/Math.h"
#include "Specific/Input/Input.h"
#include "Renderer/RendererEnums.h"
#include "Sound/sound.h"

using namespace TEN::Input;
using namespace TEN::Math;

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

	GraphicsAPI RendererAPI		  = GraphicsAPI::Auto;
	int		   ScreenWidth		  = 0;
	int		   ScreenHeight		  = 0;
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
