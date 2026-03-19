#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <vector>
#include <cstring>

namespace TEN::Renderer::Native::SDLGPU
{
	// GPU-side storage buffer for large data that exceeds push uniform limits.
	// Data is uploaded via transfer buffer and bound as a read-only storage buffer.
	class SDLGPUStorageBuffer
	{
	private:
		SDL_GPUDevice*  _device    = nullptr;
		SDL_GPUBuffer*  _gpuBuffer = nullptr;
		int             _capacity  = 0;   // Allocated size in bytes.
		int             _usedSize  = 0;   // Actually used bytes (for partial updates).
		bool            _dirty     = true;
		std::vector<char> _cpuData;       // CPU-side staging copy.

	public:
		SDLGPUStorageBuffer() = default;

		SDLGPUStorageBuffer(SDL_GPUDevice* device, int capacity)
			: _device(device), _capacity(capacity), _cpuData(capacity, 0)
		{
			SDL_GPUBufferCreateInfo bufCI = {};
			bufCI.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
			bufCI.size = (Uint32)capacity;
			_gpuBuffer = SDL_CreateGPUBuffer(device, &bufCI);
		}

		~SDLGPUStorageBuffer()
		{
			if (_gpuBuffer && _device)
				SDL_ReleaseGPUBuffer(_device, _gpuBuffer);
		}

		// Non-copyable, movable.
		SDLGPUStorageBuffer(const SDLGPUStorageBuffer&) = delete;
		SDLGPUStorageBuffer& operator=(const SDLGPUStorageBuffer&) = delete;
		SDLGPUStorageBuffer(SDLGPUStorageBuffer&& o) noexcept
			: _device(o._device), _gpuBuffer(o._gpuBuffer), _capacity(o._capacity),
			  _usedSize(o._usedSize), _dirty(o._dirty), _cpuData(std::move(o._cpuData))
		{
			o._gpuBuffer = nullptr;
		}
		SDLGPUStorageBuffer& operator=(SDLGPUStorageBuffer&& o) noexcept
		{
			if (this != &o)
			{
				if (_gpuBuffer && _device)
					SDL_ReleaseGPUBuffer(_device, _gpuBuffer);
				_device = o._device;
				_gpuBuffer = o._gpuBuffer;
				_capacity = o._capacity;
				_usedSize = o._usedSize;
				_dirty = o._dirty;
				_cpuData = std::move(o._cpuData);
				o._gpuBuffer = nullptr;
			}
			return *this;
		}

		SDL_GPUBuffer* GetGPUBuffer() const { return _gpuBuffer; }
		int GetCapacity() const { return _capacity; }
		int GetUsedSize() const { return _usedSize; }
		bool IsDirty() const { return _dirty; }

		void UpdateData(const void* data, int size)
		{
			_usedSize = std::min(size, _capacity);
			std::memcpy(_cpuData.data(), data, _usedSize);
			_dirty = true;
		}

		// Upload dirty data to GPU. Call within a copy pass context.
		void UploadIfDirty(SDL_GPUCommandBuffer* cmdBuf)
		{
			if (!_dirty || !_gpuBuffer || _usedSize == 0)
				return;

			SDL_GPUTransferBufferCreateInfo tbCI = {};
			tbCI.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			tbCI.size = (Uint32)_usedSize;
			auto* tb = SDL_CreateGPUTransferBuffer(_device, &tbCI);

			void* mapped = SDL_MapGPUTransferBuffer(_device, tb, false);
			std::memcpy(mapped, _cpuData.data(), _usedSize);
			SDL_UnmapGPUTransferBuffer(_device, tb);

			auto* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

			SDL_GPUTransferBufferLocation src = {};
			src.transfer_buffer = tb;
			src.offset = 0;

			SDL_GPUBufferRegion dst = {};
			dst.buffer = _gpuBuffer;
			dst.offset = 0;
			dst.size = (Uint32)_usedSize;

			SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
			SDL_EndGPUCopyPass(copyPass);

			SDL_ReleaseGPUTransferBuffer(_device, tb);
			_dirty = false;
		}
	};
}

#endif
