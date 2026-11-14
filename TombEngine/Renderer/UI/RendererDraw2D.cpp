#include "framework.h"

#include "Math/SimpleMath.h"

#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/spotcam.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/effects/weather.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/game_object_ids.h"
#include "Objects/Utils/object_helper.h"
#include "Renderer/Backend/SceneRenderer.h"
#include "Renderer/Renderer.h"
#include "Renderer/Structures/RendererHudBar.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/trutils.h"

// 2D layer recording: HUD bars, loading bar, screen overlays (flash and optics), full screen images and display
// sprites are added to the Backend::UiFrame, which the scene renderer composes when the frame is presented.

TEN::Renderer::RendererHudBar* g_AirBar;
TEN::Renderer::RendererHudBar* g_ExposureBar;
TEN::Renderer::RendererHudBar* g_HealthBar;
TEN::Renderer::RendererHudBar* g_StaminaBar;
TEN::Renderer::RendererHudBar* g_LoadingBar;

namespace TEN::Renderer
{
	using namespace TEN::Math::Library;
	using namespace TEN::Renderer::Structures;
	using namespace TEN::Effects::DisplaySprite;
	using namespace TEN::Effects::Environment;
	using namespace TEN::Math;
	using namespace TEN::SpotCam;

	constexpr auto AIR_BAR_POS		= Vector2(630.0f, 30.0f);
	constexpr auto EXPOSURE_BAR_POS = Vector2(630.0f, 70.0f);
	constexpr auto HEALTH_BAR_POS	= Vector2(20.0f, 30.0f);
	constexpr auto STAMINA_BAR_POS	= Vector2(630.0f, 50.0f);
	constexpr auto LOADING_BAR_POS	= Vector2(325.0f, 550.0f);

	constexpr auto AIR_BAR_VERT_POS		 = Vector2(DISPLAY_SPACE_RES.x * 0.5f, 30.0f);
	constexpr auto HEALTH_BAR_VERT_POS	 = Vector2(DISPLAY_SPACE_RES.x * 0.5f, 50.0f);
	constexpr auto STAMINA_BAR_VERT_POS  = Vector2(DISPLAY_SPACE_RES.x * 0.5f, 70.0f);
	constexpr auto EXPOSURE_BAR_VERT_POS = Vector2(DISPLAY_SPACE_RES.x * 0.5f, 90.0f);
	constexpr auto LOADING_BAR_VERT_POS	 = Vector2(DISPLAY_SPACE_RES.x * 0.5f, 550.0f);

	void Renderer::InitializeGameBars()
	{
		g_AirBar = new RendererHudBar(AIR_BAR_POS, AIR_BAR_VERT_POS, RendererHudBar::SIZE_DEFAULT);
		g_ExposureBar = new RendererHudBar(EXPOSURE_BAR_POS, EXPOSURE_BAR_VERT_POS, RendererHudBar::SIZE_DEFAULT);
		g_HealthBar = new RendererHudBar(HEALTH_BAR_POS, HEALTH_BAR_VERT_POS, RendererHudBar::SIZE_DEFAULT);
		g_StaminaBar = new RendererHudBar(STAMINA_BAR_POS, STAMINA_BAR_VERT_POS, RendererHudBar::SIZE_DEFAULT);
		g_LoadingBar = new RendererHudBar(LOADING_BAR_POS, LOADING_BAR_VERT_POS, RendererHudBar::SIZE_DEFAULT);
	}

	// Adds a HUD bar with the border sprite and the fill sprite of the texture slot.
	void Renderer::DrawBar(float percent, const RendererHudBar& bar, GAME_OBJECT_ID textureSlot, int frame, bool isPoisoned)
	{
		if (!CheckIfSlotExists(ID_BAR_BORDER_GRAPHICS, "Bar rendering"))
			return;

		_uiFrame.Bars.push_back(Backend::OverlayBar{ bar.Position, bar.VerticalPosition, bar.Size, percent, &_sprites[Objects[ID_BAR_BORDER_GRAPHICS].meshIndex],
			&_sprites[Objects[textureSlot].meshIndex], isPoisoned, frame });
		AddUiLayer(Backend::OverlayLayerType::Bars, (int)_uiFrame.Bars.size() - 1);
	}

