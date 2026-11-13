#pragma once
#include <vector>
#include "Math/SimpleMath.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/Structures/RendererPolygon.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Polygons sharing texture, blend mode, material and animation, with their vertex and index ranges in the level
	// geometry arrays.
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
		Vector3 BoundsMin = Vector3::Zero; // Box of the bucket vertices: world space for rooms, object space for meshes.
		Vector3 BoundsMax = Vector3::Zero;
		std::vector<RendererPolygon> Polygons;
	};
}
