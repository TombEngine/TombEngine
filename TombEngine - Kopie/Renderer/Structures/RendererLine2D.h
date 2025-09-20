#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererLine2D
	{
		Vector2 Origin = Vector2::Zero;
		Vector2 Target = Vector2::Zero;
		Vector4 Color  = Vector4::Zero;
	};
}
