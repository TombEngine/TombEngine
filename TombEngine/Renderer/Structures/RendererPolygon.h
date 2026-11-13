#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Polygon of a bucket: centre for depth sorting, shape (0 = quad, otherwise triangle) and first index.
	struct RendererPolygon
	{
		Vector3 Centre;
		unsigned char Shape;
		int BaseIndex;
	};
}
