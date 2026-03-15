#pragma once

#ifdef HAS_SDLGPU

#include <unordered_map>
#include <functional>
#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/RenderPipelineState.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	// Key for the PSO cache: combines shader pointers, input layout, pipeline state,
	// primitive type, color target formats, and depth format.
	struct PipelineCacheKey
	{
		SDL_GPUShader*       VertexShader   = nullptr;
		SDL_GPUShader*       FragmentShader = nullptr;
		void*                InputLayout    = nullptr;
		RenderPipelineState  PipelineState;
		PrimitiveType        Primitive      = PrimitiveType::TriangleList;

		// Current render target formats (needed to create PSO).
		SDL_GPUTextureFormat ColorFormats[4] = {};
		int                  NumColorTargets = 0;
		SDL_GPUTextureFormat DepthFormat     = SDL_GPU_TEXTUREFORMAT_INVALID;
		bool                 HasDepthTarget  = false;

		bool operator==(const PipelineCacheKey& other) const
		{
			if (VertexShader != other.VertexShader) return false;
			if (FragmentShader != other.FragmentShader) return false;
			if (InputLayout != other.InputLayout) return false;
			if (PipelineState != other.PipelineState) return false;
			if (Primitive != other.Primitive) return false;
			if (NumColorTargets != other.NumColorTargets) return false;
			if (DepthFormat != other.DepthFormat) return false;
			if (HasDepthTarget != other.HasDepthTarget) return false;
			for (int i = 0; i < NumColorTargets; ++i)
			{
				if (ColorFormats[i] != other.ColorFormats[i]) return false;
			}
			return true;
		}
	};

	struct PipelineCacheKeyHash
	{
		size_t operator()(const PipelineCacheKey& k) const
		{
			size_t h = 0;
			auto combine = [&](size_t v)
			{
				h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
			};

			combine(std::hash<void*>{}(k.VertexShader));
			combine(std::hash<void*>{}(k.FragmentShader));
			combine(std::hash<void*>{}(k.InputLayout));
			combine(std::hash<int>{}(static_cast<int>(k.PipelineState.Blend)));
			combine(std::hash<int>{}(static_cast<int>(k.PipelineState.Depth)));
			combine(std::hash<int>{}(static_cast<int>(k.PipelineState.Cull)));
			combine(std::hash<int>{}(static_cast<int>(k.Primitive)));
			combine(std::hash<int>{}(k.NumColorTargets));
			combine(std::hash<int>{}(static_cast<int>(k.DepthFormat)));
			combine(std::hash<bool>{}(k.HasDepthTarget));
			for (int i = 0; i < k.NumColorTargets; ++i)
				combine(std::hash<int>{}(static_cast<int>(k.ColorFormats[i])));

			return h;
		}
	};

	class SDLGPUPipelineCache
	{
	private:
		SDL_GPUDevice* _device = nullptr;
		std::unordered_map<PipelineCacheKey, SDL_GPUGraphicsPipeline*, PipelineCacheKeyHash> _cache;

	public:
		SDLGPUPipelineCache() = default;
		~SDLGPUPipelineCache();

		void Initialize(SDL_GPUDevice* device) { _device = device; }

		// Returns a cached or newly created PSO for the given key.
		SDL_GPUGraphicsPipeline* GetOrCreatePipeline(const PipelineCacheKey& key);

		void Clear();
	};
}

#endif
