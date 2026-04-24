#include "framework.h"
#include "Renderer/Renderer.h"
#include "Renderer/Graphics/VRAMTracker.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Internal/TEN/Flow//Level/FlowLevel.h"
#include "Specific/configuration.h"
#include "Specific/Input/InputAction.h"
#include "Specific/level.h"
#include "Specific/trutils.h"
#include "Version.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Gui;
using namespace TEN::Hud;
using namespace TEN::Input;
using namespace TEN::Math;

extern TEN::Renderer::RendererHudBar* g_SFXVolumeBar;
extern TEN::Renderer::RendererHudBar* g_MusicVolumeBar;

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
	constexpr auto MenuVerticalDisplaySettings = 110;
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

	// These bars are only used in menus.
	TEN::Renderer::RendererHudBar* g_MusicVolumeBar = nullptr;
	TEN::Renderer::RendererHudBar* g_SFXVolumeBar	= nullptr;

	void Renderer::InitializeMenuBars(int y)
	{
		static const auto soundSettingColors = std::array<Vector4, RendererHudBar::COLOR_COUNT>
		{
			// Top
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),

			// Center
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),

			// Bottom
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),
			Vector4(0.18f, 0.3f, 0.72f, 1.0f)
		};

		int shift = MenuVerticalLineSpacing / 2;

		g_MusicVolumeBar = new RendererHudBar(_graphicsDevice.get(), Vector2(MenuRightSideEntry, y + shift), RendererHudBar::SIZE_DEFAULT, 1, soundSettingColors);
		GetNextLinePosition(&y);
		g_SFXVolumeBar = new RendererHudBar(_graphicsDevice.get(), Vector2(MenuRightSideEntry, y + shift), RendererHudBar::SIZE_DEFAULT, 1, soundSettingColors);
	}

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

		case AntialiasingMode::Low:
			antialiasMode = STRING_ANTIALIASING_LOW;
			break;

		case AntialiasingMode::Medium:
			antialiasMode = STRING_ANTIALIASING_MEDIUM;
			break;

		case AntialiasingMode::High:
			antialiasMode = STRING_ANTIALIASING_HIGH;
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

			// Enable high framerate
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_HIGH_FRAMERATE), optionColor, SF(titleOption == 7));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableHighFramerate), plainColor, SF(titleOption == 7));
			GetNextLinePosition(&y);

			// Gamma correction
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_GAMMA), optionColor, SF(titleOption == 8));
			AddString(MenuRightSideEntry, y, fmt::format("{:.1f}", g_Gui.GetCurrentSettings().Configuration.Gamma).c_str(), plainColor, SF(titleOption == 8));
			GetNextBlockPosition(&y);

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), optionColor, SF_Center(titleOption == 9));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), optionColor, SF_Center(titleOption == 10));
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
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableTargetHighlighter), plainColor, SF(titleOption == 7));
			GetNextLinePosition(&y);

			// Interaction highlighter
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_INTERACTION_HIGHLIGHTER), optionColor, SF(titleOption == 8));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableInteractionHighlighter), plainColor, SF(titleOption == 8));
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
						int defaultKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Default, (ActionID)k);
						int userKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Custom, (ActionID)k);

						int key = userKeyID ? userKeyID : defaultKeyID;
						AddString(MenuRightSideEntry, y, GetKeyName(key).c_str(), optionColor, SF(false));
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
						int defaultKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Default, (ActionID)(baseIndex + k));
						int userKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Custom, (ActionID)(baseIndex + k));

						int key = userKeyID ? userKeyID : defaultKeyID;
						AddString(MenuRightSideEntry, y, GetKeyName(key).c_str(), optionColor, SF(false));
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
						int defaultKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Default, (ActionID)(baseIndex + k));
						int userKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Custom, (ActionID)(baseIndex + k));

						int key = userKeyID ? userKeyID : defaultKeyID;
						AddString(MenuRightSideEntry, y, GetKeyName(key).c_str(), optionColor, SF(false));
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
						int defaultKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Default, (ActionID)(baseIndex + k));
						int userKeyID = g_Bindings.GetBoundKeyID(BindingProfileID::Custom, (ActionID)(baseIndex + k));

						int key = userKeyID ? userKeyID : defaultKeyID;
						AddString(MenuRightSideEntry, y, GetKeyName(key).c_str(), optionColor, SF(false));
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

			// Setup needed parameters
			menuPos.y = MenuVerticalLineSpacing;

			// Title
			AddString(MenuCenterEntry, 26, g_GameFlow->GetString(STRING_SELECT_LEVEL), g_GameFlow->GetSettings()->UI.HeaderTextColor, SF_Center());
			GetNextBlockPosition(&menuPos.y);

			// Level 0 is always Title Level and level 1 might be Home Level.
			for (int i = (g_GameFlow->IsHomeLevelEnabled() ? 2 : 1); i < g_GameFlow->GetNumLevels(); i++, selectedOption++)
			{
				AddString(
					MenuCenterEntry, menuPos.y, g_GameFlow->GetString(g_GameFlow->GetLevel(i)->NameStringKey.c_str()),
					plainColor, SF_Center(titleOption == selectedOption));
				GetNextNarrowLinePosition(&menuPos.y);
			}

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

		DrawDebugInfo(_gameCamera);
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

		DrawLines2D();
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

	// TODO: Handle opacity
	void Renderer::DrawObjectIn2DSpace(int objectNumber, Vector2 pos2D, EulerAngles orient, float scale, float opacity, int meshBits)
	{
		constexpr auto AMBIENT_LIGHT_COLOR = Vector4(0.5f, 0.5f, 0.5f, 1.0f);

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
		auto projMatrix = Matrix::CreateOrthographic(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight(), -BLOCK(1), BLOCK(1));

		auto& moveableObject = _moveableObjects[objectNumber];
		if (!moveableObject.has_value())
			return;

		const auto& object = Objects[objectNumber];
		if (!object.Animations.empty())
		{
			auto interpData = KeyframeInterpolationData(
				GetAnimData(object, 0).Keyframes[0],
				GetAnimData(object, 0).Keyframes[0],
				0.0f);
			UpdateAnimation(nullptr, *moveableObject, interpData, UINT_MAX);
		}

		auto pos = _graphicsDevice->Unproject(Vector3(pos2D.x, pos2D.y, 1.0f), projMatrix, viewMatrix, Matrix::Identity);
		auto color = NEUTRAL_COLOR;
		color.w = opacity;

		// Set vertex buffer.
		_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

		// Set matrices.
		auto hudCamera = CCameraMatrixBuffer{};
		hudCamera.CamDirectionWS = -Vector4::UnitZ;
		hudCamera.ViewProjection = viewMatrix * projMatrix;
		hudCamera.Frame = GlobalCounter;
		hudCamera.InterpolatedFrame = (float)GlobalCounter + GetInterpolationFactor();
		hudCamera.Gamma = g_Configuration.Gamma;
		UpdateConstantBuffer(&hudCamera, _cbCameraMatrices.get());
		_graphicsDevice->BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		_graphicsDevice->BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Camera, _cbCameraMatrices.get());

		_shaders.Bind(Shader::Inventory);

		// Construct world matrix.
		auto translationMatrix = Matrix::CreateTranslation(pos.x, pos.y, pos.z + BLOCK(1));
		auto rotMatrix = orient.ToRotationMatrix();
		auto scaleMatrix = Matrix::CreateScale(scale);
		auto worldMatrix = scaleMatrix * rotMatrix * translationMatrix;

		auto skinMode = GetSkinningMode(*moveableObject, object.skinIndex);

		_stItem.Color = color;
		_stItem.AmbientLight = g_DrawItems.GetAmbientLight();;
		_stItem.Skinned = (int)skinMode;

		if (skinMode == SkinningMode::Full && object.skinIndex >= 0)
		{
			_stItem.World = worldMatrix;

			// Calculate bones matrices for skinning
			for (int m = 0; m < moveableObject->AnimationTransforms.size(); m++)
				_stItem.BonesMatrices[m] = moveableObject->BindPoseTransforms[m] * moveableObject->AnimationTransforms[m];

			_stItem.BoneLightModes[0] = (int)LightMode::Dynamic;

			UpdateConstantBuffer(&_stItem, _cbItem.get());
			BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Item, _cbItem.get());
			BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Item, _cbItem.get());

			// Draw the skin mesh.
			const auto skinMesh = GetMesh(object.skinIndex);

			for (int animated = 0; animated < 2; animated++)
			{
				for (const auto& bucket : skinMesh->Buckets)
				{
					if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
						continue;

					SetBlendMode(GetBlendModeFromAlpha((bucket.BlendMode == BlendMode::AlphaTest) ? BlendMode::AlphaBlend : bucket.BlendMode, color.w));
					SetCullMode(CullMode::CounterClockwise);
					SetDepthState(DepthState::Write);

					BindBucketTextures(bucket, TextureSource::Moveables, animated);
					BindMaterial(bucket.MaterialIndex, false);

					SetAlphaTest((bucket.BlendMode == BlendMode::AlphaTest) ? AlphaTestMode::GreatherThan : AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);
					_numMoveablesDrawCalls++;
				}
			}
		}

		for (int i = 0; i < moveableObject->ObjectMeshes.size(); i++)
			_stItem.BonesMatrices[i] = Matrix::Identity;

		for (int i = 0; i < moveableObject->ObjectMeshes.size(); i++)
		{
			if (meshBits && !(meshBits & (1 << i)))
				continue;

			if (skinMode == SkinningMode::Full && g_Level.Meshes[object.meshIndex + i].hidden)
				continue;

			// HACK: Rotate compass needle.
			if (objectNumber == ID_COMPASS_ITEM && i == 1)
				moveableObject->LinearizedBones[i]->ExtraRotation = EulerAngles(0, g_Gui.CompassNeedleAngle - ANGLE(180.0f), 0).ToQuaternion();

			if (!object.Animations.empty())
			{
				_stItem.World = moveableObject->AnimationTransforms[i] * worldMatrix;
			}
			else
			{
				_stItem.World = moveableObject->BindPoseTransforms[i] * worldMatrix;
			}

			_stItem.BoneLightModes[i] = (int)LightMode::Dynamic;

			UpdateConstantBuffer(&_stItem, _cbItem.get());

			BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Item, _cbItem.get());
			BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Item, _cbItem.get());

			const auto& mesh = *moveableObject->ObjectMeshes[i];

			for (int animated = 0; animated < 2; animated++)
			{		
				for (const auto& bucket : mesh.Buckets)
				{
					if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
						continue;

					SetBlendMode(GetBlendModeFromAlpha((bucket.BlendMode == BlendMode::AlphaTest) ? BlendMode::AlphaBlend : bucket.BlendMode, color.w));
					SetCullMode(CullMode::CounterClockwise);
					SetDepthState(DepthState::Write);

					BindBucketTextures(bucket, TextureSource::Moveables, animated);
					BindMaterial(bucket.MaterialIndex, false);

					SetAlphaTest((bucket.BlendMode == BlendMode::AlphaTest) ? AlphaTestMode::GreatherThan : AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);
					_numMoveablesDrawCalls++;
				}
			}
		}
	}

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

		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		float aspectRatio = (float)(_graphicsDevice->GetScreenWidth()) / _graphicsDevice->GetScreenHeight();

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
			int prevFrameNumber = item.GetPrevFrameNumber();

			auto interpData = KeyframeInterpolationData(
				GetAnimData(object, animNumber).Keyframes[prevFrameNumber],
				GetAnimData(object, animNumber).Keyframes[frameNumber],
				alpha);
			UpdateAnimation(nullptr, *moveableObject, interpData, UINT_MAX);
		}

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::Write);

		// Set vertex buffer.
		_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

		// Set shaders.
		_shaders.Bind(Shader::Inventory);

		// Set matrices.
		auto hudCamera = CCameraMatrixBuffer{};
		hudCamera.CamDirectionWS = -Vector4::UnitZ;
		hudCamera.ViewProjection = viewMatrix * projMatrix;
		hudCamera.Gamma = g_Configuration.Gamma;
		UpdateConstantBuffer(&hudCamera, _cbCameraMatrices.get());
		_graphicsDevice->BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		_graphicsDevice->BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Camera, _cbCameraMatrices.get());

		_shaders.Bind(Shader::Inventory);

		// Construct world matrix. // pos.x, pos.y, pos.z
		auto translationMatrix = Matrix::CreateTranslation(pos.x, pos.y, pos.z);
		auto rotMatrix = orient.ToRotationMatrix();
		auto scaleMatrix = Matrix::CreateScale(scale);
		auto worldMatrix = scaleMatrix * rotMatrix * translationMatrix;

		auto skinMode = GetSkinningMode(*moveableObject, object.skinIndex);

		_stItem.Color = color;
		_stItem.AmbientLight = g_DrawItems.GetAmbientLight();
		_stItem.Skinned = (int)skinMode;

		if (skinMode == SkinningMode::Full && object.skinIndex >= 0)
		{
			_stItem.World = worldMatrix;

			// Calculate bones matrices for skinning.
			for (int m = 0; m < moveableObject->AnimationTransforms.size(); m++)
				_stItem.BonesMatrices[m] = moveableObject->BindPoseTransforms[m] * moveableObject->AnimationTransforms[m];

			_stItem.BoneLightModes[0] = (int)LightMode::Dynamic;

			UpdateConstantBuffer(&_stItem, _cbItem.get());
			BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Item, _cbItem.get());
			BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Item, _cbItem.get());

			// Get skin mesh.
			const auto* skinMesh = GetMesh(object.skinIndex);

			for (int animated = 0; animated < 2; animated++)
			{
				for (const auto& bucket : skinMesh->Buckets)
				{
					// TODO: Remove ^.
					if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
						continue;

					SetBlendMode(GetBlendModeFromAlpha((bucket.BlendMode == BlendMode::AlphaTest) ? BlendMode::AlphaBlend : bucket.BlendMode, color.w));
					SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

					SetCullMode(CullMode::CounterClockwise);
					SetDepthState(DepthState::Write);

					BindBucketTextures(bucket, TextureSource::Moveables, animated);
					BindMaterial(bucket.MaterialIndex, false);

					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);
					_numMoveablesDrawCalls++;
				}
			}
		}

		for (int i = 0; i < moveableObject->ObjectMeshes.size(); i++)
			_stItem.BonesMatrices[i] = Matrix::Identity;

		for (int i = 0; i < moveableObject->ObjectMeshes.size(); i++)
		{
			if (meshBits && !item.GetMeshVisible(i))
				continue;

			if (skinMode == SkinningMode::Full && g_Level.Meshes[object.meshIndex + i].hidden)
				continue;

			auto rotOverride = item.GetInterpolatedMeshRotation(i, alpha);
			moveableObject->LinearizedBones[i]->ExtraRotation = rotOverride.ToQuaternion();

			if (!object.Animations.empty())
			{
				_stItem.World = moveableObject->AnimationTransforms[i] * worldMatrix;
			}
			else
			{
				_stItem.World = moveableObject->BindPoseTransforms[i] * worldMatrix;
			}

			_stItem.BoneLightModes[i] = (int)LightMode::Dynamic;

			UpdateConstantBuffer(&_stItem, _cbItem.get());
			BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Item, _cbItem.get());
			BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Item, _cbItem.get());

			const auto& mesh = *moveableObject->ObjectMeshes[i];

			for (int animated = 0; animated < 2; animated++)
			{
				for (const auto& bucket : mesh.Buckets)
				{
					// TODO: Remove ^.
					if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
						continue;

					SetBlendMode(GetBlendModeFromAlpha((bucket.BlendMode == BlendMode::AlphaTest) ? BlendMode::AlphaBlend : bucket.BlendMode, color.w));
					SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

					SetCullMode(CullMode::CounterClockwise);
					SetDepthState(DepthState::Write);

					BindBucketTextures(bucket, TextureSource::Moveables, animated);
					BindMaterial(bucket.MaterialIndex, false);

					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);
					_numMoveablesDrawCalls++;
				}
			}
		}
	}

	void Renderer::RenderTitleImage()
	{
		auto texture = SetTextureOrDefault(TEN::Utils::ToWString(g_GameFlow->GetGameDir() + g_GameFlow->IntroImagePath.c_str()));
		if (texture == nullptr || !texture->IsValid())
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

			DrawFullScreenImage(texture.get(), Smoothstep(currentFade), _backBuffer->GetRenderTarget(), _backBuffer->GetDepthTarget());
			Synchronize();

			_graphicsDevice->Present();
			_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
		}
	}

	void Renderer::DrawDisplayItems()
	{
		if (!g_DrawItems.IsEmpty())
		{
			_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
			g_DrawItems.Draw();
		}
	}

	void Renderer::DrawExamines()
	{
		auto screenPos = Vector2(DISPLAY_SPACE_RES.x / 2, DISPLAY_SPACE_RES.y / 2);

		static EulerAngles orient = EulerAngles::Identity;
		static float scaler = 1.2f;
		float multiplier = g_Renderer.GetFramerateMultiplier();

		short invItem = g_Gui.GetRing(RingTypes::Inventory).CurrentObjectList[g_Gui.GetRing(RingTypes::Inventory).CurrentObjectInList].InventoryItem;

		auto& object = InventoryObjectTable[invItem];

		if (IsHeld(In::Forward))
			orient.x += ANGLE(3.0f / multiplier);

		if (IsHeld(In::Back))
			orient.x -= ANGLE(3.0f / multiplier);

		if (IsHeld(In::Left))
			orient.y += ANGLE(3.0f / multiplier);

		if (IsHeld(In::Right))
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

	void Renderer::RenderInventoryScene(IRenderSurface2D* renderTarget, ITextureBase* background, float backgroundFade)
	{
		// Set basic render states
		SetBlendMode(BlendMode::Opaque, true);
		SetDepthState(DepthState::Write, true);
		SetCullMode(CullMode::CounterClockwise, true);

		// Bind and clear render target
		std::vector<IRenderTarget2D*> renderTargets;
		renderTargets.push_back(_renderTarget->GetRenderTarget());
		renderTargets.push_back(_emissiveAndRoughnessRenderTarget->GetRenderTarget());

		_graphicsDevice->SetViewport(_viewport);
		_graphicsDevice->SetScissor(_viewport);

		_graphicsDevice->ClearRenderTarget2D(_renderTarget->GetRenderTarget(), Colors::Black);
		_graphicsDevice->ClearRenderTarget2D(_emissiveAndRoughnessRenderTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->ClearDepthStencil(_renderTarget->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
		
		if (background != nullptr)
			DrawFullScreenImage(background, backgroundFade, _renderTarget->GetRenderTarget(), _renderTarget->GetDepthTarget());

		_graphicsDevice->BindRenderTargets(renderTargets, _renderTarget->GetDepthTarget());
		_graphicsDevice->ClearDepthStencil(_renderTarget->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

		// Set vertex buffer.
		_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

		// Set shaders.
		_shaders.Bind(Shader::Inventory);

		if (CurrentLevel == 0)
		{
			auto titleMenu = g_Gui.GetMenuToDisplay();
			bool drawLogo = (titleMenu == Menu::Title || titleMenu == Menu::Options);

			if (drawLogo && _logo != nullptr)
			{
				float factorX = (float)_graphicsDevice->GetScreenWidth() / DISPLAY_SPACE_RES.x;
				float factorY = (float)_graphicsDevice->GetScreenHeight() / DISPLAY_SPACE_RES.y;
				float scale = _graphicsDevice->GetScreenWidth() > _graphicsDevice->GetScreenHeight() ? factorX : factorY;

				auto& settings = g_GameFlow->GetSettings()->UI;

				float logoWidthScaled  = _logo->GetWidth() * settings.TitleLogoScale;
				float logoHeightScaled = _logo->GetHeight() * settings.TitleLogoScale;

				float centerX = (settings.TitleLogoPosition.x / 100.0f) * DISPLAY_SPACE_RES.x;
				float centerY = (settings.TitleLogoPosition.y / 100.0f) * DISPLAY_SPACE_RES.y;

				float logoLeft   = centerX - logoWidthScaled  * 0.5f;
				float logoRight  = centerX + logoWidthScaled  * 0.5f;
				float logoTop    = centerY - logoHeightScaled * 0.5f;
				float logoBottom = centerY + logoHeightScaled * 0.5f;

				RendererRectangle rect;
				rect.Left   = logoLeft   * scale;
				rect.Right  = logoRight  * scale;
				rect.Top    = logoTop    * scale;
				rect.Bottom = logoBottom * scale;

				// HACK: Color range slippage. Remove in fix color range PR.
				auto color = Vector4(settings.TitleLogoColor.GetR() / (float)UCHAR_MAX,
					settings.TitleLogoColor.GetG() / (float)UCHAR_MAX,
					settings.TitleLogoColor.GetB() / (float)UCHAR_MAX,
					settings.TitleLogoColor.GetA() / (float)UCHAR_MAX);

				_spriteBatch->Begin(SpriteSortingMode::BackToFront, BlendMode::AlphaBlend);
				_spriteBatch->Draw(_logo.get(), rect, color * ScreenFadeCurrent);

				_spriteBatch->End();
			}

			RenderTitleMenu(titleMenu);
		}
		else
		{
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

		_graphicsDevice->ClearDepthStencil(_renderTarget->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

		ApplyGlow(_renderTarget.get(), _gameCamera);
		ApplyAntialiasing(_renderTarget.get(), _gameCamera);

		CopyRenderTarget(_renderTarget.get(), renderTarget, _gameCamera);
	}

	void Renderer::SetLoadingScreen(std::wstring& fileName)
	{
		_loadingScreenTexture = SetTextureOrDefault(fileName);
	}

	void Renderer::RenderFreezeMode(float interpFactor, bool staticBackground)
	{
		_interpolationFactor = interpFactor;

		if (staticBackground)
		{
			// Set basic render states.
			SetBlendMode(BlendMode::Opaque);
			SetCullMode(CullMode::CounterClockwise);

			// Clear screen
			_graphicsDevice->ClearRenderTarget2D(_backBuffer->GetRenderTarget(), Colors::Black);
			_graphicsDevice->ClearRenderTarget2D(_emissiveAndRoughnessRenderTarget->GetRenderTarget(), Colors::Transparent);
			_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

			std::vector<IRenderTarget2D*> renderTargets;
			renderTargets.push_back(_backBuffer->GetRenderTarget());
			renderTargets.push_back(_emissiveAndRoughnessRenderTarget->GetRenderTarget());

			// Bind back buffer.
			_graphicsDevice->BindRenderTargets(renderTargets, _backBuffer->GetDepthTarget());
			_graphicsDevice->SetViewport(_viewport);
			_graphicsDevice->SetScissor(_viewport);

			// Draw full screen background.
			DrawFullScreenQuad(_dumpScreenRenderTarget->GetRenderTarget(), Vector3::One);
		}
		else
		{
			InterpolateCamera(interpFactor);
			RenderScene(_backBuffer.get(), _gameCamera, SceneRenderMode::NoHud);
		}

		// Draw display sprites sorted by priority.
		CollectDisplaySprites(_gameCamera);
		DrawDisplaySprites(_gameCamera, false);
		DrawDisplayItems();
		DrawDisplaySprites(_gameCamera, true);
		DrawAllStrings();

		if (staticBackground)
		{
			BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::PostProcess, _cbPostProcessBuffer.get());
			BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::PostProcess, _cbPostProcessBuffer.get());

			ApplyGlow(_renderTarget.get(), _gameCamera);
			ApplyAntialiasing(_renderTarget.get(), _gameCamera);
			CopyRenderTarget(_renderTarget.get(), _backBuffer.get(), _gameCamera);
		}

		ClearScene();

		_graphicsDevice->ClearState();
		_graphicsDevice->Present();
	}

	void Renderer::RenderLoadingScreen(float percentage)
	{
		// Set basic render states.
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		do
		{
			// Clear screen.
			_graphicsDevice->ClearRenderTarget2D(_backBuffer->GetRenderTarget(), Colors::Black);
			_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

			// Bind back buffer.
			_graphicsDevice->BindRenderTarget(_backBuffer->GetRenderTarget(), _backBuffer->GetDepthTarget());
			_graphicsDevice->SetViewport(_viewport);
			_graphicsDevice->SetScissor(_viewport);

			// Draw fullscreen background. If unavailable, draw last dumped game scene.
			if (_loadingScreenTexture)
			{
				DrawFullScreenQuad(_loadingScreenTexture.get(), Vector3(ScreenFadeCurrent, ScreenFadeCurrent, ScreenFadeCurrent));
			}
			else if (_dumpScreenRenderTarget)
			{
				DrawFullScreenQuad(_dumpScreenRenderTarget->GetRenderTarget(), Vector3(ScreenFadeCurrent, ScreenFadeCurrent, ScreenFadeCurrent));
			}

			if (ScreenFadeCurrent && percentage > 0.0f && percentage < 100.0f)
				DrawLoadingBar(percentage);

			_graphicsDevice->Present();
			_graphicsDevice->ClearState();

			Synchronize();
			UpdateFadeScreenAndCinematicBars();

		} while (ScreenFading || !ScreenFadedOut);
	}

	void Renderer::RenderInventory()
	{
		if (_graphicsSettingsChanged)
		{
			UpdateCameraMatrices(&Camera, BLOCK(g_GameFlow->GetLevel(CurrentLevel)->GetFarView()));
			Camera.DisableInterpolation = true;
			DumpGameScene();
			_graphicsSettingsChanged = false;
		}

		_graphicsDevice->ClearRenderTarget2D(_backBuffer->GetRenderTarget(), Colors::Black);
		_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

		// Reset GPU state.
		SetBlendMode(BlendMode::Opaque, true);
		SetDepthState(DepthState::Write, true);
		SetCullMode(CullMode::CounterClockwise, true);

		RenderInventoryScene(_backBuffer.get(), _dumpScreenRenderTarget->GetRenderTarget(), 0.5f);

		_graphicsDevice->Present();
	}

	void Renderer::RenderTitle(float interpFactor)
	{
		_stringsToDraw.clear();
		_isLocked = false;

		InterpolateCamera(interpFactor);
		DumpGameScene();

		_graphicsDevice->ClearRenderTarget2D(_backBuffer->GetRenderTarget(), Colors::Black);
		_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

		RenderInventoryScene(_backBuffer.get(), _dumpScreenRenderTarget->GetRenderTarget(), 1.0f);
		
		_graphicsDevice->Present();

		_isLocked = true;
	}

	void Renderer::DrawDebugRenderTargets(RenderView& view)
	{
		if (_debugPage != RendererDebugPage::RendererStats)
			return;

		float aspectRatio = _graphicsDevice->GetScreenWidth() / (float)_graphicsDevice->GetScreenHeight();
		int thumbWidth = _graphicsDevice->GetScreenWidth() / 8;
		int thumbY = 0;

		auto rect = RendererRectangle{};

		_spriteBatch->Begin(SpriteSortingMode::Deferred, BlendMode::Opaque);

		rect.Left = _graphicsDevice->GetScreenWidth() - thumbWidth;
		rect.Top = thumbY;
		rect.Right = rect.Left + thumbWidth;
		rect.Bottom = rect.Top + thumbWidth / aspectRatio;

		_spriteBatch->Draw(_normalsAndMaterialIndexRenderTarget->GetRenderTarget(), rect, Vector4::One);
		thumbY += thumbWidth / aspectRatio;

		rect.Left = _graphicsDevice->GetScreenWidth() - thumbWidth;
		rect.Top = thumbY;
		rect.Right = rect.Left + thumbWidth;
		rect.Bottom = rect.Top + thumbWidth / aspectRatio;

		rect.Left = _graphicsDevice->GetScreenWidth() - thumbWidth;
		rect.Top = thumbY;
		rect.Right = rect.Left + thumbWidth;
		rect.Bottom = rect.Top + thumbWidth / aspectRatio;

		_spriteBatch->Draw(_SSAOBlurredRenderTarget->GetRenderTarget(), rect, Vector4::One);
		thumbY += thumbWidth / aspectRatio;

		if (g_Configuration.AntialiasingMode > AntialiasingMode::Low)
		{
			rect.Left = _graphicsDevice->GetScreenWidth() - thumbWidth;
			rect.Top = thumbY;
			rect.Right = rect.Left + thumbWidth;
			rect.Bottom = rect.Top + thumbWidth / aspectRatio;

			_spriteBatch->Draw(_SMAAEdgesRenderTarget->GetRenderTarget(), rect, Vector4::One);
			thumbY += thumbWidth / aspectRatio;

			rect.Left = _graphicsDevice->GetScreenWidth() - thumbWidth;
			rect.Top = thumbY;
			rect.Right = rect.Left + thumbWidth;
			rect.Bottom = rect.Top + thumbWidth / aspectRatio;

			_spriteBatch->Draw(_SMAABlendRenderTarget->GetRenderTarget(), rect, Vector4::One);
			thumbY += thumbWidth / aspectRatio;
		}

		_spriteBatch->End();
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

		const auto& playerItem = *LaraItem;
		const auto& player = GetLaraInfo(playerItem);

		_currentLineHeight = DISPLAY_SPACE_RES.y / 30;

		const auto& room = g_Level.Rooms[playerItem.RoomNumber];

		switch (_debugPage)
		{
		case RendererDebugPage::None:
			break;

		case RendererDebugPage::RendererStats:
			PrintDebugMessage("RENDERER STATS");
			PrintDebugMessage(" ");
			PrintDebugMessage("FPS: %3.2f", _fps);
			PrintDebugMessage("Resolution: %d x %d", _graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
			PrintDebugMessage("GPU: %s", g_Configuration.AdapterName.c_str());
			PrintDebugMessage("Update time: %d", _timeUpdate);
			PrintDebugMessage("Frame time: %d", _timeFrame);
			PrintDebugMessage("ControlPhase() time: %d", ControlPhaseTime);
			PrintDebugMessage("TOTAL draw calls: %d", _numDrawCalls);
			PrintDebugMessage("    Rooms: %d", _numRoomsDrawCalls);
			PrintDebugMessage("    Movables: %d", _numMoveablesDrawCalls);
			PrintDebugMessage("    Statics: %d", _numStaticsDrawCalls);
			PrintDebugMessage("    Instanced Statics: %d", _numInstancedStaticsDrawCalls);
			PrintDebugMessage("    Sprites: %d", _numSpritesDrawCalls);
			PrintDebugMessage("    Instanced Sprites: %d", _numInstancedSpritesDrawCalls);
			PrintDebugMessage("TOTAL triangles: %d", _numTriangles);
			PrintDebugMessage("Sprites: %d", view.SpritesToDraw.size());
			PrintDebugMessage("SORTED draw calls: %d", (_numSortedRoomsDrawCalls + _numSortedMoveablesDrawCalls + _numSortedStaticsDrawCalls + _numSortedSpritesDrawCalls));
			PrintDebugMessage("    Rooms: %d", _numSortedRoomsDrawCalls);
			PrintDebugMessage("    Movables: %d", _numSortedMoveablesDrawCalls);
			PrintDebugMessage("    Statics: %d", _numSortedStaticsDrawCalls);
			PrintDebugMessage("    Sprites: %d", _numSortedSpritesDrawCalls);
			PrintDebugMessage("SHADOW MAP draw calls: %d", _numShadowMapDrawCalls);
			PrintDebugMessage("DEBRIS draw calls: %d", _numDebrisDrawCalls);
			PrintDebugMessage("Constant buffer updates: %d", _numConstantBufferUpdates);
			PrintDebugMessage("Material updates: %d requested, %d executed", _numRequestedMaterialsUpdates, _numExecutedMaterialsUpdates);
			break;

		case RendererDebugPage::MemoryStats:
		{
			auto& vram = Graphics::VRAMTracker::Get();
			const auto& adapter = vram.GetAdapterInfo();
			auto toMB = [](size_t bytes) { return bytes / (1024.0f * 1024.0f); };

			PrintDebugMessage("MEMORY STATS");
			PrintDebugMessage(" ");
			PrintDebugMessage("GPU: %s", adapter.Name.c_str());
			PrintDebugMessage("Installed VRAM: %.0f MB", toMB(adapter.DedicatedVideoMemory));
			PrintDebugMessage("Dedicated System Memory: %.0f MB", toMB(adapter.DedicatedSystemMemory));
			PrintDebugMessage("Shared System Memory: %.0f MB", toMB(adapter.SharedSystemMemory));
			PrintDebugMessage(" ");
			PrintDebugMessage("Tracked VRAM Usage: %.2f MB", toMB(vram.GetTotal()));
			PrintDebugMessage("    Textures: %.2f MB", toMB(vram.GetCategory(Graphics::VRAMCategory::Texture)));
			PrintDebugMessage("    Render Targets: %.2f MB", toMB(vram.GetCategory(Graphics::VRAMCategory::RenderTarget)));
			PrintDebugMessage("    Vertex Buffers: %.2f MB", toMB(vram.GetCategory(Graphics::VRAMCategory::VertexBuffer)));
			PrintDebugMessage("    Index Buffers: %.2f MB", toMB(vram.GetCategory(Graphics::VRAMCategory::IndexBuffer)));
			break;
		}

		case RendererDebugPage::DimensionStats:
			PrintDebugMessage("DIMENSION STATS");
			PrintDebugMessage(" ");
			PrintDebugMessage("Position: %d, %d, %d", playerItem.Pose.Position.x, playerItem.Pose.Position.y, playerItem.Pose.Position.z);
			PrintDebugMessage("Orientation: %d, %d, %d", playerItem.Pose.Orientation.x, playerItem.Pose.Orientation.y, playerItem.Pose.Orientation.z);
			PrintDebugMessage("Scale: %.3f, %.3f, %.3f", playerItem.Pose.Scale.x, playerItem.Pose.Scale.y, playerItem.Pose.Scale.z);
			PrintDebugMessage("Room number: %d", playerItem.RoomNumber);
			PrintDebugMessage("PathfindingBoxID: %d", playerItem.BoxNumber);
			PrintDebugMessage((player.Context.WaterSurfaceDist == -NO_HEIGHT ? "WaterSurfaceDist: N/A" : "WaterSurfaceDist: %d"), player.Context.WaterSurfaceDist);
			PrintDebugMessage("Room Bounds: %d, %d, %d, %d", room.Position.z, room.Position.z, room.Position.z + BLOCK(room.XSize), room.Position.z + BLOCK(room.ZSize));
			PrintDebugMessage("Room.y, minFloor, maxCeiling: %d, %d, %d ", room.Position.y, room.BottomHeight, room.TopHeight);
			PrintDebugMessage("Camera Position: %d, %d, %d", Camera.pos.x, Camera.pos.y, Camera.pos.z);
			PrintDebugMessage("Camera LookAt: %d, %d, %d", Camera.target.x, Camera.target.y, Camera.target.z);
			PrintDebugMessage("Camera RoomNumber: %d", Camera.pos.RoomNumber);
			break;

		case RendererDebugPage::PlayerStats:
			PrintDebugMessage("PLAYER STATS");
			PrintDebugMessage(" ");
			PrintDebugMessage("AnimObjectID: %d", playerItem.Animation.AnimObjectID);
			PrintDebugMessage("AnimNumber: %d", playerItem.Animation.AnimNumber);
			PrintDebugMessage("FrameNumber: %d", playerItem.Animation.FrameNumber);
			PrintDebugMessage("ActiveState: %d", playerItem.Animation.ActiveState);
			PrintDebugMessage("TargetState: %d", playerItem.Animation.TargetState);
			PrintDebugMessage("Velocity: %.3f, %.3f, %.3f", playerItem.Animation.Velocity.z, playerItem.Animation.Velocity.y, playerItem.Animation.Velocity.x);
			PrintDebugMessage("IsAirborne: %d", playerItem.Animation.IsAirborne);
			PrintDebugMessage("HandStatus: %d", player.Control.HandStatus);
			PrintDebugMessage("WaterStatus: %d", player.Control.WaterStatus);
			PrintDebugMessage("CanClimbLadder: %d", player.Control.CanClimbLadder);
			PrintDebugMessage("CanMonkeySwing: %d", player.Control.CanMonkeySwing);
			PrintDebugMessage("Target HitPoints: %d", player.TargetEntity ? player.TargetEntity->HitPoints : 0);
			break;

		case RendererDebugPage::InputStats:
		{
			int	 size			 = (int)ACTION_ID_GROUPS[(int)USER_ACTION_GROUP_IDS.back()].back();
			auto clickedActions	 = BitField(size);
			auto heldActions	 = BitField(size);
			auto releasedActions = BitField(size);

			for (auto actionGroupID : USER_ACTION_GROUP_IDS)
			{
				for (auto actionID : ACTION_ID_GROUPS[(int)actionGroupID])
				{
					const auto& action = ActionMap.at(actionID);

					if (action.IsClicked())
						clickedActions.Set((int)action.GetID());

					if (action.IsHeld())
						heldActions.Set((int)action.GetID());

					if (action.IsReleased())
						releasedActions.Set((int)action.GetID());
				}
			}

			PrintDebugMessage("INPUT STATS");
			PrintDebugMessage(" ");
			PrintDebugMessage(("Clicked actions: " + clickedActions.ToString()).c_str());
			PrintDebugMessage(("Held actions: " + heldActions.ToString()).c_str());
			PrintDebugMessage(("Released actions: " + releasedActions.ToString()).c_str());
			PrintDebugMessage("Move axes: %.3f, %.3f", GetMoveAxis().x, GetMoveAxis().y);
			PrintDebugMessage("Camera axes: %.3f, %.3f", GetCameraAxis().x, GetCameraAxis().y);
			PrintDebugMessage("Mouse axes: %.3f, %.3f", GetMouseAxis().x, GetMouseAxis().y);
			PrintDebugMessage("Cursor pos: %.3f, %.3f", GetMouse2DPosition().x, GetMouse2DPosition().y);
		}
			break;

		case RendererDebugPage::CollisionStats:
			PrintDebugMessage("COLLISION STATS");
			PrintDebugMessage(" ");
			PrintDebugMessage("Collision type: %d", LaraCollision.CollisionType);
			PrintDebugMessage("Bridge item ID: %d", LaraCollision.Middle.Bridge);
			PrintDebugMessage("Front floor: %d", LaraCollision.Front.Floor);
			PrintDebugMessage("Front left floor: %d", LaraCollision.FrontLeft.Floor);
			PrintDebugMessage("Front right floor: %d", LaraCollision.FrontRight.Floor);
			PrintDebugMessage("Front ceil: %d", LaraCollision.Front.Ceiling);
			PrintDebugMessage("Front left ceil: %d", LaraCollision.FrontLeft.Ceiling);
			PrintDebugMessage("Front right ceil: %d", LaraCollision.FrontRight.Ceiling);
			break;

		case RendererDebugPage::PathfindingStats:
			PrintDebugMessage("PATHFINDING STATS");
			PrintDebugMessage(" ");
			{
				int playerBoxID = playerItem.BoxNumber == NO_VALUE ? GetPointCollision(playerItem).GetBottomSector().PathfindingBoxID : playerItem.BoxNumber;
				PrintDebugMessage("Player box number: %d", playerBoxID);

				auto creatures = GetActiveCreatures();

				if (PathfindingDisplayIndex >= 0)
				{
					if (creatures.empty() || creatures.size() <= PathfindingDisplayIndex)
						break;

					auto& enemy = g_Level.Items[creatures[PathfindingDisplayIndex]];
					auto* creatureInfo = (CreatureInfo*)enemy.Data;
					auto zoneType = creatureInfo->LOT.Zone;
					auto& zones = g_Level.Zones[(int)zoneType][(int)FlipStatus];

					PrintDebugMessage("Player zone number: %d", playerBoxID == NO_VALUE ? NO_VALUE : zones[playerBoxID]);
					PrintDebugMessage("Enemy: %s", enemy.Name.c_str());
					PrintDebugMessage("Enemy box number: %d", enemy.BoxNumber);
					PrintDebugMessage("Enemy zone type: %d", zoneType);
					PrintDebugMessage("Enemy zone number: %d", enemy.BoxNumber == NO_VALUE ? NO_VALUE : zones[enemy.BoxNumber]);

					auto mood = "Unknown";
					switch (creatureInfo->Mood)
					{
						case MoodType::Attack: mood = "Attack"; break;
						case MoodType::Stalk:  mood = "Stalk";  break;
						case MoodType::Escape: mood = "Escape"; break;
						case MoodType::Bored:  mood = "Bored";  break;
					}
					PrintDebugMessage("Enemy mood: %s", mood);
				}
				else if (!creatures.empty())
					PrintDebugMessage("Push TAB to scroll through enemies");
			}
			break;

		case RendererDebugPage::CollisionMeshStats:
			PrintDebugMessage("COLLISION MESH STATS");
			PrintDebugMessage(" ");
			break;

		case RendererDebugPage::PortalStats:
			PrintDebugMessage("PORTAL STATS");
			PrintDebugMessage(" ");
			PrintDebugMessage("Camera room number: %d", Camera.pos.RoomNumber);
			PrintDebugMessage("Room collector time: %d", _timeRoomsCollector);
			PrintDebugMessage("Rooms: %d", view.RoomsToDraw.size());
			PrintDebugMessage("    CheckPortal() calls: %d", _numCheckPortalCalls);
			PrintDebugMessage("    GetVisibleRooms() calls: %d", _numGetVisibleRoomsCalls);
			PrintDebugMessage("    Dot products: %d", _numDotProducts);
			break;

		case RendererDebugPage::WireframeMode:
			PrintDebugMessage("WIREFRAME MODE");
			break;

		default:
			break;
		}
	}

	RendererDebugPage Renderer::GetCurrentDebugPage()
	{
		return _debugPage;
	}

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
	}
}
