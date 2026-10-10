#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	constexpr int VIDEO_SPRITE_ID = NO_VALUE;

	struct RendererSprite
	{
		int Index;
		int Width;
		int Height;
		Vector2 UV[4];
		int X;
		int Y;
	};
}