	void Renderer::DrawLoadingBar(float percentage)
	{
		if (!g_GameFlow->GetSettings()->Hud.LoadingBar)
			return;

		// The loading bar images were loaded from files; missing files leave no bar.
		if (!_loadingBarBorder.IsValid() || !_loadingBarInner.IsValid())
			return;

		const auto& bar = *g_LoadingBar;
		auto nextBar = Backend::OverlayBar{ bar.Position, bar.VerticalPosition, bar.Size, percentage / 100.0f };
		nextBar.BorderPath = _loadingBarBorder.Path;
		nextBar.InnerPath = _loadingBarInner.Path;
		_uiFrame.Bars.push_back(nextBar);
		AddUiLayer(Backend::OverlayLayerType::Bars, (int)_uiFrame.Bars.size() - 1);
	}

	// Binocular or laser sight sprite while looking through optics outside the title level, null otherwise.
	const RendererSprite* Renderer::GetOpticOverlaySprite() const
	{
		if (CurrentLevel == 0 || Lara.Control.Look.OpticRange == 0)
			return nullptr;

		return &_sprites[Objects[Lara.Control.Look.IsUsingLasersight ? ID_LASERSIGHT_GRAPHICS : ID_BINOCULAR_GRAPHICS].meshIndex];
	}

	// Lightning flash and binocular or laser sight sprite, drawn by the scene renderer over the scene.
	void Renderer::DrawOverlays(RenderView& view)
	{
		_uiFrame.FlashColor = Weather.FlashColor();
		_uiFrame.OpticSprite = GetOpticOverlaySprite();
		_uiFrame.IsLaserSight = Lara.Control.Look.IsUsingLasersight;
		AddUiLayer(Backend::OverlayLayerType::ScreenOverlays, 0);
	}

	void Renderer::DrawFullScreenImage(const RendererImage& image, float fade)
	{
		DrawFullScreenQuad(image, Vector3(fade), true);
	}

	// Adds the display sprites of the view with negative or non-negative priority, as rotated quads in display space.
	void Renderer::DrawDisplaySprites(RenderView& renderView, bool negativePriority)
	{
		constexpr auto VERTEX_COUNT = 4;

		for (const auto& spriteToDraw : renderView.DisplaySpritesToDraw)
		{
			if ((spriteToDraw.Priority >= 0) == negativePriority)
				continue;

			// Corners in display space. Rotated 180 degrees to account for +Y being down.
			auto vertices = std::array<Vector2, VERTEX_COUNT>
			{
				spriteToDraw.Size / 2,
				Vector2(-spriteToDraw.Size.x, spriteToDraw.Size.y) / 2,
				-spriteToDraw.Size / 2,
				Vector2(spriteToDraw.Size.x, -spriteToDraw.Size.y) / 2
			};

			auto rotMatrix = Matrix::CreateRotationZ(TO_RAD(spriteToDraw.Orientation + ANGLE(180.0f)));
			auto nextSprite = Backend::OverlaySprite{ spriteToDraw.SpritePtr, {}, spriteToDraw.Color, spriteToDraw.BlendMode };
			for (int i = 0; i < VERTEX_COUNT; i++)
				nextSprite.Corners[i] = Vector2::Transform(vertices[i], rotMatrix) * spriteToDraw.AspectCorrection + spriteToDraw.Position;

			_uiFrame.Sprites.push_back(nextSprite);
			AddUiLayer(Backend::OverlayLayerType::Sprites, (int)_uiFrame.Sprites.size() - 1);
		}
	}

