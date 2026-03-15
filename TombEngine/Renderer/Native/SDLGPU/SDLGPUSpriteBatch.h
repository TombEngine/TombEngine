#pragma once

#ifdef HAS_SDLGPU

#include <vector>
#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/ISpriteBatch.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;
	using namespace TEN::Renderer::Structures;

	class SDLGPUGraphicsDevice;

	struct SpriteBatchQuad
	{
		SDL_GPUTexture* TextureHandle = nullptr;
		float Left, Top, Right, Bottom;
		float U0 = 0, V0 = 0, U1 = 1, V1 = 1;
		Vector4 Color;
	};

	class SDLGPUSpriteBatch final : public ISpriteBatch
	{
	private:
		SDLGPUGraphicsDevice* _device = nullptr;

		std::vector<SpriteBatchQuad> _queue;
		std::vector<float>          _vertices;
		BlendMode                   _blendMode = BlendMode::AlphaBlend;

		// GPU resources.
		SDL_GPUBuffer*   _vertexBuffer   = nullptr;
		SDL_GPUShader*   _vertexShader   = nullptr;
		SDL_GPUShader*   _fragmentShader = nullptr;
		SDL_GPUSampler*  _sampler        = nullptr;

		// Cached pipeline for sprite rendering.
		SDL_GPUGraphicsPipeline* _pipeline           = nullptr;
		SDL_GPUTextureFormat     _pipelineColorFmt   = SDL_GPU_TEXTUREFORMAT_INVALID;
		SDL_GPUTextureFormat     _pipelineDepthFmt   = SDL_GPU_TEXTUREFORMAT_INVALID;
		BlendMode                _pipelineBlendMode  = BlendMode::Opaque;

		// Maximum number of quads before flush (and VBO size).
		static constexpr int MAX_BATCH_QUADS = 2048;
		static constexpr int FLOATS_PER_VERTEX = 8; // pos(2) + uv(2) + color(4)
		static constexpr int VERTICES_PER_QUAD = 6;

		void Flush();
		SDL_GPUTexture* GetTextureHandle(ITextureBase* texture);
		void CreatePipeline(SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat);

	public:
		SDLGPUSpriteBatch() = default;
		~SDLGPUSpriteBatch();

		SDLGPUSpriteBatch(SDLGPUGraphicsDevice* device);

		void Begin(SpriteSortingMode sortingMode, BlendMode blendMode) override;
		void End() override;
		void Draw(ITextureBase* texture, RendererRectangle area, Vector4 color) override;
		void DrawWithUV(SDL_GPUTexture* textureHandle, RendererRectangle area, float u0, float v0, float u1, float v1, Vector4 color);
	};
}

#endif
