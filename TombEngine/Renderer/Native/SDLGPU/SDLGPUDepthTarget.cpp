#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUDepthTarget.h"
#include "Renderer/Native/SDLGPU/SDLGPUUtils.h"

namespace TEN::Renderer::Native::SDLGPU
{
	SDLGPUDepthTarget::SDLGPUDepthTarget(SDL_GPUDevice* device, int width, int height, SDL_GPUTextureFormat format)
		: _device(device), _width(width), _height(height), _format(format), _arraySize(1)
	{
		SDL_GPUTextureCreateInfo tci = {};
		tci.type = SDL_GPU_TEXTURETYPE_2D;
		tci.format = format;
		tci.width = width;
		tci.height = height;
		tci.layer_count_or_depth = 1;
		tci.num_levels = 1;
		tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

		_layers.push_back(SDL_CreateGPUTexture(device, &tci));
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, (int)ComputeTextureSize(width, height, format), "");
	}

	SDLGPUDepthTarget::SDLGPUDepthTarget(SDL_GPUDevice* device, int width, int height,
	                                       int arraySize, SDL_GPUTextureFormat format)
		: _device(device), _width(width), _height(height), _format(format), _arraySize(arraySize)
	{
		// SDL_GPU forbids DEPTH_STENCIL_TARGET on array textures.
		// Create N separate 2D textures instead.
		_layers.reserve(arraySize);
		for (int i = 0; i < arraySize; ++i)
		{
			SDL_GPUTextureCreateInfo tci = {};
			tci.type = SDL_GPU_TEXTURETYPE_2D;
			tci.format = format;
			tci.width = width;
			tci.height = height;
			tci.layer_count_or_depth = 1;
			tci.num_levels = 1;
			tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

			_layers.push_back(SDL_CreateGPUTexture(device, &tci));
		}

		_vram = VRAMAllocation(VRAMCategory::RenderTarget, (int)(ComputeTextureSize(width, height, format) * arraySize), "");
	}

	SDLGPUDepthTarget::~SDLGPUDepthTarget()
	{
		if (_device)
		{
			for (auto* tex : _layers)
			{
				if (tex)
					SDL_ReleaseGPUTexture(_device, tex);
			}
		}
	}
}

#endif
