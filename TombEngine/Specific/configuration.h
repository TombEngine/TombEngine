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

// Remastered controls: selects between the stock control scheme and the Remastered-style scheme. -- divezzi, 08.10.26
enum class ControlMode
{
	Classic,
	RemasteredControls,

	Count
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
	int	 MusicVolume  = VOLUME_MAX;
	int	 SfxVolume	  = VOLUME_MAX;

	// Gameplay

	bool EnableSubtitles				= false;
	bool EnableAutoMonkeySwingJump		= false;
	bool EnableAutoTargeting			= false;
	bool EnableTargetHighlighter		= false;
	bool EnableInteractionHighlighter	= false;
	bool EnableRumble					= false;
	bool EnableThumbstickCamera			= false;
	bool InvertCameraYAxis				= false;
	bool EnableSprintToggle				= false;
	bool EnableCrouchToggle				= true;
	bool EnableSmoothSprintSteps		= true;

	// Input
	int					  MouseSensitivity		= DEFAULT_MOUSE_SENSITIVITY;
	MenuOptionLoopingMode MenuOptionLoopingMode = MenuOptionLoopingMode::SaveLoadOnly;
	GamepadType			  LastGamepadType		= GamepadType::Xbox;
	BindingProfile		  Bindings				= {};
	ControlMode			  ControlMode			= ControlMode::Classic;

	BindingProfile RemasteredBindings		 = {};
	BindingProfile RemasteredGamepadBindings = {};

	std::vector<Vector2i>	SupportedScreenResolutions	= {};
	std::string				AdapterName					= {};
	std::vector<BassDevice> SupportedSoundDevices				= {};

	bool IsUsingClassicControls() const			{ return ControlMode == ControlMode::Classic; }
	bool IsUsingRemasteredControls() const		{ return ControlMode == ControlMode::RemasteredControls; }
};

void InitDefaultConfiguration();
bool LoadConfiguration();
bool SaveConfiguration();
void SetAudioConfiguration(const GameConfiguration& config);

extern GameConfiguration g_Configuration;
