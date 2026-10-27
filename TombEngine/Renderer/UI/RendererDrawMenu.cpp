#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/gui.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Renderer/UI/ImGuiIntegration.h"
#include "Renderer/Backend/SceneRenderer.h"
#include "Scripting/Internal/TEN/Flow//Level/FlowLevel.h"
#include "Specific/Benchmark.h"
#include "Specific/configuration.h"
#include "Specific/Input/InputAction.h"
#include "Specific/level.h"
#include "Specific/trutils.h"
#include "version.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Gui;
using namespace TEN::Hud;
using namespace TEN::Input;
using namespace TEN::Math;

extern TEN::Renderer::RendererHudBar* g_SFXVolumeBar;
extern TEN::Renderer::RendererHudBar* g_MusicVolumeBar;

// Menu and non-game frames: title, pause, options, load/save, statistics, inventory and examine screens, display
// pickups and display items, title image, loading screen and freeze mode. Menus record the 2D layer over the last
// rendered scene and present it with PresentFrame.

namespace TEN::Renderer
{
	// Horizontal alignment constants
	constexpr auto MenuLeftSideEntry = 180;
	constexpr auto MenuCenterEntry = 400;
	constexpr auto MenuRightSideEntry = 520;

	constexpr auto MenuLoadNumberLeftSide = 80;
	constexpr auto MenuLoadNameLeftSide   = 150;
	constexpr auto MenuLoadTimestampRightSide = 600;

	// Vertical spacing templates
	constexpr auto MenuVerticalLineSpacing = 30;
	constexpr auto MenuVerticalNarrowLineSpacing = 24;
	constexpr auto MenuVerticalBlockSpacing = 50;

	// Vertical menu positioning templates
	constexpr auto MenuVerticalControls = 30;
	constexpr auto MenuVerticalDisplaySettings = 50;
	constexpr auto MenuVerticalOtherSettings = 50;
	constexpr auto MenuVerticalBottomCenter = 400;
	constexpr auto MenuVerticalStatisticsTitle = 150;
	constexpr auto MenuVerticalOptionsTitle = 350;
	constexpr auto MenuVerticalPause = 220;
	constexpr auto MenuVerticalOptionsPause = 275;

	// Used with distance travelled
	constexpr auto UnitsToMeters = 419;

	// Helper functions to jump caret to new line
	inline void GetNextLinePosition(int* value, float scale = 1.0f) { *value += MenuVerticalLineSpacing * scale; }
	inline void GetNextNarrowLinePosition(int* value) { *value += MenuVerticalNarrowLineSpacing; }
	inline void GetNextBlockPosition(int* value) { *value += MenuVerticalBlockSpacing; }

	// Helper functions to construct string flags
	inline int SF(bool selected = false) { return (int)PrintStringFlags::Outline | (selected ? (int)PrintStringFlags::Blink : 0); }
	inline int SF_Center(bool selected = false) { return (int)PrintStringFlags::Outline | (int)PrintStringFlags::Center | (selected ? (int)PrintStringFlags::Blink : 0); }

	// Helper functions to get specific generic strings
	inline const std::string Str_Enabled(bool enabled = false) { return g_GameFlow->GetString(enabled ? STRING_ENABLED : STRING_DISABLED); }
	inline const std::string Str_LoadSave(bool save = false) { return g_GameFlow->GetString(save ? STRING_SAVE_GAME : STRING_LOAD_GAME); }
	inline const std::string Str_MenuOptionLoopingMode(MenuOptionLoopingMode loopingMode)
	{
		switch (loopingMode)
		{
		default:
			case MenuOptionLoopingMode::AllMenus:
				return g_GameFlow->GetString(STRING_MENU_OPT_LOOP_ALL_MENUS);

			case MenuOptionLoopingMode::SaveLoadOnly:
				return g_GameFlow->GetString(STRING_MENU_OPT_LOOP_SAVE_LOAD_ONLY);

			case MenuOptionLoopingMode::Disabled:
				return g_GameFlow->GetString(STRING_MENU_OPT_LOOP_DISABLED);
		}
	}