	// Opaque image over the whole screen; fitted images keep their aspect and are cropped.
	void Renderer::DrawFullScreenQuad(const RendererImage& image, Vector3 color, bool fit, float customAspect)
	{
		auto uvStart = Vector2::Zero;
		auto uvEnd = Vector2::One;

		if (fit)
		{
			auto screenResolution = GetScreenResolution();
			float screenAspect = (float)screenResolution.x / (float)screenResolution.y;
			float imageAspect = (customAspect == 0.0f) ? ((float)image.Width / (float)image.Height) : customAspect;

			if (screenAspect > imageAspect)
			{
				float difference = ((screenAspect - imageAspect) / screenAspect) / 2.0f;
				uvStart.y += difference;
				uvEnd.y -= difference;
			}
			else
			{
				float difference = ((imageAspect - screenAspect) / imageAspect) / 2.0f;
				uvStart.x += difference;
				uvEnd.x -= difference;
			}
		}

		AddUiImage(image.Path, Vector2::Zero, Vector2::One, uvStart, uvEnd, Vector4(color.x, color.y, color.z, 1.0f), BlendMode::Opaque);
	}

	void Renderer::AddDisplaySprite(const RendererSprite& sprite, const Vector2& pos2D, short orient, const Vector2& size, const Vector4& color,
									  int priority, BlendMode blendMode, const Vector2& aspectCorrection, RenderView& renderView)
	{
		auto spriteToDraw = RendererDisplaySpriteToDraw{};

		spriteToDraw.SpritePtr = &sprite;
		spriteToDraw.Position = pos2D;
		spriteToDraw.Orientation = orient;
		spriteToDraw.Size = size;
		spriteToDraw.Color = color;
		spriteToDraw.Priority = priority;
		spriteToDraw.BlendMode = blendMode;
		spriteToDraw.AspectCorrection = aspectCorrection;

		renderView.DisplaySpritesToDraw.push_back(spriteToDraw);
	}

	// Lays out the scripted display sprites for the screen aspect ratio and sorts them by priority and blend mode.
	void Renderer::CollectDisplaySprites(RenderView& renderView)
	{
		// Calculate screen aspect ratio.
		auto screenRes = GetScreenResolution().ToVector2();
		float screenAspect = screenRes.x / screenRes.y;

		// Calculate aspect ratio correction base.
		float aspectCorrectionBase = screenAspect / DISPLAY_SPACE_ASPECT;

		for (const auto& displaySprite : DisplaySprites)
		{
			// If sprite is a video texture, bypass it if texture is inactive.
			if (displaySprite.SpriteID == VIDEO_SPRITE_ID && !IsVideoSpriteActive())
				continue;

			// If sprite is not a video texture and no sprites present, bypass it.
			if (displaySprite.SpriteID != VIDEO_SPRITE_ID && _sprites.empty())
				continue;

			const auto& sprite = displaySprite.SpriteID == VIDEO_SPRITE_ID ? _videoSprite : _sprites[Objects[displaySprite.ObjectID].meshIndex + displaySprite.SpriteID];

			// Calculate sprite aspect ratio.
			float spriteAspect = (float)sprite.Width / (float)sprite.Height;

			// Calculate layout using helper function.
			auto layout = CalculateDisplaySpriteLayout(
				spriteAspect, displaySprite.Scale, displaySprite.Orientation,
				displaySprite.AlignMode, displaySprite.ScaleMode,
				screenAspect, aspectCorrectionBase);

			AddDisplaySprite(
				sprite,
				displaySprite.Position + layout.Offset,
				displaySprite.Orientation,
				layout.HalfSize * 2,
				displaySprite.Color,
				displaySprite.Priority,
				displaySprite.BlendMode,
				layout.AspectCorrection,
				renderView);
		}

		std::sort(
			renderView.DisplaySpritesToDraw.begin(), renderView.DisplaySpritesToDraw.end(),
			[](const RendererDisplaySpriteToDraw& spriteToDraw0, const RendererDisplaySpriteToDraw& spriteToDraw1)
			{
				// Same priority; sort by blend mode.
				if (spriteToDraw0.Priority == spriteToDraw1.Priority)
					return (spriteToDraw0.BlendMode < spriteToDraw1.BlendMode);

				// Sort by priority.
				return (spriteToDraw0.Priority < spriteToDraw1.Priority);
			});
	}
}
