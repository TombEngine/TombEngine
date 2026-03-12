#pragma once

#include <functional>
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Graphics
{
	struct RenderPipelineState
	{
		BlendMode     Blend    = BlendMode::Opaque;
		DepthState    Depth    = DepthState::Write;
		CullMode      Cull     = CullMode::CounterClockwise;

		bool operator==(const RenderPipelineState& other) const
		{
			return Blend == other.Blend && Depth == other.Depth && Cull == other.Cull;
		}

		bool operator!=(const RenderPipelineState& other) const
		{
			return !(*this == other);
		}
	};

	struct RenderPipelineStateHash
	{
		size_t operator()(const RenderPipelineState& s) const
		{
			size_t h = 0;
			h ^= std::hash<int>{}(static_cast<int>(s.Blend)) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<int>{}(static_cast<int>(s.Depth)) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<int>{}(static_cast<int>(s.Cull))  + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	namespace Pipelines
	{
		constexpr RenderPipelineState OpaqueDefault     = { BlendMode::Opaque,     DepthState::Write, CullMode::CounterClockwise };
		constexpr RenderPipelineState AlphaTestDefault   = { BlendMode::AlphaTest,  DepthState::Write, CullMode::CounterClockwise };
		constexpr RenderPipelineState Additive           = { BlendMode::Additive,   DepthState::Read,  CullMode::CounterClockwise };
		constexpr RenderPipelineState AdditiveNoCull     = { BlendMode::Additive,   DepthState::Read,  CullMode::None };
		constexpr RenderPipelineState AlphaBlend         = { BlendMode::AlphaBlend, DepthState::Read,  CullMode::CounterClockwise };
		constexpr RenderPipelineState FullscreenPass     = { BlendMode::Opaque,     DepthState::Write, CullMode::CounterClockwise };
		constexpr RenderPipelineState DebugLines         = { BlendMode::Additive,   DepthState::Read,  CullMode::None };
		constexpr RenderPipelineState HudNoDepth         = { BlendMode::Opaque,     DepthState::None,  CullMode::None };
		constexpr RenderPipelineState Lines2D            = { BlendMode::Opaque,     DepthState::Read,  CullMode::None };
		constexpr RenderPipelineState OpaqueNoCull       = { BlendMode::Opaque,     DepthState::Write, CullMode::None };
		constexpr RenderPipelineState OpaqueCW           = { BlendMode::Opaque,     DepthState::Write, CullMode::Clockwise };
	}
}
