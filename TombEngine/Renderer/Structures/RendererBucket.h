#pragma once
#include <vector>
#include "Math/SimpleMath.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/Structures/RendererPolygon.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererBucket
	{
		int Texture;
		bool Animated;
		BlendMode BlendMode;
		int MaterialIndex;
		int StartVertex;
		int StartIndex;
		int NumVertices;
		int NumIndices;
		Vector3 Centre;
		std::vector<RendererPolygon> Polygons;
	};
}
