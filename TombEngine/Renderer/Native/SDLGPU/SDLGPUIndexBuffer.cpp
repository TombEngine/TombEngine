#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUIndexBuffer.h"

namespace TEN::Renderer::Native::SDLGPU
{
	SDLGPUIndexBuffer::SDLGPUIndexBuffer(SDL_GPUDevice* device, int numIndices, int* data)
		: _device(device), _numIndices(numIndices)
	{
		Uint32 bufferSize = (Uint32)(numIndices * sizeof(int));

		SDL_GPUBufferCreateInfo bci = {};
		bci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
		bci.size = bufferSize;

		_buffer = SDL_CreateGPUBuffer(device, &bci);
		_vram = VRAMAllocation(VRAMCategory::IndexBuffer, bufferSize, "");

		if (data)
		{
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

	SDLGPUIndexBuffer::~SDLGPUIndexBuffer()
	{
		if (_buffer && _device)
			SDL_ReleaseGPUBuffer(_device, _buffer);
	}

	bool SDLGPUIndexBuffer::Update(SDL_GPUDevice* device, SDL_GPUCopyPass* copyPass, int* data, int startIndex, int count)
	{
		return true;
	}
}

#endif
