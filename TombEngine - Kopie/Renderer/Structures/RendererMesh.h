#pragma once
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace DirectX;
	using namespace DirectX::SimpleMath;

	struct RendererMesh
	{
		LightMode LightMode;
		BoundingSphere Sphere;
		std::vector<RendererBucket> Buckets;
		std::vector<Vector3> Positions;
	};
}
