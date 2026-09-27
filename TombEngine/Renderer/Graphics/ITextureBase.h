#pragma once

namespace TEN::Renderer::Graphics
{
	class ITextureBase
	{
	public:
		virtual int GetWidth() = 0;
		virtual int GetHeight() = 0;
		virtual ~ITextureBase() = default;
	};
}
