#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/IVertexBuffer.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	class SDLGPUVertexBuffer final : public IVertexBuffer
	{
	private:
		SDL_GPUDevice*  _device      = nullptr;
		SDL_GPUBuffer*  _buffer      = nullptr;
		int             _numVertices = 0;
		int             _stride      = 0;
		bool            _dynamic     = false;
		VRAMAllocation  _vram;

	public:
		SDLGPUVertexBuffer() = default;
		~SDLGPUVertexBuffer();

		SDLGPUVertexBuffer(SDL_GPUDevice* device, int numVertices, int stride, void* data);

		SDL_GPUBuffer* GetGPUBuffer() const { return _buffer; }
		int GetStride() const { return _stride; }
		int GetNumVertices() const { return _numVertices; }

		bool Update(SDL_GPUDevice* device, SDL_GPUCopyPass* copyPass, void* data, int startVertex, int count);
	};
}

#endif
