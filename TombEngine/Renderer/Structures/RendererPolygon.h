#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererPolygon
	{
		Vector3 Centre;
		unsigned char Shape;
		int BaseIndex;
	};
}
