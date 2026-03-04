#pragma once
#include <array>
#include <cstdint>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace TEN::Math::Library;

	struct AnimatedFrame
	{
		Vector2 TopLeft;
		Vector2 TopRight;
		//--
		Vector2 BottomRight;
		Vector2 BottomLeft;
	};

	struct alignas(16) CAnimatedBuffer
	{
		std::array<AnimatedFrame, 256> Textures;
		//--
		unsigned int NumFrames;
		unsigned int Fps;
		unsigned int Type;
		unsigned int Animated;
		//--
		float UVRotateDirection;
		float UvRotateSpeed;
		int IsWaterfall;
		int CAnimatedBuffer_Padding0;
	};
}