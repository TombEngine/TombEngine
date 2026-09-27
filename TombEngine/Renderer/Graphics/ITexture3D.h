#pragma once

#include "Renderer/Graphics/ITextureBase.h"

namespace TEN::Renderer::Graphics
{
	class ITexture3D : public virtual ITextureBase
	{
	public:
		virtual ~ITexture3D() = default;
		virtual int GetDepth() = 0;
		virtual bool IsValid() = 0;
	};
}
