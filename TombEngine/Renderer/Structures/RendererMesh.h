#pragma once
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Collision;
	using namespace TEN::Math::Library;

	struct RendererMesh
	{
		LightMode LightMode;
		BoundingSphere Sphere;
		std::vector<RendererBucket> Buckets;
		std::vector<Vector3> Positions;
	};
}
