#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUPrimitiveBatch.h"
#include "Renderer/Native/SDLGPU/SDLGPUGraphicsDevice.h"

namespace TEN::Renderer::Native::SDLGPU
{
	SDLGPUPrimitiveBatch::SDLGPUPrimitiveBatch(SDLGPUGraphicsDevice* device)
		: _device(device)
	{
		auto* gpuDevice = device->GetDevice();

		// Create dynamic vertex buffer for primitive data.
		SDL_GPUBufferCreateInfo bci = {};
		bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		bci.size = MAX_BATCH_VERTICES * sizeof(Vertex);
		_vertexBuffer = SDL_CreateGPUBuffer(gpuDevice, &bci);

		_vertices.reserve(MAX_BATCH_VERTICES);
	}

	SDLGPUPrimitiveBatch::~SDLGPUPrimitiveBatch()
	{
		auto* gpuDevice = _device ? _device->GetDevice() : nullptr;
		if (gpuDevice && _vertexBuffer)
			SDL_ReleaseGPUBuffer(gpuDevice, _vertexBuffer);
	}

	void SDLGPUPrimitiveBatch::Begin()
	{
		_vertices.clear();
		_inBatch = true;
	}

	void SDLGPUPrimitiveBatch::End()
	{
		if (!_inBatch || _vertices.empty())
		{
			_inBatch = false;
			return;
		}

		auto* renderPass = _device->GetRenderPass();
		auto* cmdBuf = _device->GetCommandBuffer();
		if (!renderPass || !cmdBuf)
		{
			_inBatch = false;
			_vertices.clear();
			return;
		}

		// Upload vertex data via the device's copy pass infrastructure.
		// This handles render pass interruption and resumption transparently.
		Uint32 dataSize = (Uint32)(_vertices.size() * sizeof(Vertex));
		_device->UploadBufferDataForBatch(_vertexBuffer, _vertices.data(), dataSize);

		_device->DrawPrimitivesForBatch(_vertexBuffer, (Uint32)_vertices.size());

		_inBatch = false;
		_vertices.clear();
	}

	void SDLGPUPrimitiveBatch::DrawLine(Vertex const& v1, Vertex const& v2)
	{
		if (!_inBatch) return;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
	}

	void SDLGPUPrimitiveBatch::DrawTriangle(Vertex const& v1, Vertex const& v2, Vertex const& v3)
	{
		if (!_inBatch) return;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
		_vertices.push_back(v3);
	}

	void SDLGPUPrimitiveBatch::DrawQuad(Vertex const& v1, Vertex const& v2, Vertex const& v3, Vertex const& v4)
	{
		if (!_inBatch) return;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
		_vertices.push_back(v3);
		_vertices.push_back(v1);
		_vertices.push_back(v3);
		_vertices.push_back(v4);
	}
}

#endif
