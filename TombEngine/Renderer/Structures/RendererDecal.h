#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererDecal
	{
		Vector3 Position;
		int Pattern;
		float Radius;
		float Opacity;
	};
}
