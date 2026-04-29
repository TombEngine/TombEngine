#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>
#include "Renderer/Graphics/IRenderTarget2D.h"
#include "Renderer/Graphics/IDepthTarget.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/Structures/RendererViewport.h"

namespace TEN::Renderer::Graphics
{
	using namespace TEN::Renderer::Structures;

	enum class LoadAction  { Load, Clear, DontCare };
	enum class StoreAction { Store, DontCare };

	struct ColorAttachmentDescriptor
	{
		IRenderTarget2D*     RenderTarget = nullptr;
		int                  ArrayIndex   = 0;
		LoadAction           LoadAction   = LoadAction::Load;
		StoreAction          StoreAction  = StoreAction::Store;
		DirectX::XMVECTORF32 ClearColor   = { 0, 0, 0, 0 };
	};

	struct DepthAttachmentDescriptor
	{
		IDepthTarget*          DepthTarget  = nullptr;
		int                    ArrayIndex   = 0;
		LoadAction             LoadAction   = LoadAction::Load;
		StoreAction            StoreAction  = StoreAction::Store;
		float                  ClearDepth   = 1.0f;
		unsigned char          ClearStencil = 0;
		DepthStencilClearFlags ClearFlags   = DepthStencilClearFlags::DepthAndStencil;
	};

	struct RenderPassDescriptor
	{
		std::string                            Name;
		std::vector<ColorAttachmentDescriptor> ColorAttachments;
		DepthAttachmentDescriptor              DepthAttachment;
		RendererViewport                       Viewport;
	};
}
