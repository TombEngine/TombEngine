#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/ITexture2D.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	class SDLGPUTexture2D : public ITexture2D
	{
	protected:
		SDL_GPUDevice*       _device  = nullptr;
		SDL_GPUTexture*      _texture = nullptr;
		SDL_GPUSampler*      _sampler = nullptr;
		SDL_GPUTextureFormat _format  = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		int                  _width   = 0;
		int                  _height  = 0;
		bool                 _ownsTexture = true;
		VRAMAllocation       _vram;

	public:
		SDLGPUTexture2D() = default;
		~SDLGPUTexture2D();

		int GetWidth() override { return _width; }
		int GetHeight() override { return _height; }
		bool IsValid() override { return _texture != nullptr; }

		SDL_GPUTexture* GetGPUTexture() const { return _texture; }
		SDL_GPUSampler* GetGPUSampler() const { return _sampler; }
		SDL_GPUTextureFormat GetFormat() const { return _format; }

		// From raw pixel data.
		SDLGPUTexture2D(SDL_GPUDevice* device, int width, int height, SDL_GPUTextureFormat format, void* data);
		// From file on disk.
		SDLGPUTexture2D(SDL_GPUDevice* device, const std::string& fileName);
		// From file data in memory.
		SDLGPUTexture2D(SDL_GPUDevice* device, int dataSize, unsigned char* data);
	};
}

#endif
