#pragma once

#include "Renderer/Graphics/ITextureBase.h"

namespace TEN::Renderer::Graphics
{
	class ITexture2D : public virtual ITextureBase
	{
	public:
		virtual ~ITexture2D() = default;
		virtual bool IsValid() = 0;
	};
}
