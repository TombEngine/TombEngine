#pragma once

#include <cstdint>
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Graphics
{
	class IInputLayout;

	// Immutable snapshot of GPU pipeline state. DX11 applies it as a sequence of gated
	// Set*/BindShader calls; a future SDL_GPU backend hashes it to cache SDL_GPUGraphicsPipeline
	// objects. Render-target formats are intentionally omitted for now because DX11 doesn't need
	// them — they'll be added when the SDL_GPU backend lands.
	struct RenderPipelineState
	{
		BlendMode     Blend       = BlendMode::Opaque;
		DepthState    Depth       = DepthState::Write;
		CullMode      Cull        = CullMode::CounterClockwise;
		PrimitiveType Topology    = PrimitiveType::TriangleList;
		Shader        ShaderId    = Shader::None;
		IInputLayout* InputLayout = nullptr;

		bool operator==(const RenderPipelineState& rhs) const
		{
			return Blend == rhs.Blend
				&& Depth == rhs.Depth
				&& Cull == rhs.Cull
				&& Topology == rhs.Topology
				&& ShaderId == rhs.ShaderId
				&& InputLayout == rhs.InputLayout;
		}

		bool operator!=(const RenderPipelineState& rhs) const { return !(*this == rhs); }

		uint64_t Hash() const
		{
			uint64_t h = 1469598103934665603ull; // FNV-1a offset basis
			auto mix = [&](uint64_t v) { h ^= v; h *= 1099511628211ull; };
			mix((uint64_t)Blend);
			mix((uint64_t)Depth);
			mix((uint64_t)Cull);
			mix((uint64_t)Topology);
			mix((uint64_t)ShaderId);
			mix(reinterpret_cast<uint64_t>(InputLayout));
			return h;
		}
	};
}
