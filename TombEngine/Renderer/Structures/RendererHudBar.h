#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// HUD bar layout in display space; the scene renderer builds its border and inner quads.
	struct RendererHudBar
	{
		static constexpr auto SIZE_DEFAULT = Vector2(150.0f, 10.0f);

		Vector2 Size			 = Vector2::Zero;
		Vector2 Position		 = Vector2::Zero;
		Vector2 VerticalPosition = Vector2::Zero; // Position used on portrait screens.

		RendererHudBar(const Vector2& pos, const Vector2& verticalPos, const Vector2& size) :
			Size(size), Position(pos), VerticalPosition(verticalPos)
		{
		}

		RendererHudBar(const Vector2& pos, const Vector2& size) :
			RendererHudBar(pos, pos, size)
		{
		}
	};
}
