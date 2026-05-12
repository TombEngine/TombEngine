#pragma once

#include <cstdint>
#include <cstring>
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Graphics
{
	class IInputLayout;
}

namespace TEN::Renderer::Graphics
{
	// Immutable aggregate of all "small" pipeline state that, in modern APIs (Vulkan,
	// D3D12, Metal), is baked into a single Pipeline State Object. On DX11 we apply
	// the state piece-by-piece behind a single BindPipeline() call, with hash-based
	// dedup so identical PSOs back-to-back become a no-op.
	//
	// This structure intentionally mirrors VkPipelineCreateInfo's main fixed-function
	// fields so that a future Vulkan backend can hash this same descriptor and
	// look up / lazily create a VkGraphicsPipeline.
	//
	// Resource bindings (textures, samplers, constant buffers, vertex/index buffers)
	// are NOT part of the pipeline state — they are bound separately.
	struct RenderPipelineState
	{
		Shader        ShaderId       = Shader::None;
		BlendMode     Blend          = BlendMode::Opaque;
		DepthState    Depth          = DepthState::Write;
		CullMode      Cull           = CullMode::CounterClockwise;
		PrimitiveType Topology       = PrimitiveType::TriangleList;
		IInputLayout* InputLayout    = nullptr;
		AlphaTestMode AlphaTest      = AlphaTestMode::None;
		float         AlphaThreshold = 0.0f;

		// MSAA sample count. 1 = no multisampling. Required by Vulkan/SDL_GPU PSO since the
		// pipeline must match the sample count of the render pass it's used with. DX11
		// ignores this (it's a property of the texture) but the field is part of the hash
		// so a future MSAA pass uses a distinct cached pipeline.
		int SampleCount = 1;

		// 64-bit hash combining all fields. Two equal hashes guarantee equal PSO,
		// because we pack each enum into a fixed-width slot.
		inline uint64_t Hash() const
		{
			// Pack the enum bytes (each enum value is <256 in this codebase).
			uint64_t h = 0;
			h |= (uint64_t)((uint8_t)(int)ShaderId)    << 0;   // 16 bits is enough
			h |= (uint64_t)((uint16_t)(int)ShaderId)   << 8;
			h |= (uint64_t)((uint8_t)(int)Blend)       << 24;
			h |= (uint64_t)((uint8_t)(int)Depth)       << 32;
			h |= (uint64_t)((uint8_t)(int)Cull)        << 40;
			h |= (uint64_t)((uint8_t)(int)Topology)    << 48;
			h |= (uint64_t)((uint8_t)(int)AlphaTest)   << 56;

			// Mix in InputLayout pointer, AlphaThreshold and SampleCount via a secondary 64-bit value.
			uint64_t h2 = (uint64_t)(uintptr_t)InputLayout;
			uint32_t at;
			std::memcpy(&at, &AlphaThreshold, sizeof(at));
			h2 ^= (uint64_t)at << 32;
			h2 ^= (uint64_t)(uint8_t)SampleCount << 24;

			// Splitmix-style combine.
			h2 ^= h2 >> 33;
			h2 *= 0xff51afd7ed558ccdULL;
			h2 ^= h2 >> 33;
			h2 *= 0xc4ceb9fe1a85ec53ULL;
			h2 ^= h2 >> 33;

			return h ^ h2;
		}

		inline bool operator==(const RenderPipelineState& o) const
		{
			return ShaderId       == o.ShaderId    &&
			       Blend          == o.Blend       &&
			       Depth          == o.Depth       &&
			       Cull           == o.Cull        &&
			       Topology       == o.Topology    &&
			       InputLayout    == o.InputLayout &&
			       AlphaTest      == o.AlphaTest   &&
			       AlphaThreshold == o.AlphaThreshold &&
			       SampleCount    == o.SampleCount;
		}
	};
}
