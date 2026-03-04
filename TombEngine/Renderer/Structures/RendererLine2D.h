#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererLine2D
	{
		Vector2 Origin = Vector2::Zero;
		Vector2 Target = Vector2::Zero;
		Vector4 Color  = Vector4::Zero;
	};
}
