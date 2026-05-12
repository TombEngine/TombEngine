#pragma once

#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Graphics
{
	class IDepthTarget
	{
	public:
		virtual int         GetArraySize() = 0;
		// Backend-portable depth/stencil format. Required for Vulkan/SDL_GPU PSO creation.
		virtual DepthFormat GetFormat() = 0;
		virtual ~IDepthTarget() = default;
	};

	struct IDepthTargetBinding
	{
		IDepthTarget* DepthTarget;
		int ArrayIndex;

		IDepthTargetBinding(IDepthTarget* depthTarget, int arrayIndex)
		{
			DepthTarget = depthTarget;
			ArrayIndex = arrayIndex;
		}
	};
}
