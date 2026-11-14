#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	constexpr int VIDEO_SPRITE_ID = NO_VALUE;

	struct RendererSprite
	{
		int Width;
		int Height;
		Vector2 UV[4];
	};
}
