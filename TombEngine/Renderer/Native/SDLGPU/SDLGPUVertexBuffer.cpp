#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUVertexBuffer.h"

namespace TEN::Renderer::Native::SDLGPU
{
	SDLGPUVertexBuffer::SDLGPUVertexBuffer(SDL_GPUDevice* device, int numVertices, int stride, void* data)
		: _device(device), _numVertices(numVertices), _stride(stride)
	{
		Uint32 bufferSize = (Uint32)(numVertices * stride);

		SDL_GPUBufferCreateInfo bci = {};
		bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		bci.size = bufferSize;

		_buffer = SDL_CreateGPUBuffer(device, &bci);
		_vram = VRAMAllocation(VRAMCategory::VertexBuffer, bufferSize, "");

		if (data)
		{
			// Upload initial data via transfer buffer.
			SDL_GPUTransferBufferCreateInfo tbci = {};
			tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			tbci.size = bufferSize;
			auto transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);

			void* mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
			std::memcpy(mapped, data, bufferSize);
			SDL_UnmapGPUTransferBuffer(device, transferBuffer);

			auto cmdBuf = SDL_AcquireGPUCommandBuffer(device);
			auto copyPass = SDL_BeginGPUCopyPass(cmdBuf);

			SDL_GPUTransferBufferLocation src = {};
			src.transfer_buffer = transferBuffer;
			src.offset = 0;

			SDL_GPUBufferRegion dst = {};
			dst.buffer = _buffer;
			dst.offset = 0;
			dst.size = bufferSize;

			SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
			SDL_EndGPUCopyPass(copyPass);
			SDL_SubmitGPUCommandBuffer(cmdBuf);

			SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
		}
	}

	SDLGPUVertexBuffer::~SDLGPUVertexBuffer()
	{
		if (_buffer && _device)
			SDL_ReleaseGPUBuffer(_device, _buffer);
	}

	bool SDLGPUVertexBuffer::Update(SDL_GPUDevice* device, SDL_GPUCopyPass* copyPass, void* data, int startVertex, int count)
	{
		// This method is for updating via an externally-managed copy pass.
		// The main update path goes through SDLGPUGraphicsDevice::UpdateVertexBuffer.
		return true;
	}
}

#endif
