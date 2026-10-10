#include "framework.h"
#include "Renderer/Renderer.h"

#include <filesystem>

#include "Renderer/Next/TextureLoader.h"
#include "Specific/EngineMain.h"

namespace TEN::Renderer 
{
	void Renderer::ChangeScreenResolution(int width, int height, bool windowed, bool resyncWindow)
	{
		_isWindowed = windowed;
		InitializeScreen(width, height, true, resyncWindow);
	}

	// Image files are decoded by the next renderer; only their size is read here, for layouts.
	RendererImage Renderer::LoadImage(const std::string& path)
	{
		auto image = RendererImage{};
		if (path.empty())
			return image;

		if (!std::filesystem::is_regular_file(path))
		{
			TENLog("Texture file not found: " + path, LogLevel::Warning);
			return image;
		}

		auto file = std::ifstream(path, std::ios::binary);
		auto data = std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		auto info = Next::ImageInfo{};
		if (!Next::ReadImageInfo(data, info))
		{
			TENLog("Unsupported image file: " + path, LogLevel::Warning);
			return image;
		}

		image.Path = path;
		image.Width = info.Width;
		image.Height = info.Height;
		return image;
	}
}
