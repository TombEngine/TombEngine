#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/IRenderTarget2D.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	class SDLGPURenderTarget2D : public IRenderTarget2D
	{
	private:
		SDL_GPUDevice*       _device    = nullptr;
		SDL_GPUTexture*      _texture   = nullptr;
		SDL_GPUTextureFormat _format    = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		int                  _width     = 0;
		int                  _height    = 0;
		int                  _arraySize = 1;
		bool                 _ownsTexture = true;
		VRAMAllocation       _vram;

	public:
		SDLGPURenderTarget2D() = default;
		~SDLGPURenderTarget2D();

		int GetWidth() override { return _width; }
		int GetHeight() override { return _height; }
		int GetArraySize() override { return _arraySize; }
		bool IsValid() override { return _texture != nullptr; }

		SDL_GPUTexture* GetGPUTexture() const { return _texture; }
		SDL_GPUTextureFormat GetFormat() const { return _format; }
		bool IsArray() const { return _arraySize > 1; }

		// Single render target.
		SDLGPURenderTarget2D(SDL_GPUDevice* device, int width, int height, SDL_GPUTextureFormat format, bool isTypeless);
		// Texture array render target (shadow maps).
		SDLGPURenderTarget2D(SDL_GPUDevice* device, int width, int height, int arraySize, SDL_GPUTextureFormat format);
		// Backbuffer wrapper.
		SDLGPURenderTarget2D(int width, int height);
		// Alternate-format view for SMAA (separate texture, same dimensions).
		SDLGPURenderTarget2D(SDL_GPUDevice* device, SDLGPURenderTarget2D* parent, SDL_GPUTextureFormat viewFormat);
	};
}

#endif
