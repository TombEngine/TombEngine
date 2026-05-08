#pragma once
namespace TEN::Renderer::Structures
{
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
