#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererFogBulb
	{
		Vector3 Position;
		float Density;
		Vector3 Color;
		float Radius;
		float Distance;
		Vector3 FogBulbToCameraVector;
	};
}
