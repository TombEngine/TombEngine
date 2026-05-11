#pragma once

#include <string>
#include <vector>
#include <SimpleMath.h>
#include "Renderer/Graphics/IRenderTarget2D.h"
#include "Renderer/Graphics/IDepthTarget.h"
#include "Renderer/Structures/RendererRectangle.h"
#include "Renderer/Structures/RendererViewport.h"

namespace TEN::Renderer::Graphics
{
	using DirectX::SimpleMath::Vector4;
	using TEN::Renderer::Structures::RendererRectangle;
	using TEN::Renderer::Structures::RendererViewport;

	// What to do with an attachment when the render pass starts.
	enum class LoadAction
	{
		Load,       // Keep existing contents.
		Clear,      // Clear to ClearColor / ClearDepth / ClearStencil.
		DontCare    // No guarantee; faster on tile-based GPUs (Vulkan/Metal hint).
	};

	// What to do with an attachment when the render pass ends.
	enum class StoreAction
	{
		Store,      // Persist the contents (default).
		DontCare,   // Contents may be discarded after the pass (no resolve needed).
		Resolve     // MSAA resolve into the resolve target (future).
	};

	struct ColorAttachmentDescriptor
	{
		IRenderTarget2D* Target     = nullptr;
		int              ArrayIndex = 0;
		LoadAction       Load       = LoadAction::Load;
		StoreAction      Store      = StoreAction::Store;
		Vector4          ClearColor = Vector4(0, 0, 0, 0);
	};

	struct DepthAttachmentDescriptor
	{
		IDepthTarget* Target       = nullptr;
		int           ArrayIndex   = 0;
		LoadAction    Load         = LoadAction::Load;
		StoreAction   Store        = StoreAction::Store;
		float         ClearDepth   = 1.0f;
		unsigned char ClearStencil = 0;
	};

	// Declarative description of a render pass: which attachments are bound, what their
	// initial / final state should be, and the viewport/scissor for draws inside the pass.
	//
	// On DX11 this maps to OMSetRenderTargets + Clear* + RSSetViewports + RSSetScissorRects.
	// On Vulkan this maps 1:1 to vkCmdBeginRenderPass with VkAttachmentLoadOp/StoreOp.
	struct RenderPassDescriptor
	{
		std::vector<ColorAttachmentDescriptor> ColorAttachments;
		DepthAttachmentDescriptor              DepthAttachment;

		bool              HasViewport = false;
		RendererViewport  Viewport    = {};

		bool              HasScissor  = false;
		RendererRectangle Scissor     = {};

		std::string       DebugLabel; // RenderDoc / PIX / Aftermath annotation.
	};
}
