#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererPolygon
	{
		Vector3 Centre;
		Vector3 Normal;
		unsigned char Shape;
		int BaseIndex;
	};
}
