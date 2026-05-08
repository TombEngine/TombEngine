#pragma once
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	struct RendererMesh
	{
		LightMode LightMode;
		BoundingSphere Sphere;
		std::vector<RendererBucket> Buckets;
		std::vector<Vector3> Positions;
	};
}
