#pragma once
#include <SimpleMath.h>
#include "Renderer/Graphics/ITexture2D.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Renderer::Graphics;
	using namespace DirectX::SimpleMath;

	constexpr int VIDEO_SPRITE_ID = NO_VALUE;

	struct RendererSprite
	{
		int Index;
		int Width;
		int Height;
		Vector2 UV[4];
		ITexture2D* Texture;
		int X;
		int Y;
	};
}
