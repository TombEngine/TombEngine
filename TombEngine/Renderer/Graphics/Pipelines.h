#pragma once

#include "Renderer/Graphics/RenderPipelineState.h"

namespace TEN::Renderer::Graphics
{
	// Pre-defined Pipeline State Objects used by the engine.
	//
	// Each function returns a fully-populated RenderPipelineState ready to be passed
	// to Renderer::BindPipeline. Callers typically only override Blend/AlphaTest for
	// per-bucket variations.
	//
	// InputLayout is a per-renderer pointer so it's passed in rather than baked here.
	namespace Pipelines
	{
		// Sorted (back-to-front) transparent pass: room geometry.
		inline RenderPipelineState SortedRoom(IInputLayout* vertexInputLayout, BlendMode blend)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::Rooms;
			pso.Blend       = blend;
			pso.Depth       = DepthState::Read;
			pso.Cull        = CullMode::CounterClockwise;
			pso.Topology    = PrimitiveType::TriangleList;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}

		// Sorted moveables (items, hair).
		inline RenderPipelineState SortedItem(IInputLayout* vertexInputLayout, BlendMode blend)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::Items;
			pso.Blend       = blend;
			pso.Depth       = DepthState::Read;
			pso.Cull        = CullMode::CounterClockwise;
			pso.Topology    = PrimitiveType::TriangleList;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}

		// Sorted statics. Same binary as Items (the ShaderManager group dedup will
		// skip the actual device shader rebind), but kept separately so the enum
		// reflects the call site's intent.
		inline RenderPipelineState SortedStatic(IInputLayout* vertexInputLayout, BlendMode blend)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::InstancedStatics;
			pso.Blend       = blend;
			pso.Depth       = DepthState::Read;
			pso.Cull        = CullMode::CounterClockwise;
			pso.Topology    = PrimitiveType::TriangleList;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}

		// Moveable rendered with a static-mesh world matrix (no per-bone skinning).
		// Reads alpha-tested texels for things like trees pretending to be props.
		inline RenderPipelineState SortedMoveableAsStatic(IInputLayout* vertexInputLayout, BlendMode blend)
		{
			RenderPipelineState pso;
			pso.ShaderId       = Shader::InstancedStatics;
			pso.Blend          = blend;
			pso.Depth          = DepthState::Read;
			pso.Cull           = CullMode::CounterClockwise;
			pso.Topology       = PrimitiveType::TriangleList;
			pso.InputLayout    = vertexInputLayout;
			pso.AlphaTest      = AlphaTestMode::GreatherThan;
			pso.AlphaThreshold = 0.5f; // ALPHA_TEST_THRESHOLD constant on the engine side
			return pso;
		}

		// Sorted sprites (instanced billboards / particles).
		inline RenderPipelineState SortedSprite(IInputLayout* vertexInputLayout, BlendMode blend)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::InstancedSprites;
			pso.Blend       = blend;
			pso.Depth       = DepthState::Read;
			pso.Cull        = CullMode::None;
			pso.Topology    = PrimitiveType::TriangleList;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}
	}
}
