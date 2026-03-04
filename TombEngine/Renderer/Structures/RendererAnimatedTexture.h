#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererAnimatedTexture
	{
		Vector2 UV[4];
		Vector2 NormalizedUV[4];
	};
}
