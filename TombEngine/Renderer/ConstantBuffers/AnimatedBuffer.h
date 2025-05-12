#pragma once
#include <array>
#include <cstdint>
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

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
		uint32_t NumFrames;
		uint32_t Fps;
		uint32_t Type;
		uint32_t Padding;
	};
}