#pragma once

#include "Math/Math.h"
#include "Specific/Input/Input.h"
#include "Renderer/RendererEnums.h"
#include "Sound/sound.h"

using namespace TEN::Input;
using namespace TEN::Math;

namespace TEN::Config
{
	enum class MenuOptionLoopingMode
	{
		AllMenus,
		SaveLoadOnly,
		Off
	};

	enum class ControlMode
	{
		Classic,
		Enhanced,
		Modern,

		Count
	};

	enum class SwimControlMode
	{
		Omnidirectional,
		Planar,

		Count
	};

	struct GameConfiguration
	{
		static constexpr auto DEFAULT_MOUSE_SENSITIVITY     = 6;
		static constexpr auto DEFAULT_SHADOW_MAP_SIZE       = 1024;
		static constexpr auto DEFAULT_SHADOW_BLOB_COUNT_MAX = 16;
		static constexpr auto MOUSE_SENSITIVITY_MAX         = 35;
		static constexpr auto MOUSE_SENSITIVITY_MIN         = 1;
		static constexpr auto SOUND_VOLUME_MAX              = 100;

		// Controls

		bool                  EnableTankCameraControl = false;
		bool                  InvertCameraXAxis       = false;
		bool                  InvertCameraYAxis       = false;
		bool                  EnableRumble            = false;
		int                   MouseSensitivity        = DEFAULT_MOUSE_SENSITIVITY;
		MenuOptionLoopingMode MenuOptionLoopingMode   = MenuOptionLoopingMode::SaveLoadOnly;
		GamepadType           LastGamepadType         = GamepadType::Xbox;
		BindingProfile        Bindings                = {};

		// Gameplay

		ControlMode     ControlMode                  = ControlMode::Classic;
		SwimControlMode SwimControlMode              = SwimControlMode::Omnidirectional;
		bool            EnableWalkToggle             = false;
		bool            EnableCrouchToggle           = false;
		bool            EnableClimbToggle            = false;
		bool            EnableAutoMonkeySwingJump    = false;
		bool            EnableAutoTargeting          = false;
		bool            EnableOppositeActionRoll     = false;
		bool            EnableTargetHighlighter      = false;
		bool            EnableInteractionHighlighter = false;
		bool            EnableSubtitles              = false;

		// Graphics

		int              ScreenWidth            = 0;
		int              ScreenHeight           = 0;
		float            Gamma                  = 1.0f;
		bool             EnableWindowedMode     = false;
		ShadowMode       ShadowType             = ShadowMode::None;
		int              ShadowMapSize          = DEFAULT_SHADOW_MAP_SIZE;
		int              ShadowBlobCountMax     = DEFAULT_SHADOW_BLOB_COUNT_MAX;
		bool             EnableCaustics         = false;
		bool             EnableDecal            = true;
		bool             EnableAmbientOcclusion = false;
		bool             EnableHighFramerate    = true;
		bool             EnableDecals           = true;
		AntialiasingMode AntialiasingMode       = AntialiasingMode::None;

		// Sound

		int	 SoundDevice  = 0;
		bool EnableSound  = false;
		bool EnableReverb = false;
		int	 MusicVolume  = VOLUME_MAX;
		int	 SfxVolume    = VOLUME_MAX;

		std::vector<Vector2i>   SupportedScreenResolutions = {};
		std::string             AdapterName                = {};
		std::vector<BassDevice> SupportedSoundDevices      = {};

		// Inquirers

		bool IsUsingClassicControls() const;
		bool IsUsingEnhancedControls() const;
		bool IsUsingModernControls() const;
		bool IsUsingOmnidirectionalSwimControls() const;
		bool IsUsingPlanarSwimControls() const;
	};

	extern GameConfiguration g_Configuration;

	void InitDefaultConfiguration();
	bool LoadConfiguration();
	bool SaveConfiguration();
	void SetAudioConfiguration(const GameConfiguration& config);
}
