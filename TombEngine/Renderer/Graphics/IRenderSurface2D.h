#pragma once

#include "Renderer/Graphics/IRenderTarget2D.h"
#include "Renderer/Graphics/IDepthTarget.h"

namespace TEN::Renderer::Graphics
{
	class IRenderSurface2D
	{
	private:
		std::unique_ptr<IRenderTarget2D>	_renderTarget	= nullptr;
		std::unique_ptr<IDepthTarget>		_depthTarget	= nullptr;

	public:
		IRenderSurface2D(std::unique_ptr<IRenderTarget2D> renderTarget,
		                 std::unique_ptr<IDepthTarget>    depthTarget)
			: _renderTarget(std::move(renderTarget)),
			  _depthTarget(std::move(depthTarget))
		{
		}

		virtual ~IRenderSurface2D() = default;

		IRenderTarget2D* GetRenderTarget() const { return _renderTarget.get(); }
		IDepthTarget* GetDepthTarget() const { return _depthTarget.get(); }
	};
}
