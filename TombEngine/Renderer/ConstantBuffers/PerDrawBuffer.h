#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	// Combined per-draw constant buffer (was CMaterialBuffer + CBlendingBuffer). Both lived
	// at high update frequency (per material switch and per blend mode switch, often back-to-
	// back inside the same bucket loop), so merging halves the Map/Unmap traffic on the hot
	// draw path. Layout matches HLSL CBPerDraw — keep them in sync.
	struct alignas(16) CPerDrawBuffer
	{
		Vector4      MaterialParameters0;
		//--
		Vector4      MaterialParameters1;
		//--
		Vector4      MaterialParameters2;
		//--
		Vector4      MaterialParameters3;
		//--
		unsigned int MaterialTypeAndFlags;
		unsigned int BlendMode;
		int          AlphaTest;
		float        AlphaThreshold;
	};
}
