#include "framework.h"
#include "Renderer/Renderer.h"

#include <filesystem>

#include "Specific/EngineMain.h"

namespace TEN::Renderer 
{
	void Renderer::ChangeScreenResolution(int width, int height, bool windowed) 
	{
		_graphicsDevice->UnbindAllRenderTargets();
		_graphicsDevice->Flush();
		_graphicsDevice->ClearState();
		_graphicsDevice->ResizeSwapChain(width, height);

		_isWindowed = windowed;

		InitializeScreen(width, height, true);

		// Recreate resolution-dependent render targets that are NOT owned by InitializeScreen.
		// These were allocated at startup with the old screen dimensions and must be resized
		// so that subsequent draw passes use correctly-sized buffers.
		InitializeGodRays();          // recreates _godRayRenderTarget at new half-res
		ResizeVolumetricCloudTargets();   // recreates _cloudRenderTarget / _cloudOcclusionTarget (layer A)
		ResizeDualCloudTargets();         // recreates _cloudRenderTargetB / _cloudOcclusionTargetB (layer B)
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
