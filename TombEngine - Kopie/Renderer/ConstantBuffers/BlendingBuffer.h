#pragma once

namespace TEN::Renderer::ConstantBuffers
{
	struct alignas(16) CBlendingBuffer
	{
		unsigned int BlendMode;
		int AlphaTest;
		float AlphaThreshold;
	};
}