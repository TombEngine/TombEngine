#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Sprite ID that selects the video texture instead of a sprite of the level.
	constexpr int VIDEO_SPRITE_ID = NO_VALUE;

	// Sprite of a sprite page: Width and Height in pixels, UV of the four corners.
	struct RendererSprite
	{
		int Width;
		int Height;
		Vector2 UV[4];
	};
}
