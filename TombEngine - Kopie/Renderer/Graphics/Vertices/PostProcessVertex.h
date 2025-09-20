#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Graphics::Vertices
{
	using namespace DirectX::SimpleMath;

	struct PostProcessVertex
	{
		Vector3 Position = Vector3::Zero;
		Vector2 UV = Vector3::Zero;
		Vector4 Color = Vector4::One;
	};
}