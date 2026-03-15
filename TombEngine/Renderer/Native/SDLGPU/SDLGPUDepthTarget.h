#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL_gpu.h>
#include <vector>
#include "Renderer/Graphics/IDepthTarget.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	class SDLGPUDepthTarget : public IDepthTarget
	{
	private:
		SDL_GPUDevice*       _device    = nullptr;
		SDL_GPUTextureFormat _format    = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
		int                  _width     = 0;
		int                  _height    = 0;
		int                  _arraySize = 1;
		VRAMAllocation       _vram;

		// SDL_GPU forbids DEPTH_STENCIL_TARGET on array textures.
		// For arraySize == 1: single texture stored in _layers[0].
		// For arraySize > 1:  N separate 2D textures, one per layer.
		std::vector<SDL_GPUTexture*> _layers;

	public:
		SDLGPUDepthTarget() = default;
		~SDLGPUDepthTarget();

		int GetArraySize() override { return _arraySize; }

		// Get the GPU texture for a specific layer (default 0).
		SDL_GPUTexture* GetGPUTexture(int layer = 0) const { return _layers[layer]; }
		SDL_GPUTextureFormat GetFormat() const { return _format; }
		bool IsArray() const { return _arraySize > 1; }

		SDLGPUDepthTarget(SDL_GPUDevice* device, int width, int height, SDL_GPUTextureFormat format);
		SDLGPUDepthTarget(SDL_GPUDevice* device, int width, int height, int arraySize, SDL_GPUTextureFormat format);
	};
}

#endif
