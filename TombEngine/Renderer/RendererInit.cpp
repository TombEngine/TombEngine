#include "framework.h"
#include "Renderer/Renderer.h"

#include "Renderer/ImGuiIntegration.h"
#include "Renderer/RendererUtils.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/configuration.h"
#include "Specific/memory/Vector.h"
#include "Specific/trutils.h"
#include "Specific/Video/Video.h"
#include "Specific/EngineMain.h"

extern GameConfiguration g_Configuration;

using namespace TEN::Renderer::Utils;
using namespace TEN::Video;

namespace TEN::Renderer
{
	void Renderer::Initialize(const std::string& gameDir, int w, int h, bool windowed)
	{
		TENLog("Initializing renderer...", LogLevel::Info);

		_isWindowed = windowed;

		InitializeScreen(w, h, false);
		InitializeCommonTextures();
		_currentCausticsFrame = 0;

		// Preallocate lists.
		_lines2DToDraw = createVector<RendererLine2D>(MAX_LINES_2D);
		_lines3DToDraw = createVector<RendererLine3D>(MAX_LINES_3D);
		_triangles3DToDraw = createVector<RendererTriangle3D>(MAX_TRIANGLES_3D);

		for (auto& dynamicLightList : _dynamicLights)
			dynamicLightList = createVector<RendererLight>(MAX_DYNAMIC_LIGHTS);

		for (auto& item : _items)
			item.LightsToDraw = createVector<RendererLight*>(MAX_LIGHTS_PER_ITEM);

		InitializeGameBars();
		InitializeVolumetricClouds();

		g_VideoPlayer.Initialize(gameDir);

		// Initialize ImGui debug overlay.
		ImGuiInit(g_Platform->GetSDL3Window());
	}

	void Renderer::InitializeScreen(int w, int h, bool reset, bool resyncWindow)
	{
		_screenWidth = w;
		_screenHeight = h;

		// Keep cached render views in sync with the new screen size. Gameplay rebuilds these
		// via UpdateCameraMatrices() every frame, but title/pause paths reuse the last values.
		for (auto* view : { &_gameCamera, &_oldGameCamera, &_currentGameCamera })
		{
			view->Viewport.X = 0;
			view->Viewport.Y = 0;
			view->Viewport.Width = w;
			view->Viewport.Height = h;

			view->Camera.ViewSize = { (float)w, (float)h };
			view->Camera.InvViewSize = { 1.0f / w, 1.0f / h };
			view->Camera.Projection = Matrix::CreatePerspectiveFieldOfView(view->Camera.FOV, (float)w / (float)h, view->Camera.NearPlane, view->Camera.FarPlane);
			view->Camera.ViewProjection = view->Camera.View * view->Camera.Projection;
			view->Camera.Frustum.Update(view->Camera.View, view->Camera.Projection);
		}

		// Skip when the resize came from a user window drag: the window is already at the
		// target size and SetFullScreen() would recenter/raise the window mid-drag.
		if (resyncWindow)
			SetFullScreen();
	}

	void Renderer::InitializeCommonTextures()
	{
		auto fontPath = GetAssetPath("Textures/Font.spritefont");
		if (!std::filesystem::is_regular_file(fontPath) || !_gameFont.Load(fontPath))
			throw std::runtime_error("Font not found; path " + fontPath + " is missing.");

		_logo = LoadImage(GetAssetPath("Textures/Logo.png"));
		_loadingBarBorder = LoadImage(GetAssetPath("Textures/LoadingBarBorder.png"));
		_loadingBarInner = LoadImage(GetAssetPath("Textures/LoadingBarInner.png"));

		// Colored quads use the white texture of the next renderer.
		_whiteSprite = RendererSprite{ 0, 1, 1, { Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), Vector2(1.0f, 1.0f), Vector2(0.0f, 1.0f) } };
	}

	void Renderer::ToggleFullScreen(bool force)
	{
		_isWindowed = force ? false : !_isWindowed;
		SetFullScreen();
	}

	void Renderer::SetFullScreen()
	{
		auto window = g_Platform->GetSDL3Window();

		if (!_isWindowed)
		{
			SDL_SetWindowAlwaysOnTop(window, true);
			SDL_SetWindowFullscreen(window, true);

			SDL_SyncWindow(window);
		}
		else
		{
			SDL_SetWindowFullscreen(window, false);
			SDL_SetWindowAlwaysOnTop(window, false);
			SDL_SetWindowBordered(window, true);

			SDL_SetWindowSize(window, _screenWidth, _screenHeight);
			SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

			SDL_RestoreWindow(window);
			SDL_RaiseWindow(window);
			SDL_SyncWindow(window);
		}
	}
}
