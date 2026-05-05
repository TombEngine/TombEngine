#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	// Layout matches HLSL StructuredBuffer<AnimatedFrameUV> in AnimatedTextures.hlsli.
	struct AnimatedFrame
	{
		Vector2 TopLeft;
		Vector2 TopRight;
		Vector2 BottomRight;
		Vector2 BottomLeft;
	};

	constexpr int MAX_ANIMATED_FRAMES = 256;
}
