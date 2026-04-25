#pragma once
#include <SimpleMath.h>
#include <cstdint>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	// Combined per-draw constant buffer (was CMaterialBuffer + CBlendingBuffer + CAnimatedBuffer
	// metadata). All three updated at the same per-draw frequency, so folding them halves the
	// Map/Unmap traffic on the hot draw path. The 256-entry animated frames array now lives in
	// a dedicated structured buffer; only its scalar metadata stays here. Layout matches HLSL
	// CBPerDraw — keep them in sync.
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
		//--
		uint32_t     NumAnimFrames;
		uint32_t     AnimFps;
		uint32_t     AnimType;
		uint32_t     Animated;
		//--
		float        UVRotateDirection;
		float        UVRotateSpeed;
		int          IsWaterfall;
		int          PerDrawBuffer_Padding0;
	};
}
