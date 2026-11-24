#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Fog volume collected from a FogBulb light; Distance is measured from the camera and FogBulbToCameraVector points from the bulb to it.
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
