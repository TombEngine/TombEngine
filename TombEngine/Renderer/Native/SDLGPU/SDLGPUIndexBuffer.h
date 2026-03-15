#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/IIndexBuffer.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	class SDLGPUIndexBuffer final : public IIndexBuffer
	{
	private:
		SDL_GPUDevice*  _device     = nullptr;
		SDL_GPUBuffer*  _buffer     = nullptr;
		int             _numIndices = 0;
		VRAMAllocation  _vram;

	public:
		SDLGPUIndexBuffer() = default;
		~SDLGPUIndexBuffer();

		SDLGPUIndexBuffer(SDL_GPUDevice* device, int numIndices, int* data);

		SDL_GPUBuffer* GetGPUBuffer() const { return _buffer; }
		int GetNumIndices() const { return _numIndices; }

		bool Update(SDL_GPUDevice* device, SDL_GPUCopyPass* copyPass, int* data, int startIndex, int count);
	};
}

#endif
