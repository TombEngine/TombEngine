#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererAnimatedTexture
	{
		Vector2 UV[4];
		Vector2 NormalizedUV[4];
	};
}
