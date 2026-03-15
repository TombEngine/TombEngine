#pragma once

#ifdef HAS_SDLGPU

#include <vector>
#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/IPrimitiveBatch.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;
	using namespace TEN::Renderer::Graphics::Vertices;

	class SDLGPUGraphicsDevice;

	class SDLGPUPrimitiveBatch final : public IPrimitiveBatch
	{
	private:
		SDLGPUGraphicsDevice* _device = nullptr;

		std::vector<Vertex> _vertices;
		bool                _inBatch = false;

		SDL_GPUBuffer* _vertexBuffer = nullptr;
		static constexpr int MAX_BATCH_VERTICES = 65536;

	public:
		SDLGPUPrimitiveBatch() = default;
		~SDLGPUPrimitiveBatch();

		SDLGPUPrimitiveBatch(SDLGPUGraphicsDevice* device);

		void Begin() override;
		void End() override;
		void DrawLine(Vertex const& v1, Vertex const& v2) override;
		void DrawTriangle(Vertex const& v1, Vertex const& v2, Vertex const& v3) override;
		void DrawQuad(Vertex const& v1, Vertex const& v2, Vertex const& v3, Vertex const& v4) override;
	};
}

#endif
