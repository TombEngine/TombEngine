#pragma once

namespace TEN::Renderer::Graphics
{
	class IDepthTarget
	{
	public:
		virtual int GetArraySize() = 0;
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
