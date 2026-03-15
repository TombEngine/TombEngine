#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPURenderTarget2D.h"
#include "Renderer/Native/SDLGPU/SDLGPUUtils.h"

namespace TEN::Renderer::Native::SDLGPU
{
	SDLGPURenderTarget2D::SDLGPURenderTarget2D(SDL_GPUDevice* device, int width, int height,
	                                             SDL_GPUTextureFormat format, bool isTypeless)
		: _device(device), _width(width), _height(height), _format(format), _arraySize(1)
	{
		SDL_GPUTextureCreateInfo tci = {};
		tci.type = SDL_GPU_TEXTURETYPE_2D;
		tci.format = format;
		tci.width = width;
		tci.height = height;
		tci.layer_count_or_depth = 1;
		tci.num_levels = 1;
		tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

		_texture = SDL_CreateGPUTexture(device, &tci);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, (int)ComputeTextureSize(width, height, format), "");
	}

	SDLGPURenderTarget2D::SDLGPURenderTarget2D(SDL_GPUDevice* device, int width, int height,
	                                             int arraySize, SDL_GPUTextureFormat format)
		: _device(device), _width(width), _height(height), _format(format), _arraySize(arraySize)
	{
		SDL_GPUTextureCreateInfo tci = {};
		tci.type = (arraySize > 1) ? SDL_GPU_TEXTURETYPE_2D_ARRAY : SDL_GPU_TEXTURETYPE_2D;
		tci.format = format;
		tci.width = width;
		tci.height = height;
		tci.layer_count_or_depth = arraySize;
		tci.num_levels = 1;
		tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

		_texture = SDL_CreateGPUTexture(device, &tci);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, (int)(ComputeTextureSize(width, height, format) * arraySize), "");
	}

	SDLGPURenderTarget2D::SDLGPURenderTarget2D(int width, int height)
		: _width(width), _height(height), _ownsTexture(false)
	{
		// Backbuffer wrapper — texture managed externally (swapchain).
	}

	SDLGPURenderTarget2D::SDLGPURenderTarget2D(SDL_GPUDevice* device, SDLGPURenderTarget2D* parent,
	                                             SDL_GPUTextureFormat viewFormat)
		: _device(device), _width(parent->GetWidth()), _height(parent->GetHeight()),
		  _format(viewFormat), _arraySize(1)
	{
		// For SMAA: SDL_GPU doesn't support typeless texture views, so create a separate texture.
		SDL_GPUTextureCreateInfo tci = {};
		tci.type = SDL_GPU_TEXTURETYPE_2D;
		tci.format = viewFormat;
		tci.width = _width;
		tci.height = _height;
		tci.layer_count_or_depth = 1;
		tci.num_levels = 1;
		tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

		_texture = SDL_CreateGPUTexture(device, &tci);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, (int)ComputeTextureSize(_width, _height, viewFormat), "");
	}

	SDLGPURenderTarget2D::~SDLGPURenderTarget2D()
	{
		if (_texture && _device && _ownsTexture)
			SDL_ReleaseGPUTexture(_device, _texture);
	}
}

#endif
