#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUPipelineCache.h"
#include "Renderer/Native/SDLGPU/SDLGPUInputLayout.h"
#include "Renderer/Native/SDLGPU/SDLGPUUtils.h"

namespace TEN::Renderer::Native::SDLGPU
{
	SDLGPUPipelineCache::~SDLGPUPipelineCache()
	{
		Clear();
	}

	void SDLGPUPipelineCache::Clear()
	{
		for (auto& [key, pipeline] : _cache)
		{
			if (pipeline)
				SDL_ReleaseGPUGraphicsPipeline(_device, pipeline);
		}
		_cache.clear();
	}

	SDL_GPUGraphicsPipeline* SDLGPUPipelineCache::GetOrCreatePipeline(const PipelineCacheKey& key)
	{
		auto it = _cache.find(key);
		if (it != _cache.end())
			return it->second;

		if (!key.VertexShader || !key.FragmentShader)
			return nullptr;

		// Build the pipeline create info.
		SDL_GPUGraphicsPipelineCreateInfo pci = {};

		// Shaders.
		pci.vertex_shader = key.VertexShader;
		pci.fragment_shader = key.FragmentShader;

		// Vertex input.
		auto* layout = static_cast<SDLGPUInputLayout*>(key.InputLayout);
		if (layout)
		{
			pci.vertex_input_state.vertex_buffer_descriptions = layout->GetBufferDescriptions().data();
			pci.vertex_input_state.num_vertex_buffers = (Uint32)layout->GetBufferDescriptions().size();
			pci.vertex_input_state.vertex_attributes = layout->GetAttributes().data();
			pci.vertex_input_state.num_vertex_attributes = (Uint32)layout->GetAttributes().size();
		}

		// Primitive type.
		pci.primitive_type = GetSDLGPUPrimitiveType(key.Primitive);

		// Rasterizer state.
		pci.rasterizer_state.fill_mode = (key.PipelineState.Cull == CullMode::Wireframe)
			? SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;

		switch (key.PipelineState.Cull)
		{
		case CullMode::None:
		case CullMode::Wireframe:
			pci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
			break;
		case CullMode::CounterClockwise:
			pci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
			break;
		case CullMode::Clockwise:
			pci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
			break;
		}

		pci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE; // Match DX11 convention.
		pci.rasterizer_state.enable_depth_clip = true;

		// Depth stencil state.
		switch (key.PipelineState.Depth)
		{
		case DepthState::Write:
			pci.depth_stencil_state.enable_depth_test = true;
			pci.depth_stencil_state.enable_depth_write = true;
			pci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
			break;
		case DepthState::Read:
			pci.depth_stencil_state.enable_depth_test = true;
			pci.depth_stencil_state.enable_depth_write = false;
			pci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
			break;
		case DepthState::None:
			pci.depth_stencil_state.enable_depth_test = false;
			pci.depth_stencil_state.enable_depth_write = false;
			pci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS; // D3D12 validates even when disabled.
			break;
		}

		// Color targets / blend state.
		std::vector<SDL_GPUColorTargetDescription> colorDescs;
		for (int i = 0; i < key.NumColorTargets; ++i)
		{
			SDL_GPUColorTargetDescription desc = {};
			desc.format = key.ColorFormats[i];
			desc.blend_state = GetSDLGPUBlendState(key.PipelineState.Blend);
			colorDescs.push_back(desc);
		}

		pci.target_info.color_target_descriptions = colorDescs.empty() ? nullptr : colorDescs.data();
		pci.target_info.num_color_targets = (Uint32)colorDescs.size();

		if (key.HasDepthTarget)
		{
			pci.target_info.has_depth_stencil_target = true;
			pci.target_info.depth_stencil_format = key.DepthFormat;
		}

		// Multi-sample (no MSAA for now).
		pci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
		pci.multisample_state.sample_mask = 0xFFFFFFFF;

		auto* pipeline = SDL_CreateGPUGraphicsPipeline(_device, &pci);
		if (!pipeline)
		{
			std::string diagInfo = " VS=" + std::to_string((uintptr_t)key.VertexShader)
				+ " FS=" + std::to_string((uintptr_t)key.FragmentShader)
				+ " colors=" + std::to_string(key.NumColorTargets);
			for (int i = 0; i < key.NumColorTargets; ++i)
				diagInfo += " cFmt" + std::to_string(i) + "=" + std::to_string((int)key.ColorFormats[i]);
			diagInfo += " hasDepth=" + std::to_string(key.HasDepthTarget)
				+ " dFmt=" + std::to_string((int)key.DepthFormat)
				+ " blend=" + std::to_string((int)key.PipelineState.Blend)
				+ " depth=" + std::to_string((int)key.PipelineState.Depth)
				+ " cull=" + std::to_string((int)key.PipelineState.Cull)
				+ " prim=" + std::to_string((int)key.Primitive);
			TENLog("Pipeline cache: SDL_CreateGPUGraphicsPipeline failed — " + std::string(SDL_GetError()) + diagInfo,
				LogLevel::Warning);
		}
		_cache[key] = pipeline;
		return pipeline;
	}
}

#endif
