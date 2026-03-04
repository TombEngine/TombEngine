#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

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
