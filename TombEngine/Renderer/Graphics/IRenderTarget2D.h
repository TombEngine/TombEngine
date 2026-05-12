#pragma once

#include "Renderer/Graphics/ITexture2D.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Graphics
{
	class IRenderTarget2D : public ITexture2D
	{
	public:
		virtual int           GetArraySize() = 0;
		// Backend-portable pixel format. Required for Vulkan/SDL_GPU PSO creation,
		// where the PSO must be compatible with the render pass it'll be used in.
		virtual SurfaceFormat GetFormat() = 0;
		virtual ~IRenderTarget2D() = default;
	};

	struct IRenderTargetBinding
	{
		IRenderTarget2D* RenderTarget;
		int ArrayIndex;

		IRenderTargetBinding(IRenderTarget2D* renderTarget, int arrayIndex)
		{
			RenderTarget = renderTarget;
			ArrayIndex = arrayIndex;
		}
	};
}
