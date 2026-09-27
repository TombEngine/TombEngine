#pragma once

#include "Renderer/Graphics/ITexture2D.h"

namespace TEN::Renderer::Graphics
{
	class IRenderTarget2D : public ITexture2D
	{
	public:
		virtual int GetArraySize() = 0;
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
