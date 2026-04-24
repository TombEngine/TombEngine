#pragma once

#include "Renderer/Graphics/RenderPipelineState.h"

// Recipes that build RenderPipelineState values for the common pass flavours.
// Kept as inline functions (not constexpr globals) because the input layout pointer
// is only known at runtime after the renderer has created its layouts.
namespace TEN::Renderer::Graphics::Pipelines
{
	// Full-screen post-process triangle: opaque, depth write, CCW cull, triangle list.
	// The shader varies per effect; layout is the dedicated post-process vertex layout.
	inline RenderPipelineState PostProcess(Shader shader, IInputLayout* postProcessLayout)
	{
		return { BlendMode::Opaque, DepthState::Write, CullMode::CounterClockwise,
				 PrimitiveType::TriangleList, shader, postProcessLayout };
	}

	// Standard opaque geometry pass.
	inline RenderPipelineState Opaque(Shader shader, IInputLayout* vertexLayout,
									  CullMode cull = CullMode::CounterClockwise)
	{
		return { BlendMode::Opaque, DepthState::Write, cull,
				 PrimitiveType::TriangleList, shader, vertexLayout };
	}

	// Transparent / blended geometry, read-only depth.
	inline RenderPipelineState Transparent(Shader shader, IInputLayout* vertexLayout,
										   BlendMode blend = BlendMode::AlphaBlend,
										   CullMode cull = CullMode::CounterClockwise)
	{
		return { blend, DepthState::Read, cull,
				 PrimitiveType::TriangleList, shader, vertexLayout };
	}

	// Additive-blended geometry.
	inline RenderPipelineState Additive(Shader shader, IInputLayout* vertexLayout,
										CullMode cull = CullMode::CounterClockwise)
	{
		return { BlendMode::Additive, DepthState::Read, cull,
				 PrimitiveType::TriangleList, shader, vertexLayout };
	}

	// Shadow map pass: depth-only write, front-face culled.
	inline RenderPipelineState ShadowMap(Shader shader, IInputLayout* vertexLayout)
	{
		return { BlendMode::Opaque, DepthState::Write, CullMode::Clockwise,
				 PrimitiveType::TriangleList, shader, vertexLayout };
	}

	// 2D HUD / UI: alpha blend, no depth.
	inline RenderPipelineState Hud2D(Shader shader, IInputLayout* vertexLayout,
									 BlendMode blend = BlendMode::AlphaBlend)
	{
		return { blend, DepthState::None, CullMode::None,
				 PrimitiveType::TriangleList, shader, vertexLayout };
	}
}
