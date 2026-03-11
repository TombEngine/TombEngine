#include "framework.h"
#include "Renderer/Renderer.h"

#include <filesystem>

#include "Specific/EngineMain.h"

namespace TEN::Renderer 
{
	void Renderer::ChangeScreenResolution(int width, int height, bool windowed, bool applyWindowState)
	{
		_graphicsDevice->UnbindAllRenderTargets();
		_graphicsDevice->Flush();
		_graphicsDevice->ClearState();
		_graphicsDevice->ResizeSwapChain(width, height);

		_isWindowed = windowed;

		InitializeScreen(width, height, true);

		// Apply windowed/fullscreen state to the SDL window when the change
		// originates from the settings menu.  Skip when called from
		// ToggleFullScreen, which already applied the window state.
		if (applyWindowState)
			SetFullScreen();
	}

	std::string Renderer::GetDefaultAdapterName()
	{
		return _graphicsDevice->GetDefaultAdapterName();
	}

	const AdapterInfo& Renderer::GetAdapterInfo() const
	{
		return _adapterInfo;
	}

	std::unique_ptr<ITexture2D> Renderer::SetTextureOrDefault(std::string path)
	{
		std::unique_ptr<ITexture2D> texture;

		if (std::filesystem::is_regular_file(path))
		{
			texture = _graphicsDevice->CreateTexture2DFromFile(path);
		}
		else if (!path.empty()) // Loading default texture without path may be intentional.
		{
			texture = _graphicsDevice->CreateTexture2D(1, 1, SurfaceFormat::SF_RGBA8_Unorm, nullptr);
			TENLog("Texture file not found: " + path, LogLevel::Warning);
		}
		else
		{
			texture = _graphicsDevice->CreateTexture2D(1, 1, SurfaceFormat::SF_RGBA8_Unorm, nullptr);
		}

		return texture;
	}
}
