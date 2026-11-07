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

// Remastered controls: control mode a gameflow entry or a script asks for; Default leaves the choice to the player. -- divezzi, 03.11.26
enum class ScriptControlMode
{
	Default,
	Classic,
	Remastered,

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
	bool EnableLookToggle				= false;
	bool EnableCrouchToggle				= true;
	bool EnableGrabToggle				= true;
	bool EnableSmoothSprintSteps		= true;

	// Input
	int					  MouseSensitivity		= DEFAULT_MOUSE_SENSITIVITY;
	MenuOptionLoopingMode MenuOptionLoopingMode = MenuOptionLoopingMode::SaveLoadOnly;
	GamepadType			  LastGamepadType		= GamepadType::Xbox;
	BindingProfile		  Bindings				= {};
	ControlMode			  ControlMode			= ControlMode::Classic;

	BindingProfile RemasteredBindings		 = {};
	BindingProfile RemasteredGamepadBindings = {};

	// Remastered controls: the control mode set by the gameflow or a script, used in place of ControlMode and never saved. -- divezzi, 02.11.26
	std::optional<::ControlMode> ForcedControlMode = std::nullopt;

	std::vector<Vector2i>	SupportedScreenResolutions	= {};
	std::string				AdapterName					= {};
	std::vector<BassDevice> SupportedSoundDevices				= {};

	::ControlMode GetControlMode() const		{ return ForcedControlMode.value_or(ControlMode); }
	bool IsControlModeForced() const			{ return ForcedControlMode.has_value(); }
	bool IsUsingClassicControls() const			{ return GetControlMode() == ControlMode::Classic; }
	bool IsUsingRemasteredControls() const		{ return GetControlMode() == ControlMode::RemasteredControls; }

	// Remastered controls: Auto Targeting forced on, Auto Monkey Jump forced off; saved values unchanged. -- divezzi, 12.10.26
	bool IsAutoTargetingEnabled() const			{ return (IsUsingRemasteredControls() || EnableAutoTargeting); }
	bool IsAutoMonkeySwingJumpEnabled() const	{ return (!IsUsingRemasteredControls() && EnableAutoMonkeySwingJump); }
};

void InitDefaultConfiguration();
bool LoadConfiguration();
bool SaveConfiguration();
void SetAudioConfiguration(const GameConfiguration& config);

extern GameConfiguration g_Configuration;
