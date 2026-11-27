#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// One frame of an animated texture set: the UV of the four corners in the texture atlas, and the same corners
	// normalized to 0 or 1 within the frame bounds.
	struct RendererAnimatedTexture
	{
		Vector2 UV[4];
		Vector2 NormalizedUV[4];
	};
}