	inline const int GetEffectiveBoundKeyID(ActionID actionID)
	{
		int defaultKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Default, actionID);
		int userKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Custom, actionID);
		return (userKeyID != KEY_UNASSIGNED) ? userKeyID : defaultKeyID;
	}

	// Marks the actions of a range whose effective key is bound to another action of the same range.
	std::vector<bool> GetBindingConflictMask(ActionID baseActionID, int actionCount)
	{
		std::vector<bool> conflicts(actionCount, false);
		std::unordered_map<int, int> firstActionIndexByBinding = {};
		firstActionIndexByBinding.reserve(actionCount);

		for (int i = 0; i < actionCount; i++)
		{
			int keyID = GetEffectiveBoundKeyID((ActionID)((int)baseActionID + i));
			if (keyID == KEY_UNASSIGNED)
				continue;

			auto [it, inserted] = firstActionIndexByBinding.try_emplace(keyID, i);
			if (!inserted)
				conflicts[it->second] = conflicts[i] = true;
		}

		return conflicts;
	}

	// These bars are only used in menus.
	TEN::Renderer::RendererHudBar* g_MusicVolumeBar = nullptr;
	TEN::Renderer::RendererHudBar* g_SFXVolumeBar	= nullptr;

	void Renderer::InitializeMenuBars(int y)
	{
		int shift = MenuVerticalLineSpacing / 2;

		// Called every frame of the sound menu.
		delete g_MusicVolumeBar;
		delete g_SFXVolumeBar;

		g_MusicVolumeBar = new RendererHudBar(Vector2(MenuRightSideEntry, y + shift), RendererHudBar::SIZE_DEFAULT);
		GetNextLinePosition(&y);
		g_SFXVolumeBar = new RendererHudBar(Vector2(MenuRightSideEntry, y + shift), RendererHudBar::SIZE_DEFAULT);
	}

	// Options menus (options, display, other settings and control bindings) recorded in the 2D layer.
	void Renderer::RenderOptionsMenu(Menu menu, int initialY)
	{
		constexpr auto	  RIGHT_ARROW_X_OFFSET			  = DISPLAY_SPACE_RES.x - MenuLeftSideEntry;
		static const auto LEFT_ARROW_STRING				  = std::string("<");
		static const auto RIGHT_ARROW_STRING			  = std::string(">");
		static const auto CONTROL_SETTINGS_BLOCK_Y_OFFSET = (MenuVerticalNarrowLineSpacing * (int)QuickActionStrings.size()) + (MenuVerticalBlockSpacing * 2.5f);

		int y = 0;
		auto titleOption = g_Gui.GetSelectedOption();

		auto optionColor   = g_GameFlow->GetSettings()->UI.HeaderTextColor;
		auto headerColor   = g_GameFlow->GetSettings()->UI.OptionTextColor;
		auto plainColor    = g_GameFlow->GetSettings()->UI.PlainTextColor;
		auto disabledColor = g_GameFlow->GetSettings()->UI.DisabledTextColor;

		char stringBuffer[32] = {};
		auto screenResolution = g_Configuration.SupportedScreenResolutions[g_Gui.GetCurrentSettings().SelectedScreenResolution];
		sprintf(stringBuffer, "%d x %d", screenResolution.x, screenResolution.y);

		char soundDeviceStringBuffer[255] = {};
		sprintf(soundDeviceStringBuffer, "%s", g_Configuration.SupportedSoundDevices[g_Gui.GetCurrentSettings().SelectedSoundDevice].Name.c_str());

		auto* shadowMode = g_Gui.GetCurrentSettings().Configuration.ShadowType != ShadowMode::None ?
			(g_Gui.GetCurrentSettings().Configuration.ShadowType == ShadowMode::Player ? STRING_SHADOWS_PLAYER : STRING_SHADOWS_ALL) : STRING_SHADOWS_NONE;

		const char* antialiasMode;
		switch (g_Gui.GetCurrentSettings().Configuration.AntialiasingMode)
		{
		default:
		case AntialiasingMode::None:
			antialiasMode = STRING_ANTIALIASING_NONE;
			break;

		case AntialiasingMode::Smaa:
			antialiasMode = STRING_ANTIALIASING_SMAA;
			break;

		case AntialiasingMode::Temporal:
			antialiasMode = STRING_ANTIALIASING_TEMPORAL;
			break;
		}

		switch (menu)
		{
		case Menu::Options:
			// Setup needed parameters
			y = initialY;

			// Display
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_DISPLAY), plainColor, SF_Center(titleOption == 0));
			GetNextLinePosition(&y);

			// Other options
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OTHER_SETTINGS), plainColor, SF_Center(titleOption == 1));
			GetNextLinePosition(&y);

			// Controls
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CONTROLS), plainColor, SF_Center(titleOption == 2));
			break;

		case Menu::Display:
			// Setup needed parameters
			y = MenuVerticalDisplaySettings;

			// Title
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_DISPLAY), headerColor, SF_Center());
			GetNextBlockPosition(&y);

			// Screen resolution
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SCREEN_RESOLUTION), optionColor, SF(titleOption == 0));
			AddString(MenuRightSideEntry, y, stringBuffer, plainColor, SF(titleOption == 0));
			GetNextLinePosition(&y);

			// Windowed mode
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_WINDOWED), optionColor, SF(titleOption == 1));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableWindowedMode), plainColor, SF(titleOption == 1));
			GetNextLinePosition(&y);

			// Enable dynamic shadows
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SHADOWS), optionColor, SF(titleOption == 2));
			AddString(MenuRightSideEntry, y, g_GameFlow->GetString(shadowMode), plainColor, SF(titleOption == 2));
			GetNextLinePosition(&y);

			// Enable caustics
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_CAUSTICS), optionColor, SF(titleOption == 3));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableCaustics), plainColor, SF(titleOption == 3));
			GetNextLinePosition(&y);

			// Enable decals
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_DECALS), optionColor, SF(titleOption == 4));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableDecals), plainColor, SF(titleOption == 4));
			GetNextLinePosition(&y);

			// Enable antialiasing
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_ANTIALIASING), optionColor, SF(titleOption == 5));
			AddString(MenuRightSideEntry, y, g_GameFlow->GetString(antialiasMode), plainColor, SF(titleOption == 5));
			GetNextLinePosition(&y);

			// Enable SSAO
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AMBIENT_OCCLUSION), optionColor, SF(titleOption == 6));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_GameFlow->GetSettings()->Graphics.AmbientOcclusion && g_Gui.GetCurrentSettings().Configuration.EnableAmbientOcclusion),
				g_GameFlow->GetSettings()->Graphics.AmbientOcclusion ? plainColor : disabledColor, SF(titleOption == 6));
			GetNextLinePosition(&y);

			// HDR rendering
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_HDR), optionColor, SF(titleOption == 7));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_GameFlow->GetSettings()->Graphics.Hdr && g_Gui.GetCurrentSettings().Configuration.EnableHdr),
				g_GameFlow->GetSettings()->Graphics.Hdr ? plainColor : disabledColor, SF(titleOption == 7));
			GetNextLinePosition(&y);

			// Enable high framerate
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_HIGH_FRAMERATE), optionColor, SF(titleOption == 8));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableHighFramerate), plainColor, SF(titleOption == 8));
			GetNextLinePosition(&y);

			// Gamma correction
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_GAMMA), optionColor, SF(titleOption == 9));
			AddString(MenuRightSideEntry, y, fmt::format("{:.1f}", g_Gui.GetCurrentSettings().Configuration.Gamma).c_str(), plainColor, SF(titleOption == 9));
			GetNextLinePosition(&y);

			// Atmospheric Sky Quality
			{
				auto q = g_Gui.GetCurrentSettings().Configuration.AtmosphericSkyQuality;
				const char* qStr =
					(q == AtmosphericSkyQuality::Low)    ? STRING_QUALITY_LOW :
					(q == AtmosphericSkyQuality::Medium) ? STRING_QUALITY_MEDIUM :
					                                       STRING_QUALITY_HIGH;
				AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_ATMOSPHERIC_SKY_QUALITY), optionColor, SF(titleOption == 10));
				AddString(MenuRightSideEntry, y, g_GameFlow->GetString(qStr), plainColor, SF(titleOption == 10));
			}
			GetNextLinePosition(&y);

			// Renderer backend of the main window, applied at the next start.
			{
				constexpr const char* BACKEND_STRINGS[] = { STRING_RENDERER_VULKAN, STRING_RENDERER_DX11, STRING_RENDERER_D3D12, STRING_RENDERER_OPENGL };

				int backend = (int)g_Gui.GetCurrentSettings().Configuration.GraphicsBackend - (int)GraphicsBackend::Vulkan;
				AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_RENDERER), optionColor, SF(titleOption == 11));
				AddString(MenuRightSideEntry, y, g_GameFlow->GetString(BACKEND_STRINGS[backend]), plainColor, SF(titleOption == 11));
			}
			GetNextLinePosition(&y);

			// Render scale of the 3D scene, upscaled by TAA or FSR 1.
			{
				auto renderScale = g_Gui.GetCurrentSettings().Configuration.RenderScale;
				const char* renderScaleString = (renderScale == RenderScaleMode::Automatic) ? STRING_RENDER_SCALE_AUTOMATIC :
					((renderScale == RenderScaleMode::Performance) ? STRING_RENDER_SCALE_PERFORMANCE : STRING_RENDER_SCALE_NATIVE);
				AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_RENDER_SCALE), optionColor, SF(titleOption == 12));
				AddString(MenuRightSideEntry, y, g_GameFlow->GetString(renderScaleString), plainColor, SF(titleOption == 12));
			}
			GetNextBlockPosition(&y);

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), optionColor, SF_Center(titleOption == 13));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), optionColor, SF_Center(titleOption == 14));
			break;

		case Menu::OtherSettings:
			// Setup needed parameters
			y = MenuVerticalOtherSettings;

			// Title
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OTHER_SETTINGS), headerColor, SF_Center());
			GetNextBlockPosition(&y);

			// Sound device
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SOUND_DEVICE), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 0));
			AddString(MenuRightSideEntry, y, soundDeviceStringBuffer, PRINTSTRING_COLOR_WHITE, SF(titleOption == 0));
			GetNextLinePosition(&y);

			// Enable sound special effects
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_REVERB), optionColor, SF(titleOption == 1));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableReverb), plainColor, SF(titleOption == 1));

			GetNextLinePosition(&y);

			// Initialize bars, if not yet done. Must be done here because we're calculating Y coord on the fly.
			if (g_MusicVolumeBar == nullptr)
				InitializeMenuBars(y);

			// Music volume
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_MUSIC_VOLUME), optionColor, SF(titleOption == 2));

			DrawBar(g_Gui.GetCurrentSettings().Configuration.MusicVolume / 100.0f, *g_MusicVolumeBar, ID_SFX_BAR_TEXTURE, 0, false);
			GetNextLinePosition(&y);

			// Sound FX volume
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SFX_VOLUME), optionColor, SF(titleOption == 3));

			DrawBar(g_Gui.GetCurrentSettings().Configuration.SfxVolume / 100.0f, *g_SFXVolumeBar, ID_SFX_BAR_TEXTURE, 0, false);
			GetNextBlockPosition(&y);

			// Subtitles
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SUBTITLES), optionColor, SF(titleOption == 4));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableSubtitles), plainColor, SF(titleOption == 4));
			GetNextLinePosition(&y);

			// Auto monkey swing jump
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AUTO_MONKEY_SWING_JUMP), optionColor, SF(titleOption == 5));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableAutoMonkeySwingJump), plainColor, SF(titleOption == 5));
			GetNextLinePosition(&y);

			// Auto targeting
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AUTO_TARGETING), optionColor, SF(titleOption == 6));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableAutoTargeting), plainColor, SF(titleOption == 6));
			GetNextLinePosition(&y);

			// Target highlighter
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_TARGET_HIGHLIGHTER), optionColor, SF(titleOption == 7));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableTargetHighlighter),
				g_GameFlow->GetSettings()->Hud.TargetHighlighter ? plainColor : disabledColor, SF(titleOption == 7));
			GetNextLinePosition(&y);

			// Interaction highlighter
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_INTERACTION_HIGHLIGHTER), optionColor, SF(titleOption == 8));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableInteractionHighlighter),
				g_GameFlow->GetSettings()->Hud.InteractionHighlighter ? plainColor : disabledColor, SF(titleOption == 8));
			GetNextLinePosition(&y);

			// Vibration
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_RUMBLE), optionColor, SF(titleOption == 9));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableRumble), plainColor, SF(titleOption == 9));
			GetNextLinePosition(&y);

			// Thumbstick camera
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_THUMBSTICK_CAMERA), optionColor, SF(titleOption == 10));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableThumbstickCamera), plainColor, SF(titleOption == 10));
			GetNextBlockPosition(&y);

			// Mouse sensitivity
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_MOUSE_SENSITIVITY), optionColor, SF(titleOption == 11));
			AddString(MenuRightSideEntry, y, std::to_string(g_Gui.GetCurrentSettings().Configuration.MouseSensitivity).c_str(), plainColor, SF(titleOption == 11));
			GetNextBlockPosition(&y);

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), optionColor, SF_Center(titleOption == 12));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), optionColor, SF_Center(titleOption == 13));

			break;

		case Menu::GeneralActions:
			{
				// Set up needed parameters.
				y = MenuVerticalControls;
				auto bindingConflicts = GetBindingConflictMask(In::Forward, (int)GeneralActionStrings.size());

				// Arrows
				AddString(RIGHT_ARROW_X_OFFSET, y, RIGHT_ARROW_STRING.c_str(), headerColor, SF(true));

				// Title
				auto titleString = std::string(g_GameFlow->GetString(STRING_GENERAL_ACTIONS));
				AddString(MenuCenterEntry, y, titleString.c_str(), headerColor, SF_Center());
				GetNextBlockPosition(&y);

				// General action listing
				for (int k = 0; k < GeneralActionStrings.size(); k++)
				{
					AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(GeneralActionStrings[k].c_str()), plainColor, SF(titleOption == k));

					if (g_Gui.GetCurrentSettings().NewKeyWaitTimer > 0.0f && titleOption == k)
					{
						AddString(MenuRightSideEntry, y, g_GameFlow->GetString(STRING_WAITING_FOR_INPUT), plainColor, SF(true));
					}
					else
					{
						auto actionID = (ActionID)((int)In::Forward + k);
						AddString(MenuRightSideEntry, y, GetKeyName(GetEffectiveBoundKeyID(actionID)).c_str(), optionColor, SF(bindingConflicts[k]));
					}

					if (k < (GeneralActionStrings.size() - 1))
						GetNextNarrowLinePosition(&y);
				}

				y = CONTROL_SETTINGS_BLOCK_Y_OFFSET;

				// Reset to defaults
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_RESET_TO_DEFAULTS), optionColor, SF_Center(titleOption == GeneralActionStrings.size()));
				GetNextLinePosition(&y);

				// Apply
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), optionColor, SF_Center(titleOption == (GeneralActionStrings.size() + 1)));
				GetNextLinePosition(&y);

				// Cancel
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), optionColor, SF_Center(titleOption == (GeneralActionStrings.size() + 2)));
				break;
			}

		case Menu::VehicleActions:
			{
				// Set up needed parameters.
				y = MenuVerticalControls;
				auto bindingConflicts = GetBindingConflictMask(In::Accelerate, (int)VehicleActionStrings.size());

				// Arrows
				AddString(MenuLeftSideEntry, y, LEFT_ARROW_STRING.c_str(), headerColor, SF(true));
				AddString(RIGHT_ARROW_X_OFFSET, y, RIGHT_ARROW_STRING.c_str(), headerColor, SF(true));

				// Title
				auto titleString = std::string(g_GameFlow->GetString(STRING_VEHICLE_ACTIONS));
				AddString(MenuCenterEntry, y, titleString.c_str(), headerColor, SF_Center());
				GetNextBlockPosition(&y);

				int baseIndex = (int)In::Accelerate;

				// Vehicle action listing
				for (int k = 0; k < VehicleActionStrings.size(); k++)
				{
					AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(VehicleActionStrings[k].c_str()), plainColor, SF(titleOption == k));

					if (g_Gui.GetCurrentSettings().NewKeyWaitTimer > 0.0f && titleOption == k)
					{
						AddString(MenuRightSideEntry, y, g_GameFlow->GetString(STRING_WAITING_FOR_INPUT), plainColor, SF(true));
					}
					else
					{
						auto actionID = (ActionID)(baseIndex + k);
						AddString(MenuRightSideEntry, y, GetKeyName(GetEffectiveBoundKeyID(actionID)).c_str(), optionColor, SF(bindingConflicts[k]));
					}

					if (k < (VehicleActionStrings.size() - 1))
					{
						GetNextNarrowLinePosition(&y);
					}
					else
					{
						GetNextBlockPosition(&y);
					}
				}

				y = CONTROL_SETTINGS_BLOCK_Y_OFFSET;

				// Reset to defaults
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_RESET_TO_DEFAULTS), optionColor, SF_Center(titleOption == VehicleActionStrings.size()));
				GetNextLinePosition(&y);

				// Apply
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), optionColor, SF_Center(titleOption == (VehicleActionStrings.size() + 1)));
				GetNextLinePosition(&y);

				// Cancel
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), optionColor, SF_Center(titleOption == (VehicleActionStrings.size() + 2)));
				break;
			}

		case Menu::QuickActions:
			{
				// Set up needed parameters.
				y = MenuVerticalControls;
				auto bindingConflicts = GetBindingConflictMask(In::Flare, (int)QuickActionStrings.size());

				// Arrows
				AddString(MenuLeftSideEntry, y, LEFT_ARROW_STRING.c_str(), headerColor, SF(true));
				AddString(RIGHT_ARROW_X_OFFSET, y, RIGHT_ARROW_STRING.c_str(), headerColor, SF(true));

				// Title
				auto titleString = std::string(g_GameFlow->GetString(STRING_QUICK_ACTIONS));
				AddString(MenuCenterEntry, y, titleString.c_str(), headerColor, SF_Center());
				GetNextBlockPosition(&y);

				int baseIndex = (int)In::Flare;

				// Quick action listing
				for (int k = 0; k < QuickActionStrings.size(); k++)
				{
					AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(QuickActionStrings[k].c_str()), plainColor, SF(titleOption == k));

					if (g_Gui.GetCurrentSettings().NewKeyWaitTimer > 0.0f && titleOption == k)
					{
						AddString(MenuRightSideEntry, y, g_GameFlow->GetString(STRING_WAITING_FOR_INPUT), plainColor, SF(true));
					}
					else
					{
						auto actionID = (ActionID)(baseIndex + k);
						AddString(MenuRightSideEntry, y, GetKeyName(GetEffectiveBoundKeyID(actionID)).c_str(), optionColor, SF(bindingConflicts[k]));
					}

					if (k < (QuickActionStrings.size() - 1))
						GetNextNarrowLinePosition(&y);
				}

				y = CONTROL_SETTINGS_BLOCK_Y_OFFSET;

				// Reset to defaults
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_RESET_TO_DEFAULTS), optionColor, SF_Center(titleOption == QuickActionStrings.size()));
				GetNextLinePosition(&y);

				// Apply
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), optionColor, SF_Center(titleOption == (QuickActionStrings.size() + 1)));
				GetNextLinePosition(&y);

				// Cancel
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), optionColor, SF_Center(titleOption == (QuickActionStrings.size() + 2)));
				break;
			}

		case Menu::MenuActions:
			{
				// Setup needed parameters.
				y = MenuVerticalControls;
				auto bindingConflicts = GetBindingConflictMask(In::Select, (int)MenuActionStrings.size());

				// Arrows
				AddString(MenuLeftSideEntry, y, LEFT_ARROW_STRING.c_str(), headerColor, SF(true));

				// Title
				auto titleString = std::string(g_GameFlow->GetString(STRING_MENU_ACTIONS));
				AddString(MenuCenterEntry, y, titleString.c_str(), headerColor, SF_Center());
				GetNextBlockPosition(&y);

				int baseIndex = (int)In::Select;

				// Menu action listing.
				for (int k = 0; k < MenuActionStrings.size(); k++)
				{
					AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(MenuActionStrings[k].c_str()), plainColor, SF(titleOption == k));

					if (g_Gui.GetCurrentSettings().NewKeyWaitTimer > 0.0f && titleOption == k)
					{
						AddString(MenuRightSideEntry, y, g_GameFlow->GetString(STRING_WAITING_FOR_INPUT), plainColor, SF(true));
					}
					else
					{
						auto actionID = (ActionID)(baseIndex + k);
						AddString(MenuRightSideEntry, y, GetKeyName(GetEffectiveBoundKeyID(actionID)).c_str(), optionColor, SF(bindingConflicts[k]));
					}

					if (k < (MenuActionStrings.size() - 1))
						GetNextNarrowLinePosition(&y);
				}

				y = CONTROL_SETTINGS_BLOCK_Y_OFFSET;

				// Reset to defaults
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_RESET_TO_DEFAULTS), optionColor, SF_Center(titleOption == MenuActionStrings.size()));
				GetNextLinePosition(&y);

				// Apply
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), optionColor, SF_Center(titleOption == (MenuActionStrings.size() + 1)));
				GetNextLinePosition(&y);

				// Cancel
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), optionColor, SF_Center(titleOption == (MenuActionStrings.size() + 2)));
				break;
			}
		}
	}

	void Renderer::RenderTitleMenu(Menu menu)
	{
		int titleOption = g_Gui.GetSelectedOption();
		int selectedOption = 0;

		auto plainColor = g_GameFlow->GetSettings()->UI.PlainTextColor;
		auto menuPos = (Vector2i)(g_GameFlow->GetSettings()->UI.TitleMenuPosition * (DISPLAY_SPACE_RES / 100.0f));
		auto alignment = g_GameFlow->GetSettings()->UI.TitleMenuAlignment.has_value() ? (1 << (int)g_GameFlow->GetSettings()->UI.TitleMenuAlignment.value()) : 0;
		auto scale = g_GameFlow->GetSettings()->UI.TitleMenuScale;

		switch (menu)
		{
		case Menu::Title:

			// New game
			AddString(g_GameFlow->GetString(STRING_NEW_GAME), menuPos.ToVector2(), plainColor, scale, SF(titleOption == selectedOption) | alignment);
			GetNextLinePosition(&menuPos.y, scale);
			selectedOption++;

			// Home Level
			if (g_GameFlow->IsHomeLevelEnabled())
			{
				AddString(g_GameFlow->GetString(STRING_HOME_LEVEL), menuPos.ToVector2(), plainColor, scale, SF(titleOption == selectedOption) | alignment);
				GetNextLinePosition(&menuPos.y, scale);
				selectedOption++;
			}

			// Load game
			if (g_GameFlow->IsLoadSaveEnabled())
			{
				AddString(g_GameFlow->GetString(STRING_LOAD_GAME), menuPos.ToVector2(), plainColor, scale, SF(titleOption == selectedOption) | alignment);
				GetNextLinePosition(&menuPos.y, scale);
				selectedOption++;
			}

			// Options
			AddString(g_GameFlow->GetString(STRING_OPTIONS), menuPos.ToVector2(), plainColor, scale, SF(titleOption == selectedOption) | alignment);
			GetNextLinePosition(&menuPos.y, scale);
			selectedOption++;

			// Exit game
			AddString(g_GameFlow->GetString(STRING_EXIT_GAME), menuPos.ToVector2(), plainColor, scale, SF(titleOption == selectedOption) | alignment);
			break;

		case Menu::LoadGame:
			RenderLoadSaveMenu();
			break;

		case Menu::SelectLevel:
			RenderSelectLevel();
			break;

		case Menu::Options:
		case Menu::GeneralActions:
		case Menu::VehicleActions:
		case Menu::QuickActions:
		case Menu::MenuActions:
		case Menu::Display:
		case Menu::OtherSettings:
			RenderOptionsMenu(menu, MenuVerticalOptionsTitle);
			break;
		}

		DrawAllStrings();
	}

	void Renderer::RenderPauseMenu(Menu menu)
	{
		int y = 0;
		auto pauseOption = g_Gui.GetSelectedOption();
		auto plainColor = g_GameFlow->GetSettings()->UI.PlainTextColor;

		switch (g_Gui.GetMenuToDisplay())
		{
		case Menu::Pause:

			// Setup needed parameters
			y = MenuVerticalPause;

			// Header
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_ACTIONS_PAUSE), g_GameFlow->GetSettings()->UI.HeaderTextColor, SF_Center());
			GetNextBlockPosition(&y);

			// Statistics
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_STATISTICS), plainColor, SF_Center(pauseOption == 0));
			GetNextLinePosition(&y);

			// Options
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS), plainColor, SF_Center(pauseOption == 1));
			GetNextLinePosition(&y);

			// Exit to title
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_EXIT_TO_TITLE), plainColor, SF_Center(pauseOption == 2));
			break;

		case Menu::Statistics:
			DrawStatistics();
			break;

		case Menu::Options:
		case Menu::GeneralActions:
		case Menu::VehicleActions:
		case Menu::QuickActions:
		case Menu::MenuActions:
		case Menu::Display:
		case Menu::OtherSettings:
			RenderOptionsMenu(menu, MenuVerticalOptionsPause);
			break;
		}

		DrawAllStrings();
	}

	void Renderer::RenderLoadSaveMenu()
	{
		if (!g_GameFlow->IsLoadSaveEnabled())
		{
			g_Gui.SetInventoryMode(InventoryMode::InGame);
			return;
		}

		// Setup needed parameters
		int y = MenuVerticalLineSpacing;
		short selection = g_Gui.GetLoadSaveSelection();
		char stringBuffer[255];
		auto plainColor  = g_GameFlow->GetSettings()->UI.PlainTextColor;

		// Title
		AddString(MenuCenterEntry, MenuVerticalNarrowLineSpacing, Str_LoadSave(g_Gui.GetInventoryMode() == InventoryMode::Save),
			g_GameFlow->GetSettings()->UI.HeaderTextColor, SF_Center());
		GetNextBlockPosition(&y);

		// Savegame listing
		for (int n = 0; n < SAVEGAME_MAX; n++)
		{
			auto& save = SaveGame::Infos[n];

			if (!save.Present)
			{
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_EMPTY), plainColor, SF_Center(selection == n));
			}
			else
			{
				// Number
				sprintf(stringBuffer, "%03d", save.Count);
				AddString(MenuLoadNumberLeftSide, y, stringBuffer, plainColor, SF(selection == n));

				// Level name
				AddString(MenuLoadNameLeftSide, y, (char*)save.LevelName.c_str(), plainColor, SF(selection == n));

				// Timestamp
				sprintf(stringBuffer, g_GameFlow->GetString(STRING_SAVEGAME_TIMESTAMP), save.Hours, save.Minutes, save.Seconds);
				AddString(MenuLoadTimestampRightSide, y, stringBuffer, plainColor, SF(selection == n));
			}

			GetNextLinePosition(&y);
		}

		DrawAllStrings();
	}

	void Renderer::RenderSelectLevel()
	{
		constexpr auto SCROLL_EASE	 = 0.3f;
		constexpr auto FADE_ZONE_H	 = 25.0f;
		constexpr auto ARROW_MARGIN  = 30.0f;
		constexpr auto ARROW_SCALE	 = 0.03f;

		int titleOption  = g_Gui.GetSelectedOption();
		auto plainColor  = g_GameFlow->GetSettings()->UI.PlainTextColor;
		auto headerColor = g_GameFlow->GetSettings()->UI.HeaderTextColor;
		auto scale		 = g_GameFlow->GetSettings()->UI.TitleMenuScale;

		// Level 0 is always Title Level and level 1 might be Home Level.
		int firstLevel = g_GameFlow->IsHomeLevelEnabled() ? 2 : 1;
		int numLevels  = g_GameFlow->GetNumLevels() - firstLevel;

		if (numLevels <= 0)
			return;

		float listStartY  = (float)(MenuVerticalLineSpacing + MenuVerticalBlockSpacing);
		float lineHeight  = (float)MenuVerticalLineSpacing;
		float totalHeight = (float)numLevels * lineHeight;
		float visibleH	  = DISPLAY_SPACE_RES.y - listStartY - lineHeight * 2;
		bool  needsScroll = (totalHeight > visibleH);
		bool  needsAlpha  = false;

		// Title.
		AddString(MenuCenterEntry, MenuVerticalLineSpacing, g_GameFlow->GetString(STRING_SELECT_LEVEL), headerColor, SF_Center());

		static float selectLevelScrollY = 0.0f;

		// Compute target scroll and ease toward it.
		if (needsScroll)
		{
			float selectedCenterY = (float)titleOption * lineHeight + lineHeight * 0.5f;
			float maxScrollY	  = totalHeight - visibleH;
			float targetScrollY   = selectedCenterY - visibleH * 0.5f;

			if (targetScrollY < 0.0f)
				targetScrollY = 0.0f;
			if (targetScrollY > maxScrollY)
				targetScrollY = maxScrollY;

			auto delta = targetScrollY - selectLevelScrollY;
			needsAlpha = abs(delta) >= EPSILON;

			selectLevelScrollY += delta * SCROLL_EASE;
			if (abs(selectLevelScrollY - targetScrollY) < 0.5f)
				selectLevelScrollY = targetScrollY;
		}
		else
			selectLevelScrollY = 0.0f;

		// Draw level entries.
		for (int i = 0; i < numLevels; i++)
		{
			int levelIndex = firstLevel + i;
			float entryY = listStartY + (float)i * lineHeight - selectLevelScrollY;

			// Fade entries near clip boundaries.
			float alpha = 1.0f;
			if (needsScroll)
			{
				float topEdge    = listStartY;
				float bottomEdge = listStartY + visibleH - lineHeight;

				if (entryY < topEdge)
					alpha = std::max(0.0f, 1.0f - (topEdge - entryY) / FADE_ZONE_H);
				else if (entryY > bottomEdge)
					alpha = std::max(0.0f, 1.0f - (entryY - bottomEdge) / FADE_ZONE_H);
			}

			// Skip clipped entries.
			if ((alpha <= 0.0f) || (!needsAlpha && alpha < 1.0f))
				continue;

			auto color = Color(plainColor);
			color.w *= alpha;

			AddString(g_GameFlow->GetString(g_GameFlow->GetLevel(levelIndex)->NameStringKey.c_str()),
				Vector2((float)MenuCenterEntry, entryY), color, scale, SF_Center(titleOption == i));
		}

		// Draw scroll arrows.
		if (needsScroll && Objects[GAME_OBJECT_ID::ID_INVENTORY_SPRITES].loaded)
		{
			const float xPositions[] = { ARROW_MARGIN, DISPLAY_SPACE_RES.x - ARROW_MARGIN };
			auto maxScrollY = totalHeight - visibleH;

			for (float x : xPositions)
			{
				if (selectLevelScrollY > 1.0f)
				{
					TEN::Effects::DisplaySprite::AddDisplaySprite(ID_INVENTORY_SPRITES, 0,
						Vector2(x, ARROW_MARGIN), 0, Vector2(ARROW_SCALE), headerColor, 0,
						DisplaySpriteAlignMode::Center, DisplaySpriteScaleMode::Fit, BlendMode::AlphaBlend, DisplaySpritePhase::Draw);
				}

				if (selectLevelScrollY < maxScrollY - 1.0f)
				{
					TEN::Effects::DisplaySprite::AddDisplaySprite(ID_INVENTORY_SPRITES, 0,
						Vector2(x, DISPLAY_SPACE_RES.y - ARROW_MARGIN),
						ANGLE(180), Vector2(ARROW_SCALE), headerColor, 0,
						DisplaySpriteAlignMode::Center, DisplaySpriteScaleMode::Fit, BlendMode::AlphaBlend, DisplaySpritePhase::Draw);
				}
			}
		}

		CollectDisplaySprites(_gameCamera);
		DrawDisplaySprites(_gameCamera, false);
		DrawDisplaySprites(_gameCamera, true);
	}

	void Renderer::DrawStatistics()
	{
		char buffer[40];

		auto* lvl = g_GameFlow->GetLevel(CurrentLevel);
		auto y = MenuVerticalStatisticsTitle;
		auto plainColor = g_GameFlow->GetSettings()->UI.PlainTextColor;

		// Title
		AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_STATISTICS), g_GameFlow->GetSettings()->UI.HeaderTextColor, SF_Center());
		GetNextBlockPosition(&y);

		// Level name
		AddString(MenuCenterEntry, y, g_GameFlow->GetString(lvl->NameStringKey.c_str()), plainColor, SF_Center());
		GetNextBlockPosition(&y);

		// Time taken
		auto& gameTime = SaveGame::Statistics.Game.TimeTaken;
		sprintf(buffer, "%02d:%02d:%02d", gameTime.GetHours(), gameTime.GetMinutes(), gameTime.GetSeconds());
		AddString(MenuRightSideEntry, y, buffer, plainColor, SF());
		AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_TIME_TAKEN), plainColor, SF());
		GetNextLinePosition(&y);

		// Distance travelled
		sprintf(buffer, "%dm", SaveGame::Statistics.Game.Distance / UnitsToMeters);
		AddString(MenuRightSideEntry, y, buffer, plainColor, SF());
		AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_DISTANCE_TRAVELLED), plainColor, SF());
		GetNextLinePosition(&y);

		// Ammo used
		sprintf(buffer, "%d", SaveGame::Statistics.Game.AmmoUsed);
		AddString(MenuRightSideEntry, y, buffer, plainColor, SF());
		AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AMMO_USED), plainColor, SF());
		GetNextLinePosition(&y);

		// Medipacks used
		sprintf(buffer, "%d", SaveGame::Statistics.Game.HealthUsed);
		AddString(MenuRightSideEntry, y, buffer, plainColor, SF());
		AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_USED_MEDIPACKS), plainColor, SF());
		GetNextLinePosition(&y);

		// Secrets found in Level
		if (g_GameFlow->GetLevel(CurrentLevel)->GetSecrets() > 0)
		{
			sprintf(buffer, "%d / %d", SaveGame::Statistics.Level.Secrets, g_GameFlow->GetLevel(CurrentLevel)->GetSecrets());
			AddString(MenuRightSideEntry, y, buffer, plainColor, SF());
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_LEVEL_SECRETS_FOUND), plainColor, SF());
			GetNextLinePosition(&y);
		}

		// Secrets found total
		if (g_GameFlow->TotalNumberOfSecrets > 0)
		{
			sprintf(buffer, "%d / %d", SaveGame::Statistics.Game.Secrets, g_GameFlow->TotalNumberOfSecrets);
			AddString(MenuRightSideEntry, y, buffer, plainColor, SF());
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_TOTAL_SECRETS_FOUND), plainColor, SF());
		}

		DrawAllStrings();
	}

	void Renderer::RenderNewInventory()
	{
		g_Gui.DrawCompass(LaraItem);

		g_Gui.DrawCurrentObjectList(LaraItem, RingTypes::Inventory);

		if (g_Gui.GetRing(RingTypes::Ammo).RingActive)
			g_Gui.DrawCurrentObjectList(LaraItem, RingTypes::Ammo);

		g_Gui.DrawAmmoSelector();
		g_Gui.FadeAmmoSelector();

		DrawAllStrings();
	}

	// Interpolated pickup object in 2D space with its count string.
	void Renderer::DrawDisplayPickup(const DisplayPickup& pickup)
	{
		constexpr auto COUNT_STRING_INF	   = "Inf";
		constexpr auto COUNT_STRING_OFFSET = Vector2(DISPLAY_SPACE_RES.x / 40, 0.0f);

		auto pos = Vector2::Lerp(pickup.PrevPosition, pickup.Position, GetInterpolationFactor());
		auto orient = EulerAngles::Lerp(pickup.PrevOrientation, pickup.Orientation, GetInterpolationFactor());
		float scale = Lerp(pickup.PrevScale, pickup.Scale, GetInterpolationFactor());
		float opacity = Lerp(pickup.PrevOpacity, pickup.Opacity, GetInterpolationFactor());
		int invObjectID = g_Gui.ConvertObjectToInventoryItem(pickup.ObjectID);

		// Draw display pickup.
		DrawObjectIn2DSpace(pickup.ObjectID, pos, orient, scale, opacity, InventoryObjectTable[invObjectID].MeshBits);

		// Draw count string.
		if (pickup.Count != 1)
		{
			auto countString = (pickup.Count != NO_VALUE) ? std::to_string(pickup.Count) : COUNT_STRING_INF;
			auto countStringPos = pickup.Position + COUNT_STRING_OFFSET;
			auto countStringPrevPos = pickup.PrevPosition + COUNT_STRING_OFFSET;
			
			auto color = Color(g_GameFlow->GetSettings()->UI.PlainTextColor);
			color.w = opacity;

			AddString(countString, countStringPos, countStringPrevPos, Vector2::Zero, color, pickup.StringScale, SF());
		}
	}

	// Records an object of the 2D layer at a display space position, with an orthographic projection; inventory
	// objects use their inventory offset and orientation.
	// TODO: Handle opacity.
	void Renderer::DrawObjectIn2DSpace(int objectNumber, Vector2 pos2D, EulerAngles orient, float scale, float opacity, int meshBits)
	{
		auto screenRes = GetScreenResolution();
		auto factor = Vector2(
			screenRes.x / DISPLAY_SPACE_RES.x,
			screenRes.y / DISPLAY_SPACE_RES.y);

		pos2D *= factor;
		scale *= (factor.x > factor.y) ? factor.y : factor.x;

		int invObjectID = g_Gui.ConvertObjectToInventoryItem(objectNumber);
		if (invObjectID != NO_VALUE)
		{
			const auto& invObject = InventoryObjectTable[invObjectID];

			pos2D.y += invObject.YOffset * factor.y;
			orient += invObject.Orientation;
		}

		auto viewMatrix = Matrix::CreateLookAt(Vector3(0.0f, 0.0f, BLOCK(2)), Vector3::Zero, Vector3::Down);
		auto projMatrix = Matrix::CreateOrthographic((float)screenRes.x, (float)screenRes.y, -BLOCK(1), BLOCK(1));

		auto& moveableObject = _moveableObjects[objectNumber];
		if (!moveableObject.has_value())
			return;

		const auto& object = Objects[objectNumber];
		if (!object.Animations.empty())
		{
			const auto& frame = GetAnimData(object, 0).Frames.front();
			UpdateAnimation(nullptr, *moveableObject, frame, UINT_MAX);
		}

		auto viewport = TEN::Math::Library::Viewport(0.0f, 0.0f, (float)screenRes.x, (float)screenRes.y);
		auto pos = viewport.Unproject(Vector3(pos2D.x, pos2D.y, 1.0f), projMatrix, viewMatrix, Matrix::Identity);
		auto color = NEUTRAL_COLOR;
		color.w = opacity;

		auto translationMatrix = Matrix::CreateTranslation(pos.x, pos.y, pos.z + BLOCK(1));
		auto rotMatrix = orient.ToRotationMatrix();
		auto scaleMatrix = Matrix::CreateScale(scale);
		auto worldMatrix = scaleMatrix * rotMatrix * translationMatrix;

		auto skinMode = GetSkinningMode(*moveableObject, object.skinIndex);
		float interpolatedFrame = (float)GlobalCounter + GetInterpolationFactor();
		BeginUiObject(viewMatrix * projMatrix, color, g_DrawItems.GetAmbientLight(), GlobalCounter, interpolatedFrame, true);

		if (skinMode == SkinningMode::Full && object.skinIndex >= 0)
			AddUiSkinMesh(*moveableObject, GetMesh(object.skinIndex), worldMatrix);

		for (int i = 0; i < moveableObject->ObjectMeshes.size(); i++)
		{
			if (meshBits && !(meshBits & (1 << i)))
				continue;

			if (skinMode == SkinningMode::Full && g_Level.Meshes[object.meshIndex + i].hidden)
				continue;

			// HACK: Rotate compass needle.
			if (objectNumber == ID_COMPASS_ITEM && i == 1)
				moveableObject->LinearizedBones[i]->ExtraRotation = EulerAngles(0, g_Gui.CompassNeedleAngle - ANGLE(180.0f), 0).ToQuaternion();

			const auto& transform = object.Animations.empty() ? moveableObject->BindPoseTransforms[i] : moveableObject->AnimationTransforms[i];
			AddUiObjectMesh(moveableObject->ObjectMeshes[i], transform * worldMatrix);
		}
	}

	// Records a scripted display item of the 2D layer with the perspective camera of the display items.
	void Renderer::DrawObjectIn3DSpace(const DisplayItem& item)
	{
		if (!item.GetVisible())
			return;

		float alpha = GetInterpolationFactor(true);
		auto color = item.GetInterpolatedColor(alpha);

		if (color.A() <= EPSILON)
			return;

		auto objectNumber = item.GetObjectID();
		auto pos = item.GetInterpolatedPosition(alpha);
		auto orient = item.GetInterpolatedOrientation(alpha);
		auto scale = item.GetInterpolatedScale(alpha);
		int meshBits = item.GetMeshBits();

		auto screenRes = GetScreenResolution();
		float aspectRatio = (float)screenRes.x / (float)screenRes.y;

		auto viewMatrix = Matrix::CreateLookAt(g_DrawItems.GetInterpolatedCameraPosition(alpha), g_DrawItems.GetInterpolatedCameraTargetPosition(alpha), Vector3::Down);
		auto projMatrix = Matrix::CreatePerspectiveFieldOfView(g_DrawItems.GetInterpolatedFov(alpha), aspectRatio, DISPLAY_ITEM_NEAR_PLANE, DISPLAY_ITEM_FAR_PLANE);

		auto& moveableObject = _moveableObjects[objectNumber];
		if (!moveableObject.has_value())
			return;

		const auto& object = Objects[objectNumber];
		if (!object.Animations.empty())
		{
			int animNumber = item.GetAnimNumber();
			int frameNumber = item.GetFrameNumber();
			UpdateAnimation(nullptr, *moveableObject, GetAnimData(object, animNumber).Frames[frameNumber], UINT_MAX);
		}

		auto translationMatrix = Matrix::CreateTranslation(pos.x, pos.y, pos.z);
		auto rotMatrix = orient.ToRotationMatrix();
		auto scaleMatrix = Matrix::CreateScale(scale);
		auto worldMatrix = scaleMatrix * rotMatrix * translationMatrix;

		auto skinMode = GetSkinningMode(*moveableObject, object.skinIndex);
		BeginUiObject(viewMatrix * projMatrix, color, g_DrawItems.GetAmbientLight(), 0, 0.0f, false, item.GetScissor());

		if (skinMode == SkinningMode::Full && object.skinIndex >= 0)
			AddUiSkinMesh(*moveableObject, GetMesh(object.skinIndex), worldMatrix);

		for (int i = 0; i < moveableObject->ObjectMeshes.size(); i++)
		{
			if (meshBits && !item.GetMeshVisible(i))
				continue;

			if (skinMode == SkinningMode::Full && g_Level.Meshes[object.meshIndex + i].hidden)
				continue;

			auto rotOverride = item.GetInterpolatedMeshRotation(i, alpha);
			moveableObject->LinearizedBones[i]->ExtraRotation = rotOverride.ToQuaternion();

			const auto& transform = object.Animations.empty() ? moveableObject->BindPoseTransforms[i] : moveableObject->AnimationTransforms[i];
			AddUiObjectMesh(moveableObject->ObjectMeshes[i], transform * worldMatrix);
		}
	}

	// Skinned mesh of a 2D or 3D space object, with the bone matrices of its current animation.
	void Renderer::AddUiSkinMesh(const RendererObject& moveableObject, const RendererMesh* skinMesh, const Matrix& world)
	{
		auto bones = std::array<Matrix, BONE_COUNT_MAX>{};
		int boneCount = std::min((int)moveableObject.AnimationTransforms.size(), BONE_COUNT_MAX);
		for (int i = 0; i < boneCount; i++)
			bones[i] = moveableObject.BindPoseTransforms[i] * moveableObject.AnimationTransforms[i];

		AddUiObjectMesh(skinMesh, world, bones.data(), boneCount);
	}

	// Intro image shown before the title: fades in, holds for a while and fades out, presenting each frame.
	void Renderer::RenderTitleImage()
	{
		auto image = LoadImage(g_GameFlow->GetGameDir() + g_GameFlow->IntroImagePath);
		if (!image.IsValid())
			return;

		int timeout = 20;
		float currentFade = FADE_FACTOR;

		while (timeout || currentFade > 0.0f)
		{
			if (timeout)
			{
				if (currentFade < 1.0f)
				{
					currentFade = std::clamp(currentFade += FADE_FACTOR, 0.0f, 1.0f);
				}
				else
				{
					timeout--;
				}
			}
			else
			{
				currentFade = std::clamp(currentFade -= FADE_FACTOR, 0.0f, 1.0f);
			}

			DrawFullScreenImage(image, Smoothstep(currentFade));
			Synchronize();

			_uiFrame.BackgroundFade = 0.0f;
			PresentFrame();
		}
	}

	void Renderer::DrawDisplayItems()
	{
		if (!g_DrawItems.IsEmpty())
			g_DrawItems.Draw();
	}

	// Examine screen: the selected inventory object, rotated and scaled by input, with its optional description.
	void Renderer::DrawExamines()
	{
		auto screenPos = Vector2(DISPLAY_SPACE_RES.x / 2, DISPLAY_SPACE_RES.y / 2);

		static EulerAngles orient = EulerAngles::Identity;
		static float scaler = 1.2f;
		float multiplier = g_Renderer.GetFramerateMultiplier();

		short invItem = g_Gui.GetRing(RingTypes::Inventory).CurrentObjectList[g_Gui.GetRing(RingTypes::Inventory).CurrentObjectInList].InventoryItem;

		auto& object = InventoryObjectTable[invItem];

		if (IsHeld(In::MenuUp))
			orient.x += ANGLE(3.0f / multiplier);

		if (IsHeld(In::MenuDown))
			orient.x -= ANGLE(3.0f / multiplier);

		if (IsHeld(In::MenuLeft))
			orient.y += ANGLE(3.0f / multiplier);

		if (IsHeld(In::MenuRight))
			orient.y -= ANGLE(3.0f / multiplier);

		if (IsHeld(In::Sprint))
			scaler += 0.03f / multiplier;

		if (IsHeld(In::Crouch))
			scaler -= 0.03f / multiplier;

		if (scaler > 1.6f)
			scaler = 1.6f;

		if (scaler < 0.8f)
			scaler = 0.8f;

		// Construct string key and try to get it.
		auto stringKey = TEN::Utils::ToLower(GetObjectName((GAME_OBJECT_ID)object.ObjectNumber)) + "_text";
		auto string = g_GameFlow->GetString(stringKey.c_str());

		// If string is found, draw it and shift examine position upwards.
		if (GetHash(string) != GetHash(stringKey))
		{
			AddString(screenPos.x, screenPos.y + screenPos.y / 2.0f, g_GameFlow->GetString(stringKey.c_str()),
				g_GameFlow->GetSettings()->UI.PlainTextColor, SF_Center() | (int)PrintStringFlags::VerticalCenter);

			screenPos.y -= screenPos.y / 4.0f;
		}

		float savedScale = object.Scale1;
		object.Scale1 = scaler;
		DrawObjectIn2DSpace(g_Gui.ConvertInventoryItemToObject(invItem), screenPos, orient, object.Scale1);
		object.Scale1 = savedScale;

		DrawAllStrings();
	}

	// Menu frames: the 2D layer over the last scene, faded by backgroundFade.
	void Renderer::RenderInventoryScene(float backgroundFade)
	{
		_uiFrame.BackgroundFade = backgroundFade;
		_uiFrame.IsMenu = true;

		if (CurrentLevel == 0)
		{
			auto titleMenu = g_Gui.GetMenuToDisplay();
			if (titleMenu == Menu::Title || titleMenu == Menu::Options)
				AddTitleLogo();

			RenderTitleMenu(titleMenu);
			return;
		}

		switch (g_Gui.GetInventoryMode())
		{
		case InventoryMode::Load:
		case InventoryMode::Save:
			RenderLoadSaveMenu();
			break;

		case InventoryMode::InGame:
			RenderNewInventory();
			break;

		case InventoryMode::Statistics:
			DrawStatistics();
			break;

		case InventoryMode::Examine:
			DrawExamines();
			break;

		case InventoryMode::Pause:
			RenderPauseMenu(g_Gui.GetMenuToDisplay());
			break;
		}
	}

	// The logo keeps its aspect ratio and is anchored at a percentage of the window, across any aspect ratio.
	void Renderer::AddTitleLogo()
	{
		if (!_logo.IsValid())
			return;

		auto screenRes = GetScreenResolution();
		float sizeScale = std::min(screenRes.x / DISPLAY_SPACE_RES.x, screenRes.y / DISPLAY_SPACE_RES.y);

		const auto& settings = g_GameFlow->GetSettings()->UI;
		auto halfSize = Vector2((float)_logo.Width, (float)_logo.Height) * settings.TitleLogoScale * sizeScale * 0.5f;
		auto center = Vector2(settings.TitleLogoPosition.x / 100.0f * screenRes.x, settings.TitleLogoPosition.y / 100.0f * screenRes.y);

		// HACK: Color range slippage. Remove in fix color range PR.
		auto color = Vector4(
			settings.TitleLogoColor.GetR() / (float)UCHAR_MAX,
			settings.TitleLogoColor.GetG() / (float)UCHAR_MAX,
			settings.TitleLogoColor.GetB() / (float)UCHAR_MAX,
			settings.TitleLogoColor.GetA() / (float)UCHAR_MAX);

		// The logo rectangle is snapped to integer pixel coordinates.
		auto screenSize = screenRes.ToVector2();
		auto min = Vector2(std::trunc(center.x - halfSize.x), std::trunc(center.y - halfSize.y)) / screenSize;
		auto max = Vector2(std::trunc(center.x + halfSize.x), std::trunc(center.y + halfSize.y)) / screenSize;
		AddUiImage(_logo.Path, min, max, Vector2::Zero, Vector2::One, color * ScreenFadeCurrent, BlendMode::AlphaBlend);
	}

	void Renderer::SetLoadingScreen(const std::string& fileName)
	{
		_loadingScreenTexture = LoadImage(fileName);
	}

	// Freeze mode frame: the last scene as static background or a live scene without HUD, with display sprites,
	// display items and strings on top.
	void Renderer::RenderFreezeMode(float interpFactor, bool staticBackground)
	{
		_interpolationFactor = interpFactor;

		if (staticBackground)
		{
			// The last scene stays as background, as in menus.
			ResetDebugVariables();
			UpdateDumpScreenRenderTarget();
			_uiFrame.IsMenu = true;
		}
		else
		{
			InterpolateCamera(interpFactor);
			RenderScene(_gameCamera, SceneRenderMode::NoHud);
		}

		// Display sprites sorted by priority.
		CollectDisplaySprites(_gameCamera);
		DrawDisplaySprites(_gameCamera, false);
		DrawDisplayItems();
		DrawDisplaySprites(_gameCamera, true);
		DrawAllStrings();

		ClearScene();
		PresentFrame();
	}

	// Presents loading screen frames until the screen fade completes.
	void Renderer::RenderLoadingScreen(float percentage)
	{
		do
		{
			// Loading screen image, or the last scene when there is none.
			if (_loadingScreenTexture.IsValid())
				DrawFullScreenQuad(_loadingScreenTexture, Vector3(ScreenFadeCurrent));

			if (ScreenFadeCurrent && percentage > 0.0f && percentage < 100.0f)
				DrawLoadingBar(percentage);

			_uiFrame.BackgroundFade = _loadingScreenTexture.IsValid() ? 0.0f : ScreenFadeCurrent;
			PresentFrame();

			Synchronize();
			UpdateFadeScreenAndCinematicBars();

		} while (ScreenFading || !ScreenFadedOut);
	}

	// In-game menu frame over the half faded last scene.
	void Renderer::RenderInventory()
	{
		bool isCaptured = TEN::Benchmark::ConsumeMenuScreenshotRequest();

		ResetDebugVariables();
		UpdateDumpScreenRenderTarget();
		RenderInventoryScene(0.5f);

		if (isCaptured)
			CaptureMenuScreenshot();

		PresentFrame();
	}

	// Title frame: the live title level scene with the title menu on top.
	void Renderer::RenderTitle(float interpFactor)
	{
		// The captured frame is not interpolated, as at 30 frames per second.
		bool isCaptured = TEN::Benchmark::ConsumeMenuScreenshotRequest();
		if (isCaptured)
			interpFactor = 0.0f;

		InterpolateCamera(interpFactor);
		DumpGameScene(g_Gui.GetMenuToDisplay() == Menu::SelectLevel ? SceneRenderMode::NoHud : SceneRenderMode::Full);
		RenderInventoryScene(1.0f);

		if (isCaptured)
			CaptureMenuScreenshot();

		PresentFrame();
	}

	// Benchmark runs of the title and inventory capture the menu at a fixed menu tick.
	void Renderer::CaptureMenuScreenshot()
	{
		Backend::g_SceneRenderer.RequestScreenshot(g_GameFlow->GetGameDir() + "Logs/Benchmark.png");
	}

	// The scene behind menus is rendered again after graphics settings change.
	void Renderer::UpdateDumpScreenRenderTarget()
	{
		if (!_graphicsSettingsChanged)
			return;

		UpdateCameraMatrices(&Camera, BLOCK(g_GameFlow->GetLevel(CurrentLevel)->GetFarView()));
		Camera.DisableInterpolation = true;
		DumpGameScene(SceneRenderMode::NoHud, g_GameFlow->GetSettings()->UI.MenuBackgroundBlur);
		_graphicsSettingsChanged = false;
	}

	void Renderer::DrawDebugInfo(RenderView& view)
	{
#if TEST_BUILD
		if (CurrentLevel == 0)
		{
			AddString("TombEngine " + std::string(TEN_VERSION_STRING) + " test build - not for distribution",
				Vector2(20, 560), Vector4(1.0f, 0, 0, 0.5f), 0.7f, 0);
		}
#endif
	}

	// The debug window selects the tab of the new page; the ImGui overlay is shown while a page is active.
	void Renderer::SwitchDebugPage(bool goBack)
	{
		int page = (int)_debugPage;
		goBack ? --page : ++page;

		if (page < (int)RendererDebugPage::None)
		{
			page = (int)RendererDebugPage::Count - 1;
		}
		else if (page >= (int)RendererDebugPage::Count)
		{
			page = (int)RendererDebugPage::None;
		}

		_debugPage = (RendererDebugPage)page;
		_debugPageSelectionFrames = 2;
	}
}
