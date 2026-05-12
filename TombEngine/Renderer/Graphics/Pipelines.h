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
		// Opaque/AlphaTest room geometry. Per-bucket blend/alpha-test variations come
		// later via SetBlendMode/SetAlphaTest inside the bucket loop (DX11 model). On
		// Vulkan/SDL_GPU the backend lazy-creates a distinct PSO for each combination.
		inline RenderPipelineState Rooms(IInputLayout* vertexInputLayout)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::Rooms;
			pso.Blend       = BlendMode::Opaque;
			pso.Depth       = DepthState::Write;
			pso.Cull        = CullMode::CounterClockwise;
			pso.Topology    = PrimitiveType::TriangleList;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}

		// G-Buffer pass over room geometry. Pixel stage Shader::GBuffer (PS-only) +
		// vertex stage Shader::GBufferRooms (VS-only).
		inline RenderPipelineState GBufferRooms(IInputLayout* vertexInputLayout)
		{
			RenderPipelineState pso;
			pso.VertexShaderId = Shader::GBufferRooms;
			pso.ShaderId       = Shader::GBuffer;
			pso.Blend          = BlendMode::Opaque;
			pso.Depth          = DepthState::Write;
			pso.Cull           = CullMode::CounterClockwise;
			pso.Topology       = PrimitiveType::TriangleList;
			pso.InputLayout    = vertexInputLayout;
			pso.AlphaTest      = AlphaTestMode::None;
			return pso;
		}

		// Moveable items, opaque/alpha-test pass.
		inline RenderPipelineState Items(IInputLayout* vertexInputLayout)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::Items;
			pso.Blend       = BlendMode::Opaque;
			pso.Depth       = DepthState::Write;
			pso.Cull        = CullMode::CounterClockwise;
			pso.Topology    = PrimitiveType::TriangleList;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}

		// G-Buffer pass for moveable items.
		inline RenderPipelineState GBufferItems(IInputLayout* vertexInputLayout)
		{
			RenderPipelineState pso;
			pso.VertexShaderId = Shader::GBufferItems;
			pso.ShaderId       = Shader::GBuffer;
			pso.Blend          = BlendMode::Opaque;
			pso.Depth          = DepthState::Write;
			pso.Cull           = CullMode::CounterClockwise;
			pso.Topology       = PrimitiveType::TriangleList;
			pso.InputLayout    = vertexInputLayout;
			pso.AlphaTest      = AlphaTestMode::None;
			return pso;
		}

		// Instanced static meshes.
		inline RenderPipelineState InstancedStatics(IInputLayout* vertexInputLayout)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::InstancedStatics;
			pso.Blend       = BlendMode::Opaque;
			pso.Depth       = DepthState::Write;
			pso.Cull        = CullMode::CounterClockwise;
			pso.Topology    = PrimitiveType::TriangleList;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}

		// G-Buffer pass for instanced statics.
		inline RenderPipelineState GBufferInstancedStatics(IInputLayout* vertexInputLayout)
		{
			RenderPipelineState pso;
			pso.VertexShaderId = Shader::GBufferInstancedStatics;
			pso.ShaderId       = Shader::GBuffer;
			pso.Blend          = BlendMode::Opaque;
			pso.Depth          = DepthState::Write;
			pso.Cull           = CullMode::CounterClockwise;
			pso.Topology       = PrimitiveType::TriangleList;
			pso.InputLayout    = vertexInputLayout;
			pso.AlphaTest      = AlphaTestMode::None;
			return pso;
		}

		// Shadow map pass — depth-only render of shadow casters into a cube/array slice.
		inline RenderPipelineState ShadowMap(IInputLayout* vertexInputLayout)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::ShadowMap;
			pso.Blend       = BlendMode::Opaque;
			pso.Depth       = DepthState::Write;
			pso.Cull        = CullMode::CounterClockwise;
			pso.Topology    = PrimitiveType::TriangleList;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest      = AlphaTestMode::GreatherThan;
			pso.AlphaThreshold = ALPHA_TEST_THRESHOLD;
			return pso;
		}

		// Solid-color debug lines/triangles. Both blend (Opaque for 2D HUD lines, Additive
		// for 3D debug lines/triangles) and topology vary by caller.
		inline RenderPipelineState SolidDebug(IInputLayout* vertexInputLayout, BlendMode blend = BlendMode::Additive, PrimitiveType topology = PrimitiveType::LineList)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::Solid;
			pso.Blend       = blend;
			pso.Depth       = DepthState::Read;
			pso.Cull        = CullMode::None;
			pso.Topology    = topology;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}

		// Instanced sprite batch (stars, lens flares, lasers).
		inline RenderPipelineState InstancedSprites(IInputLayout* vertexInputLayout, BlendMode blend = BlendMode::Additive, PrimitiveType topology = PrimitiveType::TriangleStrip)
		{
			RenderPipelineState pso;
			pso.ShaderId    = Shader::InstancedSprites;
			pso.Blend       = blend;
			pso.Depth       = DepthState::Read;
			pso.Cull        = CullMode::None;
			pso.Topology    = topology;
			pso.InputLayout = vertexInputLayout;
			pso.AlphaTest   = AlphaTestMode::None;
			return pso;
		}

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
