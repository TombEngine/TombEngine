#pragma once
#include "Math/SimpleMath.h"
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Collision;
	using namespace TEN::Math::Library;

	// Mesh converted from the level data: bounding sphere and positions in object space, and its buckets, whose ranges
	// point into the shared vertex and index arrays of the renderer.
	struct RendererMesh
	{
		LightMode LightMode;
		BoundingSphere Sphere;
		std::vector<RendererBucket> Buckets;
		std::vector<Vector3> Positions;
	};
}
